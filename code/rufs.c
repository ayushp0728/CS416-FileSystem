/*
 *  Copyright (C) 2025 CS416 Rutgers CS
 *	Rutgers Tiny File System
 *	File:	rufs.c
 *
 */

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/time.h>
#include <libgen.h>
#include <limits.h>

#include "block.h"
#include "rufs.h"

char diskfile_path[PATH_MAX];



// Declare your in-memory data structures here
struct superblock sb;
bitmap_t inode_bitmap;
bitmap_t data_bitmap;

/* 
 * Get available inode number from bitmap
 */
int get_avail_ino() {

	// Step 1: Read inode bitmap from disk
	
	// Step 2: Traverse inode bitmap to find an available slot

	// Step 3: Update inode bitmap and write to disk 

	for(int i = 0; i < sb.max_inum; i++) {
		if(get_bitmap(inode_bitmap, i)==0) {
			set_bitmap(inode_bitmap, i);
			bio_write(sb.i_bitmap_blk, (char *)inode_bitmap);
			return i;
		}
	}
	return -1;
}

/* 
 * Get available data block number from bitmap
 */
int get_avail_blkno() {

	// Step 1: Read data block bitmap from disk
	
	// Step 2: Traverse data block bitmap to find an available slot

	// Step 3: Update data block bitmap and write to disk 
	for(int i = 0; i < sb.max_dnum; i++) {
		if(get_bitmap(data_bitmap, i)==0) {
			set_bitmap(data_bitmap, i);
			bio_write(sb.d_bitmap_blk, data_bitmap);
			return sb.d_start_blk + i;
		}
	}
	return -1;
}

/* 
 * inode operations
 */
int readi(uint16_t ino, struct inode *inode) {

	// Step 1: Get the inode's on-disk block number
	int InodeCount = BLOCK_SIZE / sizeof(struct inode);
	int BlockIndex = ino / InodeCount;
	// Step 2: Get offset of the inode in the inode on-disk block
	int Offset = ino % InodeCount;
	int ExactIndex = sb.i_start_blk + BlockIndex;
	
	char buf[BLOCK_SIZE];
	memset(buf, 0, BLOCK_SIZE);
	// Step 3: Read the block from disk and then copy into inode structure

	if(bio_read(ExactIndex, buf) < 0){return -1;}
	struct inode *InodeOnDisk = (struct inode *)buf;
	*inode = InodeOnDisk[Offset];
	return 0;
}

int writei(uint16_t ino, struct inode *inode) {

	// Step 1: Get the block number where this inode resides on disk
	int InodeCount = BLOCK_SIZE / sizeof(struct inode);
	int BlockIndex = ino / InodeCount;
	// Step 2: Get the offset in the block where this inode resides on disk
	int Offset = ino % InodeCount;
	int ExactIndex = sb.i_start_blk + BlockIndex;
	// Step 3: Write inode to disk 

	char buffer[BLOCK_SIZE];
	if(bio_read(ExactIndex, buffer) < 0){return -1;} 
	struct inode *DiskNode = (struct inode *)buffer;

	DiskNode[Offset] = *inode;

	if(bio_write(ExactIndex, buffer) < 0){return -1; } return 0;}
	
int dir_find(uint16_t ino, const char *fname, size_t name_len, struct dirent *dirent) {
	


   // Step 1: Call readi() to get the inode using ino (inode number of current directory)
   printf("dir_find: in dir=%d looking for '%s'\n", ino, fname);
   struct inode DirectoryToFind;
   if(readi(ino, &DirectoryToFind) == -1) {return -1;}
   // Step 2: Get data block of current directory from inode
   char block[BLOCK_SIZE];
   int Entries = BLOCK_SIZE / sizeof(struct dirent);
   int numDirect = sizeof(DirectoryToFind.direct_ptr) / sizeof(DirectoryToFind.direct_ptr[0]);
   // Step 3: Read directory's data block and check each directory entry.
   for(int i = 0; i < numDirect; i++) { // why is this 16?? because .h is set to 16? if they change in testing, this would fail
       int blockNum = DirectoryToFind.direct_ptr[i];
       if(blockNum == 0) {continue;}
       if(bio_read(blockNum, block) == -1) {return -1;}

       struct dirent *Dir = (struct dirent *)block;

       for(int j = 0; j < Entries; j++) {
           if(Dir[j].valid == 0) {
               continue;}
           // Step 4: Compare each directory entry's name with fname
           if(Dir[j].valid==1 && Dir[j].len == name_len && strncmp(Dir[j].name, fname, name_len) == 0) {
               // If the name matches, then copy directory entry to dirent structure
               *dirent = Dir[j];
               printf("dir_find: FOUND %s -> ino=%d\n", fname, dirent->ino);
               return 0;
           }
       }
   }
   // If the name matches, then copy directory entry to dirent structure


   return -1;
}



int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len) {
	// Step 1: Read dir_inode's data block and check each directory entry of dir_inode

	if(fname == NULL){ return -1;}
	int freeSlot = -1; 
	int freeBlock = -1; 
	int Insert = -1;

	char block[BLOCK_SIZE];
    int Entries = BLOCK_SIZE / sizeof(struct dirent);
	int numDirect = sizeof(dir_inode.direct_ptr) / sizeof(dir_inode.direct_ptr[0]);

	for(int i = 0; i < numDirect; i++){
		int blockNum = dir_inode.direct_ptr[i];
		
		if(bio_read(blockNum, block) == -1) { return -1;}
		if(blockNum == 0){
			if(freeSlot < 0){freeSlot = i;}
			continue;}
		struct dirent *DirectoryEntry = (struct dirent *)block;
	// Step 2: Check if fname (directory name) is already used in other entries

		for(int j = 0; j < Entries; j++) {
			if((DirectoryEntry[j].valid) && (DirectoryEntry[j].valid==1 && DirectoryEntry[j].len == name_len && strncmp(DirectoryEntry[j].name, fname, name_len) == 0)) {
			return -1;} //directory already exists.
			else if((!DirectoryEntry[j].valid)  && (freeBlock < 0)){freeBlock = i; Insert = j;}}
	}

	if(freeBlock >= 0){

		int blockNum = dir_inode.direct_ptr[freeBlock];

		if(bio_read(blockNum, block) == -1){return -1;}

		struct dirent *DirectoryEntry = (struct dirent *)block;
		
		struct dirent *directoryEntryNextAvailable = &DirectoryEntry[Insert];
		directoryEntryNextAvailable->ino   = f_ino;

		// clamp name_len to fit in the field
		if (name_len >= sizeof(directoryEntryNextAvailable->name)) {
			name_len = sizeof(directoryEntryNextAvailable->name) - 1;
		}
		directoryEntryNextAvailable->len   = name_len;
		directoryEntryNextAvailable->valid = 1;

		memset(directoryEntryNextAvailable->name, 0,
			sizeof(directoryEntryNextAvailable->name));
		memcpy(directoryEntryNextAvailable->name, fname, name_len);
		directoryEntryNextAvailable->name[name_len] = '\0';


		int offset = freeBlock * Entries + Insert;
		size_t size = (offset+1) * sizeof(struct dirent);

		if(dir_inode.size <= size){ dir_inode.size = size; dir_inode.vstat.st_size = dir_inode.size;}
		if(bio_write(blockNum,block) == -1){return -1;}

		return writei(dir_inode.ino, &dir_inode); 

	}
		// Step 3: Add directory entry in dir_inode's data block and write to disk

	if(freeSlot >= 0){

		int newBlock = get_avail_blkno();
		if(newBlock < 0){ return -1;}
		struct dirent *DirectoryEntry = (struct dirent *)block;
		
		dir_inode.direct_ptr[freeSlot] = newBlock;
		memset(block, 0, BLOCK_SIZE);

		struct dirent *directoryEntryNextAvailable = &DirectoryEntry[0];
		directoryEntryNextAvailable->ino   = f_ino;
		if (name_len >= sizeof(directoryEntryNextAvailable->name)) {
			name_len = sizeof(directoryEntryNextAvailable->name) - 1;
		}
		directoryEntryNextAvailable->len   = name_len;
		directoryEntryNextAvailable->valid = 1;

		memset(directoryEntryNextAvailable->name, 0,
			sizeof(directoryEntryNextAvailable->name));
		memcpy(directoryEntryNextAvailable->name, fname, name_len);
		directoryEntryNextAvailable->name[name_len] = '\0';



		int offset = freeSlot * Entries;
		size_t size = (offset+1) * sizeof(struct dirent);

		if(dir_inode.size < size){ dir_inode.size = size; dir_inode.vstat.st_size = dir_inode.size;}
		if(bio_write(newBlock,block) == -1){return -1;}
		return writei(dir_inode.ino, &dir_inode); 
	
	}
	// Allocate a new data block for this directory if it does not exist

	// Update directory inode

	// Write directory entry

	return -1;}

/* 
 * namei operation
 */
int get_node_by_path(const char *path, uint16_t ino, struct inode *inode) {
/*
	Really simple logic: we are just tracing if a path exists. 2 index, start of current dir and end of current dir. 
	think: /asdf/foo/bar/qwerty.
	 We first check if asdf exists, if it does move on, check if foo exists and so on. 
	and check if it exists. If it does, we move down the path until end or return failure.
	
	
	*/
	printf("get_node_by_path: path=%s\n", path);

	if(path == NULL){ 
		return -1;
	}

	// empty or root
	if(path[0] == '\0'){
		return readi(0, inode); // inode 0 = root
	}
	uint16_t lookupIno = ino;


	int Start = 0;
	int End;

	if(path[0]== '/'){ 
		Start++;}

	
	while(path[Start] != '\0'){
		End = Start;
	while((path[End] != '\0') && path[End] != '/'){
		End++;
	}

	int len = End-Start;

	if(len <= 0){ return -1;}

	struct dirent dir; 

	char name[NAME_MAX];
	if(len > NAME_MAX){ len = NAME_MAX-1;} //failed somewhere because len < Name_Max since we ignore the first /
	memcpy(name, &path[Start], len);
	name[len] = '\0';


	if(dir_find(lookupIno, name, len, &dir) < 0){ 
		return -1;
	}
	lookupIno = dir.ino; 
	if(path[End] == '\0'){ 
		break;
	}

	Start = End + 1;

	}
	return readi(lookupIno, inode);

	
	// Step 1: Resolve the path name, walk through path, and finally, find its inode.
	// Note: You could either implement it in a iterative way or recursive way
}

/* 
 * Make file system
 */
int rufs_mkfs() {

	// Call dev_init() to initialize (Create) Diskfile
	dev_init(diskfile_path);
	// write superblock information
	memset(&sb, 0, sizeof(struct superblock));
	sb.magic_num = MAGIC_NUM;
	sb.max_inum = MAX_INUM;
	sb.max_dnum = MAX_DNUM;
	// initialize inode bitmap
	int inodes_per_block = BLOCK_SIZE / sizeof(struct inode);
	int inode_block_count = (sb.max_inum + inodes_per_block - 1) / inodes_per_block;
	sb.i_bitmap_blk = 1;
	sb.d_bitmap_blk = sb.i_bitmap_blk + 1;
	sb.i_start_blk = sb.d_bitmap_blk + 1;
	sb.d_start_blk = sb.i_start_blk + inode_block_count;
	// initialize data block bitmap

	//copy superblock to block 0
	char block[BLOCK_SIZE];
	memset(block, 0, BLOCK_SIZE);
	memcpy(block, &sb, sizeof(struct superblock));

	if (bio_write(0, block) == -1) {
		perror("rufs_mkfs: bio_write");
		return -1;
	}

	//allocate bitmaps
	inode_bitmap = (bitmap_t)malloc(BLOCK_SIZE);
	data_bitmap = (bitmap_t)malloc(BLOCK_SIZE);
	memset(inode_bitmap, 0, BLOCK_SIZE);
	memset(data_bitmap, 0, BLOCK_SIZE);
	// update inode for root directory

	set_bitmap(inode_bitmap, 0); // allocate inode 0
	set_bitmap(data_bitmap, 0); // allocate data block 0
	bio_write(sb.i_bitmap_blk, (char *)inode_bitmap);
	bio_write(sb.d_bitmap_blk, (char *)data_bitmap);
	//now ondisk block 0 has the super, block 1 has inode, block 2 has data

	struct inode root_inode;
	memset(&root_inode, 0, sizeof(struct inode));
	root_inode.ino = 0;
	root_inode.valid = 1;
	root_inode.size = 0;
	root_inode.type = __S_IFDIR; // directory
	root_inode.link = 2;
	root_inode.direct_ptr[0] = sb.d_start_blk; // first data block
	
	root_inode.vstat.st_mode = __S_IFDIR | 0755;
	root_inode.vstat.st_nlink = 2;
	root_inode.vstat.st_uid = getuid();
	root_inode.vstat.st_gid = getgid();
	root_inode.vstat.st_size = root_inode.size;
	root_inode.vstat.st_blksize = BLOCK_SIZE;
	root_inode.vstat.st_blocks = 1;

	int block_index = root_inode.ino / inodes_per_block;
	int inode_offset = root_inode.ino % inodes_per_block;
	int inode_block_num = sb.i_start_blk + block_index;

	memset(block, 0, BLOCK_SIZE);
	if (bio_read(inode_block_num, block) == -1) {
		perror("rufs_mkfs: bio_read");
		return -1;
	}

	struct inode *disk_inodes = (struct inode *)block;
	disk_inodes[inode_offset] = root_inode;

	if(bio_write(inode_block_num, block) == -1) {
		perror("rufs_mkfs: bio_write root inode");
		return -1;
	}

	memset(block, 0, BLOCK_SIZE);
	if(bio_write(root_inode.direct_ptr[0], block) == -1) {
		perror("rufs_mkfs: bio_write root data block");
		return -1;
	}


	return 0;
}


/* 
 * FUSE file operations
 */
static void *rufs_init(struct fuse_conn_info *conn) {

	// Step 1a: If disk file is not found, call mkfs
	if(access(diskfile_path, F_OK) == -1) {
		rufs_mkfs();}
	// Step 1b: If disk file is found, just initialize in-memory data structures
	else{
		dev_open(diskfile_path);

		char block[BLOCK_SIZE];
		memset(block, 0, BLOCK_SIZE);
		if(bio_read(0, block) == -1) {
			perror("cant read in rufs init");
			return NULL;
		}
		memcpy(&sb, block, sizeof(struct superblock));

		inode_bitmap = (bitmap_t)malloc(BLOCK_SIZE);
		data_bitmap = (bitmap_t)malloc(BLOCK_SIZE);

		if(bio_read(sb.i_bitmap_blk, inode_bitmap) == -1){
        perror("inode read issue in rufs init");
		}
		if(bio_read(sb.d_bitmap_blk, data_bitmap) == -1){
			perror("data read issue in rufs init");
		}

	}
	// and read superblock from disk

	return NULL;
}

static void rufs_destroy(void *userdata) {

	// Step 1: De-allocate in-memory data structures
	if(inode_bitmap) {
		free(inode_bitmap);
	}
	if(data_bitmap) {
		free(data_bitmap);
	}	
	inode_bitmap = NULL;
	data_bitmap = NULL;
	// Step 2: Close diskfile

	dev_close();

}

static int rufs_getattr(const char *path, struct stat *stbuf) {
	struct inode temp; 

	if(get_node_by_path(path, 0, &temp) < 0){return -ENOENT;}

	stbuf->st_mode = temp.vstat.st_mode;
	stbuf->st_uid = temp.vstat.st_uid;
	stbuf->st_gid = temp.vstat.st_gid;
	stbuf->st_nlink = temp.vstat.st_nlink;
	stbuf->st_size = temp.vstat.st_size;
	stbuf->st_mtime = temp.vstat.st_mtime;
	stbuf->st_blksize = temp.vstat.st_blksize;
	stbuf->st_blocks = temp.vstat.st_blocks;

	// Step 1: call get_node_by_path() to get inode from path
	// Step 2: fill attribute of file into stbuf from inode
	return 0;
}

static int rufs_opendir(const char *path, struct fuse_file_info *fi) {
	struct inode temp; 
	// Step 1: Call get_node_by_path() to get inode from path

	if(get_node_by_path(path, 0, &temp)< 0){return -ENOENT;}



	// Step 2: If not find, return -1
	//decided to just do it the inverse way
	
	return 0;
}

static int rufs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: Read directory entries from its data blocks, and copy them to filler
	struct inode dirInode;
	
    if (get_node_by_path(path, 0, &dirInode) < 0) {
        return -ENOENT; }

    // always add . and ..
    filler(buffer, ".",  NULL, 0);
    filler(buffer, "..", NULL, 0);

    char block[BLOCK_SIZE];
    int Entries = BLOCK_SIZE / sizeof(struct dirent);

	int numDirect = sizeof(dirInode.direct_ptr) / sizeof(dirInode.direct_ptr[0]);

    for(int i = 0; i < numDirect; i++){
        int blkno = dirInode.direct_ptr[i];
        if(blkno == 0) continue;

        if(bio_read(blkno, block) < 0) continue;

        struct dirent *entry = (struct dirent*)block;

        for(int j = 0; j < Entries; j++){
            if(!entry[j].valid) continue;

            filler(buffer, entry[j].name, NULL, 0);
        }
    }


	return 0;
}


static int rufs_mkdir(const char *path, mode_t mode) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name


	// Step 2: Call get_node_by_path() to get inode of parent directory

	// Step 3: Call get_avail_ino() to get an available inode number

	// Step 4: Call dir_add() to add directory entry of target directory to parent directory

	// Step 5: Update inode for target directory

	// Step 6: Call writei() to write inode to disk


	printf("\n>>> FUSE CALL: mkdir(%s)\n", path);

	char *Path = strdup(path);
	char *Parent = dirname(Path);

	char *Path2 = strdup(path);
	char *Target = basename(Path2);

	if (!Parent) {
		free(Path);
		free(Path2);
		return -EINVAL;}
	if(!Target){
		free(Path);
		free(Path2);
		return -EINVAL;
	}

	// Step 2: get parent inode
	struct inode ParentInode;
	if (get_node_by_path(Parent, 0, &ParentInode) < 0) {
		free(Path);
		free(Path2);
		return -ENOENT;
	}

	// Step 3: alloc new inode number
	int Ino = get_avail_ino();
	if (Ino < 0) {
		free(Path);
		free(Path2);
		return -ENOSPC;
	}

	// Step 4: add entry to parent
	if (dir_add(ParentInode, Ino, Target, strlen(Target)) < 0) {
		free(Path);
		free(Path2);
		return -EEXIST;
	}

	// Step 5: build new child inode
	struct inode temp;
	memset(&temp, 0, sizeof(temp));
	temp.ino = Ino;
	temp.valid = 1;
	temp.type = __S_IFDIR;
	temp.link = 2;
	temp.size = 0;

	int blk = get_avail_blkno();
	if (blk < 0) {
		free(Path);
		free(Path2);
		return -ENOSPC;
	}
	temp.direct_ptr[0] = blk;

	temp.vstat.st_mode = __S_IFDIR | mode;
	temp.vstat.st_nlink = 2;
	temp.vstat.st_uid = getuid();
	temp.vstat.st_gid = getgid();
	temp.vstat.st_size = temp.size;
	temp.vstat.st_blksize = BLOCK_SIZE;
	temp.vstat.st_blocks = 1;

	// Step 6: persist inode
	if (writei(Ino, &temp) < 0) {
		free(Path);
		free(Path2);
		return -EIO;
	}

	// zero out its data block
	char block[BLOCK_SIZE];
	memset(block, 0, BLOCK_SIZE);
	bio_write(blk, block);

	free(Path);
	free(Path2);

	return 0;
}


static int rufs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name

	// Step 2: Call get_node_by_path() to get inode of parent directory

	// Step 3: Call get_avail_ino() to get an available inode number

	// Step 4: Call dir_add() to add directory entry of target file to parent directory

	// Step 5: Update inode for target file

	// Step 6: Call writei() to write inode to disk

	// 1: split parent + filename
	printf("\n>>> FUSE CALL: create(%s)\n", path);
    char *Path   = strdup(path);
    char *Parent = dirname(Path);

    char *Path2  = strdup(path);
    char *Target = basename(Path2);

	printf("rufs_create called for %s\n", path);

    if (!Parent) {
        free(Path);
        free(Path2);
        return -EINVAL;
    }
	if (!Target) {
        free(Path);
        free(Path2);
        return -EINVAL;
    }

    // 2: get parent dir inode
    struct inode ParentInode;
    if (get_node_by_path(Parent, 0, &ParentInode) < 0) {
		printf("create: parent lookup FAILED for %s\n", Parent);
        free(Path);
        free(Path2);
        return -ENOENT;
    }
	printf("create: parent lookup OK for %s (ino=%d)\n", Parent, ParentInode.ino);

	printf("calling dir_add\n");

    // 3: get new inode number
    int Ino = get_avail_ino();
    if (Ino < 0) {
        free(Path);
        free(Path2);
        return -ENOSPC;
    }

    // 4: add dir entry to parent
    if (dir_add(ParentInode, Ino, Target, strlen(Target)) < 0) {
        free(Path);
        free(Path2);
        return -EEXIST;       // name already exists
    }

	printf("dir_add ok\n");

    // 5: init inode
	printf("writei for new inode\n");

    struct inode temp;
    memset(&temp, 0, sizeof(temp));
    temp.ino   = Ino;
    temp.valid = 1;
    temp.type  = __S_IFREG;
    temp.size  = 0;
    temp.link  = 1;

    // no block yet — allocate lazily during write()
    temp.vstat.st_mode   = __S_IFREG | mode;
    temp.vstat.st_nlink  = 1;
    temp.vstat.st_uid    = getuid();
    temp.vstat.st_gid    = getgid();
    temp.vstat.st_size   = 0;
    temp.vstat.st_blksize = BLOCK_SIZE;
    temp.vstat.st_blocks = 0;

    // 6: persist it
    if (writei(Ino, &temp) < 0) {
        free(Path);
        free(Path2);
        return -EIO;
    }

    free(Path);
    free(Path2);
	fi->fh = Ino;

	return 0;
}



static int rufs_open(const char *path, struct fuse_file_info *fi) {
	printf("\n>>> FUSE CALL: open(%s)\n", path);
	struct inode node;
    
    if(get_node_by_path(path, 0, &node) < 0){
        return -ENOENT;
    }

    if(!(node.type & __S_IFREG)){
        return -EISDIR;  
    }

    return 0;
}

static int rufs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
	printf("\n>>> FUSE CALL: read(%s)\n", path);
	// Step 1: You could call get_node_by_path() to get inode from path

	// struct inode temp; 

	// if(get_node_by_path(path, 0, &temp)< 0){return -ENOENT;}

	// // Step 2: Based on size and offset, read its data blocks from disk
	
	// char Buffer[size];
	// // Step 3: copy the correct amount of data from offset to buffer

	// // Note: this function should return the amount of bytes you copied to buffer
	// return 0;



	struct inode inode;

    if(get_node_by_path(path, 0, &inode) < 0)
        return -ENOENT;

    if(!inode.valid)
        return -ENOENT;

    // step 1: don't read past file size
    if(offset >= inode.size) return 0;
    if(offset + size > inode.size)
        size = inode.size - offset;

    size_t bytesRead = 0;
    size_t remaining = size;

    int blockIndex = offset / BLOCK_SIZE;
    int blockOffset = offset % BLOCK_SIZE;

    while(remaining > 0 && blockIndex < 16)
    {
        int blkno = inode.direct_ptr[blockIndex];
        if(blkno <= 0) break; // unmapped block

        char block[BLOCK_SIZE];

        if(bio_read(blkno, block) < 0)
            break;

        size_t chunk = BLOCK_SIZE - blockOffset;
        if(chunk > remaining) chunk = remaining;

        memcpy(buffer + bytesRead, block + blockOffset, chunk);

        bytesRead += chunk;
        remaining -= chunk;

        // subsequent iterations read from start of blocks
        blockOffset = 0;
        blockIndex++;
    }

    return bytesRead;
}

static int rufs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {

	// Step 1: You could call get_node_by_path() to get inode from path

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: Write the correct amount of data from offset to disk

	// Step 4: Update the inode info and write it to disk

	// Note: this function should return the amount of bytes you write to disk
	// return size;

	printf("\n>>> FUSE CALL: write(%s)\n", path);
	struct inode node;

    // get inode
    if(get_node_by_path(path, 0, &node) < 0){
        return -ENOENT;
    }

    size_t bytes_written = 0;
    size_t remaining     = size;

    int blockIndex  = offset / BLOCK_SIZE;
    int blockOffset = offset % BLOCK_SIZE;

    while(remaining > 0 && blockIndex < 16)
    {
        int blkno = node.direct_ptr[blockIndex];

        // allocate new block if needed
        if(blkno <= 0){
            blkno = get_avail_blkno();
            if(blkno < 0){
                break;  // no space
            }
            node.direct_ptr[blockIndex] = blkno;
        }

        char blkbuf[BLOCK_SIZE];
        memset(blkbuf, 0, BLOCK_SIZE);

        // load existing content (in case of partial overwrite)
        bio_read(blkno, blkbuf);

        size_t chunk = BLOCK_SIZE - blockOffset;
        if(chunk > remaining) chunk = remaining;

        memcpy(blkbuf + blockOffset,
               buffer + bytes_written,
               chunk);

        bio_write(blkno, blkbuf);

        bytes_written = bytes_written + chunk;
        remaining     = remaining- chunk;

        blockOffset = 0;  // next block starts at 0
        blockIndex++;
    }

    // update inode size if file grew
    if(offset + bytes_written > node.size){
        node.size = offset + bytes_written;
        node.vstat.st_size = node.size;
    }

    // update inode metadata
    node.vstat.st_blocks = (node.size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    node.vstat.st_mtime = time(NULL);

    writei(node.ino, &node);

    return bytes_written;
}


/* 
 * Functions you DO NOT need to implement for this project
 * (stubs provided for completeness)
 */

static int rufs_rmdir(const char *path) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int rufs_releasedir(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int rufs_unlink(const char *path) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int rufs_truncate(const char *path, off_t size) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int rufs_release(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int rufs_flush(const char * path, struct fuse_file_info * fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int rufs_utimens(const char *path, const struct timespec tv[2]) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}


static struct fuse_operations rufs_ope = {
	.init		= rufs_init,
	.destroy	= rufs_destroy,

	.getattr	= rufs_getattr,
	.readdir	= rufs_readdir,
	.opendir	= rufs_opendir,
	.mkdir		= rufs_mkdir,

	.create		= rufs_create,
	.open		= rufs_open,
	.read 		= rufs_read,
	.write		= rufs_write,

	//Operations that you don't have to implement.
	.rmdir		= rufs_rmdir,
	.releasedir	= rufs_releasedir,
	.unlink		= rufs_unlink,
	.truncate   = rufs_truncate,
	.flush      = rufs_flush,
	.utimens    = rufs_utimens,
	.release	= rufs_release
};


int main(int argc, char *argv[]) {
	int fuse_stat;

	getcwd(diskfile_path, PATH_MAX);
	strcat(diskfile_path, "/DISKFILE");

	fuse_stat = fuse_main(argc, argv, &rufs_ope, NULL);

	return fuse_stat;
}
