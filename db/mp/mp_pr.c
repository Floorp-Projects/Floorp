/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */
#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)mp_pr.c	10.26 (Sleepycat) 5/23/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "shqueue.h"
#include "db_shash.h"
#include "mp.h"
#include "db_auto.h"
#include "db_ext.h"
#include "common_ext.h"

static void __memp_pbh __P((DB_MPOOL *, BH *, size_t *, FILE *));

/*
 * memp_stat --
 *	Display MPOOL statistics.
 */
int
memp_stat(dbmp, gspp, fspp, db_malloc)
	DB_MPOOL *dbmp;
	DB_MPOOL_STAT **gspp;
	DB_MPOOL_FSTAT ***fspp;
	void *(*db_malloc) __P((size_t));
{
	DB_MPOOL_FSTAT **tfsp;
	MPOOLFILE *mfp;
	size_t len, nlen;
	char *name;

	/* Allocate space for the global statistics. */
	if (gspp != NULL) {
		*gspp = NULL;

		if ((*gspp = db_malloc == NULL ?
		    (DB_MPOOL_STAT *)__db_malloc(sizeof(**gspp)) :
		    (DB_MPOOL_STAT *)db_malloc(sizeof(**gspp))) == NULL)
			return (ENOMEM);

		LOCKREGION(dbmp);

		/* Copy out the global statistics. */
		**gspp = dbmp->mp->stat;
		(*gspp)->st_hash_buckets = dbmp->mp->htab_buckets;
		(*gspp)->st_region_wait =
		    dbmp->mp->rlayout.lock.mutex_set_wait;
		(*gspp)->st_region_nowait =
		    dbmp->mp->rlayout.lock.mutex_set_nowait;
		(*gspp)->st_refcnt = dbmp->mp->rlayout.refcnt;
		(*gspp)->st_regsize = dbmp->mp->rlayout.size;

		UNLOCKREGION(dbmp);
	}

	if (fspp != NULL) {
		*fspp = NULL;

		LOCKREGION(dbmp);

		/* Count the MPOOLFILE structures. */
		for (len = 0,
		    mfp = SH_TAILQ_FIRST(&dbmp->mp->mpfq, __mpoolfile);
		    mfp != NULL;
		    ++len, mfp = SH_TAILQ_NEXT(mfp, q, __mpoolfile))
			;

		UNLOCKREGION(dbmp);

		if (len == 0)
			return (0);

		/* Allocate space for the pointers. */
		len = (len + 1) * sizeof(DB_MPOOL_FSTAT *);
		if ((*fspp = db_malloc == NULL ?
		    (DB_MPOOL_FSTAT **)__db_malloc(len) :
		    (DB_MPOOL_FSTAT **)db_malloc(len)) == NULL)
			return (ENOMEM);

		LOCKREGION(dbmp);

		/* Build each individual entry. */
		for (tfsp = *fspp,
		    mfp = SH_TAILQ_FIRST(&dbmp->mp->mpfq, __mpoolfile);
		    mfp != NULL;
		    ++tfsp, mfp = SH_TAILQ_NEXT(mfp, q, __mpoolfile)) {
			name = __memp_fns(dbmp, mfp);
			nlen = strlen(name);
			len = sizeof(DB_MPOOL_FSTAT) + nlen + 1;
			if ((*tfsp = db_malloc == NULL ?
			    (DB_MPOOL_FSTAT *)__db_malloc(len) :
			    (DB_MPOOL_FSTAT *)db_malloc(len)) == NULL)
				return (ENOMEM);
			**tfsp = mfp->stat;
			(*tfsp)->file_name = (char *)
			    (u_int8_t *)*tfsp + sizeof(DB_MPOOL_FSTAT);
			memcpy((*tfsp)->file_name, name, nlen + 1);
		}
		*tfsp = NULL;

		UNLOCKREGION(dbmp);
	}
	return (0);
}

/*
 * __memp_fn --
 *	On errors we print whatever is available as the file name.
 *
 * PUBLIC: char * __memp_fn __P((DB_MPOOLFILE *));
 */
char *
__memp_fn(dbmfp)
	DB_MPOOLFILE *dbmfp;
{
	return (__memp_fns(dbmfp->dbmp, dbmfp->mfp));
}

/*
 * __memp_fns --
 *	On errors we print whatever is available as the file name.
 *
 * PUBLIC: char * __memp_fns __P((DB_MPOOL *, MPOOLFILE *));
 *
 */
char *
__memp_fns(dbmp, mfp)
	DB_MPOOL *dbmp;
	MPOOLFILE *mfp;
{
	if (mfp->path_off == 0)
		return ((char *)"temporary");

	return ((char *)R_ADDR(dbmp, mfp->path_off));
}

#define	FMAP_ENTRIES	200			/* Files we map. */

#define	MPOOL_DUMP_HASH	0x01			/* Debug hash chains. */
#define	MPOOL_DUMP_LRU	0x02			/* Debug LRU chains. */
#define	MPOOL_DUMP_MEM	0x04			/* Debug region memory. */
#define	MPOOL_DUMP_ALL	0x07			/* Debug all. */


/*
 * __memp_dump_region --
 *	Display MPOOL structures.
 *
 * PUBLIC: void __memp_dump_region __P((DB_MPOOL *, char *, FILE *));
 */
void
__memp_dump_region(dbmp, area, fp)
	DB_MPOOL *dbmp;
	char *area;
	FILE *fp;
{
	BH *bhp;
	DB_HASHTAB *htabp;
	DB_MPOOLFILE *dbmfp;
	MPOOL *mp;
	MPOOLFILE *mfp;
	size_t bucket, fmap[FMAP_ENTRIES + 1];
	u_int32_t flags;
	int cnt;

	/* Make it easy to call from the debugger. */
	if (fp == NULL)
		fp = stderr;

	for (flags = 0; *area != '\0'; ++area)
		switch (*area) {
		case 'A':
			LF_SET(MPOOL_DUMP_ALL);
			break;
		case 'h':
			LF_SET(MPOOL_DUMP_HASH);
			break;
		case 'l':
			LF_SET(MPOOL_DUMP_LRU);
			break;
		case 'm':
			LF_SET(MPOOL_DUMP_MEM);
			break;
		}

	LOCKREGION(dbmp);

	mp = dbmp->mp;

	/* Display MPOOL structures. */
	(void)fprintf(fp, "%s\nPool (region addr 0x%lx, alloc addr 0x%lx)\n",
	    DB_LINE, (u_long)dbmp->reginfo.addr, (u_long)dbmp->addr);

	/* Display the MPOOLFILE structures. */
	cnt = 0;
	for (mfp = SH_TAILQ_FIRST(&dbmp->mp->mpfq, __mpoolfile);
	    mfp != NULL; mfp = SH_TAILQ_NEXT(mfp, q, __mpoolfile), ++cnt) {
		(void)fprintf(fp, "file #%d: %s: %lu references: %s\n",
		    cnt + 1, __memp_fns(dbmp, mfp), (u_long)mfp->ref,
		    F_ISSET(mfp, MP_CAN_MMAP) ? "mmap" : "read/write");
		    if (cnt < FMAP_ENTRIES)
			fmap[cnt] = R_OFFSET(dbmp, mfp);
	}

	for (dbmfp = TAILQ_FIRST(&dbmp->dbmfq);
	    dbmfp != NULL; dbmfp = TAILQ_NEXT(dbmfp, q), ++cnt) {
		(void)fprintf(fp, "file #%d: %s: fd: %d: per-process, %s\n",
		    cnt + 1, __memp_fn(dbmfp), dbmfp->fd,
		    F_ISSET(dbmfp, MP_READONLY) ? "readonly" : "read/write");
		    if (cnt < FMAP_ENTRIES)
			fmap[cnt] = R_OFFSET(dbmp, mfp);
	}
	if (cnt < FMAP_ENTRIES)
		fmap[cnt] = INVALID;
	else
		fmap[FMAP_ENTRIES] = INVALID;

	/* Display the hash table list of BH's. */
	if (LF_ISSET(MPOOL_DUMP_HASH)) {
		(void)fprintf(fp,
	    "%s\nBH hash table (%lu hash slots)\npageno, file, ref, address\n",
		    DB_LINE, (u_long)mp->htab_buckets);
		for (htabp = dbmp->htab,
		    bucket = 0; bucket < mp->htab_buckets; ++htabp, ++bucket) {
			if (SH_TAILQ_FIRST(&dbmp->htab[bucket], __bh) != NULL)
				(void)fprintf(fp, "%lu:\n", (u_long)bucket);
			for (bhp = SH_TAILQ_FIRST(&dbmp->htab[bucket], __bh);
			    bhp != NULL; bhp = SH_TAILQ_NEXT(bhp, hq, __bh))
				__memp_pbh(dbmp, bhp, fmap, fp);
		}
	}

	/* Display the LRU list of BH's. */
	if (LF_ISSET(MPOOL_DUMP_LRU)) {
		(void)fprintf(fp, "%s\nBH LRU list\n", DB_LINE);
		(void)fprintf(fp, "pageno, file, ref, address\n");
		for (bhp = SH_TAILQ_FIRST(&dbmp->mp->bhq, __bh);
		    bhp != NULL; bhp = SH_TAILQ_NEXT(bhp, q, __bh))
			__memp_pbh(dbmp, bhp, fmap, fp);
	}

	if (LF_ISSET(MPOOL_DUMP_MEM))
		__db_shalloc_dump(dbmp->addr, fp);

	UNLOCKREGION(dbmp);

	/* Flush in case we're debugging. */
	(void)fflush(fp);
}

/*
 * __memp_pbh --
 *	Display a BH structure.
 */
static void
__memp_pbh(dbmp, bhp, fmap, fp)
	DB_MPOOL *dbmp;
	BH *bhp;
	size_t *fmap;
	FILE *fp;
{
	static const FN fn[] = {
		{ BH_CALLPGIN,	"callpgin" },
		{ BH_DIRTY,	"dirty" },
		{ BH_DISCARD,	"discard" },
		{ BH_LOCKED,	"locked" },
		{ BH_TRASH,	"trash" },
		{ BH_WRITE,	"write" },
		{ 0 },
	};
	int i;

	for (i = 0; i < FMAP_ENTRIES; ++i)
		if (fmap[i] == INVALID || fmap[i] == bhp->mf_offset)
			break;

	if (fmap[i] == INVALID)
		(void)fprintf(fp, "  %4lu, %lu, %2lu, %lu",
		    (u_long)bhp->pgno, (u_long)bhp->mf_offset,
		    (u_long)bhp->ref, (u_long)R_OFFSET(dbmp, bhp));
	else
		(void)fprintf(fp, "  %4lu,   #%d,  %2lu, %lu",
		    (u_long)bhp->pgno, i + 1,
		    (u_long)bhp->ref, (u_long)R_OFFSET(dbmp, bhp));

	__db_prflags(bhp->flags, fn, fp);

	(void)fprintf(fp, "\n");
}
