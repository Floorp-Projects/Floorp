/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */
#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)log_get.c	10.32 (Sleepycat) 5/6/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <string.h>
#include <unistd.h>
#endif

#include "db_int.h"
#include "shqueue.h"
#include "db_page.h"
#include "log.h"
#include "hash.h"
#include "common_ext.h"

/*
 * log_get --
 *	Get a log record.
 */
int
log_get(dblp, alsn, dbt, flags)
	DB_LOG *dblp;
	DB_LSN *alsn;
	DBT *dbt;
	u_int32_t flags;
{
	int ret;

	/* Validate arguments. */
#define	OKFLAGS	(DB_CHECKPOINT | \
    DB_CURRENT | DB_FIRST | DB_LAST | DB_NEXT | DB_PREV | DB_SET)
	if ((ret = __db_fchk(dblp->dbenv, "log_get", flags, OKFLAGS)) != 0)
		return (ret);
	switch (flags) {
	case DB_CHECKPOINT:
	case DB_CURRENT:
	case DB_FIRST:
	case DB_LAST:
	case DB_NEXT:
	case DB_PREV:
	case DB_SET:
		break;
	default:
		return (__db_ferr(dblp->dbenv, "log_get", 1));
	}

	if (F_ISSET(dblp, DB_AM_THREAD)) {
		if (LF_ISSET(DB_NEXT | DB_PREV | DB_CURRENT))
			return (__db_ferr(dblp->dbenv, "log_get", 1));
		if (!F_ISSET(dbt, DB_DBT_USERMEM | DB_DBT_MALLOC))
			return (__db_ferr(dblp->dbenv, "threaded data", 1));
	}

	LOCK_LOGREGION(dblp);

	/*
	 * If we get one of the log's header records, repeat the operation.
	 * This assumes that applications don't ever request the log header
	 * records by LSN, but that seems reasonable to me.
	 */
	ret = __log_get(dblp, alsn, dbt, flags, 0);
	if (ret == 0 && alsn->offset == 0) {
		switch (flags) {
		case DB_FIRST:
			flags = DB_NEXT;
			break;
		case DB_LAST:
			flags = DB_PREV;
			break;
		}
		ret = __log_get(dblp, alsn, dbt, flags, 0);
	}

	UNLOCK_LOGREGION(dblp);

	return (ret);
}

/*
 * __log_get --
 *	Get a log record; internal version.
 *
 * PUBLIC: int __log_get __P((DB_LOG *, DB_LSN *, DBT *, u_int32_t, int));
 */
int
__log_get(dblp, alsn, dbt, flags, silent)
	DB_LOG *dblp;
	DB_LSN *alsn;
	DBT *dbt;
	u_int32_t flags;
	int silent;
{
	DB_LSN nlsn;
	HDR hdr;
	LOG *lp;
	size_t len;
	ssize_t nr;
	int cnt, ret;
	char *np, *tbuf;
	const char *fail;
	void *p, *shortp;

	lp = dblp->lp;
	fail = np = tbuf = NULL;

	nlsn = dblp->c_lsn;
	switch (flags) {
	case DB_CHECKPOINT:
		nlsn = lp->chkpt_lsn;
		if (IS_ZERO_LSN(nlsn)) {
			__db_err(dblp->dbenv,
	"log_get: unable to find checkpoint record: no checkpoint set.");
			ret = ENOENT;
			goto err2;
		}
		break;
	case DB_NEXT:				/* Next log record. */
		if (!IS_ZERO_LSN(nlsn)) {
			/* Increment the cursor by the cursor record size. */
			nlsn.offset += dblp->c_len;
			break;
		}
		/* FALLTHROUGH */
	case DB_FIRST:				/* Find the first log record. */
		/* Find the first log file. */
		if ((ret = __log_find(dblp, 1, &cnt)) != 0)
			goto err2;

		/*
		 * We may have only entered records in the buffer, and not
		 * yet written a log file.  If no log files were found and
		 * there's anything in the buffer, it belongs to file 1.
		 */
		if (cnt == 0)
			cnt = 1;

		nlsn.file = cnt;
		nlsn.offset = 0;
		break;
	case DB_CURRENT:			/* Current log record. */
		break;
	case DB_PREV:				/* Previous log record. */
		if (!IS_ZERO_LSN(nlsn)) {
			/* If at start-of-file, move to the previous file. */
			if (nlsn.offset == 0) {
				if (nlsn.file == 1 ||
				    __log_valid(dblp, NULL, nlsn.file - 1) != 0)
					return (DB_NOTFOUND);

				--nlsn.file;
				nlsn.offset = dblp->c_off;
			} else
				nlsn.offset = dblp->c_off;
			break;
		}
		/* FALLTHROUGH */
	case DB_LAST:				/* Last log record. */
		nlsn.file = lp->lsn.file;
		nlsn.offset = lp->lsn.offset - lp->len;
		break;
	case DB_SET:				/* Set log record. */
		nlsn = *alsn;
		break;
	}

retry:
	/* Return 1 if the request is past end-of-file. */
	if (nlsn.file > lp->lsn.file ||
	    (nlsn.file == lp->lsn.file && nlsn.offset >= lp->lsn.offset))
		return (DB_NOTFOUND);

	/* If we've switched files, discard the current fd. */
	if (dblp->c_lsn.file != nlsn.file && dblp->c_fd != -1) {
		(void)__db_close(dblp->c_fd);
		dblp->c_fd = -1;
	}

	/* If the entire record is in the in-memory buffer, copy it out. */
	if (nlsn.file == lp->lsn.file && nlsn.offset >= lp->w_off) {
		/* Copy the header. */
		p = lp->buf + (nlsn.offset - lp->w_off);
		memcpy(&hdr, p, sizeof(HDR));

		/* Copy the record. */
		len = hdr.len - sizeof(HDR);
		if ((ret = __db_retcopy(dbt, (u_int8_t *)p + sizeof(HDR),
		    len, &dblp->c_dbt.data, &dblp->c_dbt.ulen, NULL)) != 0)
			goto err1;
		goto cksum;
	}

	/* Acquire a file descriptor. */
	if (dblp->c_fd == -1) {
		if ((ret = __log_name(dblp, nlsn.file, &np)) != 0)
			goto err1;
		if ((ret = __db_open(np, DB_RDONLY | DB_SEQUENTIAL,
		    DB_RDONLY | DB_SEQUENTIAL, 0, &dblp->c_fd)) != 0) {
			fail = np;
			goto err1;
		}
		__db_free(np);
		np = NULL;
	}

	/* Seek to the header offset and read the header. */
	if ((ret =
	    __db_seek(dblp->c_fd, 0, 0, nlsn.offset, 0, SEEK_SET)) != 0) {
		fail = "seek";
		goto err1;
	}
	if ((ret = __db_read(dblp->c_fd, &hdr, sizeof(HDR), &nr)) != 0) {
		fail = "read";
		goto err1;
	}
	if (nr == sizeof(HDR))
		shortp = NULL;
	else {
		/* If read returns EOF, try the next file. */
		if (nr == 0) {
			if (flags != DB_NEXT || nlsn.file == lp->lsn.file)
				goto corrupt;

			/* Move to the next file. */
			++nlsn.file;
			nlsn.offset = 0;
			goto retry;
		}

		/*
		 * If read returns a short count the rest of the record has
		 * to be in the in-memory buffer.
		 */
		if (lp->b_off < sizeof(HDR) - nr)
			goto corrupt;

		/* Get the rest of the header from the in-memory buffer. */
		memcpy((u_int8_t *)&hdr + nr, lp->buf, sizeof(HDR) - nr);
		shortp = lp->buf + (sizeof(HDR) - nr);
	}

	/*
	 * Check for buffers of 0's, that's what we usually see during
	 * recovery, although it's certainly not something on which we
	 * can depend.
	 */
	if (hdr.len <= sizeof(HDR))
		goto corrupt;
	len = hdr.len - sizeof(HDR);

	/* If we've already moved to the in-memory buffer, fill from there. */
	if (shortp != NULL) {
		if (lp->b_off < ((u_int8_t *)shortp - lp->buf) + len)
			goto corrupt;
		if ((ret = __db_retcopy(dbt, shortp, len,
		    &dblp->c_dbt.data, &dblp->c_dbt.ulen, NULL)) != 0)
			goto err1;
		goto cksum;
	}

	/*
	 * Allocate temporary memory to hold the record.
	 *
	 * XXX
	 * We're calling malloc(3) with a region locked.  This isn't
	 * a good idea.
	 */
	if ((tbuf = (char *)__db_malloc(len)) == NULL) {
		ret = ENOMEM;
		goto err1;
	}

	/*
	 * Read the record into the buffer.  If read returns a short count,
	 * there was an error or the rest of the record is in the in-memory
	 * buffer.  Note, the information may be garbage if we're in recovery,
	 * so don't read past the end of the buffer's memory.
	 */
	if ((ret = __db_read(dblp->c_fd, tbuf, len, &nr)) != 0) {
		fail = "read";
		goto err1;
	}
	if (len - nr > sizeof(lp->buf))
		goto corrupt;
	if (nr != (ssize_t)len) {
		if (lp->b_off < len - nr)
			goto corrupt;

		/* Get the rest of the record from the in-memory buffer. */
		memcpy((u_int8_t *)tbuf + nr, lp->buf, len - nr);
	}

	/* Copy the record into the user's DBT. */
	if ((ret = __db_retcopy(dbt, tbuf, len,
	    &dblp->c_dbt.data, &dblp->c_dbt.ulen, NULL)) != 0)
		goto err1;
	__db_free(tbuf);
	tbuf = NULL;

cksum:	if (hdr.cksum != __ham_func4(dbt->data, dbt->size)) {
		if (!silent)
			__db_err(dblp->dbenv, "log_get: checksum mismatch");
		goto corrupt;
	}

	/* Update the cursor and the return lsn. */
	dblp->c_off = hdr.prev;
	dblp->c_len = hdr.len;
	dblp->c_lsn = *alsn = nlsn;

	return (0);

corrupt:/*
	 * This is the catchall -- for some reason we didn't find enough
	 * information or it wasn't reasonable information, and it wasn't
	 * because a system call failed.
	 */
	ret = EIO;
	fail = "read";

err1:	if (!silent)
		if (fail == NULL)
			__db_err(dblp->dbenv, "log_get: %s", strerror(ret));
		else
			__db_err(dblp->dbenv,
			    "log_get: %s: %s", fail, strerror(ret));
err2:	if (np != NULL)
		__db_free(np);
	if (tbuf != NULL)
		__db_free(tbuf);
	return (ret);
}
