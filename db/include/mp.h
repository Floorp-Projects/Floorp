/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 *
 *	@(#)mp.h	10.33 (Sleepycat) 5/4/98
 */

struct __bh;		typedef struct __bh BH;
struct __db_mpreg;	typedef struct __db_mpreg DB_MPREG;
struct __mpool;		typedef struct __mpool MPOOL;
struct __mpoolfile;	typedef struct __mpoolfile MPOOLFILE;

					/* Default mpool name. */
#define	DB_DEFAULT_MPOOL_FILE	"__db_mpool.share"

/*
 * We default to 128K (16 8K pages) if the user doesn't specify, and
 * require a minimum of 20K.
 */
#ifndef	DB_CACHESIZE_DEF
#define	DB_CACHESIZE_DEF	(128 * 1024)
#endif
#define	DB_CACHESIZE_MIN	( 20 * 1024)

#define	INVALID		0		/* Invalid shared memory offset. */

/*
 * There are three ways we do locking in the mpool code:
 *
 * Locking a handle mutex to provide concurrency for DB_THREAD operations.
 * Locking the region mutex to provide mutual exclusion while reading and
 *    writing structures in the shared region.
 * Locking buffer header mutexes during I/O.
 *
 * The first will not be further described here.  We use the shared mpool
 * region lock to provide mutual exclusion while reading/modifying all of
 * the data structures, including the buffer headers.  We use a per-buffer
 * header lock to wait on buffer I/O.  The order of locking is as follows:
 *
 * Searching for a buffer:
 *	Acquire the region lock.
 *	Find the buffer header.
 *	Increment the reference count (guarantee the buffer stays).
 *	While the BH_LOCKED flag is set (I/O is going on) {
 *	    Release the region lock.
 *		Explicitly yield the processor if it's not the first pass
 *		through this loop, otherwise, we can simply spin because
 *		we'll be simply switching between the two locks.
 *	    Request the buffer lock.
 *	    The I/O will complete...
 *	    Acquire the buffer lock.
 *	    Release the buffer lock.
 *	    Acquire the region lock.
 *	}
 *	Return the buffer.
 *
 * Reading/writing a buffer:
 *	Acquire the region lock.
 *	Find/create the buffer header.
 *	If reading, increment the reference count (guarantee the buffer stays).
 *	Set the BH_LOCKED flag.
 *	Acquire the buffer lock (guaranteed not to block).
 *	Release the region lock.
 *	Do the I/O and/or initialize the buffer contents.
 *	Release the buffer lock.
 *	    At this point, the buffer lock is available, but the logical
 *	    operation (flagged by BH_LOCKED) is not yet completed.  For
 *	    this reason, among others, threads checking the BH_LOCKED flag
 *	    must loop around their test.
 *	Acquire the region lock.
 *	Clear the BH_LOCKED flag.
 *	Release the region lock.
 *	Return/discard the buffer.
 *
 * Pointers to DB_MPOOL, MPOOL, DB_MPOOLFILE and MPOOLFILE structures are not
 * reacquired when a region lock is reacquired because they couldn't have been
 * closed/discarded and because they never move in memory.
 */
#define	LOCKINIT(dbmp, mutexp)						\
	if (F_ISSET(dbmp, MP_LOCKHANDLE | MP_LOCKREGION))		\
		(void)__db_mutex_init(mutexp,				\
		    MUTEX_LOCK_OFFSET((dbmp)->reginfo.addr, mutexp))

#define	LOCKHANDLE(dbmp, mutexp)					\
	if (F_ISSET(dbmp, MP_LOCKHANDLE))				\
		(void)__db_mutex_lock(mutexp, (dbmp)->reginfo.fd)
#define	UNLOCKHANDLE(dbmp, mutexp)					\
	if (F_ISSET(dbmp, MP_LOCKHANDLE))				\
		(void)__db_mutex_unlock(mutexp, (dbmp)->reginfo.fd)

#define	LOCKREGION(dbmp)						\
	if (F_ISSET(dbmp, MP_LOCKREGION))				\
		(void)__db_mutex_lock(&((RLAYOUT *)(dbmp)->mp)->lock,	\
		    (dbmp)->reginfo.fd)
#define	UNLOCKREGION(dbmp)						\
	if (F_ISSET(dbmp, MP_LOCKREGION))				\
		(void)__db_mutex_unlock(&((RLAYOUT *)(dbmp)->mp)->lock,	\
		(dbmp)->reginfo.fd)

#define	LOCKBUFFER(dbmp, bhp)						\
	if (F_ISSET(dbmp, MP_LOCKREGION))				\
		(void)__db_mutex_lock(&(bhp)->mutex, (dbmp)->reginfo.fd)
#define	UNLOCKBUFFER(dbmp, bhp)						\
	if (F_ISSET(dbmp, MP_LOCKREGION))				\
		(void)__db_mutex_unlock(&(bhp)->mutex, (dbmp)->reginfo.fd)

/*
 * DB_MPOOL --
 *	Per-process memory pool structure.
 */
struct __db_mpool {
/* These fields need to be protected for multi-threaded support. */
	db_mutex_t	*mutexp;	/* Structure lock. */

					/* List of pgin/pgout routines. */
	LIST_HEAD(__db_mpregh, __db_mpreg) dbregq;

					/* List of DB_MPOOLFILE's. */
	TAILQ_HEAD(__db_mpoolfileh, __db_mpoolfile) dbmfq;

/* These fields are not protected. */
	DB_ENV     *dbenv;		/* Reference to error information. */
	REGINFO	    reginfo;		/* Region information. */

	MPOOL	   *mp;			/* Address of the shared MPOOL. */

	void	   *addr;		/* Address of shalloc() region. */

	DB_HASHTAB *htab;		/* Hash table of bucket headers. */

#define	MP_LOCKHANDLE	0x01		/* Threaded, lock handles and region. */
#define	MP_LOCKREGION	0x02		/* Concurrent access, lock region. */
	u_int32_t  flags;
};

/*
 * DB_MPREG --
 *	DB_MPOOL registry of pgin/pgout functions.
 */
struct __db_mpreg {
	LIST_ENTRY(__db_mpreg) q;	/* Linked list. */

	int ftype;			/* File type. */
					/* Pgin, pgout routines. */
	int (DB_CALLBACK *pgin) __P((db_pgno_t, void *, DBT *));
	int (DB_CALLBACK *pgout) __P((db_pgno_t, void *, DBT *));
};

/*
 * DB_MPOOLFILE --
 *	Per-process DB_MPOOLFILE information.
 */
struct __db_mpoolfile {
/* These fields need to be protected for multi-threaded support. */
	db_mutex_t	*mutexp;	/* Structure lock. */

	int	   fd;			/* Underlying file descriptor. */

	u_int32_t pinref;		/* Pinned block reference count. */

/* These fields are not protected. */
	TAILQ_ENTRY(__db_mpoolfile) q;	/* Linked list of DB_MPOOLFILE's. */

	DB_MPOOL  *dbmp;		/* Overlying DB_MPOOL. */
	MPOOLFILE *mfp;			/* Underlying MPOOLFILE. */

	void	  *addr;		/* Address of mmap'd region. */
	size_t	   len;			/* Length of mmap'd region. */

/* These fields need to be protected for multi-threaded support. */
#define	MP_READONLY	0x01		/* File is readonly. */
#define	MP_UPGRADE	0x02		/* File descriptor is readwrite. */
#define	MP_UPGRADE_FAIL	0x04		/* Upgrade wasn't possible. */
	u_int32_t  flags;
};

/*
 * MPOOL --
 *	Shared memory pool region.  One of these is allocated in shared
 *	memory, and describes the pool.
 */
struct __mpool {
	RLAYOUT	    rlayout;		/* General region information. */

	SH_TAILQ_HEAD(__bhq) bhq;	/* LRU list of buckets. */
	SH_TAILQ_HEAD(__bhfq) bhfq;	/* Free buckets. */
	SH_TAILQ_HEAD(__mpfq) mpfq;	/* List of MPOOLFILEs. */

	/*
	 * We make the assumption that the early pages of the file are far
	 * more likely to be retrieved than the later pages, which means
	 * that the top bits are more interesting for hashing since they're
	 * less likely to collide.  On the other hand, since 512 4K pages
	 * represents a 2MB file, only the bottom 9 bits of the page number
	 * are likely to be set.  We XOR in the offset in the MPOOL of the
	 * MPOOLFILE that backs this particular page, since that should also
	 * be unique for the page.
	 */
#define	BUCKET(mp, mf_offset, pgno)					\
	(((pgno) ^ ((mf_offset) << 9)) % (mp)->htab_buckets)

	size_t	    htab;		/* Hash table offset. */
	size_t	    htab_buckets;	/* Number of hash table entries. */

	DB_LSN	    lsn;		/* Maximum checkpoint LSN. */
	u_int32_t   lsn_cnt;		/* Checkpoint buffers left to write. */

	DB_MPOOL_STAT stat;		/* Global mpool statistics. */

#define	MP_LSN_RETRY	0x01		/* Retry all BH_WRITE buffers. */
	u_int32_t  flags;
};

/*
 * MPOOLFILE --
 *	Shared DB_MPOOLFILE information.
 */
struct __mpoolfile {
	SH_TAILQ_ENTRY  q;		/* List of MPOOLFILEs */

	u_int32_t ref;			/* Reference count. */

	int	  ftype;		/* File type. */

	int32_t	  lsn_off;		/* Page's LSN offset. */
	u_int32_t clear_len;		/* Bytes to clear on page create. */

	size_t	  path_off;		/* File name location. */
	size_t	  fileid_off;		/* File identification location. */

	size_t	  pgcookie_len;		/* Pgin/pgout cookie length. */
	size_t	  pgcookie_off;		/* Pgin/pgout cookie location. */

	u_int32_t lsn_cnt;		/* Checkpoint buffers left to write. */

	db_pgno_t last_pgno;		/* Last page in the file. */
	db_pgno_t orig_last_pgno;	/* Original last page in the file. */

#define	MP_CAN_MMAP	0x01		/* If the file can be mmap'd. */
#define	MP_TEMP		0x02		/* Backing file is a temporary. */
	u_int32_t  flags;

	DB_MPOOL_FSTAT stat;		/* Per-file mpool statistics. */
};

/*
 * BH --
 *	Buffer header.
 */
struct __bh {
	db_mutex_t	mutex;		/* Structure lock. */

	u_int16_t	ref;		/* Reference count. */

#define	BH_CALLPGIN	0x001		/* Page needs to be reworked... */
#define	BH_DIRTY	0x002		/* Page was modified. */
#define	BH_DISCARD	0x004		/* Page is useless. */
#define	BH_LOCKED	0x008		/* Page is locked (I/O in progress). */
#define	BH_TRASH	0x010		/* Page is garbage. */
#define	BH_WRITE	0x020		/* Page scheduled for writing. */
	u_int16_t  flags;

	SH_TAILQ_ENTRY	q;		/* LRU queue. */
	SH_TAILQ_ENTRY	hq;		/* MPOOL hash bucket queue. */

	db_pgno_t pgno;			/* Underlying MPOOLFILE page number. */
	size_t	  mf_offset;		/* Associated MPOOLFILE offset. */

	/*
	 * !!!
	 * This array must be size_t aligned -- the DB access methods put PAGE
	 * and other structures into it, and expect to be able to access them
	 * directly.  (We guarantee size_t alignment in the db_mpool(3) manual
	 * page as well.)
	 */
	u_int8_t   buf[1];		/* Variable length data. */
};

#include "mp_ext.h"
