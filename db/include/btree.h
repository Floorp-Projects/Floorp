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
 *
 *	@(#)btree.h	10.21 (Sleepycat) 5/23/98
 */

/* Forward structure declarations. */
struct __btree;		typedef struct __btree BTREE;
struct __cursor;	typedef struct __cursor CURSOR;
struct __epg;		typedef struct __epg EPG;
struct __rcursor;	typedef struct __rcursor RCURSOR;
struct __recno;		typedef struct __recno RECNO;

#undef	DEFMINKEYPAGE			/* Minimum keys per page */
#define	DEFMINKEYPAGE	 (2)

#undef	ISINTERNAL			/* If an internal page. */
#define	ISINTERNAL(p)	(TYPE(p) == P_IBTREE || TYPE(p) ==  P_IRECNO)
#undef	ISLEAF				/* If a leaf page. */
#define	ISLEAF(p)	(TYPE(p) == P_LBTREE || TYPE(p) ==  P_LRECNO)

/* Allocate and discard thread structures. */
#define	GETHANDLE(dbp, set_txn, dbpp, ret) {				\
	if (F_ISSET(dbp, DB_AM_THREAD)) {				\
		if ((ret = __db_gethandle(dbp, __bam_bdup, dbpp)) != 0)	\
			return (ret);					\
	} else								\
		*dbpp = dbp;						\
	*dbpp->txn = set_txn;						\
}
#define	PUTHANDLE(dbp) {						\
	dbp->txn = NULL;						\
	if (F_ISSET(dbp, DB_AM_THREAD))					\
		__db_puthandle(dbp);					\
}

/*
 * If doing transactions we have to hold the locks associated with a data item
 * from a page for the entire transaction.  However, we don't have to hold the
 * locks associated with walking the tree.  Distinguish between the two so that
 * we don't tie up the internal pages of the tree longer than necessary.
 */
#define	__BT_LPUT(dbp, lock)						\
	(F_ISSET((dbp), DB_AM_LOCKING) ?				\
	    lock_put((dbp)->dbenv->lk_info, lock) : 0)
#define	__BT_TLPUT(dbp, lock)						\
	(F_ISSET((dbp), DB_AM_LOCKING) && (dbp)->txn == NULL ?		\
	    lock_put((dbp)->dbenv->lk_info, lock) : 0)

/*
 * Flags to __bt_search() and __rec_search().
 *
 * Note, internal page searches must find the largest record less than key in
 * the tree so that descents work.  Leaf page searches must find the smallest
 * record greater than key so that the returned index is the record's correct
 * position for insertion.
 *
 * The flags parameter to the search routines describes three aspects of the
 * search: the type of locking required (including if we're locking a pair of
 * pages), the item to return in the presence of duplicates and whether or not
 * to return deleted entries.  To simplify both the mnemonic representation
 * and the code that checks for various cases, we construct a set of bitmasks.
 */
#define	S_READ		0x00001		/* Read locks. */
#define	S_WRITE		0x00002		/* Write locks. */

#define	S_APPEND	0x00040		/* Append to the tree. */
#define	S_DELNO		0x00080		/* Don't return deleted items. */
#define	S_DUPFIRST	0x00100		/* Return first duplicate. */
#define	S_DUPLAST	0x00200		/* Return last duplicate. */
#define	S_EXACT		0x00400		/* Exact items only. */
#define	S_PARENT	0x00800		/* Lock page pair. */
#define	S_STACK		0x01000		/* Need a complete stack. */

#define	S_DELETE	(S_WRITE | S_DUPFIRST | S_DELNO | S_EXACT | S_STACK)
#define	S_FIND		(S_READ | S_DUPFIRST | S_DELNO)
#define	S_INSERT	(S_WRITE | S_DUPLAST | S_STACK)
#define	S_KEYFIRST	(S_WRITE | S_DUPFIRST | S_STACK)
#define	S_KEYLAST	(S_WRITE | S_DUPLAST | S_STACK)
#define	S_WRPAIR	(S_WRITE | S_DUPLAST | S_PARENT)

/*
 * If doing insert search (including keyfirst or keylast operations) or a
 * split search on behalf of an insert, it's okay to return the entry one
 * past the end of the page.
 */
#define	PAST_END_OK(f)							\
	((f) == S_INSERT ||						\
	(f) == S_KEYFIRST || (f) == S_KEYLAST || (f) == S_WRPAIR)

/*
 * Flags to __bam_iitem().
 */
#define	BI_DELETED	0x01		/* Key/data pair only placeholder. */
#define	BI_DOINCR	0x02		/* Increment the record count. */
#define	BI_NEWKEY	0x04		/* New key. */

/*
 * Various routines pass around page references.  A page reference can be a
 * pointer to the page or a page number; for either, an indx can designate
 * an item on the page.
 */
struct __epg {
	PAGE	 *page;			/* The page. */
	db_indx_t indx;			/* The index on the page. */
	DB_LOCK	  lock;			/* The page's lock. */
};

/*
 * All cursors are queued from the master DB structure.  Convert the user's
 * DB reference to the master DB reference.  We lock the master DB mutex
 * so that we can walk the cursor queue.  There's no race in accessing the
 * cursors, because if we're modifying a page, we have a write lock on it,
 * and therefore no other thread than the current one can have a cursor that
 * references the page.
 */
#define	CURSOR_SETUP(dbp) {						\
	(dbp) = (dbp)->master;						\
	DB_THREAD_LOCK(dbp);						\
}
#define	CURSOR_TEARDOWN(dbp)						\
	DB_THREAD_UNLOCK(dbp);

/*
 * Btree cursor.
 *
 * Arguments passed to __bam_ca_replace().
 */
typedef enum {
	REPLACE_SETUP,
	REPLACE_SUCCESS,
	REPLACE_FAILED
} ca_replace_arg;
struct __cursor {
	DBC		*dbc;		/* Enclosing DBC. */

	PAGE		*page;		/* Cursor page. */

	db_pgno_t	 pgno;		/* Page. */
	db_indx_t	 indx;		/* Page item ref'd by the cursor. */

	db_pgno_t	 dpgno;		/* Duplicate page. */
	db_indx_t	 dindx;		/* Page item ref'd by the cursor. */

	DB_LOCK		 lock;		/* Cursor read lock. */
	db_lockmode_t	 mode;		/* Lock mode. */

	/*
	 * If a cursor record is deleted, the key/data pair has to remain on
	 * the page so that subsequent inserts/deletes don't interrupt the
	 * cursor progression through the file.  This results in interesting
	 * cases when "standard" operations, e.g., dbp->put() are done in the
	 * context of "deleted" cursors.
	 *
	 * C_DELETED -- The item referenced by the cursor has been "deleted"
	 *		but not physically removed from the page.
	 * C_REPLACE -- The "deleted" item referenced by a cursor has been
	 *		replaced by a dbp->put(), so the cursor is no longer
	 *		responsible for physical removal from the page.
	 * C_REPLACE_SETUP --
	 *		We are about to overwrite a "deleted" item, flag any
	 *		cursors referencing it for transition to C_REPLACE
	 *		state.
	 */
#define	C_DELETED	0x0001
#define	C_REPLACE	0x0002
#define	C_REPLACE_SETUP	0x0004

	/*
	 * Internal cursor held for DB->get; don't hold locks unless involved
	 * in a TXN.
	 */
#define	C_INTERNAL	0x0008
	u_int32_t	 flags;
};

/*
 * Recno cursor.
 *
 * Arguments passed to __ram_ca().
 */
typedef enum {
	CA_DELETE,
	CA_IAFTER,
	CA_IBEFORE
} ca_recno_arg;
struct __rcursor {
	DBC		*dbc;		/* Enclosing DBC. */

	db_recno_t	 recno;		/* Current record number. */

	/*
	 * Cursors referencing "deleted" records are positioned between
	 * two records, and so must be specially adjusted until they are
	 * moved.
	 */
#define	CR_DELETED	0x0001		/* Record deleted. */
	u_int32_t	 flags;
};

/*
 * We maintain a stack of the pages that we're locking in the tree.  Btree's
 * (currently) only save two levels of the tree at a time, so the default
 * stack is always large enough.  Recno trees have to lock the entire tree to
 * do inserts/deletes, however.  Grow the stack as necessary.
 */
#undef	BT_STK_CLR
#define	BT_STK_CLR(t)							\
	((t)->bt_csp = (t)->bt_sp)

#undef	BT_STK_ENTER
#define	BT_STK_ENTER(t, pagep, page_indx, lock, ret) do {		\
	if ((ret =							\
	    (t)->bt_csp == (t)->bt_esp ? __bam_stkgrow(t) : 0) == 0) {	\
		(t)->bt_csp->page = pagep;				\
		(t)->bt_csp->indx = page_indx;				\
		(t)->bt_csp->lock = lock;				\
	}								\
} while (0)

#undef	BT_STK_PUSH
#define	BT_STK_PUSH(t, pagep, page_indx, lock, ret) do {		\
	BT_STK_ENTER(t, pagep, page_indx, lock, ret);			\
	++(t)->bt_csp;							\
} while (0)

#undef	BT_STK_POP
#define	BT_STK_POP(t)							\
	((t)->bt_csp == (t)->bt_stack ? NULL : --(t)->bt_csp)

/*
 * The in-memory recno data structure.
 *
 * !!!
 * These fields are ignored as far as multi-threading is concerned.  There
 * are no transaction semantics associated with backing files, nor is there
 * any thread protection.
 */
#undef	RECNO_OOB
#define	RECNO_OOB	0		/* Illegal record number. */

struct __recno {
	int		 re_delim;	/* Variable-length delimiting byte. */
	int		 re_pad;	/* Fixed-length padding byte. */
	u_int32_t	 re_len;	/* Length for fixed-length records. */

	char		*re_source;	/* Source file name. */
	int		 re_fd;		/* Source file descriptor */
	db_recno_t	 re_last;	/* Last record number read. */
	void		*re_cmap;	/* Current point in mapped space. */
	void		*re_smap;	/* Start of mapped space. */
	void		*re_emap;	/* End of mapped space. */
	size_t		 re_msize;	/* Size of mapped region. */
					/* Recno input function. */
	int (*re_irec) __P((DB *, db_recno_t));

#define	RECNO_EOF	0x0001		/* EOF on backing source file. */
#define	RECNO_MODIFIED	0x0002		/* Tree was modified. */
	u_int32_t	 flags;
};

/*
 * The in-memory btree data structure.
 */
struct __btree {
/*
 * These fields are per-thread and are initialized when the BTREE structure
 * is created.
 */
	db_pgno_t	 bt_lpgno;	/* Last insert location. */

	DBT		 bt_rkey;	/* Returned key. */
	DBT		 bt_rdata;	/* Returned data. */

	EPG		*bt_sp;		/* Stack pointer. */
	EPG	 	*bt_csp;	/* Current stack entry. */
	EPG		*bt_esp;	/* End stack pointer. */
	EPG		 bt_stack[5];

	RECNO		*bt_recno;	/* Private recno structure. */

	DB_BTREE_LSTAT lstat;		/* Btree local statistics. */

/*
 * These fields are copied from the original BTREE structure and never
 * change.
 */
	db_indx_t 	 bt_maxkey;	/* Maximum keys per page. */
	db_indx_t 	 bt_minkey;	/* Minimum keys per page. */

	int (*bt_compare)		/* Comparison function. */
	    __P((const DBT *, const DBT *));
	size_t(*bt_prefix)		/* Prefix function. */
	    __P((const DBT *, const DBT *));

	db_indx_t	 bt_ovflsize;	/* Maximum key/data on-page size. */
};

#include "btree_auto.h"
#include "btree_ext.h"
#include "db_am.h"
#include "common_ext.h"
