/* Do not edit: automatically built by dist/db_gen.sh. */
#include "config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#endif

#include "db_int.h"
#include "shqueue.h"
#include "db_page.h"
#include "db_dispatch.h"
#include "log.h"
#include "db_am.h"
/*
 * PUBLIC: int __log_register_log
 * PUBLIC:     __P((DB_LOG *, DB_TXN *, DB_LSN *, u_int32_t,
 * PUBLIC:     u_int32_t, const DBT *, const DBT *, u_int32_t,
 * PUBLIC:     DBTYPE));
 */
int __log_register_log(logp, txnid, ret_lsnp, flags,
	opcode, name, uid, id, ftype)
	DB_LOG *logp;
	DB_TXN *txnid;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	u_int32_t opcode;
	const DBT *name;
	const DBT *uid;
	u_int32_t id;
	DBTYPE ftype;
{
	DBT logrec;
	DB_LSN *lsnp, null_lsn;
	u_int32_t zero;
	u_int32_t rectype, txn_num;
	int ret;
	u_int8_t *bp;

	rectype = DB_log_register;
	txn_num = txnid == NULL ? 0 : txnid->txnid;
	if (txnid == NULL) {
		null_lsn.file = 0;
		null_lsn.offset = 0;
		lsnp = &null_lsn;
	} else
		lsnp = &txnid->last_lsn;
	logrec.size = sizeof(rectype) + sizeof(txn_num) + sizeof(DB_LSN)
	    + sizeof(opcode)
	    + sizeof(u_int32_t) + (name == NULL ? 0 : name->size)
	    + sizeof(u_int32_t) + (uid == NULL ? 0 : uid->size)
	    + sizeof(id)
	    + sizeof(ftype);
	if ((logrec.data = (void *)__db_malloc(logrec.size)) == NULL)
		return (ENOMEM);

	bp = logrec.data;
	memcpy(bp, &rectype, sizeof(rectype));
	bp += sizeof(rectype);
	memcpy(bp, &txn_num, sizeof(txn_num));
	bp += sizeof(txn_num);
	memcpy(bp, lsnp, sizeof(DB_LSN));
	bp += sizeof(DB_LSN);
	memcpy(bp, &opcode, sizeof(opcode));
	bp += sizeof(opcode);
	if (name == NULL) {
		zero = 0;
		memcpy(bp, &zero, sizeof(u_int32_t));
		bp += sizeof(u_int32_t);
	} else {
		memcpy(bp, &name->size, sizeof(name->size));
		bp += sizeof(name->size);
		memcpy(bp, name->data, name->size);
		bp += name->size;
	}
	if (uid == NULL) {
		zero = 0;
		memcpy(bp, &zero, sizeof(u_int32_t));
		bp += sizeof(u_int32_t);
	} else {
		memcpy(bp, &uid->size, sizeof(uid->size));
		bp += sizeof(uid->size);
		memcpy(bp, uid->data, uid->size);
		bp += uid->size;
	}
	memcpy(bp, &id, sizeof(id));
	bp += sizeof(id);
	memcpy(bp, &ftype, sizeof(ftype));
	bp += sizeof(ftype);
#ifdef DIAGNOSTIC
	if ((u_int32_t)(bp - (u_int8_t *)logrec.data) != logrec.size)
		fprintf(stderr, "Error in log record length");
#endif
	ret = __log_put(logp, ret_lsnp, (DBT *)&logrec, flags);
	if (txnid != NULL)
		txnid->last_lsn = *ret_lsnp;
	__db_free(logrec.data);
	return (ret);
}

/*
 * PUBLIC: int __log_register_print
 * PUBLIC:    __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
 */
int
__log_register_print(notused1, dbtp, lsnp, notused2, notused3)
	DB_LOG *notused1;
	DBT *dbtp;
	DB_LSN *lsnp;
	int notused2;
	void *notused3;
{
	__log_register_args *argp;
	u_int32_t i;
	u_int ch;
	int ret;

	i = 0;
	ch = 0;
	notused1 = NULL;
	notused2 = 0;
	notused3 = NULL;

	if ((ret = __log_register_read(dbtp->data, &argp)) != 0)
		return (ret);
	printf("[%lu][%lu]log_register: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	printf("\topcode: %lu\n", (u_long)argp->opcode);
	printf("\tname: ");
	for (i = 0; i < argp->name.size; i++) {
		ch = ((u_int8_t *)argp->name.data)[i];
		if (isprint(ch) || ch == 0xa)
			putchar(ch);
		else
			printf("%#x ", ch);
	}
	printf("\n");
	printf("\tuid: ");
	for (i = 0; i < argp->uid.size; i++) {
		ch = ((u_int8_t *)argp->uid.data)[i];
		if (isprint(ch) || ch == 0xa)
			putchar(ch);
		else
			printf("%#x ", ch);
	}
	printf("\n");
	printf("\tid: %lu\n", (u_long)argp->id);
	printf("\tftype: 0x%lx\n", (u_long)argp->ftype);
	printf("\n");
	__db_free(argp);
	return (0);
}

/*
 * PUBLIC: int __log_register_read __P((void *, __log_register_args **));
 */
int
__log_register_read(recbuf, argpp)
	void *recbuf;
	__log_register_args **argpp;
{
	__log_register_args *argp;
	u_int8_t *bp;

	argp = (__log_register_args *)__db_malloc(sizeof(__log_register_args) +
	    sizeof(DB_TXN));
	if (argp == NULL)
		return (ENOMEM);
	argp->txnid = (DB_TXN *)&argp[1];
	bp = recbuf;
	memcpy(&argp->type, bp, sizeof(argp->type));
	bp += sizeof(argp->type);
	memcpy(&argp->txnid->txnid,  bp, sizeof(argp->txnid->txnid));
	bp += sizeof(argp->txnid->txnid);
	memcpy(&argp->prev_lsn, bp, sizeof(DB_LSN));
	bp += sizeof(DB_LSN);
	memcpy(&argp->opcode, bp, sizeof(argp->opcode));
	bp += sizeof(argp->opcode);
	memcpy(&argp->name.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->name.data = bp;
	bp += argp->name.size;
	memcpy(&argp->uid.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->uid.data = bp;
	bp += argp->uid.size;
	memcpy(&argp->id, bp, sizeof(argp->id));
	bp += sizeof(argp->id);
	memcpy(&argp->ftype, bp, sizeof(argp->ftype));
	bp += sizeof(argp->ftype);
	*argpp = argp;
	return (0);
}

/*
 * PUBLIC: int __log_init_print __P((DB_ENV *));
 */
int
__log_init_print(dbenv)
	DB_ENV *dbenv;
{
	int ret;

	if ((ret = __db_add_recovery(dbenv,
	    __log_register_print, DB_log_register)) != 0)
		return (ret);
	return (0);
}

/*
 * PUBLIC: int __log_init_recover __P((DB_ENV *));
 */
int
__log_init_recover(dbenv)
	DB_ENV *dbenv;
{
	int ret;

	if ((ret = __db_add_recovery(dbenv,
	    __log_register_recover, DB_log_register)) != 0)
		return (ret);
	return (0);
}

