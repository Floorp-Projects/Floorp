/*-
 * Copyright (c) 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Margo Seltzer.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)hash.c	8.9 (Berkeley) 6/16/94";
#endif /* LIBC_SCCS and not lint */

#include "watcomfx.h"

#if !defined(_WIN32) && !defined(_WINDOWS) && !defined(macintosh) && !defined(XP_OS2_VACPP)
#include <sys/param.h>
#endif

#if !defined(macintosh)
#ifdef XP_OS2_EMX
#include <sys/types.h>
#endif
#include <sys/stat.h>
#endif

#if defined(macintosh)
#include <unix.h>
#include <unistd.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32) && !defined(_WINDOWS) && !defined(macintosh) && !defined(XP_OS2_VACPP)
#include <unistd.h>
#endif
#if defined(_WIN32) || defined(_WINDOWS) 
#include <windows.h>
#endif

#ifdef XP_OS2_VACPP
#include "types.h"
#define EPERM SOCEPERM
#endif

#include <assert.h>

#include "mcom_db.h"
#include "hash.h"
#include "page.h"

/*
#include "extern.h"
*/
static int   alloc_segs __P((HTAB *, int));
static int   flush_meta __P((HTAB *));
static int   hash_access __P((HTAB *, ACTION, DBT *, DBT *));
static int   hash_close __P((DB *));
static int   hash_delete __P((const DB *, const DBT *, uint));
static int   hash_fd __P((const DB *));
static int   hash_get __P((const DB *, const DBT *, DBT *, uint));
static int   hash_put __P((const DB *, DBT *, const DBT *, uint));
static void *hash_realloc __P((SEGMENT **, size_t, size_t));
static int   hash_seq __P((const DB *, DBT *, DBT *, uint));
static int   hash_sync __P((const DB *, uint));
static int   hdestroy __P((HTAB *));
static HTAB *init_hash __P((HTAB *, const char *, HASHINFO *));
static int   init_htab __P((HTAB *, int));
#if BYTE_ORDER == LITTLE_ENDIAN
static void  swap_header __P((HTAB *));
static void  swap_header_copy __P((HASHHDR *, HASHHDR *));
#endif

/* Fast arithmetic, relying on powers of 2, */
#define MOD(x, y)		((x) & ((y) - 1))

#define RETURN_ERROR(ERR, LOC)	{ save_errno = ERR; goto LOC; }

/* Return values */
#define	SUCCESS	 (0)
#define	DBM_ERROR	(-1)
#define	ABNORMAL (1)

#ifdef HASH_STATISTICS
int hash_accesses, hash_collisions, hash_expansions, hash_overflows;
#endif

/* A new Lou (montulli@mozilla.com) routine.
 *
 * The database is screwed.  
 *
 * This closes the file, flushing buffers as appropriate.
 */
static void
__remove_database(DB *dbp)
{
	HTAB *hashp = (HTAB *)dbp->internal;

	assert(0);

	if (!hashp)
		return;
	hdestroy(hashp);
	dbp->internal = NULL; 
}

/************************** INTERFACE ROUTINES ***************************/
/* OPEN/CLOSE */


extern DB *
__hash_open(const char *file, int flags, int mode, const HASHINFO *info, int dflags)
{
	HTAB *hashp=NULL;
	struct stat statbuf;
	DB *dbp;
	int bpages, hdrsize, new_table, nsegs, save_errno;

	if ((flags & O_ACCMODE) == O_WRONLY) {
		errno = EINVAL;
		RETURN_ERROR(ENOMEM, error0);
	}

	/* zero the statbuffer so that
	 * we can check it for a non-zero
	 * date to see if stat succeeded
	 */
	memset(&statbuf, 0, sizeof(struct stat));

	if (!(hashp = (HTAB *)calloc(1, sizeof(HTAB))))
		RETURN_ERROR(ENOMEM, error0);
	hashp->fp = NO_FILE;
	if(file)
		hashp->filename = strdup(file);

	/*
	 * Even if user wants write only, we need to be able to read
	 * the actual file, so we need to open it read/write. But, the
	 * field in the hashp structure needs to be accurate so that
	 * we can check accesses.
	 */
	hashp->flags = flags;

	new_table = 0;
	if (!file || (flags & O_TRUNC) 	|| (stat(file, &statbuf)  && (errno == ENOENT))) 
	{
		if (errno == ENOENT)
			errno = 0; /* Just in case someone looks at errno */
		new_table = 1;
	}
	else if(statbuf.st_mtime && statbuf.st_size == 0)
	{
		/* check for a zero length file and delete it
	 	 * if it exists
	 	 */
		new_table = 1;
	}
	hashp->file_size = statbuf.st_size;

	if (file) {				 

#if defined(_WIN32) || defined(_WINDOWS) || defined (macintosh)  || defined(XP_OS2_VACPP)
		if ((hashp->fp = DBFILE_OPEN(file, flags | O_BINARY, mode)) == -1)
			RETURN_ERROR(errno, error0);
#else
 	if ((hashp->fp = open(file, flags, mode)) == -1)
		RETURN_ERROR(errno, error0);
	(void)fcntl(hashp->fp, F_SETFD, 1);
/* We can't use fcntl because of NFS bugs. SIGH */
#if 0
    {
	struct flock fl;
	memset(&fl, 0, sizeof(fl));
	fl.l_type = F_WRLCK;
	if (fcntl(hashp->fp, F_SETLK, &fl) < 0) {
#ifdef DEBUG
	    fprintf(stderr, "unable to open %s because it's locked (flags=0x%x)\n", file, flags);
#endif
	    RETURN_ERROR(EACCES, error1);
	}
    }
#endif

#endif
	}
	if (new_table) {
		if (!init_hash(hashp, file, (HASHINFO *)info))
			RETURN_ERROR(errno, error1);
	} else {
		/* Table already exists */
		if (info && info->hash)
			hashp->hash = info->hash;
		else
			hashp->hash = __default_hash;

		hdrsize = read(hashp->fp, (char *)&hashp->hdr, sizeof(HASHHDR));
#if BYTE_ORDER == LITTLE_ENDIAN
		swap_header(hashp);
#endif
		if (hdrsize == -1)
			RETURN_ERROR(errno, error1);
		if (hdrsize != sizeof(HASHHDR))
			RETURN_ERROR(EFTYPE, error1);
		/* Verify file type, versions and hash function */
		if (hashp->MAGIC != HASHMAGIC)
			RETURN_ERROR(EFTYPE, error1);
#define	OLDHASHVERSION	1
		if (hashp->VERSION != HASHVERSION &&
		    hashp->VERSION != OLDHASHVERSION)
			RETURN_ERROR(EFTYPE, error1);
		if (hashp->hash(CHARKEY, sizeof(CHARKEY)) != hashp->H_CHARKEY)
			RETURN_ERROR(EFTYPE, error1);
		if (hashp->NKEYS < 0) {
		    /*
		    ** OOPS. Old bad database from previously busted
		    ** code. Blow it away.
		    */
		    close(hashp->fp);
		    if (remove(file) < 0) {
#if defined(DEBUG) && defined(XP_UNIX)
			fprintf(stderr,
				"WARNING: You have an old bad cache.db file"
				" '%s', and I couldn't remove it!\n", file);
#endif
		    } else {
#if defined(DEBUG) && defined(XP_UNIX)
			fprintf(stderr,
				"WARNING: I blew away your %s file because"
				" it was bad due to a recently fixed bug\n",
				file);
#endif
		    }
		    RETURN_ERROR(ENOENT, error0);
		}

		/*
		 * Figure out how many segments we need.  Max_Bucket is the
		 * maximum bucket number, so the number of buckets is
		 * max_bucket + 1.
		 */
		nsegs = (hashp->MAX_BUCKET + 1 + hashp->SGSIZE - 1) /
			 hashp->SGSIZE;
		hashp->nsegs = 0;
		if (alloc_segs(hashp, nsegs))
			/*
			 * If alloc_segs fails, table will have been destroyed
			 * and errno will have been set.
			 */
			RETURN_ERROR(ENOMEM, error0);
		/* Read in bitmaps */
		bpages = (hashp->SPARES[hashp->OVFL_POINT] +
		    (hashp->BSIZE << BYTE_SHIFT) - 1) >>
		    (hashp->BSHIFT + BYTE_SHIFT);

		hashp->nmaps = bpages;
		(void)memset(&hashp->mapp[0], 0, bpages * sizeof(uint32 *));
	}

	/* Initialize Buffer Manager */
	if (info && info->cachesize)
		__buf_init(hashp, (int32) info->cachesize);
	else
		__buf_init(hashp, DEF_BUFSIZE);

	hashp->new_file = new_table;
#ifdef macintosh
	hashp->save_file = file && !(hashp->flags & O_RDONLY);
#else
	hashp->save_file = file && (hashp->flags & O_RDWR);
#endif
	hashp->cbucket = -1;
	if (!(dbp = (DB *)malloc(sizeof(DB)))) {
		save_errno = errno;
		hdestroy(hashp);
		errno = save_errno;
		RETURN_ERROR(ENOMEM, error0);
	}
	dbp->internal = hashp;
	dbp->close = hash_close;
	dbp->del = hash_delete;
	dbp->fd = hash_fd;
	dbp->get = hash_get;
	dbp->put = hash_put;
	dbp->seq = hash_seq;
	dbp->sync = hash_sync;
	dbp->type = DB_HASH;

#if 0
#if defined(DEBUG) && !defined(_WINDOWS)
{
extern int MKLib_trace_flag;

  if(MKLib_trace_flag)
	(void)fprintf(stderr,
"%s\n%s%lx\n%s%d\n%s%d\n%s%d\n%s%d\n%s%d\n%s%d\n%s%d\n%s%d\n%s%d\n%s%x\n%s%x\n%s%d\n%s%d\n",
	    "init_htab:",
	    "TABLE POINTER   ", (unsigned long) hashp,
	    "BUCKET SIZE     ", hashp->BSIZE,
	    "BUCKET SHIFT    ", hashp->BSHIFT,
	    "DIRECTORY SIZE  ", hashp->DSIZE,
	    "SEGMENT SIZE    ", hashp->SGSIZE,
	    "SEGMENT SHIFT   ", hashp->SSHIFT,
	    "FILL FACTOR     ", hashp->FFACTOR,
	    "MAX BUCKET      ", hashp->MAX_BUCKET,
	    "OVFL POINT	     ", hashp->OVFL_POINT,
	    "LAST FREED      ", hashp->LAST_FREED,
	    "HIGH MASK       ", hashp->HIGH_MASK,
	    "LOW  MASK       ", hashp->LOW_MASK,
	    "NSEGS           ", hashp->nsegs,
	    "NKEYS           ", hashp->NKEYS);
}
#endif
#endif /* 0 */
#ifdef HASH_STATISTICS
	hash_overflows = hash_accesses = hash_collisions = hash_expansions = 0;
#endif
	return (dbp);

error1:
	if (hashp != NULL)
		(void)close(hashp->fp);

error0:
	free(hashp);
	errno = save_errno;
	return (NULL);
}

static int
hash_close(DB *dbp)
{
	HTAB *hashp;
	int retval;

	if (!dbp)
		return (DBM_ERROR);

	hashp = (HTAB *)dbp->internal;
	if(!hashp)
		return (DBM_ERROR);

	retval = hdestroy(hashp);
	free(dbp);
	return (retval);
}

static int hash_fd(const DB *dbp)
{
	HTAB *hashp;

	if (!dbp)
		return (DBM_ERROR);

	hashp = (HTAB *)dbp->internal;
	if(!hashp)
		return (DBM_ERROR);

	if (hashp->fp == -1) {
		errno = ENOENT;
		return (-1);
	}
	return (hashp->fp);
}

/************************** LOCAL CREATION ROUTINES **********************/
static HTAB *
init_hash(HTAB *hashp, const char *file, HASHINFO *info)
{
	struct stat statbuf;
	int nelem;

	nelem = 1;
	hashp->NKEYS = 0;
	hashp->LORDER = BYTE_ORDER;
	hashp->BSIZE = DEF_BUCKET_SIZE;
	hashp->BSHIFT = DEF_BUCKET_SHIFT;
	hashp->SGSIZE = DEF_SEGSIZE;
	hashp->SSHIFT = DEF_SEGSIZE_SHIFT;
	hashp->DSIZE = DEF_DIRSIZE;
	hashp->FFACTOR = DEF_FFACTOR;
	hashp->hash = __default_hash;
	memset(hashp->SPARES, 0, sizeof(hashp->SPARES));
	memset(hashp->BITMAPS, 0, sizeof (hashp->BITMAPS));

	/* Fix bucket size to be optimal for file system */
	if (file != NULL) {
		if (stat(file, &statbuf))
			return (NULL);

#if !defined(_WIN32) && !defined(_WINDOWS) && !defined(macintosh) && !defined(VMS) && !defined(XP_OS2)
#ifdef __QNX__
		hashp->BSIZE = statbuf.st_size;
#else
		hashp->BSIZE = statbuf.st_blksize;
#endif

       	/* new code added by Lou to reduce block
       	 * size down below MAX_BSIZE
       	 */
       	if (hashp->BSIZE > MAX_BSIZE)
       		hashp->BSIZE = MAX_BSIZE;
#endif
		hashp->BSHIFT = __log2((uint32)hashp->BSIZE);
	}

	if (info) {
		if (info->bsize) {
			/* Round pagesize up to power of 2 */
			hashp->BSHIFT = __log2(info->bsize);
			hashp->BSIZE = 1 << hashp->BSHIFT;
			if (hashp->BSIZE > MAX_BSIZE) {
				errno = EINVAL;
				return (NULL);
			}
		}
		if (info->ffactor)
			hashp->FFACTOR = info->ffactor;
		if (info->hash)
			hashp->hash = info->hash;
		if (info->nelem)
			nelem = info->nelem;
		if (info->lorder) {
			if (info->lorder != BIG_ENDIAN &&
			    info->lorder != LITTLE_ENDIAN) {
				errno = EINVAL;
				return (NULL);
			}
			hashp->LORDER = info->lorder;
		}
	}
	/* init_htab should destroy the table and set errno if it fails */
	if (init_htab(hashp, nelem))
		return (NULL);
	else
		return (hashp);
}
/*
 * This calls alloc_segs which may run out of memory.  Alloc_segs will destroy
 * the table and set errno, so we just pass the error information along.
 *
 * Returns 0 on No Error
 */
static int
init_htab(HTAB *hashp, int nelem)
{
	register int nbuckets, nsegs;
	int l2;

	/*
	 * Divide number of elements by the fill factor and determine a
	 * desired number of buckets.  Allocate space for the next greater
	 * power of two number of buckets.
	 */
	nelem = (nelem - 1) / hashp->FFACTOR + 1;

	l2 = __log2((uint32)PR_MAX(nelem, 2));
	nbuckets = 1 << l2;

	hashp->SPARES[l2] = l2 + 1;
	hashp->SPARES[l2 + 1] = l2 + 1;
	hashp->OVFL_POINT = l2;
	hashp->LAST_FREED = 2;

	/* First bitmap page is at: splitpoint l2 page offset 1 */
	if (__ibitmap(hashp, (int)OADDR_OF(l2, 1), l2 + 1, 0))
		return (-1);

	hashp->MAX_BUCKET = hashp->LOW_MASK = nbuckets - 1;
	hashp->HIGH_MASK = (nbuckets << 1) - 1;
	hashp->HDRPAGES = ((PR_MAX(sizeof(HASHHDR), MINHDRSIZE) - 1) >>
	    hashp->BSHIFT) + 1;

	nsegs = (nbuckets - 1) / hashp->SGSIZE + 1;
	nsegs = 1 << __log2((uint32)nsegs);

	if (nsegs > hashp->DSIZE)
		hashp->DSIZE = nsegs;
	return (alloc_segs(hashp, nsegs));
}

/********************** DESTROY/CLOSE ROUTINES ************************/

/*
 * Flushes any changes to the file if necessary and destroys the hashp
 * structure, freeing all allocated space.
 */
static int
hdestroy(HTAB *hashp)
{
	int i, save_errno;

	save_errno = 0;

#ifdef HASH_STATISTICS
	(void)fprintf(stderr, "hdestroy: accesses %ld collisions %ld\n",
	    hash_accesses, hash_collisions);
	(void)fprintf(stderr, "hdestroy: expansions %ld\n",
	    hash_expansions);
	(void)fprintf(stderr, "hdestroy: overflows %ld\n",
	    hash_overflows);
	(void)fprintf(stderr, "keys %ld maxp %d segmentcount %d\n",
	    hashp->NKEYS, hashp->MAX_BUCKET, hashp->nsegs);

	for (i = 0; i < NCACHED; i++)
		(void)fprintf(stderr,
		    "spares[%d] = %d\n", i, hashp->SPARES[i]);
#endif
	/*
	 * Call on buffer manager to free buffers, and if required,
	 * write them to disk.
	 */
	if (__buf_free(hashp, 1, hashp->save_file))
		save_errno = errno;
	if (hashp->dir) {
		free(*hashp->dir);	/* Free initial segments */
		/* Free extra segments */
		while (hashp->exsegs--)
			free(hashp->dir[--hashp->nsegs]);
		free(hashp->dir);
	}
	if (flush_meta(hashp) && !save_errno)
		save_errno = errno;
	/* Free Bigmaps */
	for (i = 0; i < hashp->nmaps; i++)
		if (hashp->mapp[i])
			free(hashp->mapp[i]);

	if (hashp->fp != -1)
		(void)close(hashp->fp);

	if(hashp->filename) {
#if defined(_WIN32) || defined(_WINDOWS) || defined(XP_OS2)
		if (hashp->is_temp)
			(void)unlink(hashp->filename);
#endif
		free(hashp->filename);
	}

	free(hashp);

	if (save_errno) {
		errno = save_errno;
		return (DBM_ERROR);
	}
	return (SUCCESS);
}

#if defined(_WIN32) || defined(_WINDOWS) 
/*
 * Close and reopen file to force file length update on windows. 
 *
 * Returns:
 *	 0 == OK
 *	-1 DBM_ERROR
 */
static int
update_EOF(HTAB *hashp)
{
#if defined(DBM_REOPEN_ON_FLUSH)
	char *      file       = hashp->filename;
	off_t       file_size;
	int         flags;
	int         mode       = -1;
	struct stat statbuf;

	memset(&statbuf, 0, sizeof statbuf);

	/* make sure we won't lose the file by closing it. */
	if (!file || (stat(file, &statbuf)  && (errno == ENOENT)))  {
		/* pretend we did it. */
		return 0;
	}

	(void)close(hashp->fp);

	flags = hashp->flags & ~(O_TRUNC | O_CREAT | O_EXCL);

	if ((hashp->fp = DBFILE_OPEN(file, flags | O_BINARY, mode)) == -1)
		return -1;
	file_size = lseek(hashp->fp, (off_t)0, SEEK_END);
	if (file_size == -1) 
		return -1;
	hashp->file_size = file_size;
	return 0;
#else
	int    fd        = hashp->fp;
	off_t  file_size = lseek(fd, (off_t)0, SEEK_END);
	HANDLE handle    = (HANDLE)_get_osfhandle(fd);
	BOOL   cool      = FlushFileBuffers(handle);
#ifdef DEBUG3
	if (!cool) {
		DWORD err = GetLastError();
		(void)fprintf(stderr,
			"FlushFileBuffers failed, last error = %d, 0x%08x\n",
			err, err);
	}
#endif
	if (file_size == -1) 
		return -1;
	hashp->file_size = file_size;
	return cool ? 0 : -1;
#endif
}
#endif

/*
 * Write modified pages to disk
 *
 * Returns:
 *	 0 == OK
 *	-1 DBM_ERROR
 */
static int
hash_sync(const DB *dbp, uint flags)
{
	HTAB *hashp;

	if (flags != 0) {
		errno = EINVAL;
		return (DBM_ERROR);
	}

	if (!dbp)
		return (DBM_ERROR);

	hashp = (HTAB *)dbp->internal;
	if(!hashp)
		return (DBM_ERROR);

	if (!hashp->save_file)
		return (0);
	if (__buf_free(hashp, 0, 1) || flush_meta(hashp))
		return (DBM_ERROR);
#if defined(_WIN32) || defined(_WINDOWS) 
	if (hashp->updateEOF && hashp->filename && !hashp->is_temp) {
		int status = update_EOF(hashp);
		hashp->updateEOF = 0;
		if (status)
			return status;
	}
#endif
	hashp->new_file = 0;
	return (0);
}

/*
 * Returns:
 *	 0 == OK
 *	-1 indicates that errno should be set
 */
static int
flush_meta(HTAB *hashp)
{
	HASHHDR *whdrp;
#if BYTE_ORDER == LITTLE_ENDIAN
	HASHHDR whdr;
#endif
	int fp, i, wsize;

	if (!hashp->save_file)
		return (0);
	hashp->MAGIC = HASHMAGIC;
	hashp->VERSION = HASHVERSION;
	hashp->H_CHARKEY = hashp->hash(CHARKEY, sizeof(CHARKEY));

	fp = hashp->fp;
	whdrp = &hashp->hdr;
#if BYTE_ORDER == LITTLE_ENDIAN
	whdrp = &whdr;
	swap_header_copy(&hashp->hdr, whdrp);
#endif
	if ((lseek(fp, (off_t)0, SEEK_SET) == -1) ||
	    ((wsize = write(fp, (char*)whdrp, sizeof(HASHHDR))) == -1))
		return (-1);
	else
		if (wsize != sizeof(HASHHDR)) {
			errno = EFTYPE;
			hashp->dbmerrno = errno;
			return (-1);
		}
	for (i = 0; i < NCACHED; i++)
		if (hashp->mapp[i])
			if (__put_page(hashp, (char *)hashp->mapp[i],
				hashp->BITMAPS[i], 0, 1))
				return (-1);
	return (0);
}

/*******************************SEARCH ROUTINES *****************************/
/*
 * All the access routines return
 *
 * Returns:
 *	 0 on SUCCESS
 *	 1 to indicate an external DBM_ERROR (i.e. key not found, etc)
 *	-1 to indicate an internal DBM_ERROR (i.e. out of memory, etc)
 */
static int
hash_get(
	const DB *dbp,
	const DBT *key,
	DBT *data,
	uint flag)
{
	HTAB *hashp;
	int rv;

	hashp = (HTAB *)dbp->internal;
	if (!hashp)
		return (DBM_ERROR);

	if (flag) {
		hashp->dbmerrno = errno = EINVAL;
		return (DBM_ERROR);
	}

	rv = hash_access(hashp, HASH_GET, (DBT *)key, data);

	if(rv == DATABASE_CORRUPTED_ERROR)
	  {
#if defined(unix) && defined(DEBUG)
		printf("\n\nDBM Database has been corrupted, tell Lou...\n\n");
#endif
	  	__remove_database((DB *)dbp);
	  }

	return(rv);
}

static int
hash_put(
	const DB *dbp,
	DBT *key,
	const DBT *data,
	uint flag)
{
	HTAB *hashp;
	int rv;

	hashp = (HTAB *)dbp->internal;
	if (!hashp)
		return (DBM_ERROR);

	if (flag && flag != R_NOOVERWRITE) {
		hashp->dbmerrno = errno = EINVAL;
		return (DBM_ERROR);
	}
	if ((hashp->flags & O_ACCMODE) == O_RDONLY) {
		hashp->dbmerrno = errno = EPERM;
		return (DBM_ERROR);
	}

	rv =  hash_access(hashp, flag == R_NOOVERWRITE ?
	    HASH_PUTNEW : HASH_PUT, (DBT *)key, (DBT *)data);

	if(rv == DATABASE_CORRUPTED_ERROR)
	  {
#if defined(unix) && defined(DEBUG)
		printf("\n\nDBM Database has been corrupted, tell Lou...\n\n");
#endif
	  	__remove_database((DB *)dbp);
	  }

	return(rv);
}

static int
hash_delete(
	const DB *dbp,
	const DBT *key,
	uint flag)		/* Ignored */
{
	HTAB *hashp;
	int rv;

	hashp = (HTAB *)dbp->internal;
	if (!hashp)
		return (DBM_ERROR);

	if (flag && flag != R_CURSOR) {
		hashp->dbmerrno = errno = EINVAL;
		return (DBM_ERROR);
	}
	if ((hashp->flags & O_ACCMODE) == O_RDONLY) {
		hashp->dbmerrno = errno = EPERM;
		return (DBM_ERROR);
	}
	rv = hash_access(hashp, HASH_DELETE, (DBT *)key, NULL);

	if(rv == DATABASE_CORRUPTED_ERROR)
	  {
#if defined(unix) && defined(DEBUG)
		printf("\n\nDBM Database has been corrupted, tell Lou...\n\n");
#endif
	  	__remove_database((DB *)dbp);
	  }

	return(rv);
}

#define MAX_OVERFLOW_HASH_ACCESS_LOOPS 2000
/*
 * Assume that hashp has been set in wrapper routine.
 */
static int
hash_access(
	HTAB *hashp,
	ACTION action,
	DBT *key, DBT *val)
{
	register BUFHEAD *rbufp;
	BUFHEAD *bufp, *save_bufp;
	register uint16 *bp;
	register long n, ndx, off;
	register size_t size;
	register char *kp;
	uint16 pageno;
	uint32 ovfl_loop_count=0;
    int32 last_overflow_page_no = -1;

#ifdef HASH_STATISTICS
	hash_accesses++;
#endif

	off = hashp->BSIZE;
	size = key->size;
	kp = (char *)key->data;
	rbufp = __get_buf(hashp, __call_hash(hashp, kp, size), NULL, 0);
	if (!rbufp)
		return (DATABASE_CORRUPTED_ERROR);
	save_bufp = rbufp;

	/* Pin the bucket chain */
	rbufp->flags |= BUF_PIN;
	for (bp = (uint16 *)rbufp->page, n = *bp++, ndx = 1; ndx < n;)
	{

		if (bp[1] >= REAL_KEY) {
			/* Real key/data pair */
			if (size == (unsigned long)(off - *bp) &&
			    memcmp(kp, rbufp->page + *bp, size) == 0)
				goto found;
			off = bp[1];
#ifdef HASH_STATISTICS
			hash_collisions++;
#endif
			bp += 2;
			ndx += 2;
		} else if (bp[1] == OVFLPAGE) {

            /* database corruption: overflow loop detection */
            if(last_overflow_page_no == (int32)*bp)
    			return (DATABASE_CORRUPTED_ERROR);

            last_overflow_page_no = *bp;

			rbufp = __get_buf(hashp, *bp, rbufp, 0);
			if (!rbufp) {
				save_bufp->flags &= ~BUF_PIN;
				return (DBM_ERROR);
			}

            ovfl_loop_count++;
            if(ovfl_loop_count > MAX_OVERFLOW_HASH_ACCESS_LOOPS)
    			return (DATABASE_CORRUPTED_ERROR);

			/* FOR LOOP INIT */
			bp = (uint16 *)rbufp->page;
			n = *bp++;
			ndx = 1;
			off = hashp->BSIZE;
		                } else if (bp[1] < REAL_KEY) {
			if ((ndx =
			    __find_bigpair(hashp, rbufp, ndx, kp, (int)size)) > 0)
				goto found;
			if (ndx == -2) {
				bufp = rbufp;
				if (!(pageno =
				    __find_last_page(hashp, &bufp))) {
					ndx = 0;
					rbufp = bufp;
					break;	/* FOR */
				}
				rbufp = __get_buf(hashp, pageno, bufp, 0);
				if (!rbufp) {
					save_bufp->flags &= ~BUF_PIN;
					return (DBM_ERROR);
				}
				/* FOR LOOP INIT */
				bp = (uint16 *)rbufp->page;
				n = *bp++;
				ndx = 1;
				off = hashp->BSIZE;
			} else {
				save_bufp->flags &= ~BUF_PIN;
				return (DBM_ERROR);

			}
		}
	}

	/* Not found */
	switch (action) {
	case HASH_PUT:
	case HASH_PUTNEW:
		if (__addel(hashp, rbufp, key, val)) {
			save_bufp->flags &= ~BUF_PIN;
			return (DBM_ERROR);
		} else {
			save_bufp->flags &= ~BUF_PIN;
			return (SUCCESS);
		}
	case HASH_GET:
	case HASH_DELETE:
	default:
		save_bufp->flags &= ~BUF_PIN;
		return (ABNORMAL);
	}

found:
	switch (action) {
	case HASH_PUTNEW:
		save_bufp->flags &= ~BUF_PIN;
		return (ABNORMAL);
	case HASH_GET:
		bp = (uint16 *)rbufp->page;
		if (bp[ndx + 1] < REAL_KEY) {
			if (__big_return(hashp, rbufp, ndx, val, 0))
				return (DBM_ERROR);
		} else {
			val->data = (uint8 *)rbufp->page + (int)bp[ndx + 1];
			val->size = bp[ndx] - bp[ndx + 1];
		}
		break;
	case HASH_PUT:
		if ((__delpair(hashp, rbufp, ndx)) ||
		    (__addel(hashp, rbufp, key, val))) {
			save_bufp->flags &= ~BUF_PIN;
			return (DBM_ERROR);
		}
		break;
	case HASH_DELETE:
		if (__delpair(hashp, rbufp, ndx))
			return (DBM_ERROR);
		break;
	default:
		abort();
	}
	save_bufp->flags &= ~BUF_PIN;
	return (SUCCESS);
}

static int
hash_seq(
	const DB *dbp,
	DBT *key, DBT *data,
	uint flag)
{
	register uint32 bucket;
	register BUFHEAD *bufp;
	HTAB *hashp;
	uint16 *bp, ndx;

	hashp = (HTAB *)dbp->internal;
	if (!hashp)
		return (DBM_ERROR);

	if (flag && flag != R_FIRST && flag != R_NEXT) {
		hashp->dbmerrno = errno = EINVAL;
		return (DBM_ERROR);
	}
#ifdef HASH_STATISTICS
	hash_accesses++;
#endif
	if ((hashp->cbucket < 0) || (flag == R_FIRST)) {
		hashp->cbucket = 0;
		hashp->cndx = 1;
		hashp->cpage = NULL;
	}

	for (bp = NULL; !bp || !bp[0]; ) {
		if (!(bufp = hashp->cpage)) {
			for (bucket = hashp->cbucket;
			    bucket <= (uint32)hashp->MAX_BUCKET;
			    bucket++, hashp->cndx = 1) {
				bufp = __get_buf(hashp, bucket, NULL, 0);
				if (!bufp)
					return (DBM_ERROR);
				hashp->cpage = bufp;
				bp = (uint16 *)bufp->page;
				if (bp[0])
					break;
			}
			hashp->cbucket = bucket;
			if (hashp->cbucket > hashp->MAX_BUCKET) {
				hashp->cbucket = -1;
				return (ABNORMAL);
			}
		} else
			bp = (uint16 *)hashp->cpage->page;

#ifdef DEBUG
		assert(bp);
		assert(bufp);
#endif
		while (bp[hashp->cndx + 1] == OVFLPAGE) {
			bufp = hashp->cpage =
			    __get_buf(hashp, bp[hashp->cndx], bufp, 0);
			if (!bufp)
				return (DBM_ERROR);
			bp = (uint16 *)(bufp->page);
			hashp->cndx = 1;
		}
		if (!bp[0]) {
			hashp->cpage = NULL;
			++hashp->cbucket;
		}
	}
	ndx = hashp->cndx;
	if (bp[ndx + 1] < REAL_KEY) {
		if (__big_keydata(hashp, bufp, key, data, 1))
			return (DBM_ERROR);
	} else {
		key->data = (uint8 *)hashp->cpage->page + bp[ndx];
		key->size = (ndx > 1 ? bp[ndx - 1] : hashp->BSIZE) - bp[ndx];
		data->data = (uint8 *)hashp->cpage->page + bp[ndx + 1];
		data->size = bp[ndx] - bp[ndx + 1];
		ndx += 2;
		if (ndx > bp[0]) {
			hashp->cpage = NULL;
			hashp->cbucket++;
			hashp->cndx = 1;
		} else
			hashp->cndx = ndx;
	}
	return (SUCCESS);
}

/********************************* UTILITIES ************************/

/*
 * Returns:
 *	 0 ==> OK
 *	-1 ==> Error
 */
extern int
__expand_table(HTAB *hashp)
{
	uint32 old_bucket, new_bucket;
	int new_segnum, spare_ndx;
	size_t dirsize;

#ifdef HASH_STATISTICS
	hash_expansions++;
#endif
	new_bucket = ++hashp->MAX_BUCKET;
	old_bucket = (hashp->MAX_BUCKET & hashp->LOW_MASK);

	new_segnum = new_bucket >> hashp->SSHIFT;

	/* Check if we need a new segment */
	if (new_segnum >= hashp->nsegs) {
		/* Check if we need to expand directory */
		if (new_segnum >= hashp->DSIZE) {
			/* Reallocate directory */
			dirsize = hashp->DSIZE * sizeof(SEGMENT *);
			if (!hash_realloc(&hashp->dir, dirsize, dirsize << 1))
				return (-1);
			hashp->DSIZE = dirsize << 1;
		}
		if ((hashp->dir[new_segnum] =
		    (SEGMENT)calloc((size_t)hashp->SGSIZE, sizeof(SEGMENT))) == NULL)
			return (-1);
		hashp->exsegs++;
		hashp->nsegs++;
	}
	/*
	 * If the split point is increasing (MAX_BUCKET's log base 2
	 * * increases), we need to copy the current contents of the spare
	 * split bucket to the next bucket.
	 */
	spare_ndx = __log2((uint32)(hashp->MAX_BUCKET + 1));
	if (spare_ndx > hashp->OVFL_POINT) {
		hashp->SPARES[spare_ndx] = hashp->SPARES[hashp->OVFL_POINT];
		hashp->OVFL_POINT = spare_ndx;
	}

	if (new_bucket > (uint32)hashp->HIGH_MASK) {
		/* Starting a new doubling */
		hashp->LOW_MASK = hashp->HIGH_MASK;
		hashp->HIGH_MASK = new_bucket | hashp->LOW_MASK;
	}
	/* Relocate records to the new bucket */
	return (__split_page(hashp, old_bucket, new_bucket));
}

/*
 * If realloc guarantees that the pointer is not destroyed if the realloc
 * fails, then this routine can go away.
 */
static void *
hash_realloc(
	SEGMENT **p_ptr,
	size_t oldsize, size_t newsize)
{
	register void *p;

	if ((p = malloc(newsize))) {
		memmove(p, *p_ptr, oldsize);
		memset((char *)p + oldsize, 0, newsize - oldsize);
		free(*p_ptr);
		*p_ptr = (SEGMENT *)p;
	}
	return (p);
}

extern uint32
__call_hash(HTAB *hashp, char *k, size_t len)
{
	uint32 n, bucket;

	n = hashp->hash(k, len);
	bucket = n & hashp->HIGH_MASK;
	if (bucket > (uint32)hashp->MAX_BUCKET)
		bucket = bucket & hashp->LOW_MASK;
	return (bucket);
}

/*
 * Allocate segment table.  On error, destroy the table and set errno.
 *
 * Returns 0 on success
 */
static int
alloc_segs(
	HTAB *hashp,
	int nsegs)
{
	register int i;
	register SEGMENT store;

	int save_errno;

	if ((hashp->dir =
	    (SEGMENT *)calloc((size_t)hashp->DSIZE, sizeof(SEGMENT *))) == NULL) {
		save_errno = errno;
		(void)hdestroy(hashp);
		errno = save_errno;
		return (-1);
	}
	/* Allocate segments */
	if ((store =
	    (SEGMENT)calloc((size_t)nsegs << hashp->SSHIFT, sizeof(SEGMENT))) == NULL) {
		save_errno = errno;
		(void)hdestroy(hashp);
		errno = save_errno;
		return (-1);
	}
	for (i = 0; i < nsegs; i++, hashp->nsegs++)
		hashp->dir[i] = &store[i << hashp->SSHIFT];
	return (0);
}

#if BYTE_ORDER == LITTLE_ENDIAN
/*
 * Hashp->hdr needs to be byteswapped.
 */
static void
swap_header_copy(
	HASHHDR *srcp, HASHHDR *destp)
{
	int i;

	P_32_COPY(srcp->magic, destp->magic);
	P_32_COPY(srcp->version, destp->version);
	P_32_COPY(srcp->lorder, destp->lorder);
	P_32_COPY(srcp->bsize, destp->bsize);
	P_32_COPY(srcp->bshift, destp->bshift);
	P_32_COPY(srcp->dsize, destp->dsize);
	P_32_COPY(srcp->ssize, destp->ssize);
	P_32_COPY(srcp->sshift, destp->sshift);
	P_32_COPY(srcp->ovfl_point, destp->ovfl_point);
	P_32_COPY(srcp->last_freed, destp->last_freed);
	P_32_COPY(srcp->max_bucket, destp->max_bucket);
	P_32_COPY(srcp->high_mask, destp->high_mask);
	P_32_COPY(srcp->low_mask, destp->low_mask);
	P_32_COPY(srcp->ffactor, destp->ffactor);
	P_32_COPY(srcp->nkeys, destp->nkeys);
	P_32_COPY(srcp->hdrpages, destp->hdrpages);
	P_32_COPY(srcp->h_charkey, destp->h_charkey);
	for (i = 0; i < NCACHED; i++) {
		P_32_COPY(srcp->spares[i], destp->spares[i]);
		P_16_COPY(srcp->bitmaps[i], destp->bitmaps[i]);
	}
}

static void
swap_header(HTAB *hashp)
{
	HASHHDR *hdrp;
	int i;

	hdrp = &hashp->hdr;

	M_32_SWAP(hdrp->magic);
	M_32_SWAP(hdrp->version);
	M_32_SWAP(hdrp->lorder);
	M_32_SWAP(hdrp->bsize);
	M_32_SWAP(hdrp->bshift);
	M_32_SWAP(hdrp->dsize);
	M_32_SWAP(hdrp->ssize);
	M_32_SWAP(hdrp->sshift);
	M_32_SWAP(hdrp->ovfl_point);
	M_32_SWAP(hdrp->last_freed);
	M_32_SWAP(hdrp->max_bucket);
	M_32_SWAP(hdrp->high_mask);
	M_32_SWAP(hdrp->low_mask);
	M_32_SWAP(hdrp->ffactor);
	M_32_SWAP(hdrp->nkeys);
	M_32_SWAP(hdrp->hdrpages);
	M_32_SWAP(hdrp->h_charkey);
	for (i = 0; i < NCACHED; i++) {
		M_32_SWAP(hdrp->spares[i]);
		M_16_SWAP(hdrp->bitmaps[i]);
	}
}
#endif
