/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */
/*
 * Copyright (c) 1990, 1993, 1994
 *	Margo Seltzer.  All rights reserved.
 */
/*
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
 *
 *	@(#)hash.h	10.8 (Sleepycat) 4/10/98
 */

/* Cursor structure definitions. */
typedef struct cursor_t {
	DBC		*db_cursor;
	db_pgno_t	bucket;		/* Bucket we are traversing. */
	DB_LOCK		lock;		/* Lock held on the current bucket. */
	PAGE		*pagep;		/* The current page. */
	db_pgno_t	pgno;		/* Current page number. */
	db_indx_t	bndx;		/* Index within the current page. */
	PAGE		*dpagep;	/* Duplicate page pointer. */
	db_pgno_t	dpgno;		/* Duplicate page number. */
	db_indx_t	dndx;		/* Index within a duplicate set. */
	db_indx_t	dup_off;	/* Offset within a duplicate set. */
	db_indx_t	dup_len;	/* Length of current duplicate. */
	db_indx_t	dup_tlen;	/* Total length of duplicate entry. */
	u_int32_t	seek_size;	/* Number of bytes we need for add. */
	db_pgno_t	seek_found_page;/* Page on which we can insert. */
	u_int32_t	big_keylen;	/* Length of big_key buffer. */
	void		*big_key;	/* Temporary buffer for big keys. */
	u_int32_t	big_datalen;	/* Length of big_data buffer. */
	void		*big_data;	/* Temporary buffer for big data. */
#define	H_OK		0x0001
#define	H_NOMORE	0x0002
#define	H_DELETED	0x0004
#define	H_ISDUP		0x0008
#define	H_EXPAND	0x0020
	u_int32_t	flags;		/* Is cursor inside a dup set. */
} HASH_CURSOR;

#define	IS_VALID(C) ((C)->bucket != BUCKET_INVALID)


typedef struct htab {		/* Memory resident data structure. */
	DB *dbp;		/* Pointer to parent db structure. */
	DB_LOCK hlock;		/* Metadata page lock. */
	HASHHDR *hdr;		/* Pointer to meta-data page. */
	u_int32_t (*hash) __P((const void *, u_int32_t)); /* Hash Function */
	PAGE *split_buf;	/* Temporary buffer for splits. */
	int local_errno;	/* Error Number -- for DBM compatability */
	u_long hash_accesses;	/* Number of accesses to this table. */
	u_long hash_collisions;	/* Number of collisions on search. */
	u_long hash_expansions;	/* Number of times we added a bucket. */
	u_long hash_overflows;	/* Number of overflow pages. */
	u_long hash_bigpages;	/* Number of big key/data pages. */
} HTAB;

/*
 * Macro used for interface functions to set the txnid in the DBP.
 */
#define	SET_LOCKER(D, T) ((D)->txn = (T))

/*
 * More interface macros used to get/release the meta data page.
 */
#define	GET_META(D, H) {						\
	int _r;								\
	if (F_ISSET(D, DB_AM_LOCKING) && !F_ISSET(D, DB_AM_RECOVER)) {	\
		(D)->lock.pgno = BUCKET_INVALID;			\
	    	if ((_r = lock_get((D)->dbenv->lk_info,			\
	    	    (D)->txn == NULL ? (D)->locker : (D)->txn->txnid,	\
		    0, &(D)->lock_dbt, DB_LOCK_READ,			\
		    &(H)->hlock)) != 0)					\
			return (_r < 0 ? EAGAIN : _r);			\
	}								\
	if ((_r = __ham_get_page(D, 0, (PAGE **)&((H)->hdr))) != 0) {	\
		if ((H)->hlock) {					\
			(void)lock_put((D)->dbenv->lk_info, (H)->hlock);\
			(H)->hlock = 0;					\
		}							\
		return (_r);						\
	}								\
}

#define	RELEASE_META(D, H) {						\
	if (!F_ISSET(D, DB_AM_RECOVER) &&				\
	    (D)->txn == NULL && (H)->hlock)				\
		(void)lock_put((H)->dbp->dbenv->lk_info, (H)->hlock);	\
	(H)->hlock = 0;							\
	if ((H)->hdr)							\
		(void)__ham_put_page(D, (PAGE *)(H)->hdr,		\
		    F_ISSET(D, DB_HS_DIRTYMETA) ? 1 : 0);		\
	(H)->hdr = NULL;						\
	F_CLR(D, DB_HS_DIRTYMETA);					\
}

#define	DIRTY_META(H, R) {						\
	if (F_ISSET((H)->dbp, DB_AM_LOCKING) &&				\
	    !F_ISSET((H)->dbp, DB_AM_RECOVER)) {			\
		DB_LOCK _tmp;						\
		(H)->dbp->lock.pgno = BUCKET_INVALID;			\
	    	if (((R) = lock_get((H)->dbp->dbenv->lk_info,		\
	    	    (H)->dbp->txn ? (H)->dbp->txn->txnid :		\
	    	    (H)->dbp->locker, 0, &(H)->dbp->lock_dbt,		\
	    	    DB_LOCK_WRITE, &_tmp)) == 0)			\
			(R) = lock_put((H)->dbp->dbenv->lk_info,	\
			    (H)->hlock);				\
		else if ((R) < 0)					\
			(R) = EAGAIN;					\
		(H)->hlock = _tmp;					\
	}								\
	F_SET((H)->dbp, DB_HS_DIRTYMETA);				\
}

/* Allocate and discard thread structures. */
#define	H_GETHANDLE(dbp, dbpp, ret)					\
	if (F_ISSET(dbp, DB_AM_THREAD))					\
		ret = __db_gethandle(dbp, __ham_hdup, dbpp);		\
	else {								\
		ret = 0;						\
		*dbpp = dbp;						\
	}

#define	H_PUTHANDLE(dbp) {						\
	if (F_ISSET(dbp, DB_AM_THREAD))					\
		__db_puthandle(dbp);					\
}

/* Test string. */
#define	CHARKEY			"%$sniglet^&"

/* Overflow management */
/*
 * Overflow page numbers are allocated per split point.  At each doubling of
 * the table, we can allocate extra pages.  We keep track of how many pages
 * we've allocated at each point to calculate bucket to page number mapping.
 */
#define	BUCKET_TO_PAGE(H, B) \
	((B) + 1 + ((B) ? (H)->hdr->spares[__db_log2((B)+1)-1] : 0))

#define	PGNO_OF(H, S, O) (BUCKET_TO_PAGE((H), (1 << (S)) - 1) + (O))

/* Constraints about number of pages and how much data goes on a page. */

#define	MAX_PAGES(H)	UINT32_T_MAX
#define	MINFILL		4
#define	ISBIG(H, N)	(((N) > ((H)->hdr->pagesize / MINFILL)) ? 1 : 0)

/* Shorthands for accessing structure */
#define	NDX_INVALID	0xFFFF
#define	BUCKET_INVALID	0xFFFFFFFF

/* On page duplicates are stored as a string of size-data-size triples. */
#define	DUP_SIZE(len)	((len) + 2 * sizeof(db_indx_t))

/* Log messages types (these are subtypes within a record type) */
#define	PAIR_KEYMASK		0x1
#define	PAIR_DATAMASK		0x2
#define	PAIR_ISKEYBIG(N)	(N & PAIR_KEYMASK)
#define	PAIR_ISDATABIG(N)	(N & PAIR_DATAMASK)
#define	OPCODE_OF(N)    	(N & ~(PAIR_KEYMASK | PAIR_DATAMASK))

#define	PUTPAIR		0x20
#define	DELPAIR		0x30
#define	PUTOVFL		0x40
#define	DELOVFL		0x50
#define	ALLOCPGNO	0x60
#define	DELPGNO		0x70
#define	SPLITOLD	0x80
#define	SPLITNEW	0x90

#include "hash_auto.h"
#include "hash_ext.h"
#include "db_am.h"
#include "common_ext.h"
