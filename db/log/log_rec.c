/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */
/*
 * Copyright (c) 1995, 1996
 *	The President and Fellows of Harvard University.  All rights reserved.
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
static const char sccsid[] = "@(#)log_rec.c	10.20 (Sleepycat) 4/28/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <string.h>
#endif

#include "db_int.h"
#include "shqueue.h"
#include "log.h"
#include "db_dispatch.h"
#include "common_ext.h"

static int __log_open_file __P((DB_LOG *,
    u_int8_t *, char *, DBTYPE, u_int32_t));

/*
 * PUBLIC: int __log_register_recover
 * PUBLIC:     __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
 */
int
__log_register_recover(logp, dbtp, lsnp, redo, info)
	DB_LOG *logp;
	DBT *dbtp;
	DB_LSN *lsnp;
	int redo;
	void *info;
{
	__log_register_args *argp;
	int ret;

#ifdef DEBUG_RECOVER
	__log_register_print(logp, dbtp, lsnp, redo, info);
#endif
	COMPQUIET(info, NULL);
	COMPQUIET(lsnp, NULL);

	F_SET(logp, DB_AM_RECOVER);

	if ((ret = __log_register_read(dbtp->data, &argp)) != 0)
		goto out;

	if ((argp->opcode == LOG_CHECKPOINT && redo == TXN_OPENFILES) ||
	    (argp->opcode == LOG_OPEN &&
	    (redo == TXN_REDO || redo == TXN_OPENFILES ||
	     redo == TXN_FORWARD_ROLL)) ||
	    (argp->opcode == LOG_CLOSE &&
	    (redo == TXN_UNDO || redo == TXN_BACKWARD_ROLL))) {
		/*
		 * If we are redoing an open or undoing a close, then we need
		 * to open a file.
		 */
		ret = __log_open_file(logp,
		    argp->uid.data, argp->name.data, argp->ftype, argp->id);
		if (ret == ENOENT) {
			if (redo == TXN_OPENFILES)
				__db_err(logp->dbenv,
				    "warning: file %s not found",
				    argp->name.data);
			ret = 0;
		}
	} else if (argp->opcode != LOG_CHECKPOINT) {
		/*
		 * If we are redoing a close or undoing an open, then we need
		 * to close the file.
		 *
		 * If the file is deleted, then we can just ignore this close.
		 * Otherwise, we'd better have a valid dbp that we should either
		 * close or whose reference count should be decremented.
		 */
		LOCK_LOGTHREAD(logp);
		if (logp->dbentry[argp->id].dbp == NULL) {
			if (!logp->dbentry[argp->id].deleted)
				ret = EINVAL;
		} else if (--logp->dbentry[argp->id].refcount == 0) {
			F_SET(logp->dbentry[argp->id].dbp, DB_AM_RECOVER);
			ret = logp->dbentry[argp->id].dbp->close(
			    logp->dbentry[argp->id].dbp, 0);
			logp->dbentry[argp->id].dbp = NULL;
		}
		UNLOCK_LOGTHREAD(logp);
	}

out:	F_CLR(logp, DB_AM_RECOVER);
	if (argp != NULL)
		__db_free(argp);
	return (ret);
}

/* Hand coded routines. */

/*
 * Called during log_register recovery.  Make sure that we have an
 * entry in the dbentry table for this ndx.
 * Returns 0 on success, non-zero on error.
 */
static int
__log_open_file(lp, uid, name, ftype, ndx)
	DB_LOG *lp;
	u_int8_t *uid;
	char *name;
	DBTYPE ftype;
	u_int32_t ndx;
{
	DB *dbp;
	int ret;

	LOCK_LOGTHREAD(lp);
	if (ndx < lp->dbentry_cnt &&
	    (lp->dbentry[ndx].deleted == 1 || lp->dbentry[ndx].dbp != NULL)) {
		lp->dbentry[ndx].refcount++;

		UNLOCK_LOGTHREAD(lp);
		return (0);
	}
	UNLOCK_LOGTHREAD(lp);

	/* Need to open file. */
	dbp = NULL;
	if ((ret = db_open(name, ftype, 0, 0, lp->dbenv, NULL, &dbp)) == 0) {
		/*
		 * Verify that we are opening the same file that we were
		 * referring to when we wrote this log record.
		 */
		if (memcmp(uid, dbp->lock.fileid, DB_FILE_ID_LEN) != 0) {
			(void)dbp->close(dbp, 0);
			dbp = NULL;
			ret = ENOENT;
		}
	}

	if (ret == 0 || ret == ENOENT)
		(void)__log_add_logid(lp, dbp, ndx);

	return (ret);
}

/*
 * This function returns:
 *	0 SUCCESS (the entry was not previously set and is now set or the
 *		entry was previously set and we just inced the ref count.
 *	>0 on system error (returns errno value).
 * PUBLIC: int __log_add_logid __P((DB_LOG *, DB *, u_int32_t));
 */
int
__log_add_logid(logp, dbp, ndx)
	DB_LOG *logp;
	DB *dbp;
	u_int32_t ndx;
{
	DB_ENTRY *temp_entryp;
	u_int32_t i;
	int ret;

	ret = 0;

	LOCK_LOGTHREAD(logp);
	/*
	 * Check if we need to grow the table.
	 */
	if (logp->dbentry_cnt <= ndx) {
		if (logp->dbentry_cnt == 0) {
			logp->dbentry = (DB_ENTRY *)
			    __db_malloc(DB_GROW_SIZE * sizeof(DB_ENTRY));
			if (logp->dbentry == NULL) {
				ret = ENOMEM;
				goto err;
			}
		} else {
			temp_entryp = (DB_ENTRY *)__db_realloc(logp->dbentry,
			    (DB_GROW_SIZE + logp->dbentry_cnt) *
			    sizeof(DB_ENTRY));
			if (temp_entryp == NULL) {
				ret = ENOMEM;
				goto err;
			}
			logp->dbentry = temp_entryp;

		}
		/* Initialize the new entries. */
		for (i = logp->dbentry_cnt;
		    i < logp->dbentry_cnt + DB_GROW_SIZE; i++) {
			logp->dbentry[i].dbp = NULL;
			logp->dbentry[i].deleted = 0;
		}

		logp->dbentry_cnt += DB_GROW_SIZE;
	}

	if (logp->dbentry[ndx].deleted == 0 && logp->dbentry[ndx].dbp == NULL) {
		logp->dbentry[ndx].dbp = dbp;
		logp->dbentry[ndx].refcount = 1;
		logp->dbentry[ndx].deleted = dbp == NULL;
	} else
		logp->dbentry[ndx].refcount++;

err:	UNLOCK_LOGTHREAD(logp);
	return (ret);
}


/*
 * __db_fileid_to_db --
 *	Return the DB corresponding to the specified fileid.
 *
 * PUBLIC: int __db_fileid_to_db __P((DB_LOG *, DB **, u_int32_t));
 */
int
__db_fileid_to_db(logp, dbpp, ndx)
	DB_LOG *logp;
	DB **dbpp;
	u_int32_t ndx;
{
	int ret;

	ret = 0;
	LOCK_LOGTHREAD(logp);

	/*
	 * Return DB_DELETED if the file has been deleted
	 * (it's not an error).
	 */
	if (logp->dbentry[ndx].deleted) {
		ret = DB_DELETED;
		goto err;
	}

	/*
	 * Otherwise return 0, but if we don't have a corresponding DB,
	 * it's an error.
	 */
	if ((*dbpp = logp->dbentry[ndx].dbp) == NULL)
		ret = ENOENT;

err:	UNLOCK_LOGTHREAD(logp);
	return (ret);
}

/*
 * Close files that were opened by the recovery daemon.
 *
 * PUBLIC: void __log_close_files __P((DB_LOG *));
 */
void
__log_close_files(logp)
	DB_LOG *logp;
{
	u_int32_t i;

	LOCK_LOGTHREAD(logp);
	for (i = 0; i < logp->dbentry_cnt; i++)
		if (logp->dbentry[i].dbp)
			logp->dbentry[i].dbp->close(logp->dbentry[i].dbp, 0);
	UNLOCK_LOGTHREAD(logp);
}

/*
 * PUBLIC: void __log_rem_logid __P((DB_LOG *, u_int32_t));
 */
void
__log_rem_logid(logp, ndx)
	DB_LOG *logp;
	u_int32_t ndx;
{
	LOCK_LOGTHREAD(logp);
	if (--logp->dbentry[ndx].refcount == 0) {
		logp->dbentry[ndx].dbp = NULL;
		logp->dbentry[ndx].deleted = 0;
	}
	UNLOCK_LOGTHREAD(logp);
}
