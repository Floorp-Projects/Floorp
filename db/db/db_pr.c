/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)db_pr.c	10.29 (Sleepycat) 5/23/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "btree.h"
#include "hash.h"
#include "db_am.h"

static void __db_proff __P((void *));
static void __db_psize __P((DB_MPOOLFILE *));

/*
 * __db_loadme --
 *	Force loading of this file.
 *
 * PUBLIC: void __db_loadme __P((void));
 */
void
__db_loadme()
{
	getpid();
}

static FILE *set_fp;

/*
 * 64K is the maximum page size, so by default we check for offsets
 * larger than that, and, where possible, we refine the test.
 */
#define	PSIZE_BOUNDARY	(64 * 1024 + 1)
static size_t set_psize = PSIZE_BOUNDARY;

/*
 * __db_prinit --
 *	Initialize tree printing routines.
 *
 * PUBLIC: FILE *__db_prinit __P((FILE *));
 */
FILE *
__db_prinit(fp)
	FILE *fp;
{
	if (set_fp == NULL)
		set_fp = fp == NULL ? stdout : fp;
	return (set_fp);
}

/*
 * __db_dump --
 *	Dump the tree to a file.
 *
 * PUBLIC: int __db_dump __P((DB *, char *, int));
 */
int
__db_dump(dbp, name, all)
	DB *dbp;
	char *name;
	int all;
{
	FILE *fp, *save_fp;

	COMPQUIET(save_fp, NULL);

	if (set_psize == PSIZE_BOUNDARY)
		__db_psize(dbp->mpf);

	if (name != NULL) {
		if ((fp = fopen(name, "w")) == NULL)
			return (errno);
		save_fp = set_fp;
		set_fp = fp;
	} else
		fp = __db_prinit(NULL);

	(void)__db_prdb(dbp);
	if (dbp->type == DB_HASH)
		(void)__db_prhash(dbp);
	else
		(void)__db_prbtree(dbp);
	fprintf(fp, "%s\n", DB_LINE);
	__db_prtree(dbp->mpf, all);

	if (name != NULL) {
		(void)fclose(fp);
		set_fp = save_fp;
	}
	return (0);
}

/*
 * __db_prdb --
 *	Print out the DB structure information.
 *
 * PUBLIC: int __db_prdb __P((DB *));
 */
int
__db_prdb(dbp)
	DB *dbp;
{
	static const FN fn[] = {
		{ DB_AM_DUP,		"duplicates" },
		{ DB_AM_INMEM,		"in-memory" },
		{ DB_AM_LOCKING,	"locking" },
		{ DB_AM_LOGGING,	"logging" },
		{ DB_AM_MLOCAL,		"local mpool" },
		{ DB_AM_PGDEF,		"default page size" },
		{ DB_AM_RDONLY,		"read-only" },
		{ DB_AM_RECOVER,	"recover" },
		{ DB_AM_SWAP,		"needswap" },
		{ DB_AM_THREAD,		"thread" },
		{ DB_BT_RECNUM,		"btree:records" },
		{ DB_HS_DIRTYMETA,	"hash:dirty-meta" },
		{ DB_RE_DELIMITER,	"recno:delimiter" },
		{ DB_RE_FIXEDLEN,	"recno:fixed-length" },
		{ DB_RE_PAD,		"recno:pad" },
		{ DB_RE_RENUMBER,	"recno:renumber" },
		{ DB_RE_SNAPSHOT,	"recno:snapshot" },
		{ 0 },
	};
	FILE *fp;
	const char *t;

	fp = __db_prinit(NULL);

	switch (dbp->type) {
	case DB_BTREE:
		t = "btree";
		break;
	case DB_HASH:
		t = "hash";
		break;
	case DB_RECNO:
		t = "recno";
		break;
	default:
		t = "UNKNOWN";
		break;
	}

	fprintf(fp, "%s ", t);
	__db_prflags(dbp->flags, fn, fp);
	fprintf(fp, "\n");

	return (0);
}

/*
 * __db_prbtree --
 *	Print out the btree internal information.
 *
 * PUBLIC: int __db_prbtree __P((DB *));
 */
int
__db_prbtree(dbp)
	DB *dbp;
{
	static const FN mfn[] = {
		{ BTM_DUP,	"duplicates" },
		{ BTM_RECNO,	"recno" },
		{ BTM_RECNUM,	"btree:records" },
		{ BTM_FIXEDLEN,	"recno:fixed-length" },
		{ BTM_RENUMBER,	"recno:renumber" },
		{ 0 },
	};
	BTMETA *mp;
	BTREE *t;
	EPG *epg;
	FILE *fp;
	PAGE *h;
	RECNO *rp;
	db_pgno_t i;
	int ret;

	t = dbp->internal;
	fp = __db_prinit(NULL);

	(void)fprintf(fp, "%s\nOn-page metadata:\n", DB_LINE);

	i = PGNO_METADATA;
	if ((ret = __bam_pget(dbp, (PAGE **)&mp, &i, 0)) != 0)
		return (ret);

	(void)fprintf(fp, "magic %#lx\n", (u_long)mp->magic);
	(void)fprintf(fp, "version %#lx\n", (u_long)mp->version);
	(void)fprintf(fp, "pagesize %lu\n", (u_long)mp->pagesize);
	(void)fprintf(fp, "maxkey: %lu minkey: %lu\n",
	    (u_long)mp->maxkey, (u_long)mp->minkey);

	(void)fprintf(fp, "free %lu", (u_long)mp->free);
	for (i = mp->free; i != PGNO_INVALID;) {
		if ((ret = __bam_pget(dbp, &h, &i, 0)) != 0)
			return (ret);
		i = h->next_pgno;
		(void)memp_fput(dbp->mpf, h, 0);
		(void)fprintf(fp, ", %lu", (u_long)i);
	}
	(void)fprintf(fp, "\n");

	(void)fprintf(fp, "flags %#lx", (u_long)mp->flags);
	__db_prflags(mp->flags, mfn, fp);
	(void)fprintf(fp, "\n");
	(void)memp_fput(dbp->mpf, mp, 0);

	(void)fprintf(fp, "%s\nDB_INFO:\n", DB_LINE);
	(void)fprintf(fp, "bt_maxkey: %lu bt_minkey: %lu\n",
	    (u_long)t->bt_maxkey, (u_long)t->bt_minkey);
	(void)fprintf(fp, "bt_compare: %#lx bt_prefix: %#lx\n",
	    (u_long)t->bt_compare, (u_long)t->bt_prefix);
	if ((rp = t->bt_recno) != NULL) {
		(void)fprintf(fp,
		    "re_delim: %#lx re_pad: %#lx re_len: %lu re_source: %s\n",
		    (u_long)rp->re_delim, (u_long)rp->re_pad,
		    (u_long)rp->re_len,
		    rp->re_source == NULL ? "" : rp->re_source);
		(void)fprintf(fp,
		    "cmap: %#lx smap: %#lx emap: %#lx msize: %lu\n",
		    (u_long)rp->re_cmap, (u_long)rp->re_smap,
		    (u_long)rp->re_emap, (u_long)rp->re_msize);
	}
	(void)fprintf(fp, "stack:");
	for (epg = t->bt_stack; epg < t->bt_sp; ++epg)
		(void)fprintf(fp, " %lu", (u_long)epg->page->pgno);
	(void)fprintf(fp, "\n");
	(void)fprintf(fp, "ovflsize: %lu\n", (u_long)t->bt_ovflsize);
	(void)fflush(fp);
	return (0);
}

/*
 * __db_prhash --
 *	Print out the hash internal information.
 *
 * PUBLIC: int __db_prhash __P((DB *));
 */
int
__db_prhash(dbp)
	DB *dbp;
{
	FILE *fp;
	HTAB *t;
	int i, put_page, ret;
	db_pgno_t pgno;

	t = dbp->internal;

	fp = __db_prinit(NULL);

	fprintf(fp, "\thash_accesses    %lu\n", (u_long)t->hash_accesses);
	fprintf(fp, "\thash_collisions  %lu\n", (u_long)t->hash_collisions);
	fprintf(fp, "\thash_expansions  %lu\n", (u_long)t->hash_expansions);
	fprintf(fp, "\thash_overflows 	%lu\n", (u_long)t->hash_overflows);
	fprintf(fp, "\thash_bigpages    %lu\n", (u_long)t->hash_bigpages);
	fprintf(fp, "\n");

	if (t->hdr == NULL) {
		pgno = PGNO_METADATA;
		if ((ret = memp_fget(dbp->mpf, &pgno, 0, &t->hdr)) != 0)
			return (ret);
		put_page = 1;
	} else
		put_page = 0;

	fprintf(fp, "\tmagic      %#lx\n", (u_long)t->hdr->magic);
	fprintf(fp, "\tversion    %lu\n", (u_long)t->hdr->version);
	fprintf(fp, "\tpagesize   %lu\n", (u_long)t->hdr->pagesize);
	fprintf(fp, "\tovfl_point %lu\n", (u_long)t->hdr->ovfl_point);
	fprintf(fp, "\tlast_freed %lu\n", (u_long)t->hdr->last_freed);
	fprintf(fp, "\tmax_bucket %lu\n", (u_long)t->hdr->max_bucket);
	fprintf(fp, "\thigh_mask  %#lx\n", (u_long)t->hdr->high_mask);
	fprintf(fp, "\tlow_mask   %#lx\n", (u_long)t->hdr->low_mask);
	fprintf(fp, "\tffactor    %lu\n", (u_long)t->hdr->ffactor);
	fprintf(fp, "\tnelem      %lu\n", (u_long)t->hdr->nelem);
	fprintf(fp, "\th_charkey  %#lx\n", (u_long)t->hdr->h_charkey);

	for (i = 0; i < NCACHED; i++)
		fprintf(fp, "%lu ", (u_long)t->hdr->spares[i]);
	fprintf(fp, "\n");

	(void)fflush(fp);
	if (put_page) {
		(void)memp_fput(dbp->mpf, (PAGE *)t->hdr, 0);
		t->hdr = NULL;
	}
	return (0);
}

/*
 * __db_prtree --
 *	Print out the entire tree.
 *
 * PUBLIC: int __db_prtree __P((DB_MPOOLFILE *, int));
 */
int
__db_prtree(mpf, all)
	DB_MPOOLFILE *mpf;
	int all;
{
	PAGE *h;
	db_pgno_t i;
	int ret, t_ret;

	if (set_psize == PSIZE_BOUNDARY)
		__db_psize(mpf);

	ret = 0;
	for (i = PGNO_ROOT;; ++i) {
		if ((ret = memp_fget(mpf, &i, 0, &h)) != 0)
			break;
		if (TYPE(h) != P_INVALID)
			if ((t_ret = __db_prpage(h, all)) != 0 && ret == 0)
				ret = t_ret;
		(void)memp_fput(mpf, h, 0);
	}
	(void)fflush(__db_prinit(NULL));
	return (ret);
}

/*
 * __db_prnpage
 *	-- Print out a specific page.
 *
 * PUBLIC: int __db_prnpage __P((DB_MPOOLFILE *, db_pgno_t));
 */
int
__db_prnpage(mpf, pgno)
	DB_MPOOLFILE *mpf;
	db_pgno_t pgno;
{
	PAGE *h;
	int ret;

	if (set_psize == PSIZE_BOUNDARY)
		__db_psize(mpf);

	if ((ret = memp_fget(mpf, &pgno, 0, &h)) != 0)
		return (ret);

	ret = __db_prpage(h, 1);
	(void)fflush(__db_prinit(NULL));

	(void)memp_fput(mpf, h, 0);
	return (ret);
}

/*
 * __db_prpage
 *	-- Print out a page.
 *
 * PUBLIC: int __db_prpage __P((PAGE *, int));
 */
int
__db_prpage(h, all)
	PAGE *h;
	int all;
{
	BINTERNAL *bi;
	BKEYDATA *bk;
	HOFFPAGE a_hkd;
	FILE *fp;
	RINTERNAL *ri;
	db_indx_t dlen, len, i;
	db_pgno_t pgno;
	int deleted, ret;
	const char *s;
	u_int8_t *ep, *hk, *p;
	void *sp;

	fp = __db_prinit(NULL);

	switch (TYPE(h)) {
	case P_DUPLICATE:
		s = "duplicate";
		break;
	case P_HASH:
		s = "hash";
		break;
	case P_IBTREE:
		s = "btree internal";
		break;
	case P_INVALID:
		s = "invalid";
		break;
	case P_IRECNO:
		s = "recno internal";
		break;
	case P_LBTREE:
		s = "btree leaf";
		break;
	case P_LRECNO:
		s = "recno leaf";
		break;
	case P_OVERFLOW:
		s = "overflow";
		break;
	default:
		fprintf(fp, "ILLEGAL PAGE TYPE: page: %lu type: %lu\n",
		    (u_long)h->pgno, (u_long)TYPE(h));
			return (1);
	}
	fprintf(fp, "page %4lu: (%s)\n", (u_long)h->pgno, s);
	fprintf(fp, "    lsn.file: %lu lsn.offset: %lu",
	    (u_long)LSN(h).file, (u_long)LSN(h).offset);
	if (TYPE(h) == P_IBTREE || TYPE(h) == P_IRECNO ||
	    (TYPE(h) == P_LRECNO && h->pgno == PGNO_ROOT))
		fprintf(fp, " total records: %4lu", (u_long)RE_NREC(h));
	fprintf(fp, "\n");
	if (TYPE(h) == P_LBTREE || TYPE(h) == P_LRECNO ||
	    TYPE(h) == P_DUPLICATE || TYPE(h) == P_OVERFLOW)
		fprintf(fp, "    prev: %4lu next: %4lu",
		    (u_long)PREV_PGNO(h), (u_long)NEXT_PGNO(h));
	if (TYPE(h) == P_IBTREE || TYPE(h) == P_LBTREE)
		fprintf(fp, " level: %2lu", (u_long)h->level);
	if (TYPE(h) == P_OVERFLOW) {
		fprintf(fp, " ref cnt: %4lu ", (u_long)OV_REF(h));
		__db_pr((u_int8_t *)h + P_OVERHEAD, OV_LEN(h));
		return (0);
	}
	fprintf(fp, " entries: %4lu", (u_long)NUM_ENT(h));
	fprintf(fp, " offset: %4lu\n", (u_long)HOFFSET(h));

	if (!all || TYPE(h) == P_INVALID)
		return (0);

	ret = 0;
	for (i = 0; i < NUM_ENT(h); i++) {
		if (P_ENTRY(h, i) - (u_int8_t *)h < P_OVERHEAD ||
		    (size_t)(P_ENTRY(h, i) - (u_int8_t *)h) >= set_psize) {
			fprintf(fp,
			    "ILLEGAL PAGE OFFSET: indx: %lu of %lu\n",
			    (u_long)i, (u_long)h->inp[i]);
			ret = EINVAL;
			continue;
		}
		deleted = 0;
		switch (TYPE(h)) {
		case P_HASH:
		case P_IBTREE:
		case P_IRECNO:
			sp = P_ENTRY(h, i);
			break;
		case P_LBTREE:
			sp = P_ENTRY(h, i);
			deleted = i % 2 == 0 &&
			    B_DISSET(GET_BKEYDATA(h, i + O_INDX)->type);
			break;
		case P_LRECNO:
		case P_DUPLICATE:
			sp = P_ENTRY(h, i);
			deleted = B_DISSET(GET_BKEYDATA(h, i)->type);
			break;
		default:
			fprintf(fp,
			    "ILLEGAL PAGE ITEM: %lu\n", (u_long)TYPE(h));
			ret = EINVAL;
			continue;
		}
		fprintf(fp, "   %s[%03lu] %4lu ",
		    deleted ? "D" : " ", (u_long)i, (u_long)h->inp[i]);
		switch (TYPE(h)) {
		case P_HASH:
			hk = sp;
			switch (HPAGE_PTYPE(hk)) {
			case H_OFFDUP:
				memcpy(&pgno,
				    HOFFDUP_PGNO(hk), sizeof(db_pgno_t));
				fprintf(fp,
				    "%4lu [offpage dups]\n", (u_long)pgno);
				break;
			case H_DUPLICATE:
				/*
				 * If this is the first item on a page, then
				 * we cannot figure out how long it is, so
				 * we only print the first one in the duplicate
				 * set.
				 */
				if (i != 0)
					len = LEN_HKEYDATA(h, 0, i);
				else
					len = 1;

				fprintf(fp, "Duplicates:\n");
				for (p = HKEYDATA_DATA(hk),
				    ep = p + len; p < ep;) {
					memcpy(&dlen, p, sizeof(db_indx_t));
					p += sizeof(db_indx_t);
					fprintf(fp, "\t\t");
					__db_pr(p, dlen);
					p += sizeof(db_indx_t) + dlen;
				}
				break;
			case H_KEYDATA:
				if (i != 0)
					__db_pr(HKEYDATA_DATA(hk),
					    LEN_HKEYDATA(h, 0, i));
				else
					fprintf(fp, "%s\n", HKEYDATA_DATA(hk));
				break;
			case H_OFFPAGE:
				memcpy(&a_hkd, hk, HOFFPAGE_SIZE);
				fprintf(fp,
				    "overflow: total len: %4lu page: %4lu\n",
				    (u_long)a_hkd.tlen, (u_long)a_hkd.pgno);
				break;
			}
			break;
		case P_IBTREE:
			bi = sp;
			fprintf(fp, "count: %4lu pgno: %4lu ",
			    (u_long)bi->nrecs, (u_long)bi->pgno);
			switch (B_TYPE(bi->type)) {
			case B_KEYDATA:
				__db_pr(bi->data, bi->len);
				break;
			case B_DUPLICATE:
			case B_OVERFLOW:
				__db_proff(bi->data);
				break;
			default:
				fprintf(fp, "ILLEGAL BINTERNAL TYPE: %lu\n",
				    (u_long)B_TYPE(bi->type));
				ret = EINVAL;
				break;
			}
			break;
		case P_IRECNO:
			ri = sp;
			fprintf(fp, "entries %4lu pgno %4lu\n",
			    (u_long)ri->nrecs, (u_long)ri->pgno);
			break;
		case P_LBTREE:
		case P_LRECNO:
		case P_DUPLICATE:
			bk = sp;
			switch (B_TYPE(bk->type)) {
			case B_KEYDATA:
				__db_pr(bk->data, bk->len);
				break;
			case B_DUPLICATE:
			case B_OVERFLOW:
				__db_proff(bk);
				break;
			default:
				fprintf(fp,
			    "ILLEGAL DUPLICATE/LBTREE/LRECNO TYPE: %lu\n",
				    (u_long)B_TYPE(bk->type));
				ret = EINVAL;
				break;
			}
			break;
		}
	}
	(void)fflush(fp);
	return (ret);
}

/*
 * __db_isbad
 *	-- Decide if a page is corrupted.
 *
 * PUBLIC: int __db_isbad __P((PAGE *, int));
 */
int
__db_isbad(h, die)
	PAGE *h;
	int die;
{
	BINTERNAL *bi;
	BKEYDATA *bk;
	FILE *fp;
	db_indx_t i;
	u_int type;

	fp = __db_prinit(NULL);

	switch (TYPE(h)) {
	case P_DUPLICATE:
	case P_HASH:
	case P_IBTREE:
	case P_INVALID:
	case P_IRECNO:
	case P_LBTREE:
	case P_LRECNO:
	case P_OVERFLOW:
		break;
	default:
		fprintf(fp, "ILLEGAL PAGE TYPE: page: %lu type: %lu\n",
		    (u_long)h->pgno, (u_long)TYPE(h));
		goto bad;
	}

	for (i = 0; i < NUM_ENT(h); i++) {
		if (P_ENTRY(h, i) - (u_int8_t *)h < P_OVERHEAD ||
		    (size_t)(P_ENTRY(h, i) - (u_int8_t *)h) >= set_psize) {
			fprintf(fp,
			    "ILLEGAL PAGE OFFSET: indx: %lu of %lu\n",
			    (u_long)i, (u_long)h->inp[i]);
			goto bad;
		}
		switch (TYPE(h)) {
		case P_HASH:
			type = HPAGE_TYPE(h, i);
			if (type != H_OFFDUP &&
			    type != H_DUPLICATE &&
			    type != H_KEYDATA &&
			    type != H_OFFPAGE) {
				fprintf(fp, "ILLEGAL HASH TYPE: %lu\n",
				    (u_long)type);
				goto bad;
			}
			break;
		case P_IBTREE:
			bi = GET_BINTERNAL(h, i);
			if (B_TYPE(bi->type) != B_KEYDATA &&
			    B_TYPE(bi->type) != B_DUPLICATE &&
			    B_TYPE(bi->type) != B_OVERFLOW) {
				fprintf(fp, "ILLEGAL BINTERNAL TYPE: %lu\n",
				    (u_long)B_TYPE(bi->type));
				goto bad;
			}
			break;
		case P_IRECNO:
		case P_LBTREE:
		case P_LRECNO:
			break;
		case P_DUPLICATE:
			bk = GET_BKEYDATA(h, i);
			if (B_TYPE(bk->type) != B_KEYDATA &&
			    B_TYPE(bk->type) != B_DUPLICATE &&
			    B_TYPE(bk->type) != B_OVERFLOW) {
				fprintf(fp,
			    "ILLEGAL DUPLICATE/LBTREE/LRECNO TYPE: %lu\n",
				    (u_long)B_TYPE(bk->type));
				goto bad;
			}
			break;
		default:
			fprintf(fp,
			    "ILLEGAL PAGE ITEM: %lu\n", (u_long)TYPE(h));
			goto bad;
		}
	}
	return (0);

bad:	if (die) {
		abort();
		/* NOTREACHED */
	}
	return (1);
}

/*
 * __db_pr --
 *	Print out a data element.
 *
 * PUBLIC: void __db_pr __P((u_int8_t *, u_int32_t));
 */
void
__db_pr(p, len)
	u_int8_t *p;
	u_int32_t len;
{
	FILE *fp;
	u_int lastch;
	int i;

	fp = __db_prinit(NULL);

	fprintf(fp, "len: %3lu", (u_long)len);
	lastch = '.';
	if (len != 0) {
		fprintf(fp, " data: ");
		for (i = len <= 20 ? len : 20; i > 0; --i, ++p) {
			lastch = *p;
			if (isprint(*p) || *p == '\n')
				fprintf(fp, "%c", *p);
			else
				fprintf(fp, "0x%.2x", (u_int)*p);
		}
		if (len > 20) {
			fprintf(fp, "...");
			lastch = '.';
		}
	}
	if (lastch != '\n')
		fprintf(fp, "\n");
}

/*
 * __db_prdbt --
 *	Print out a DBT data element.
 *
 * PUBLIC: int __db_prdbt __P((DBT *, int, FILE *));
 */
int
__db_prdbt(dbtp, checkprint, fp)
	DBT *dbtp;
	int checkprint;
	FILE *fp;
{
	static const char hex[] = "0123456789abcdef";
	u_int8_t *p;
	u_int32_t len;

	/*
	 * !!!
	 * This routine is the routine that dumps out items in the format
	 * used by db_dump(1) and db_load(1).  This means that the format
	 * cannot change.
	 */
	if (checkprint) {
		for (len = dbtp->size, p = dbtp->data; len--; ++p)
			if (isprint(*p)) {
				if (*p == '\\' && fprintf(fp, "\\") != 1)
					return (EIO);
				if (fprintf(fp, "%c", *p) != 1)
					return (EIO);
			} else
				if (fprintf(fp, "\\%c%c",
				    hex[(u_int8_t)(*p & 0xf0) >> 4],
				    hex[*p & 0x0f]) != 3)
					return (EIO);
	} else
		for (len = dbtp->size, p = dbtp->data; len--; ++p)
			if (fprintf(fp, "%c%c",
			    hex[(u_int8_t)(*p & 0xf0) >> 4],
			    hex[*p & 0x0f]) != 2)
				return (EIO);

	return (fprintf(fp, "\n") == 1 ? 0 : EIO);
}

/*
 * __db_proff --
 *	Print out an off-page element.
 */
static void
__db_proff(vp)
	void *vp;
{
	FILE *fp;
	BOVERFLOW *bo;

	fp = __db_prinit(NULL);

	bo = vp;
	switch (B_TYPE(bo->type)) {
	case B_OVERFLOW:
		fprintf(fp, "overflow: total len: %4lu page: %4lu\n",
		    (u_long)bo->tlen, (u_long)bo->pgno);
		break;
	case B_DUPLICATE:
		fprintf(fp, "duplicate: page: %4lu\n", (u_long)bo->pgno);
		break;
	}
}

/*
 * __db_prflags --
 *	Print out flags values.
 *
 * PUBLIC: void __db_prflags __P((u_int32_t, const FN *, FILE *));
 */
void
__db_prflags(flags, fn, fp)
	u_int32_t flags;
	FN const *fn;
	FILE *fp;
{
	const FN *fnp;
	int found;
	const char *sep;

	sep = " (";
	for (found = 0, fnp = fn; fnp->mask != 0; ++fnp)
		if (LF_ISSET(fnp->mask)) {
			fprintf(fp, "%s%s", sep, fnp->name);
			sep = ", ";
			found = 1;
		}
	if (found)
		fprintf(fp, ")");
}

/*
 * __db_psize --
 *	Get the page size.
 */
static void
__db_psize(mpf)
	DB_MPOOLFILE *mpf;
{
	BTMETA *mp;
	db_pgno_t pgno;

	set_psize = PSIZE_BOUNDARY - 1;

	pgno = PGNO_METADATA;
	if (memp_fget(mpf, &pgno, 0, &mp) != 0)
		return;

	switch (mp->magic) {
	case DB_BTREEMAGIC:
	case DB_HASHMAGIC:
		set_psize = mp->pagesize;
		break;
	}
	(void)memp_fput(mpf, mp, 0);
}
