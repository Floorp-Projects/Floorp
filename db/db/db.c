/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */
/*
 * Copyright (c) 1990, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 */
/*
 * Copyright (c) 1990, 1993, 1994, 1995
 *	The Regents of the University of California.  All rights reserved.
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

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)db.c	10.57 (Sleepycat) 5/7/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#endif

#include "db_int.h"
#include "shqueue.h"
#include "db_page.h"
#include "db_shash.h"
#include "db_swap.h"
#include "btree.h"
#include "hash.h"
#include "mp.h"
#include "db_am.h"
#include "common_ext.h"

static int db_close __P((DB *, u_int32_t));
static int db_fd __P((DB *, int *));

/*
 * If the metadata page has the flag set, set the local flag.  If the page
 * does NOT have the flag set, return EINVAL if the user's dbinfo argument
 * caused us to already set the local flag.
 */
#define	DBINFO_FCHK(dbp, fn, meta_flags, m_name, dbp_name) {		\
	if ((meta_flags) & (m_name))					\
		F_SET(dbp, dbp_name);					\
	else								\
		if (F_ISSET(dbp, dbp_name)) {				\
			__db_err(dbenv,					\
	    "%s: %s specified in dbinfo argument but not set in file",	\
			    fname, fn);					\
			goto einval;					\
		}							\
}

/*
 * db_open --
 *	Main library interface to the DB access methods.
 */
int
db_open(fname, type, flags, mode, dbenv, dbinfo, dbpp)
	const char *fname;
	DBTYPE type;
	u_int32_t flags;
	int mode;
	DB_ENV *dbenv;
	DB_INFO *dbinfo;
	DB **dbpp;
{
	BTMETA *btm;
	DB *dbp;
	DBT pgcookie;
	DB_ENV *envp, t_dbenv;
	DB_MPOOL_FINFO finfo;
	DB_PGINFO pginfo;
	HASHHDR *hashm;
	size_t cachesize;
	ssize_t nr;
	u_int32_t iopsize;
	int fd, ftype, need_fileid, restore, ret, retry_cnt, swapped;
	char *real_name, mbuf[512];

	/* Validate arguments. */
#ifdef HAVE_SPINLOCKS
#define	OKFLAGS	(DB_CREATE | DB_NOMMAP | DB_RDONLY | DB_THREAD | DB_TRUNCATE)
#else
#define	OKFLAGS	(DB_CREATE | DB_NOMMAP | DB_RDONLY | DB_TRUNCATE)
#endif
	if ((ret = __db_fchk(dbenv, "db_open", flags, OKFLAGS)) != 0)
		return (ret);

	if (dbenv != NULL) {
		/*
		 * You can't specify threads during the db_open() if the
		 * environment wasn't configured with them.
		 */
		if (LF_ISSET(DB_THREAD) && !F_ISSET(dbenv, DB_ENV_THREAD)) {
			__db_err(dbenv,
			    "environment not created using DB_THREAD");
			return (EINVAL);
		}

		/*
		 * Specifying a cachesize to db_open(3), after creating an
		 * environment, is a common mistake.
		 */
		if (dbinfo != NULL && dbinfo->db_cachesize != 0) {
			__db_err(dbenv,
			    "cachesize will be ignored if environment exists");
			return (EINVAL);
		}
	}

	/* Initialize for error return. */
	fd = -1;
	need_fileid = 1;
	real_name = NULL;

	/* Allocate the DB structure, reference the DB_ENV structure. */
	if ((dbp = (DB *)__db_calloc(1, sizeof(DB))) == NULL) {
		__db_err(dbenv, "%s", strerror(ENOMEM));
		return (ENOMEM);
	}
	dbp->dbenv = dbenv;

	/* Convert the db_open(3) flags. */
	if (LF_ISSET(DB_RDONLY))
		F_SET(dbp, DB_AM_RDONLY);
	if (LF_ISSET(DB_THREAD))
		F_SET(dbp, DB_AM_THREAD);

	/* Convert the dbinfo structure flags. */
	if (dbinfo != NULL) {
		/*
		 * !!!
		 * We can't check for illegal flags until we know what type
		 * of open we're doing.
		 */
		if (F_ISSET(dbinfo, DB_DELIMITER))
			F_SET(dbp, DB_RE_DELIMITER);
		if (F_ISSET(dbinfo, DB_DUP))
			F_SET(dbp, DB_AM_DUP);
		if (F_ISSET(dbinfo, DB_FIXEDLEN))
			F_SET(dbp, DB_RE_FIXEDLEN);
		if (F_ISSET(dbinfo, DB_PAD))
			F_SET(dbp, DB_RE_PAD);
		if (F_ISSET(dbinfo, DB_RECNUM))
			F_SET(dbp, DB_BT_RECNUM);
		if (F_ISSET(dbinfo, DB_RENUMBER))
			F_SET(dbp, DB_RE_RENUMBER);
		if (F_ISSET(dbinfo, DB_SNAPSHOT))
			F_SET(dbp, DB_RE_SNAPSHOT);
	}

	/*
	 * Always set the master and initialize the queues, so we can
	 * use these fields without checking the thread bit.
	 */
	dbp->master = dbp;
	LIST_INIT(&dbp->handleq);
	LIST_INSERT_HEAD(&dbp->handleq, dbp, links);
	TAILQ_INIT(&dbp->curs_queue);

	/*
	 * Set based on the dbenv fields, although no logging or transactions
	 * are possible for temporary files.
	 */
	if (dbenv != NULL) {
		if (dbenv->lk_info != NULL)
			F_SET(dbp, DB_AM_LOCKING);
		if (fname != NULL && dbenv->lg_info != NULL)
			F_SET(dbp, DB_AM_LOGGING);
	}

	/* Set the common fields. */
	if (dbinfo == NULL) {
		dbp->pgsize = 0;
		dbp->db_malloc = NULL;
	} else {
		dbp->pgsize = dbinfo->db_pagesize;
		dbp->db_malloc = dbinfo->db_malloc;
	}

	/* Fill in the default file mode. */
	if (mode == 0)
		mode = __db_omode("rwrw--");

	/* Check if the user wants us to swap byte order. */
	if (dbinfo != NULL)
		switch (ret = __db_byteorder(dbenv, dbinfo->db_lorder)) {
		case 0:
			break;
		case DB_SWAPBYTES:
			F_SET(dbp, DB_AM_SWAP);
			break;
		default:
			goto err;
		}

	/*
	 * If we have a file name, try and read the first page, figure out
	 * what type of file it is, and initialize everything we can based
	 * on that file's meta-data page.
	 *
	 * XXX
	 * We don't actually expect zero-length strings as arguments.  We
	 * do the check, permitting them, because scripting languages, e.g.,
	 * the Tcl test suite, doesn't know anything about passing NULL's.
	 */
	if (fname != NULL && fname[0] != '\0') {
		/* Get the real file name. */
		if ((ret = __db_appname(dbenv,
		     DB_APP_DATA, NULL, fname, 0, NULL, &real_name)) != 0)
			goto err;

		/*
		 * Open the backing file.  We need to make sure that multiple
		 * processes attempting to create the file at the same time
		 * are properly ordered so that only one of them creates the
		 * "unique" file id, so we open it O_EXCL and O_CREAT so two
		 * simultaneous attempts to create the region will return
		 * failure in one of the attempts.  If we're one of the ones
		 * that fail, we simply retry without the O_CREAT flag, which
		 * will require that the meta-data page exist.
		 */
		retry_cnt = 0;
open_retry:	if (LF_ISSET(DB_CREATE)) {
			if ((ret = __db_open(real_name, flags | DB_EXCL,
			    OKFLAGS | DB_EXCL, mode, &fd)) != 0)
				if (ret == EEXIST) {
					LF_CLR(DB_CREATE);
					goto open_retry;
				} else {
					__db_err(dbenv,
					    "%s: %s", fname, strerror(ret));
					goto err;
				}
		} else
			if ((ret = __db_open(real_name,
			    flags, OKFLAGS, mode, &fd)) != 0) {
				__db_err(dbenv, "%s: %s", fname, strerror(ret));
				goto err;
			}

		/*
		 * Use the optimum I/O size as the pagesize if a pagesize not
		 * specified.  Some filesystems have 64K as their optimum I/O
		 * size, but as that results in impossibly large default cache
		 * sizes, we limit the default pagesize to 16K.
		 */
		if (dbp->pgsize == 0) {
			if ((ret = __db_ioinfo(real_name,
			    fd, NULL, NULL, &iopsize)) != 0) {
				__db_err(dbenv,
				    "%s: %s", real_name, strerror(ret));
				goto err;
			}
			if (iopsize < 512)
				iopsize = 512;
			if (iopsize > 16 * 1024)
				iopsize = 16 * 1024;
			dbp->pgsize = iopsize;
			F_SET(dbp, DB_AM_PGDEF);
		}

		/*
		 * Try and read the first disk sector -- this code assumes
		 * that the meta-data for all access methods fits in 512
		 * bytes, and that no database will be smaller than that.
		 */
		if ((ret = __db_read(fd, mbuf, sizeof(mbuf), &nr)) != 0)
			goto err;

		/* The fd is no longer needed. */
		(void)__db_close(fd);
		fd = -1;

		if (nr != sizeof(mbuf)) {
			if (nr != 0) {
				__db_err(dbenv,
				    "%s: unexpected file format", fname);
				goto einval;
			}
			/*
			 * The only way we can reach here with the DB_CREATE
			 * flag set is if we created the file.  If that's not
			 * the case, then a) someone else created the file
			 * but has not yet written out the meta-data page, or
			 * b) we truncated the file (DB_TRUNCATE) leaving it
			 * zero-length.  In the case of a), we want to sleep
			 * and give the file creator some time to write the
			 * metadata page.  In the case of b), charge forward.
			 * Note, there is a race in the case of two processes
			 * opening the file with the DB_TRUNCATE flag set at
			 * roughly the same time, and they could theoretically
			 * hurt each other, although it's pretty unlikely.
			 */
			if (retry_cnt++ < 3 &&
			    !LF_ISSET(DB_CREATE | DB_TRUNCATE)) {
				__db_sleep(1, 0);
				goto open_retry;
			}
			if (type == DB_UNKNOWN) {
				__db_err(dbenv,
				    "%s: DBTYPE of unknown with empty file",
				    fname);
				goto einval;
			}
			goto empty;
		}

		/*
		 * A found file overrides some user information.  We'll check
		 * for possible error conditions based on conflicts between
		 * the file and the user's arguments below.
		 */
		swapped = 0;
		F_CLR(dbp, DB_AM_SWAP);

retry:		switch (((BTMETA *)mbuf)->magic) {
		case DB_BTREEMAGIC:
			if (type != DB_BTREE &&
			    type != DB_RECNO && type != DB_UNKNOWN)
				goto einval;

			btm = (BTMETA *)mbuf;
			if (swapped && (ret = __bam_mswap((PAGE *)btm)) != 0)
				goto err;

			if (btm->version < DB_BTREEOLDVER ||
			    btm->version > DB_BTREEVERSION) {
				__db_err(dbenv,
				    "%s: unsupported btree version number %lu",
				    fname, (u_long)btm->version);
				goto einval;
			}
			dbp->pgsize = btm->pagesize;
			F_CLR(dbp, DB_AM_PGDEF);

			if ((ret = __db_fchk(dbenv,
			    "db_open", btm->flags, BTM_MASK)) != 0)
				goto err;
			DBINFO_FCHK(dbp, "DB_DUP",
			    btm->flags, BTM_DUP, DB_AM_DUP);
			if (F_ISSET(btm, BTM_RECNO)) {
				DBINFO_FCHK(dbp, "DB_FIXEDLEN",
				    btm->flags, BTM_FIXEDLEN, DB_RE_FIXEDLEN);
				DBINFO_FCHK(dbp, "DB_RENUMBER",
				    btm->flags, BTM_RENUMBER, DB_RE_RENUMBER);
				type = DB_RECNO;
			} else {
				DBINFO_FCHK(dbp, "DB_RECNUM",
				    btm->flags, BTM_RECNUM, DB_BT_RECNUM);
				type = DB_BTREE;
			}

			/* Copy the file's unique id. */
			need_fileid = 0;
			memcpy(dbp->lock.fileid, btm->uid, DB_FILE_ID_LEN);
			break;
		case DB_HASHMAGIC:
			if (type != DB_HASH && type != DB_UNKNOWN)
				goto einval;

			hashm = (HASHHDR *)mbuf;
			if (swapped && (ret = __ham_mswap((PAGE *)hashm)) != 0)
				goto err;

			if (hashm->version < DB_HASHOLDVER ||
			    hashm->version > DB_HASHVERSION) {
				__db_err(dbenv,
				    "%s: unsupported hash version number %lu",
				    fname, hashm->version);
				goto einval;
			}
			dbp->pgsize = hashm->pagesize;
			F_CLR(dbp, DB_AM_PGDEF);

			if ((ret = __db_fchk(dbenv,
			    "db_open", hashm->flags, DB_HASH_DUP)) != 0)
				goto err;
			DBINFO_FCHK(dbp, "DB_DUP",
			    hashm->flags, DB_HASH_DUP, DB_AM_DUP);
			type = DB_HASH;

			/* Copy the file's unique id. */
			need_fileid = 0;
			memcpy(dbp->lock.fileid, hashm->uid, DB_FILE_ID_LEN);
			break;
		default:
			if (swapped) {
				__db_err(dbenv, "unrecognized file type");
				goto einval;
			}
			M_32_SWAP(((BTMETA *)mbuf)->magic);
			F_SET(dbp, DB_AM_SWAP);

			swapped = 1;
			goto retry;
		}
	} else {
		fname = real_name = NULL;

		if (type == DB_UNKNOWN) {
			__db_err(dbenv,
			    "DBTYPE of unknown without existing file");
			goto einval;
		}
		F_SET(dbp, DB_AM_INMEM);
	}

empty:	/*
	 * By the time we get here we've either set the type or we're taking
	 * it from the user.
	 */
	dbp->type = type;

	/*
	 * Set the page size to the best value for I/O to this file.  Don't
	 * overflow the page offset type.  The page size must be db_indx_t
	 * aligned and >= MIN_PAGE_SIZE.
	 *
	 * XXX
	 * Should we be checking for a page size that's not a multiple of 512?
	 */
	if (dbp->pgsize == 0) {
		F_SET(dbp, DB_AM_PGDEF);
		dbp->pgsize = 8 * 1024;
	}
	if (dbp->pgsize < DB_MIN_PGSIZE ||
	    dbp->pgsize > DB_MAX_PGSIZE ||
	    dbp->pgsize & (sizeof(db_indx_t) - 1)) {
		__db_err(dbenv, "illegal page size");
		goto einval;
	}

	/*
	 * If no mpool supplied by the application, attach to a local,
	 * created buffer pool.
	 *
	 * XXX
	 * If the user has a DB_ENV structure, we have to use a temporary
	 * one so that we don't step on their values.  If the user doesn't,
	 * we have to create one, and keep it around until the call to the
	 * memp_close() function.  This is all so the mpool functions get
	 * the error stuff right.
	 */
	if (dbenv == NULL || dbenv->mp_info == NULL) {
		F_SET(dbp, DB_AM_MLOCAL);

		if (dbenv == NULL) {
			if ((dbp->mp_dbenv =
			    (DB_ENV *)__db_calloc(sizeof(DB_ENV), 1)) == NULL) {
				ret = ENOMEM;
				goto err;
			}

			envp = dbp->mp_dbenv;
			restore = 0;
		} else {
			t_dbenv = *dbenv;

			envp = dbenv;
			restore = 1;
		}

		/*
		 * Set and/or correct the cache size; must be a multiple of
		 * the page size.
		 */
		if (dbinfo == NULL || dbinfo->db_cachesize == 0)
			cachesize = dbp->pgsize * DB_MINCACHE;
		else {
			cachesize = dbinfo->db_cachesize;
			if (cachesize & (dbp->pgsize - 1))
				cachesize +=
				    (~cachesize & (dbp->pgsize - 1)) + 1;
			if (cachesize < dbp->pgsize * DB_MINCACHE)
				cachesize = dbp->pgsize * DB_MINCACHE;
			if (cachesize < 20 * 1024)
				cachesize = 20 * 1024;
		}
		envp->mp_size = cachesize;

		if ((ret = memp_open(NULL, DB_CREATE | DB_MPOOL_PRIVATE |
		    (F_ISSET(dbp, DB_AM_THREAD) ? DB_THREAD : 0),
		    __db_omode("rw----"), envp, &dbp->mp)) != 0)
			goto err;
		if (restore)
			*dbenv = t_dbenv;
	} else
		dbp->mp = dbenv->mp_info;

	/* Register DB's pgin/pgout functions. */
	if ((ret = memp_register(dbp->mp,
	    DB_FTYPE_BTREE, __bam_pgin, __bam_pgout)) != 0)
		goto err;
	if ((ret = memp_register(dbp->mp,
	    DB_FTYPE_HASH, __ham_pgin, __ham_pgout)) != 0)
		goto err;

	/*
	 * If we don't already have one, get a unique file ID.  If the file
	 * is a temporary file, then we have to create a unique file ID --
	 * no backing file will be created until the mpool cache is filled
	 * forcing it to go to disk.  The created ID must never match any
	 * potential real file ID -- we know it won't because real file IDs
	 * contain a time stamp after the dev/ino pair, and we're simply
	 * storing a 4-byte locker ID.
	 *
	 * XXX
	 * Store the file id in the locker structure -- we can get it from
	 * there as necessary, and it saves having two copies.
	 */
	if (need_fileid)
		if (fname == NULL) {
			memset(dbp->lock.fileid, 0, DB_FILE_ID_LEN);
			if (F_ISSET(dbp, DB_AM_LOCKING) &&
			    (ret = lock_id(dbenv->lk_info,
			    (u_int32_t *)dbp->lock.fileid)) != 0)
				goto err;
		} else
			if ((ret = __db_fileid(dbenv,
			    real_name, 1, dbp->lock.fileid)) != 0)
				goto err;

	/* No further use for the real name. */
	if (real_name != NULL)
		FREES(real_name);
	real_name = NULL;

	/*
	 * Open a backing file in the memory pool.
	 *
	 * If we need to process the file's pages on I/O, set the file type.
	 * If it's a hash file, always call pgin and pgout routines.  This
	 * means that hash files can never be mapped into process memory.  If
	 * it's a btree file and requires swapping, we need to page the file
	 * in and out.  This has to be right -- we can't mmap files that are
	 * being paged in and out.
	 */
	if (type == DB_HASH)
		ftype = DB_FTYPE_HASH;
	else
		ftype = F_ISSET(dbp, DB_AM_SWAP) ? DB_FTYPE_BTREE : 0;
	pginfo.db_pagesize = dbp->pgsize;
	pginfo.needswap = F_ISSET(dbp, DB_AM_SWAP);
	pgcookie.data = &pginfo;
	pgcookie.size = sizeof(DB_PGINFO);

	/*
	 * Set up additional memp_fopen information.
	 */
	memset(&finfo, 0, sizeof(finfo));
	finfo.ftype = ftype;
	finfo.pgcookie = &pgcookie;
	finfo.fileid = dbp->lock.fileid;
	finfo.lsn_offset = 0;
	finfo.clear_len = DB_PAGE_CLEAR_LEN;
	if ((ret = memp_fopen(dbp->mp, fname,
	    F_ISSET(dbp, DB_AM_RDONLY) ? DB_RDONLY : 0,
	    0, dbp->pgsize, &finfo, &dbp->mpf)) != 0)
		goto err;

	/*
	 * XXX
	 * Truly spectacular layering violation.  We need a per-thread mutex
	 * that lives in shared memory (thanks, HP-UX!) and so we acquire a
	 * pointer to the mpool one.
	 */
	if (F_ISSET(dbp, DB_AM_THREAD))
		dbp->mutexp = dbp->mpf->mutexp;

	/* Get a log file id. */
	if (F_ISSET(dbp, DB_AM_LOGGING) &&
	    (ret = log_register(dbenv->lg_info,
	    dbp, fname, type, &dbp->log_fileid)) != 0)
		goto err;

	/*
	 * Get a locker id for this DB, and build the lock cookie: the first
	 * db_pgno_t bytes are the page number, the next N bytes are the file
	 * id.
	 */
	if (F_ISSET(dbp, DB_AM_LOCKING)) {
		if ((ret = lock_id(dbenv->lk_info, &dbp->locker)) != 0)
			goto err;
		dbp->lock_dbt.size = sizeof(dbp->lock);
		dbp->lock_dbt.data = &dbp->lock;
	}

	/* Call the real open function. */
	switch (type) {
	case DB_BTREE:
		if (dbinfo != NULL && (ret = __db_fchk(dbenv,
		    "db_open", dbinfo->flags, DB_RECNUM | DB_DUP)) != 0)
			goto err;
		if (dbinfo != NULL && (ret = __db_fcchk(dbenv,
		    "db_open", dbinfo->flags, DB_DUP, DB_RECNUM)) != 0)
			goto err;
		if ((ret = __bam_open(dbp, type, dbinfo)) != 0)
			goto err;
		break;
	case DB_HASH:
		if (dbinfo != NULL && (ret = __db_fchk(dbenv,
		    "db_open", dbinfo->flags, DB_DUP)) != 0)
			goto err;
		if ((ret = __ham_open(dbp, dbinfo)) != 0)
			goto err;
		break;
	case DB_RECNO:
#define	DB_INFO_FLAGS \
	(DB_DELIMITER | DB_FIXEDLEN | DB_PAD | DB_RENUMBER | DB_SNAPSHOT)
		if (dbinfo != NULL && (ret = __db_fchk(dbenv,
		    "db_open", dbinfo->flags, DB_INFO_FLAGS)) != 0)
			goto err;
		if ((ret = __ram_open(dbp, type, dbinfo)) != 0)
			goto err;
		break;
	default:
		abort();
	}

	/* Call a local close routine. */
	dbp->close = db_close;
	dbp->fd = db_fd;

	*dbpp = dbp;
	return (0);

einval:	ret = EINVAL;
err:	/* Close the file descriptor. */
	if (fd != -1)
		(void)__db_close(fd);

	/* Discard the log file id. */
	if (dbp->log_fileid != 0)
		(void)log_unregister(dbenv->lg_info, dbp->log_fileid);

	/* Close the memory pool file. */
	if (dbp->mpf != NULL)
		(void)memp_fclose(dbp->mpf);

	/* If the memory pool was local, close it. */
	if (F_ISSET(dbp, DB_AM_MLOCAL) && dbp->mp != NULL)
		(void)memp_close(dbp->mp);

	/* If we allocated a DB_ENV, discard it. */
	if (dbp->mp_dbenv != NULL)
		FREE(dbp->mp_dbenv, sizeof(DB_ENV));

	if (real_name != NULL)
		FREES(real_name);
	if (dbp != NULL)
		FREE(dbp, sizeof(DB));

	return (ret);
}

/*
 * db_close --
 *	Close a DB tree.
 */
static int
db_close(dbp, flags)
	DB *dbp;
	u_int32_t flags;
{
	DBC *dbc;
	DB *tdbp;
	int ret, t_ret;

	/* Validate arguments. */
	if ((ret = __db_fchk(dbp->dbenv, "db_close", flags, DB_NOSYNC)) != 0)
		return (ret);

	/* Sync the underlying file. */
	if (!LF_ISSET(DB_NOSYNC) &&
	    (t_ret = dbp->sync(dbp, 0)) != 0 && ret == 0)
		ret = t_ret;

	/*
	 * Call the underlying access method close routine for all the
	 * cursors and handles.
	 */
	for (tdbp = LIST_FIRST(&dbp->handleq);
	    tdbp != NULL; tdbp = LIST_NEXT(tdbp, links)) {
		while ((dbc = TAILQ_FIRST(&tdbp->curs_queue)) != NULL)
			switch (tdbp->type) {
			case DB_BTREE:
				if ((t_ret =
				    __bam_c_iclose(tdbp, dbc)) != 0 && ret == 0)
					ret = t_ret;
				break;
			case DB_HASH:
				if ((t_ret =
				    __ham_c_iclose(tdbp, dbc)) != 0 && ret == 0)
					ret = t_ret;
				break;
			case DB_RECNO:
				if ((t_ret =
				    __ram_c_iclose(tdbp, dbc)) != 0 && ret == 0)
					ret = t_ret;
				break;
			default:
				abort();
			}

		switch (tdbp->type) {
		case DB_BTREE:
			if ((t_ret = __bam_close(tdbp)) != 0 && ret == 0)
				ret = t_ret;
			break;
		case DB_HASH:
			if ((t_ret = __ham_close(tdbp)) != 0 && ret == 0)
				ret = t_ret;
			break;
		case DB_RECNO:
			if ((t_ret = __ram_close(tdbp)) != 0 && ret == 0)
				ret = t_ret;
			break;
		default:
			abort();
		}
	}

	/* Sync the memory pool. */
	if (!LF_ISSET(DB_NOSYNC) && (t_ret = memp_fsync(dbp->mpf)) != 0 &&
	    t_ret != DB_INCOMPLETE && ret == 0)
		ret = t_ret;

	/* Close the memory pool file. */
	if ((t_ret = memp_fclose(dbp->mpf)) != 0 && ret == 0)
		ret = t_ret;

	/* If the memory pool was local, close it. */
	if (F_ISSET(dbp, DB_AM_MLOCAL) &&
	    (t_ret = memp_close(dbp->mp)) != 0 && ret == 0)
		ret = t_ret;

	/* Discard the log file id. */
	if (F_ISSET(dbp, DB_AM_LOGGING))
		(void)log_unregister(dbp->dbenv->lg_info, dbp->log_fileid);

	/* Discard the lock cookie for all handles. */
	for (tdbp = LIST_FIRST(&dbp->handleq);
	    tdbp != NULL; tdbp = LIST_NEXT(tdbp, links))
		if (F_ISSET(tdbp, DB_AM_LOCKING)) {
#ifdef DEBUG
			DB_LOCKREQ request;

			/*
			 * If we're running tests, display any locks currently
			 * held.  It's possible that some applications may hold
			 * locks for long periods, e.g., conference room locks,
			 * but the DB tests should never close holding locks.
			 */
			request.op = DB_LOCK_DUMP;
			if ((t_ret = lock_vec(tdbp->dbenv->lk_info,
			    tdbp->locker, 0, &request, 1, NULL)) != 0 &&
			    ret == 0)
				ret = EAGAIN;
#endif
		}

	/* If we allocated a DB_ENV, discard it. */
	if (dbp->mp_dbenv != NULL)
		FREE(dbp->mp_dbenv, sizeof(DB_ENV));

	/* Free all of the DB's. */
	LIST_REMOVE(dbp, links);
	while ((tdbp = LIST_FIRST(&dbp->handleq)) != NULL) {
		LIST_REMOVE(tdbp, links);
		FREE(tdbp, sizeof(*tdbp));
	}
	FREE(dbp, sizeof(*dbp));

	return (ret);
}

/*
 * db_fd --
 *	Return a file descriptor for flock'ing.
 */
static int
db_fd(dbp, fdp)
        DB *dbp;
	int *fdp;
{
	/*
	 * XXX
	 * Truly spectacular layering violation.
	 */
	return (__mp_xxx_fd(dbp->mpf, fdp));
}

/*
 * __db_pgerr --
 *	Error when unable to retrieve a specified page.
 *
 * PUBLIC: int __db_pgerr __P((DB *, db_pgno_t));
 */
int
__db_pgerr(dbp, pgno)
	DB *dbp;
	db_pgno_t pgno;
{
	/*
	 * Three things are certain:
	 * Death, taxes, and lost data.
	 * Guess which has occurred.
	 */
	__db_err(dbp->dbenv,
	    "unable to create/retrieve page %lu", (u_long)pgno);
	return (__db_panic(dbp));
}

/*
 * __db_pgfmt --
 *	Error when a page has the wrong format.
 *
 * PUBLIC: int __db_pgfmt __P((DB *, db_pgno_t));
 */
int
__db_pgfmt(dbp, pgno)
	DB *dbp;
	db_pgno_t pgno;
{
	__db_err(dbp->dbenv,
	    "page %lu: illegal page type or format", (u_long)pgno);
	return (__db_panic(dbp));
}
