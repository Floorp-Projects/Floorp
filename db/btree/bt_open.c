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
 * This code is derived from software contributed to Berkeley by
 * Mike Olson.
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
static const char sccsid[] = "@(#)bt_open.c	10.27 (Sleepycat) 5/6/98";
#endif /* not lint */

/*
 * Implementation of btree access method for 4.4BSD.
 *
 * The design here was originally based on that of the btree access method
 * used in the Postgres database system at UC Berkeley.  This implementation
 * is wholly independent of the Postgres code.
 */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <limits.h>
#include <string.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "btree.h"

static int __bam_keyalloc __P((BTREE *));
static int __bam_setmeta __P((DB *, BTREE *));

/*
 * __bam_open --
 *	Open a btree.
 *
 * PUBLIC: int __bam_open __P((DB *, DBTYPE, DB_INFO *));
 */
int
__bam_open(dbp, type, dbinfo)
	DB *dbp;
	DBTYPE type;
	DB_INFO *dbinfo;
{
	BTREE *t;
	int ret;

	/* Allocate the btree internal structure. */
	if ((t = (BTREE *)__db_calloc(1, sizeof(BTREE))) == NULL)
		return (ENOMEM);

	t->bt_sp = t->bt_csp = t->bt_stack;
	t->bt_esp = t->bt_stack + sizeof(t->bt_stack) / sizeof(t->bt_stack[0]);

	if ((type == DB_RECNO || F_ISSET(dbp, DB_BT_RECNUM)) &&
	    (ret = __bam_keyalloc(t)) != 0)
		goto err;

	/*
	 * Intention is to make sure all of the user's selections are okay
	 * here and then use them without checking.
	 */
	if (dbinfo != NULL) {
		/* Minimum number of keys per page. */
		if (dbinfo->bt_minkey == 0)
			t->bt_minkey = DEFMINKEYPAGE;
		else {
			if (dbinfo->bt_minkey < 2)
				goto einval;
			t->bt_minkey = dbinfo->bt_minkey;
		}

		/* Maximum number of keys per page. */
		if (dbinfo->bt_maxkey == 0)
			t->bt_maxkey = 0;
		else {
			if (dbinfo->bt_maxkey < 1)
				goto einval;
			t->bt_maxkey = dbinfo->bt_maxkey;
		}

		/*
		 * If no comparison, use default comparison.  If no comparison
		 * and no prefix, use default prefix.  (We can't default the
		 * prefix if the user supplies a comparison routine; shortening
		 * the keys may break their comparison algorithm.)
		 */
		t->bt_compare = dbinfo->bt_compare == NULL ?
		    __bam_defcmp : dbinfo->bt_compare;
		t->bt_prefix = dbinfo->bt_prefix == NULL ?
		    (dbinfo->bt_compare == NULL ?
		    __bam_defpfx : NULL) : dbinfo->bt_prefix;
	} else {
		t->bt_minkey = DEFMINKEYPAGE;
		t->bt_compare = __bam_defcmp;
		t->bt_prefix = __bam_defpfx;
	}

	/* Initialize the remaining fields of the DB. */
	dbp->type = type;
	dbp->internal = t;
	dbp->cursor = __bam_cursor;
	dbp->del = __bam_delete;
	dbp->get = __bam_get;
	dbp->put = __bam_put;
	dbp->stat = __bam_stat;
	dbp->sync = __bam_sync;

	/*
	 * The btree data structure requires that at least two key/data pairs
	 * can fit on a page, but other than that there's no fixed requirement.
	 * Translate the minimum number of items into the bytes a key/data pair
	 * can use before being placed on an overflow page.  We calculate for
	 * the worst possible alignment by assuming every item requires the
	 * maximum alignment for padding.
	 *
	 * Recno uses the btree bt_ovflsize value -- it's close enough.
	 */
	t->bt_ovflsize = (dbp->pgsize - P_OVERHEAD) / (t->bt_minkey * P_INDX)
	    - (BKEYDATA_PSIZE(0) + ALIGN(1, 4));

	/* Create a root page if new tree. */
	if ((ret = __bam_setmeta(dbp, t)) != 0)
		goto err;

	return (0);

einval:	ret = EINVAL;

err:	if (t != NULL) {
		/* If we allocated room for key/data return, discard it. */
		if (t->bt_rkey.data != NULL)
			__db_free(t->bt_rkey.data);

		FREE(t, sizeof(BTREE));
	}
	return (ret);
}

/*
 * __bam_bdup --
 *	Create a BTREE handle for a threaded DB handle.
 *
 * PUBLIC: int __bam_bdup __P((DB *, DB *));
 */
int
__bam_bdup(orig, new)
	DB *orig, *new;
{
	BTREE *t, *ot;
	int ret;

	ot = orig->internal;

	if ((t = (BTREE *)__db_calloc(1, sizeof(*t))) == NULL)
		return (ENOMEM);

	/*
	 * !!!
	 * Ignore the cursor queue, only the first DB has attached cursors.
	 */

	t->bt_sp = t->bt_csp = t->bt_stack;
	t->bt_esp = t->bt_stack + sizeof(t->bt_stack) / sizeof(t->bt_stack[0]);

	if ((orig->type == DB_RECNO || F_ISSET(orig, DB_BT_RECNUM)) &&
	    (ret = __bam_keyalloc(t)) != 0) {
		FREE(t, sizeof(*t));
		return (ret);
	}

	t->bt_maxkey = ot->bt_maxkey;
	t->bt_minkey = ot->bt_minkey;
	t->bt_compare = ot->bt_compare;
	t->bt_prefix = ot->bt_prefix;
	t->bt_ovflsize = ot->bt_ovflsize;

	/*
	 * !!!
	 * The entire RECNO structure is shared.  If it breaks, the application
	 * was misusing it to start with.
	 */
	t->bt_recno = ot->bt_recno;

	new->internal = t;

	return (0);
}

/*
 * __bam_keyalloc --
 *	Allocate return memory for recno keys.
 */
static int
__bam_keyalloc(t)
	BTREE *t;
{
	/*
	 * Recno keys are always the same size, and we don't want to have
	 * to check for space on each return.  Allocate it now.
	 */
	if ((t->bt_rkey.data = (void *)__db_malloc(sizeof(db_recno_t))) == NULL)
		return (ENOMEM);
	t->bt_rkey.ulen = sizeof(db_recno_t);
	return (0);
}

/*
 * __bam_setmeta --
 *	Check (and optionally create) a tree.
 */
static int
__bam_setmeta(dbp, t)
	DB *dbp;
	BTREE *t;
{
	BTMETA *meta;
	PAGE *root;
	DB_LOCK metalock, rootlock;
	db_pgno_t pgno;
	int ret;

	/* Get, and optionally create the metadata page. */
	pgno = PGNO_METADATA;
	if ((ret =
	    __bam_lget(dbp, 0, PGNO_METADATA, DB_LOCK_WRITE, &metalock)) != 0)
		return (ret);
	if ((ret =
	    __bam_pget(dbp, (PAGE **)&meta, &pgno, DB_MPOOL_CREATE)) != 0) {
		(void)__BT_LPUT(dbp, metalock);
		return (ret);
	}

	/*
	 * If the magic number is correct, we're not creating the tree.
	 * Correct any fields that may not be right.  Note, all of the
	 * local flags were set by db_open(3).
	 */
	if (meta->magic != 0) {
		t->bt_maxkey = meta->maxkey;
		t->bt_minkey = meta->minkey;

		(void)memp_fput(dbp->mpf, (PAGE *)meta, 0);
		(void)__BT_LPUT(dbp, metalock);
		return (0);
	}

	/* Initialize the tree structure metadata information. */
	memset(meta, 0, sizeof(BTMETA));
	ZERO_LSN(meta->lsn);
	meta->pgno = PGNO_METADATA;
	meta->magic = DB_BTREEMAGIC;
	meta->version = DB_BTREEVERSION;
	meta->pagesize = dbp->pgsize;
	meta->maxkey = t->bt_maxkey;
	meta->minkey = t->bt_minkey;
	meta->free = PGNO_INVALID;
	if (dbp->type == DB_RECNO)
		F_SET(meta, BTM_RECNO);
	if (F_ISSET(dbp, DB_AM_DUP))
		F_SET(meta, BTM_DUP);
	if (F_ISSET(dbp, DB_RE_FIXEDLEN))
		F_SET(meta, BTM_FIXEDLEN);
	if (F_ISSET(dbp, DB_BT_RECNUM))
		F_SET(meta, BTM_RECNUM);
	if (F_ISSET(dbp, DB_RE_RENUMBER))
		F_SET(meta, BTM_RENUMBER);
	memcpy(meta->uid, dbp->lock.fileid, DB_FILE_ID_LEN);

	/* Create and initialize a root page. */
	pgno = PGNO_ROOT;
	if ((ret =
	    __bam_lget(dbp, 0, PGNO_ROOT, DB_LOCK_WRITE, &rootlock)) != 0)
		return (ret);
	if ((ret = __bam_pget(dbp, &root, &pgno, DB_MPOOL_CREATE)) != 0) {
		(void)__BT_LPUT(dbp, rootlock);
		return (ret);
	}
	P_INIT(root, dbp->pgsize, PGNO_ROOT, PGNO_INVALID,
	    PGNO_INVALID, 1, dbp->type == DB_RECNO ? P_LRECNO : P_LBTREE);
	ZERO_LSN(root->lsn);

	/* Release the metadata and root pages. */
	if ((ret = memp_fput(dbp->mpf, (PAGE *)meta, DB_MPOOL_DIRTY)) != 0)
		return (ret);
	if ((ret = memp_fput(dbp->mpf, root, DB_MPOOL_DIRTY)) != 0)
		return (ret);

	/*
	 * Flush the metadata and root pages to disk -- since the user can't
	 * transaction protect open, the pages have to exist during recovery.
	 *
	 * XXX
	 * It's not useful to return not-yet-flushed here -- convert it to
	 * an error.
	 */
	if ((ret = memp_fsync(dbp->mpf)) == DB_INCOMPLETE)
		ret = EINVAL;

	/* Release the locks. */
	(void)__BT_LPUT(dbp, metalock);
	(void)__BT_LPUT(dbp, rootlock);

	return (ret);
}
