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
static const char sccsid[] = "@(#)bt_close.c	10.32 (Sleepycat) 5/6/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "btree.h"

static void __bam_upstat __P((DB *dbp));

/*
 * __bam_close --
 *	Close a btree.
 *
 * PUBLIC: int __bam_close __P((DB *));
 */
int
__bam_close(dbp)
	DB *dbp;
{
	BTREE *t;

	DEBUG_LWRITE(dbp, NULL, "bam_close", NULL, NULL, 0);

	t = dbp->internal;

	/* Update tree statistics. */
	__bam_upstat(dbp);

	/* Free any allocated memory. */
	if (t->bt_rkey.data)
		FREE(t->bt_rkey.data, t->bt_rkey.size);
	if (t->bt_rdata.data)
		FREE(t->bt_rdata.data, t->bt_rdata.ulen);
	if (t->bt_sp != t->bt_stack)
		FREE(t->bt_sp, (t->bt_esp - t->bt_sp) * sizeof(EPG));

	FREE(t, sizeof(BTREE));
	dbp->internal = NULL;

	return (0);
}

/*
 * __bam_sync --
 *	Sync the btree to disk.
 *
 * PUBLIC: int __bam_sync __P((DB *, u_int32_t));
 */
int
__bam_sync(argdbp, flags)
	DB *argdbp;
	u_int32_t flags;
{
	DB *dbp;
	int ret;

	DEBUG_LWRITE(argdbp, NULL, "bam_sync", NULL, NULL, flags);

	/* Check for invalid flags. */
	if ((ret = __db_syncchk(argdbp, flags)) != 0)
		return (ret);

	/* If it wasn't possible to modify the file, we're done. */
	if (F_ISSET(argdbp, DB_AM_INMEM | DB_AM_RDONLY))
		return (0);

	GETHANDLE(argdbp, NULL, &dbp, ret);

	/* Flush any dirty pages from the cache to the backing file. */
	if ((ret = memp_fsync(dbp->mpf)) == DB_INCOMPLETE)
		ret = 0;

	PUTHANDLE(dbp);
	return (ret);
}

/*
 * __bam_upstat --
 *	Update tree statistics.
 */
static void
__bam_upstat(dbp)
	DB *dbp;
{
	BTREE *t;
	BTMETA *meta;
	DB_LOCK metalock;
	db_pgno_t pgno;
	u_int32_t flags;

	/*
	 * We use a no-op log call to log the update of the statistics onto the
	 * metadata page.  The Db->close call isn't transaction protected to
	 * start with, and I'm not sure what undoing a statistics update means,
	 * anyway.
	 */
	if (F_ISSET(dbp, DB_AM_INMEM | DB_AM_RDONLY))
		return;

	flags = 0;
	pgno = PGNO_METADATA;

	/* Lock and retrieve the page. */
	if (__bam_lget(dbp, 0, pgno, DB_LOCK_WRITE, &metalock) != 0)
		return;
	if (__bam_pget(dbp, (PAGE **)&meta, &pgno, 0) == 0) {
		/* Log the change. */
		if (DB_LOGGING(dbp) &&
		    __db_noop_log(dbp->dbenv->lg_info, dbp->txn, &LSN(meta), 0,
		    dbp->log_fileid, PGNO_METADATA, &LSN(meta)) != 0)
			goto err;

		/* Update the statistics. */
		t = dbp->internal;
		__bam_add_mstat(&t->lstat, &meta->stat);

		flags = DB_MPOOL_DIRTY;
	}

err:	(void)memp_fput(dbp->mpf, (PAGE *)meta, flags);
	(void)__BT_LPUT(dbp, metalock);
}
