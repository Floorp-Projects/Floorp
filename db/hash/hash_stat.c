/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)hash_stat.c	10.8 (Sleepycat) 4/26/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "hash.h"

/*
 * __ham_stat --
 *	Gather/print the hash statistics.
 *
 * PUBLIC: int __ham_stat __P((DB *, FILE *));
 */
int
__ham_stat(dbp, fp)
	DB *dbp;
	FILE *fp;
{
	HTAB *hashp;
	int i;

	hashp = (HTAB *)dbp->internal;

	fprintf(fp, "hash: accesses %lu collisions %lu\n",
	    hashp->hash_accesses, hashp->hash_collisions);
	fprintf(fp, "hash: expansions %lu\n", hashp->hash_expansions);
	fprintf(fp, "hash: overflows %lu\n", hashp->hash_overflows);
	fprintf(fp, "hash: big key/data pages %lu\n", hashp->hash_bigpages);

	SET_LOCKER(dbp, NULL);
	GET_META(dbp, hashp);
	fprintf(fp, "keys %lu maxp %lu\n",
	    (u_long)hashp->hdr->nelem, (u_long)hashp->hdr->max_bucket);

	for (i = 0; i < NCACHED; i++)
		fprintf(fp,
		    "spares[%d] = %lu\n", i, (u_long)hashp->hdr->spares[i]);

	RELEASE_META(dbp, hashp);
	return (0);
}
