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
#include "hash.h"
#include "db_am.h"
/*
 * PUBLIC: int __ham_insdel_log
 * PUBLIC:     __P((DB_LOG *, DB_TXN *, DB_LSN *, u_int32_t,
 * PUBLIC:     u_int32_t, u_int32_t, db_pgno_t, u_int32_t,
 * PUBLIC:     DB_LSN *, const DBT *, const DBT *));
 */
int __ham_insdel_log(logp, txnid, ret_lsnp, flags,
	opcode, fileid, pgno, ndx, pagelsn, key,
	data)
	DB_LOG *logp;
	DB_TXN *txnid;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	u_int32_t opcode;
	u_int32_t fileid;
	db_pgno_t pgno;
	u_int32_t ndx;
	DB_LSN * pagelsn;
	const DBT *key;
	const DBT *data;
{
	DBT logrec;
	DB_LSN *lsnp, null_lsn;
	u_int32_t zero;
	u_int32_t rectype, txn_num;
	int ret;
	u_int8_t *bp;

	rectype = DB_ham_insdel;
	txn_num = txnid == NULL ? 0 : txnid->txnid;
	if (txnid == NULL) {
		null_lsn.file = 0;
		null_lsn.offset = 0;
		lsnp = &null_lsn;
	} else
		lsnp = &txnid->last_lsn;
	logrec.size = sizeof(rectype) + sizeof(txn_num) + sizeof(DB_LSN)
	    + sizeof(opcode)
	    + sizeof(fileid)
	    + sizeof(pgno)
	    + sizeof(ndx)
	    + sizeof(*pagelsn)
	    + sizeof(u_int32_t) + (key == NULL ? 0 : key->size)
	    + sizeof(u_int32_t) + (data == NULL ? 0 : data->size);
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
	memcpy(bp, &fileid, sizeof(fileid));
	bp += sizeof(fileid);
	memcpy(bp, &pgno, sizeof(pgno));
	bp += sizeof(pgno);
	memcpy(bp, &ndx, sizeof(ndx));
	bp += sizeof(ndx);
	if (pagelsn != NULL)
		memcpy(bp, pagelsn, sizeof(*pagelsn));
	else
		memset(bp, 0, sizeof(*pagelsn));
	bp += sizeof(*pagelsn);
	if (key == NULL) {
		zero = 0;
		memcpy(bp, &zero, sizeof(u_int32_t));
		bp += sizeof(u_int32_t);
	} else {
		memcpy(bp, &key->size, sizeof(key->size));
		bp += sizeof(key->size);
		memcpy(bp, key->data, key->size);
		bp += key->size;
	}
	if (data == NULL) {
		zero = 0;
		memcpy(bp, &zero, sizeof(u_int32_t));
		bp += sizeof(u_int32_t);
	} else {
		memcpy(bp, &data->size, sizeof(data->size));
		bp += sizeof(data->size);
		memcpy(bp, data->data, data->size);
		bp += data->size;
	}
#ifdef DIAGNOSTIC
	if ((u_int32_t)(bp - (u_int8_t *)logrec.data) != logrec.size)
		fprintf(stderr, "Error in log record length");
#endif
	ret = log_put(logp, ret_lsnp, (DBT *)&logrec, flags);
	if (txnid != NULL)
		txnid->last_lsn = *ret_lsnp;
	__db_free(logrec.data);
	return (ret);
}

/*
 * PUBLIC: int __ham_insdel_print
 * PUBLIC:    __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
 */
int
__ham_insdel_print(notused1, dbtp, lsnp, notused2, notused3)
	DB_LOG *notused1;
	DBT *dbtp;
	DB_LSN *lsnp;
	int notused2;
	void *notused3;
{
	__ham_insdel_args *argp;
	u_int32_t i;
	u_int ch;
	int ret;

	i = 0;
	ch = 0;
	notused1 = NULL;
	notused2 = 0;
	notused3 = NULL;

	if ((ret = __ham_insdel_read(dbtp->data, &argp)) != 0)
		return (ret);
	printf("[%lu][%lu]ham_insdel: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	printf("\topcode: %lu\n", (u_long)argp->opcode);
	printf("\tfileid: %lu\n", (u_long)argp->fileid);
	printf("\tpgno: %lu\n", (u_long)argp->pgno);
	printf("\tndx: %lu\n", (u_long)argp->ndx);
	printf("\tpagelsn: [%lu][%lu]\n",
	    (u_long)argp->pagelsn.file, (u_long)argp->pagelsn.offset);
	printf("\tkey: ");
	for (i = 0; i < argp->key.size; i++) {
		ch = ((u_int8_t *)argp->key.data)[i];
		if (isprint(ch) || ch == 0xa)
			putchar(ch);
		else
			printf("%#x ", ch);
	}
	printf("\n");
	printf("\tdata: ");
	for (i = 0; i < argp->data.size; i++) {
		ch = ((u_int8_t *)argp->data.data)[i];
		if (isprint(ch) || ch == 0xa)
			putchar(ch);
		else
			printf("%#x ", ch);
	}
	printf("\n");
	printf("\n");
	__db_free(argp);
	return (0);
}

/*
 * PUBLIC: int __ham_insdel_read __P((void *, __ham_insdel_args **));
 */
int
__ham_insdel_read(recbuf, argpp)
	void *recbuf;
	__ham_insdel_args **argpp;
{
	__ham_insdel_args *argp;
	u_int8_t *bp;

	argp = (__ham_insdel_args *)__db_malloc(sizeof(__ham_insdel_args) +
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
	memcpy(&argp->fileid, bp, sizeof(argp->fileid));
	bp += sizeof(argp->fileid);
	memcpy(&argp->pgno, bp, sizeof(argp->pgno));
	bp += sizeof(argp->pgno);
	memcpy(&argp->ndx, bp, sizeof(argp->ndx));
	bp += sizeof(argp->ndx);
	memcpy(&argp->pagelsn, bp,  sizeof(argp->pagelsn));
	bp += sizeof(argp->pagelsn);
	memcpy(&argp->key.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->key.data = bp;
	bp += argp->key.size;
	memcpy(&argp->data.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->data.data = bp;
	bp += argp->data.size;
	*argpp = argp;
	return (0);
}

/*
 * PUBLIC: int __ham_newpage_log
 * PUBLIC:     __P((DB_LOG *, DB_TXN *, DB_LSN *, u_int32_t,
 * PUBLIC:     u_int32_t, u_int32_t, db_pgno_t, DB_LSN *,
 * PUBLIC:     db_pgno_t, DB_LSN *, db_pgno_t, DB_LSN *));
 */
int __ham_newpage_log(logp, txnid, ret_lsnp, flags,
	opcode, fileid, prev_pgno, prevlsn, new_pgno, pagelsn,
	next_pgno, nextlsn)
	DB_LOG *logp;
	DB_TXN *txnid;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	u_int32_t opcode;
	u_int32_t fileid;
	db_pgno_t prev_pgno;
	DB_LSN * prevlsn;
	db_pgno_t new_pgno;
	DB_LSN * pagelsn;
	db_pgno_t next_pgno;
	DB_LSN * nextlsn;
{
	DBT logrec;
	DB_LSN *lsnp, null_lsn;
	u_int32_t rectype, txn_num;
	int ret;
	u_int8_t *bp;

	rectype = DB_ham_newpage;
	txn_num = txnid == NULL ? 0 : txnid->txnid;
	if (txnid == NULL) {
		null_lsn.file = 0;
		null_lsn.offset = 0;
		lsnp = &null_lsn;
	} else
		lsnp = &txnid->last_lsn;
	logrec.size = sizeof(rectype) + sizeof(txn_num) + sizeof(DB_LSN)
	    + sizeof(opcode)
	    + sizeof(fileid)
	    + sizeof(prev_pgno)
	    + sizeof(*prevlsn)
	    + sizeof(new_pgno)
	    + sizeof(*pagelsn)
	    + sizeof(next_pgno)
	    + sizeof(*nextlsn);
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
	memcpy(bp, &fileid, sizeof(fileid));
	bp += sizeof(fileid);
	memcpy(bp, &prev_pgno, sizeof(prev_pgno));
	bp += sizeof(prev_pgno);
	if (prevlsn != NULL)
		memcpy(bp, prevlsn, sizeof(*prevlsn));
	else
		memset(bp, 0, sizeof(*prevlsn));
	bp += sizeof(*prevlsn);
	memcpy(bp, &new_pgno, sizeof(new_pgno));
	bp += sizeof(new_pgno);
	if (pagelsn != NULL)
		memcpy(bp, pagelsn, sizeof(*pagelsn));
	else
		memset(bp, 0, sizeof(*pagelsn));
	bp += sizeof(*pagelsn);
	memcpy(bp, &next_pgno, sizeof(next_pgno));
	bp += sizeof(next_pgno);
	if (nextlsn != NULL)
		memcpy(bp, nextlsn, sizeof(*nextlsn));
	else
		memset(bp, 0, sizeof(*nextlsn));
	bp += sizeof(*nextlsn);
#ifdef DIAGNOSTIC
	if ((u_int32_t)(bp - (u_int8_t *)logrec.data) != logrec.size)
		fprintf(stderr, "Error in log record length");
#endif
	ret = log_put(logp, ret_lsnp, (DBT *)&logrec, flags);
	if (txnid != NULL)
		txnid->last_lsn = *ret_lsnp;
	__db_free(logrec.data);
	return (ret);
}

/*
 * PUBLIC: int __ham_newpage_print
 * PUBLIC:    __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
 */
int
__ham_newpage_print(notused1, dbtp, lsnp, notused2, notused3)
	DB_LOG *notused1;
	DBT *dbtp;
	DB_LSN *lsnp;
	int notused2;
	void *notused3;
{
	__ham_newpage_args *argp;
	u_int32_t i;
	u_int ch;
	int ret;

	i = 0;
	ch = 0;
	notused1 = NULL;
	notused2 = 0;
	notused3 = NULL;

	if ((ret = __ham_newpage_read(dbtp->data, &argp)) != 0)
		return (ret);
	printf("[%lu][%lu]ham_newpage: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	printf("\topcode: %lu\n", (u_long)argp->opcode);
	printf("\tfileid: %lu\n", (u_long)argp->fileid);
	printf("\tprev_pgno: %lu\n", (u_long)argp->prev_pgno);
	printf("\tprevlsn: [%lu][%lu]\n",
	    (u_long)argp->prevlsn.file, (u_long)argp->prevlsn.offset);
	printf("\tnew_pgno: %lu\n", (u_long)argp->new_pgno);
	printf("\tpagelsn: [%lu][%lu]\n",
	    (u_long)argp->pagelsn.file, (u_long)argp->pagelsn.offset);
	printf("\tnext_pgno: %lu\n", (u_long)argp->next_pgno);
	printf("\tnextlsn: [%lu][%lu]\n",
	    (u_long)argp->nextlsn.file, (u_long)argp->nextlsn.offset);
	printf("\n");
	__db_free(argp);
	return (0);
}

/*
 * PUBLIC: int __ham_newpage_read __P((void *, __ham_newpage_args **));
 */
int
__ham_newpage_read(recbuf, argpp)
	void *recbuf;
	__ham_newpage_args **argpp;
{
	__ham_newpage_args *argp;
	u_int8_t *bp;

	argp = (__ham_newpage_args *)__db_malloc(sizeof(__ham_newpage_args) +
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
	memcpy(&argp->fileid, bp, sizeof(argp->fileid));
	bp += sizeof(argp->fileid);
	memcpy(&argp->prev_pgno, bp, sizeof(argp->prev_pgno));
	bp += sizeof(argp->prev_pgno);
	memcpy(&argp->prevlsn, bp,  sizeof(argp->prevlsn));
	bp += sizeof(argp->prevlsn);
	memcpy(&argp->new_pgno, bp, sizeof(argp->new_pgno));
	bp += sizeof(argp->new_pgno);
	memcpy(&argp->pagelsn, bp,  sizeof(argp->pagelsn));
	bp += sizeof(argp->pagelsn);
	memcpy(&argp->next_pgno, bp, sizeof(argp->next_pgno));
	bp += sizeof(argp->next_pgno);
	memcpy(&argp->nextlsn, bp,  sizeof(argp->nextlsn));
	bp += sizeof(argp->nextlsn);
	*argpp = argp;
	return (0);
}

/*
 * PUBLIC: int __ham_splitmeta_log
 * PUBLIC:     __P((DB_LOG *, DB_TXN *, DB_LSN *, u_int32_t,
 * PUBLIC:     u_int32_t, u_int32_t, u_int32_t, u_int32_t,
 * PUBLIC:     DB_LSN *));
 */
int __ham_splitmeta_log(logp, txnid, ret_lsnp, flags,
	fileid, bucket, ovflpoint, spares, metalsn)
	DB_LOG *logp;
	DB_TXN *txnid;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	u_int32_t fileid;
	u_int32_t bucket;
	u_int32_t ovflpoint;
	u_int32_t spares;
	DB_LSN * metalsn;
{
	DBT logrec;
	DB_LSN *lsnp, null_lsn;
	u_int32_t rectype, txn_num;
	int ret;
	u_int8_t *bp;

	rectype = DB_ham_splitmeta;
	txn_num = txnid == NULL ? 0 : txnid->txnid;
	if (txnid == NULL) {
		null_lsn.file = 0;
		null_lsn.offset = 0;
		lsnp = &null_lsn;
	} else
		lsnp = &txnid->last_lsn;
	logrec.size = sizeof(rectype) + sizeof(txn_num) + sizeof(DB_LSN)
	    + sizeof(fileid)
	    + sizeof(bucket)
	    + sizeof(ovflpoint)
	    + sizeof(spares)
	    + sizeof(*metalsn);
	if ((logrec.data = (void *)__db_malloc(logrec.size)) == NULL)
		return (ENOMEM);

	bp = logrec.data;
	memcpy(bp, &rectype, sizeof(rectype));
	bp += sizeof(rectype);
	memcpy(bp, &txn_num, sizeof(txn_num));
	bp += sizeof(txn_num);
	memcpy(bp, lsnp, sizeof(DB_LSN));
	bp += sizeof(DB_LSN);
	memcpy(bp, &fileid, sizeof(fileid));
	bp += sizeof(fileid);
	memcpy(bp, &bucket, sizeof(bucket));
	bp += sizeof(bucket);
	memcpy(bp, &ovflpoint, sizeof(ovflpoint));
	bp += sizeof(ovflpoint);
	memcpy(bp, &spares, sizeof(spares));
	bp += sizeof(spares);
	if (metalsn != NULL)
		memcpy(bp, metalsn, sizeof(*metalsn));
	else
		memset(bp, 0, sizeof(*metalsn));
	bp += sizeof(*metalsn);
#ifdef DIAGNOSTIC
	if ((u_int32_t)(bp - (u_int8_t *)logrec.data) != logrec.size)
		fprintf(stderr, "Error in log record length");
#endif
	ret = log_put(logp, ret_lsnp, (DBT *)&logrec, flags);
	if (txnid != NULL)
		txnid->last_lsn = *ret_lsnp;
	__db_free(logrec.data);
	return (ret);
}

/*
 * PUBLIC: int __ham_splitmeta_print
 * PUBLIC:    __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
 */
int
__ham_splitmeta_print(notused1, dbtp, lsnp, notused2, notused3)
	DB_LOG *notused1;
	DBT *dbtp;
	DB_LSN *lsnp;
	int notused2;
	void *notused3;
{
	__ham_splitmeta_args *argp;
	u_int32_t i;
	u_int ch;
	int ret;

	i = 0;
	ch = 0;
	notused1 = NULL;
	notused2 = 0;
	notused3 = NULL;

	if ((ret = __ham_splitmeta_read(dbtp->data, &argp)) != 0)
		return (ret);
	printf("[%lu][%lu]ham_splitmeta: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	printf("\tfileid: %lu\n", (u_long)argp->fileid);
	printf("\tbucket: %lu\n", (u_long)argp->bucket);
	printf("\tovflpoint: %lu\n", (u_long)argp->ovflpoint);
	printf("\tspares: %lu\n", (u_long)argp->spares);
	printf("\tmetalsn: [%lu][%lu]\n",
	    (u_long)argp->metalsn.file, (u_long)argp->metalsn.offset);
	printf("\n");
	__db_free(argp);
	return (0);
}

/*
 * PUBLIC: int __ham_splitmeta_read __P((void *, __ham_splitmeta_args **));
 */
int
__ham_splitmeta_read(recbuf, argpp)
	void *recbuf;
	__ham_splitmeta_args **argpp;
{
	__ham_splitmeta_args *argp;
	u_int8_t *bp;

	argp = (__ham_splitmeta_args *)__db_malloc(sizeof(__ham_splitmeta_args) +
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
	memcpy(&argp->fileid, bp, sizeof(argp->fileid));
	bp += sizeof(argp->fileid);
	memcpy(&argp->bucket, bp, sizeof(argp->bucket));
	bp += sizeof(argp->bucket);
	memcpy(&argp->ovflpoint, bp, sizeof(argp->ovflpoint));
	bp += sizeof(argp->ovflpoint);
	memcpy(&argp->spares, bp, sizeof(argp->spares));
	bp += sizeof(argp->spares);
	memcpy(&argp->metalsn, bp,  sizeof(argp->metalsn));
	bp += sizeof(argp->metalsn);
	*argpp = argp;
	return (0);
}

/*
 * PUBLIC: int __ham_splitdata_log
 * PUBLIC:     __P((DB_LOG *, DB_TXN *, DB_LSN *, u_int32_t,
 * PUBLIC:     u_int32_t, u_int32_t, db_pgno_t, const DBT *,
 * PUBLIC:     DB_LSN *));
 */
int __ham_splitdata_log(logp, txnid, ret_lsnp, flags,
	fileid, opcode, pgno, pageimage, pagelsn)
	DB_LOG *logp;
	DB_TXN *txnid;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	u_int32_t fileid;
	u_int32_t opcode;
	db_pgno_t pgno;
	const DBT *pageimage;
	DB_LSN * pagelsn;
{
	DBT logrec;
	DB_LSN *lsnp, null_lsn;
	u_int32_t zero;
	u_int32_t rectype, txn_num;
	int ret;
	u_int8_t *bp;

	rectype = DB_ham_splitdata;
	txn_num = txnid == NULL ? 0 : txnid->txnid;
	if (txnid == NULL) {
		null_lsn.file = 0;
		null_lsn.offset = 0;
		lsnp = &null_lsn;
	} else
		lsnp = &txnid->last_lsn;
	logrec.size = sizeof(rectype) + sizeof(txn_num) + sizeof(DB_LSN)
	    + sizeof(fileid)
	    + sizeof(opcode)
	    + sizeof(pgno)
	    + sizeof(u_int32_t) + (pageimage == NULL ? 0 : pageimage->size)
	    + sizeof(*pagelsn);
	if ((logrec.data = (void *)__db_malloc(logrec.size)) == NULL)
		return (ENOMEM);

	bp = logrec.data;
	memcpy(bp, &rectype, sizeof(rectype));
	bp += sizeof(rectype);
	memcpy(bp, &txn_num, sizeof(txn_num));
	bp += sizeof(txn_num);
	memcpy(bp, lsnp, sizeof(DB_LSN));
	bp += sizeof(DB_LSN);
	memcpy(bp, &fileid, sizeof(fileid));
	bp += sizeof(fileid);
	memcpy(bp, &opcode, sizeof(opcode));
	bp += sizeof(opcode);
	memcpy(bp, &pgno, sizeof(pgno));
	bp += sizeof(pgno);
	if (pageimage == NULL) {
		zero = 0;
		memcpy(bp, &zero, sizeof(u_int32_t));
		bp += sizeof(u_int32_t);
	} else {
		memcpy(bp, &pageimage->size, sizeof(pageimage->size));
		bp += sizeof(pageimage->size);
		memcpy(bp, pageimage->data, pageimage->size);
		bp += pageimage->size;
	}
	if (pagelsn != NULL)
		memcpy(bp, pagelsn, sizeof(*pagelsn));
	else
		memset(bp, 0, sizeof(*pagelsn));
	bp += sizeof(*pagelsn);
#ifdef DIAGNOSTIC
	if ((u_int32_t)(bp - (u_int8_t *)logrec.data) != logrec.size)
		fprintf(stderr, "Error in log record length");
#endif
	ret = log_put(logp, ret_lsnp, (DBT *)&logrec, flags);
	if (txnid != NULL)
		txnid->last_lsn = *ret_lsnp;
	__db_free(logrec.data);
	return (ret);
}

/*
 * PUBLIC: int __ham_splitdata_print
 * PUBLIC:    __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
 */
int
__ham_splitdata_print(notused1, dbtp, lsnp, notused2, notused3)
	DB_LOG *notused1;
	DBT *dbtp;
	DB_LSN *lsnp;
	int notused2;
	void *notused3;
{
	__ham_splitdata_args *argp;
	u_int32_t i;
	u_int ch;
	int ret;

	i = 0;
	ch = 0;
	notused1 = NULL;
	notused2 = 0;
	notused3 = NULL;

	if ((ret = __ham_splitdata_read(dbtp->data, &argp)) != 0)
		return (ret);
	printf("[%lu][%lu]ham_splitdata: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	printf("\tfileid: %lu\n", (u_long)argp->fileid);
	printf("\topcode: %lu\n", (u_long)argp->opcode);
	printf("\tpgno: %lu\n", (u_long)argp->pgno);
	printf("\tpageimage: ");
	for (i = 0; i < argp->pageimage.size; i++) {
		ch = ((u_int8_t *)argp->pageimage.data)[i];
		if (isprint(ch) || ch == 0xa)
			putchar(ch);
		else
			printf("%#x ", ch);
	}
	printf("\n");
	printf("\tpagelsn: [%lu][%lu]\n",
	    (u_long)argp->pagelsn.file, (u_long)argp->pagelsn.offset);
	printf("\n");
	__db_free(argp);
	return (0);
}

/*
 * PUBLIC: int __ham_splitdata_read __P((void *, __ham_splitdata_args **));
 */
int
__ham_splitdata_read(recbuf, argpp)
	void *recbuf;
	__ham_splitdata_args **argpp;
{
	__ham_splitdata_args *argp;
	u_int8_t *bp;

	argp = (__ham_splitdata_args *)__db_malloc(sizeof(__ham_splitdata_args) +
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
	memcpy(&argp->fileid, bp, sizeof(argp->fileid));
	bp += sizeof(argp->fileid);
	memcpy(&argp->opcode, bp, sizeof(argp->opcode));
	bp += sizeof(argp->opcode);
	memcpy(&argp->pgno, bp, sizeof(argp->pgno));
	bp += sizeof(argp->pgno);
	memcpy(&argp->pageimage.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->pageimage.data = bp;
	bp += argp->pageimage.size;
	memcpy(&argp->pagelsn, bp,  sizeof(argp->pagelsn));
	bp += sizeof(argp->pagelsn);
	*argpp = argp;
	return (0);
}

/*
 * PUBLIC: int __ham_replace_log
 * PUBLIC:     __P((DB_LOG *, DB_TXN *, DB_LSN *, u_int32_t,
 * PUBLIC:     u_int32_t, db_pgno_t, u_int32_t, DB_LSN *,
 * PUBLIC:     int32_t, const DBT *, const DBT *, u_int32_t));
 */
int __ham_replace_log(logp, txnid, ret_lsnp, flags,
	fileid, pgno, ndx, pagelsn, off, olditem,
	newitem, makedup)
	DB_LOG *logp;
	DB_TXN *txnid;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	u_int32_t fileid;
	db_pgno_t pgno;
	u_int32_t ndx;
	DB_LSN * pagelsn;
	int32_t off;
	const DBT *olditem;
	const DBT *newitem;
	u_int32_t makedup;
{
	DBT logrec;
	DB_LSN *lsnp, null_lsn;
	u_int32_t zero;
	u_int32_t rectype, txn_num;
	int ret;
	u_int8_t *bp;

	rectype = DB_ham_replace;
	txn_num = txnid == NULL ? 0 : txnid->txnid;
	if (txnid == NULL) {
		null_lsn.file = 0;
		null_lsn.offset = 0;
		lsnp = &null_lsn;
	} else
		lsnp = &txnid->last_lsn;
	logrec.size = sizeof(rectype) + sizeof(txn_num) + sizeof(DB_LSN)
	    + sizeof(fileid)
	    + sizeof(pgno)
	    + sizeof(ndx)
	    + sizeof(*pagelsn)
	    + sizeof(off)
	    + sizeof(u_int32_t) + (olditem == NULL ? 0 : olditem->size)
	    + sizeof(u_int32_t) + (newitem == NULL ? 0 : newitem->size)
	    + sizeof(makedup);
	if ((logrec.data = (void *)__db_malloc(logrec.size)) == NULL)
		return (ENOMEM);

	bp = logrec.data;
	memcpy(bp, &rectype, sizeof(rectype));
	bp += sizeof(rectype);
	memcpy(bp, &txn_num, sizeof(txn_num));
	bp += sizeof(txn_num);
	memcpy(bp, lsnp, sizeof(DB_LSN));
	bp += sizeof(DB_LSN);
	memcpy(bp, &fileid, sizeof(fileid));
	bp += sizeof(fileid);
	memcpy(bp, &pgno, sizeof(pgno));
	bp += sizeof(pgno);
	memcpy(bp, &ndx, sizeof(ndx));
	bp += sizeof(ndx);
	if (pagelsn != NULL)
		memcpy(bp, pagelsn, sizeof(*pagelsn));
	else
		memset(bp, 0, sizeof(*pagelsn));
	bp += sizeof(*pagelsn);
	memcpy(bp, &off, sizeof(off));
	bp += sizeof(off);
	if (olditem == NULL) {
		zero = 0;
		memcpy(bp, &zero, sizeof(u_int32_t));
		bp += sizeof(u_int32_t);
	} else {
		memcpy(bp, &olditem->size, sizeof(olditem->size));
		bp += sizeof(olditem->size);
		memcpy(bp, olditem->data, olditem->size);
		bp += olditem->size;
	}
	if (newitem == NULL) {
		zero = 0;
		memcpy(bp, &zero, sizeof(u_int32_t));
		bp += sizeof(u_int32_t);
	} else {
		memcpy(bp, &newitem->size, sizeof(newitem->size));
		bp += sizeof(newitem->size);
		memcpy(bp, newitem->data, newitem->size);
		bp += newitem->size;
	}
	memcpy(bp, &makedup, sizeof(makedup));
	bp += sizeof(makedup);
#ifdef DIAGNOSTIC
	if ((u_int32_t)(bp - (u_int8_t *)logrec.data) != logrec.size)
		fprintf(stderr, "Error in log record length");
#endif
	ret = log_put(logp, ret_lsnp, (DBT *)&logrec, flags);
	if (txnid != NULL)
		txnid->last_lsn = *ret_lsnp;
	__db_free(logrec.data);
	return (ret);
}

/*
 * PUBLIC: int __ham_replace_print
 * PUBLIC:    __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
 */
int
__ham_replace_print(notused1, dbtp, lsnp, notused2, notused3)
	DB_LOG *notused1;
	DBT *dbtp;
	DB_LSN *lsnp;
	int notused2;
	void *notused3;
{
	__ham_replace_args *argp;
	u_int32_t i;
	u_int ch;
	int ret;

	i = 0;
	ch = 0;
	notused1 = NULL;
	notused2 = 0;
	notused3 = NULL;

	if ((ret = __ham_replace_read(dbtp->data, &argp)) != 0)
		return (ret);
	printf("[%lu][%lu]ham_replace: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	printf("\tfileid: %lu\n", (u_long)argp->fileid);
	printf("\tpgno: %lu\n", (u_long)argp->pgno);
	printf("\tndx: %lu\n", (u_long)argp->ndx);
	printf("\tpagelsn: [%lu][%lu]\n",
	    (u_long)argp->pagelsn.file, (u_long)argp->pagelsn.offset);
	printf("\toff: %ld\n", (long)argp->off);
	printf("\tolditem: ");
	for (i = 0; i < argp->olditem.size; i++) {
		ch = ((u_int8_t *)argp->olditem.data)[i];
		if (isprint(ch) || ch == 0xa)
			putchar(ch);
		else
			printf("%#x ", ch);
	}
	printf("\n");
	printf("\tnewitem: ");
	for (i = 0; i < argp->newitem.size; i++) {
		ch = ((u_int8_t *)argp->newitem.data)[i];
		if (isprint(ch) || ch == 0xa)
			putchar(ch);
		else
			printf("%#x ", ch);
	}
	printf("\n");
	printf("\tmakedup: %lu\n", (u_long)argp->makedup);
	printf("\n");
	__db_free(argp);
	return (0);
}

/*
 * PUBLIC: int __ham_replace_read __P((void *, __ham_replace_args **));
 */
int
__ham_replace_read(recbuf, argpp)
	void *recbuf;
	__ham_replace_args **argpp;
{
	__ham_replace_args *argp;
	u_int8_t *bp;

	argp = (__ham_replace_args *)__db_malloc(sizeof(__ham_replace_args) +
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
	memcpy(&argp->fileid, bp, sizeof(argp->fileid));
	bp += sizeof(argp->fileid);
	memcpy(&argp->pgno, bp, sizeof(argp->pgno));
	bp += sizeof(argp->pgno);
	memcpy(&argp->ndx, bp, sizeof(argp->ndx));
	bp += sizeof(argp->ndx);
	memcpy(&argp->pagelsn, bp,  sizeof(argp->pagelsn));
	bp += sizeof(argp->pagelsn);
	memcpy(&argp->off, bp, sizeof(argp->off));
	bp += sizeof(argp->off);
	memcpy(&argp->olditem.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->olditem.data = bp;
	bp += argp->olditem.size;
	memcpy(&argp->newitem.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->newitem.data = bp;
	bp += argp->newitem.size;
	memcpy(&argp->makedup, bp, sizeof(argp->makedup));
	bp += sizeof(argp->makedup);
	*argpp = argp;
	return (0);
}

/*
 * PUBLIC: int __ham_newpgno_log
 * PUBLIC:     __P((DB_LOG *, DB_TXN *, DB_LSN *, u_int32_t,
 * PUBLIC:     u_int32_t, u_int32_t, db_pgno_t, db_pgno_t,
 * PUBLIC:     u_int32_t, db_pgno_t, u_int32_t, DB_LSN *,
 * PUBLIC:     DB_LSN *));
 */
int __ham_newpgno_log(logp, txnid, ret_lsnp, flags,
	opcode, fileid, pgno, free_pgno, old_type, old_pgno,
	new_type, pagelsn, metalsn)
	DB_LOG *logp;
	DB_TXN *txnid;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	u_int32_t opcode;
	u_int32_t fileid;
	db_pgno_t pgno;
	db_pgno_t free_pgno;
	u_int32_t old_type;
	db_pgno_t old_pgno;
	u_int32_t new_type;
	DB_LSN * pagelsn;
	DB_LSN * metalsn;
{
	DBT logrec;
	DB_LSN *lsnp, null_lsn;
	u_int32_t rectype, txn_num;
	int ret;
	u_int8_t *bp;

	rectype = DB_ham_newpgno;
	txn_num = txnid == NULL ? 0 : txnid->txnid;
	if (txnid == NULL) {
		null_lsn.file = 0;
		null_lsn.offset = 0;
		lsnp = &null_lsn;
	} else
		lsnp = &txnid->last_lsn;
	logrec.size = sizeof(rectype) + sizeof(txn_num) + sizeof(DB_LSN)
	    + sizeof(opcode)
	    + sizeof(fileid)
	    + sizeof(pgno)
	    + sizeof(free_pgno)
	    + sizeof(old_type)
	    + sizeof(old_pgno)
	    + sizeof(new_type)
	    + sizeof(*pagelsn)
	    + sizeof(*metalsn);
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
	memcpy(bp, &fileid, sizeof(fileid));
	bp += sizeof(fileid);
	memcpy(bp, &pgno, sizeof(pgno));
	bp += sizeof(pgno);
	memcpy(bp, &free_pgno, sizeof(free_pgno));
	bp += sizeof(free_pgno);
	memcpy(bp, &old_type, sizeof(old_type));
	bp += sizeof(old_type);
	memcpy(bp, &old_pgno, sizeof(old_pgno));
	bp += sizeof(old_pgno);
	memcpy(bp, &new_type, sizeof(new_type));
	bp += sizeof(new_type);
	if (pagelsn != NULL)
		memcpy(bp, pagelsn, sizeof(*pagelsn));
	else
		memset(bp, 0, sizeof(*pagelsn));
	bp += sizeof(*pagelsn);
	if (metalsn != NULL)
		memcpy(bp, metalsn, sizeof(*metalsn));
	else
		memset(bp, 0, sizeof(*metalsn));
	bp += sizeof(*metalsn);
#ifdef DIAGNOSTIC
	if ((u_int32_t)(bp - (u_int8_t *)logrec.data) != logrec.size)
		fprintf(stderr, "Error in log record length");
#endif
	ret = log_put(logp, ret_lsnp, (DBT *)&logrec, flags);
	if (txnid != NULL)
		txnid->last_lsn = *ret_lsnp;
	__db_free(logrec.data);
	return (ret);
}

/*
 * PUBLIC: int __ham_newpgno_print
 * PUBLIC:    __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
 */
int
__ham_newpgno_print(notused1, dbtp, lsnp, notused2, notused3)
	DB_LOG *notused1;
	DBT *dbtp;
	DB_LSN *lsnp;
	int notused2;
	void *notused3;
{
	__ham_newpgno_args *argp;
	u_int32_t i;
	u_int ch;
	int ret;

	i = 0;
	ch = 0;
	notused1 = NULL;
	notused2 = 0;
	notused3 = NULL;

	if ((ret = __ham_newpgno_read(dbtp->data, &argp)) != 0)
		return (ret);
	printf("[%lu][%lu]ham_newpgno: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	printf("\topcode: %lu\n", (u_long)argp->opcode);
	printf("\tfileid: %lu\n", (u_long)argp->fileid);
	printf("\tpgno: %lu\n", (u_long)argp->pgno);
	printf("\tfree_pgno: %lu\n", (u_long)argp->free_pgno);
	printf("\told_type: %lu\n", (u_long)argp->old_type);
	printf("\told_pgno: %lu\n", (u_long)argp->old_pgno);
	printf("\tnew_type: %lu\n", (u_long)argp->new_type);
	printf("\tpagelsn: [%lu][%lu]\n",
	    (u_long)argp->pagelsn.file, (u_long)argp->pagelsn.offset);
	printf("\tmetalsn: [%lu][%lu]\n",
	    (u_long)argp->metalsn.file, (u_long)argp->metalsn.offset);
	printf("\n");
	__db_free(argp);
	return (0);
}

/*
 * PUBLIC: int __ham_newpgno_read __P((void *, __ham_newpgno_args **));
 */
int
__ham_newpgno_read(recbuf, argpp)
	void *recbuf;
	__ham_newpgno_args **argpp;
{
	__ham_newpgno_args *argp;
	u_int8_t *bp;

	argp = (__ham_newpgno_args *)__db_malloc(sizeof(__ham_newpgno_args) +
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
	memcpy(&argp->fileid, bp, sizeof(argp->fileid));
	bp += sizeof(argp->fileid);
	memcpy(&argp->pgno, bp, sizeof(argp->pgno));
	bp += sizeof(argp->pgno);
	memcpy(&argp->free_pgno, bp, sizeof(argp->free_pgno));
	bp += sizeof(argp->free_pgno);
	memcpy(&argp->old_type, bp, sizeof(argp->old_type));
	bp += sizeof(argp->old_type);
	memcpy(&argp->old_pgno, bp, sizeof(argp->old_pgno));
	bp += sizeof(argp->old_pgno);
	memcpy(&argp->new_type, bp, sizeof(argp->new_type));
	bp += sizeof(argp->new_type);
	memcpy(&argp->pagelsn, bp,  sizeof(argp->pagelsn));
	bp += sizeof(argp->pagelsn);
	memcpy(&argp->metalsn, bp,  sizeof(argp->metalsn));
	bp += sizeof(argp->metalsn);
	*argpp = argp;
	return (0);
}

/*
 * PUBLIC: int __ham_ovfl_log
 * PUBLIC:     __P((DB_LOG *, DB_TXN *, DB_LSN *, u_int32_t,
 * PUBLIC:     u_int32_t, db_pgno_t, u_int32_t, db_pgno_t,
 * PUBLIC:     u_int32_t, DB_LSN *));
 */
int __ham_ovfl_log(logp, txnid, ret_lsnp, flags,
	fileid, start_pgno, npages, free_pgno, ovflpoint, metalsn)
	DB_LOG *logp;
	DB_TXN *txnid;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	u_int32_t fileid;
	db_pgno_t start_pgno;
	u_int32_t npages;
	db_pgno_t free_pgno;
	u_int32_t ovflpoint;
	DB_LSN * metalsn;
{
	DBT logrec;
	DB_LSN *lsnp, null_lsn;
	u_int32_t rectype, txn_num;
	int ret;
	u_int8_t *bp;

	rectype = DB_ham_ovfl;
	txn_num = txnid == NULL ? 0 : txnid->txnid;
	if (txnid == NULL) {
		null_lsn.file = 0;
		null_lsn.offset = 0;
		lsnp = &null_lsn;
	} else
		lsnp = &txnid->last_lsn;
	logrec.size = sizeof(rectype) + sizeof(txn_num) + sizeof(DB_LSN)
	    + sizeof(fileid)
	    + sizeof(start_pgno)
	    + sizeof(npages)
	    + sizeof(free_pgno)
	    + sizeof(ovflpoint)
	    + sizeof(*metalsn);
	if ((logrec.data = (void *)__db_malloc(logrec.size)) == NULL)
		return (ENOMEM);

	bp = logrec.data;
	memcpy(bp, &rectype, sizeof(rectype));
	bp += sizeof(rectype);
	memcpy(bp, &txn_num, sizeof(txn_num));
	bp += sizeof(txn_num);
	memcpy(bp, lsnp, sizeof(DB_LSN));
	bp += sizeof(DB_LSN);
	memcpy(bp, &fileid, sizeof(fileid));
	bp += sizeof(fileid);
	memcpy(bp, &start_pgno, sizeof(start_pgno));
	bp += sizeof(start_pgno);
	memcpy(bp, &npages, sizeof(npages));
	bp += sizeof(npages);
	memcpy(bp, &free_pgno, sizeof(free_pgno));
	bp += sizeof(free_pgno);
	memcpy(bp, &ovflpoint, sizeof(ovflpoint));
	bp += sizeof(ovflpoint);
	if (metalsn != NULL)
		memcpy(bp, metalsn, sizeof(*metalsn));
	else
		memset(bp, 0, sizeof(*metalsn));
	bp += sizeof(*metalsn);
#ifdef DIAGNOSTIC
	if ((u_int32_t)(bp - (u_int8_t *)logrec.data) != logrec.size)
		fprintf(stderr, "Error in log record length");
#endif
	ret = log_put(logp, ret_lsnp, (DBT *)&logrec, flags);
	if (txnid != NULL)
		txnid->last_lsn = *ret_lsnp;
	__db_free(logrec.data);
	return (ret);
}

/*
 * PUBLIC: int __ham_ovfl_print
 * PUBLIC:    __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
 */
int
__ham_ovfl_print(notused1, dbtp, lsnp, notused2, notused3)
	DB_LOG *notused1;
	DBT *dbtp;
	DB_LSN *lsnp;
	int notused2;
	void *notused3;
{
	__ham_ovfl_args *argp;
	u_int32_t i;
	u_int ch;
	int ret;

	i = 0;
	ch = 0;
	notused1 = NULL;
	notused2 = 0;
	notused3 = NULL;

	if ((ret = __ham_ovfl_read(dbtp->data, &argp)) != 0)
		return (ret);
	printf("[%lu][%lu]ham_ovfl: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	printf("\tfileid: %lu\n", (u_long)argp->fileid);
	printf("\tstart_pgno: %lu\n", (u_long)argp->start_pgno);
	printf("\tnpages: %lu\n", (u_long)argp->npages);
	printf("\tfree_pgno: %lu\n", (u_long)argp->free_pgno);
	printf("\tovflpoint: %lu\n", (u_long)argp->ovflpoint);
	printf("\tmetalsn: [%lu][%lu]\n",
	    (u_long)argp->metalsn.file, (u_long)argp->metalsn.offset);
	printf("\n");
	__db_free(argp);
	return (0);
}

/*
 * PUBLIC: int __ham_ovfl_read __P((void *, __ham_ovfl_args **));
 */
int
__ham_ovfl_read(recbuf, argpp)
	void *recbuf;
	__ham_ovfl_args **argpp;
{
	__ham_ovfl_args *argp;
	u_int8_t *bp;

	argp = (__ham_ovfl_args *)__db_malloc(sizeof(__ham_ovfl_args) +
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
	memcpy(&argp->fileid, bp, sizeof(argp->fileid));
	bp += sizeof(argp->fileid);
	memcpy(&argp->start_pgno, bp, sizeof(argp->start_pgno));
	bp += sizeof(argp->start_pgno);
	memcpy(&argp->npages, bp, sizeof(argp->npages));
	bp += sizeof(argp->npages);
	memcpy(&argp->free_pgno, bp, sizeof(argp->free_pgno));
	bp += sizeof(argp->free_pgno);
	memcpy(&argp->ovflpoint, bp, sizeof(argp->ovflpoint));
	bp += sizeof(argp->ovflpoint);
	memcpy(&argp->metalsn, bp,  sizeof(argp->metalsn));
	bp += sizeof(argp->metalsn);
	*argpp = argp;
	return (0);
}

/*
 * PUBLIC: int __ham_copypage_log
 * PUBLIC:     __P((DB_LOG *, DB_TXN *, DB_LSN *, u_int32_t,
 * PUBLIC:     u_int32_t, db_pgno_t, DB_LSN *, db_pgno_t,
 * PUBLIC:     DB_LSN *, db_pgno_t, DB_LSN *, const DBT *));
 */
int __ham_copypage_log(logp, txnid, ret_lsnp, flags,
	fileid, pgno, pagelsn, next_pgno, nextlsn, nnext_pgno,
	nnextlsn, page)
	DB_LOG *logp;
	DB_TXN *txnid;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	u_int32_t fileid;
	db_pgno_t pgno;
	DB_LSN * pagelsn;
	db_pgno_t next_pgno;
	DB_LSN * nextlsn;
	db_pgno_t nnext_pgno;
	DB_LSN * nnextlsn;
	const DBT *page;
{
	DBT logrec;
	DB_LSN *lsnp, null_lsn;
	u_int32_t zero;
	u_int32_t rectype, txn_num;
	int ret;
	u_int8_t *bp;

	rectype = DB_ham_copypage;
	txn_num = txnid == NULL ? 0 : txnid->txnid;
	if (txnid == NULL) {
		null_lsn.file = 0;
		null_lsn.offset = 0;
		lsnp = &null_lsn;
	} else
		lsnp = &txnid->last_lsn;
	logrec.size = sizeof(rectype) + sizeof(txn_num) + sizeof(DB_LSN)
	    + sizeof(fileid)
	    + sizeof(pgno)
	    + sizeof(*pagelsn)
	    + sizeof(next_pgno)
	    + sizeof(*nextlsn)
	    + sizeof(nnext_pgno)
	    + sizeof(*nnextlsn)
	    + sizeof(u_int32_t) + (page == NULL ? 0 : page->size);
	if ((logrec.data = (void *)__db_malloc(logrec.size)) == NULL)
		return (ENOMEM);

	bp = logrec.data;
	memcpy(bp, &rectype, sizeof(rectype));
	bp += sizeof(rectype);
	memcpy(bp, &txn_num, sizeof(txn_num));
	bp += sizeof(txn_num);
	memcpy(bp, lsnp, sizeof(DB_LSN));
	bp += sizeof(DB_LSN);
	memcpy(bp, &fileid, sizeof(fileid));
	bp += sizeof(fileid);
	memcpy(bp, &pgno, sizeof(pgno));
	bp += sizeof(pgno);
	if (pagelsn != NULL)
		memcpy(bp, pagelsn, sizeof(*pagelsn));
	else
		memset(bp, 0, sizeof(*pagelsn));
	bp += sizeof(*pagelsn);
	memcpy(bp, &next_pgno, sizeof(next_pgno));
	bp += sizeof(next_pgno);
	if (nextlsn != NULL)
		memcpy(bp, nextlsn, sizeof(*nextlsn));
	else
		memset(bp, 0, sizeof(*nextlsn));
	bp += sizeof(*nextlsn);
	memcpy(bp, &nnext_pgno, sizeof(nnext_pgno));
	bp += sizeof(nnext_pgno);
	if (nnextlsn != NULL)
		memcpy(bp, nnextlsn, sizeof(*nnextlsn));
	else
		memset(bp, 0, sizeof(*nnextlsn));
	bp += sizeof(*nnextlsn);
	if (page == NULL) {
		zero = 0;
		memcpy(bp, &zero, sizeof(u_int32_t));
		bp += sizeof(u_int32_t);
	} else {
		memcpy(bp, &page->size, sizeof(page->size));
		bp += sizeof(page->size);
		memcpy(bp, page->data, page->size);
		bp += page->size;
	}
#ifdef DIAGNOSTIC
	if ((u_int32_t)(bp - (u_int8_t *)logrec.data) != logrec.size)
		fprintf(stderr, "Error in log record length");
#endif
	ret = log_put(logp, ret_lsnp, (DBT *)&logrec, flags);
	if (txnid != NULL)
		txnid->last_lsn = *ret_lsnp;
	__db_free(logrec.data);
	return (ret);
}

/*
 * PUBLIC: int __ham_copypage_print
 * PUBLIC:    __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
 */
int
__ham_copypage_print(notused1, dbtp, lsnp, notused2, notused3)
	DB_LOG *notused1;
	DBT *dbtp;
	DB_LSN *lsnp;
	int notused2;
	void *notused3;
{
	__ham_copypage_args *argp;
	u_int32_t i;
	u_int ch;
	int ret;

	i = 0;
	ch = 0;
	notused1 = NULL;
	notused2 = 0;
	notused3 = NULL;

	if ((ret = __ham_copypage_read(dbtp->data, &argp)) != 0)
		return (ret);
	printf("[%lu][%lu]ham_copypage: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	printf("\tfileid: %lu\n", (u_long)argp->fileid);
	printf("\tpgno: %lu\n", (u_long)argp->pgno);
	printf("\tpagelsn: [%lu][%lu]\n",
	    (u_long)argp->pagelsn.file, (u_long)argp->pagelsn.offset);
	printf("\tnext_pgno: %lu\n", (u_long)argp->next_pgno);
	printf("\tnextlsn: [%lu][%lu]\n",
	    (u_long)argp->nextlsn.file, (u_long)argp->nextlsn.offset);
	printf("\tnnext_pgno: %lu\n", (u_long)argp->nnext_pgno);
	printf("\tnnextlsn: [%lu][%lu]\n",
	    (u_long)argp->nnextlsn.file, (u_long)argp->nnextlsn.offset);
	printf("\tpage: ");
	for (i = 0; i < argp->page.size; i++) {
		ch = ((u_int8_t *)argp->page.data)[i];
		if (isprint(ch) || ch == 0xa)
			putchar(ch);
		else
			printf("%#x ", ch);
	}
	printf("\n");
	printf("\n");
	__db_free(argp);
	return (0);
}

/*
 * PUBLIC: int __ham_copypage_read __P((void *, __ham_copypage_args **));
 */
int
__ham_copypage_read(recbuf, argpp)
	void *recbuf;
	__ham_copypage_args **argpp;
{
	__ham_copypage_args *argp;
	u_int8_t *bp;

	argp = (__ham_copypage_args *)__db_malloc(sizeof(__ham_copypage_args) +
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
	memcpy(&argp->fileid, bp, sizeof(argp->fileid));
	bp += sizeof(argp->fileid);
	memcpy(&argp->pgno, bp, sizeof(argp->pgno));
	bp += sizeof(argp->pgno);
	memcpy(&argp->pagelsn, bp,  sizeof(argp->pagelsn));
	bp += sizeof(argp->pagelsn);
	memcpy(&argp->next_pgno, bp, sizeof(argp->next_pgno));
	bp += sizeof(argp->next_pgno);
	memcpy(&argp->nextlsn, bp,  sizeof(argp->nextlsn));
	bp += sizeof(argp->nextlsn);
	memcpy(&argp->nnext_pgno, bp, sizeof(argp->nnext_pgno));
	bp += sizeof(argp->nnext_pgno);
	memcpy(&argp->nnextlsn, bp,  sizeof(argp->nnextlsn));
	bp += sizeof(argp->nnextlsn);
	memcpy(&argp->page.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->page.data = bp;
	bp += argp->page.size;
	*argpp = argp;
	return (0);
}

/*
 * PUBLIC: int __ham_init_print __P((DB_ENV *));
 */
int
__ham_init_print(dbenv)
	DB_ENV *dbenv;
{
	int ret;

	if ((ret = __db_add_recovery(dbenv,
	    __ham_insdel_print, DB_ham_insdel)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __ham_newpage_print, DB_ham_newpage)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __ham_splitmeta_print, DB_ham_splitmeta)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __ham_splitdata_print, DB_ham_splitdata)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __ham_replace_print, DB_ham_replace)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __ham_newpgno_print, DB_ham_newpgno)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __ham_ovfl_print, DB_ham_ovfl)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __ham_copypage_print, DB_ham_copypage)) != 0)
		return (ret);
	return (0);
}

/*
 * PUBLIC: int __ham_init_recover __P((DB_ENV *));
 */
int
__ham_init_recover(dbenv)
	DB_ENV *dbenv;
{
	int ret;

	if ((ret = __db_add_recovery(dbenv,
	    __ham_insdel_recover, DB_ham_insdel)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __ham_newpage_recover, DB_ham_newpage)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __ham_splitmeta_recover, DB_ham_splitmeta)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __ham_splitdata_recover, DB_ham_splitdata)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __ham_replace_recover, DB_ham_replace)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __ham_newpgno_recover, DB_ham_newpgno)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __ham_ovfl_recover, DB_ham_ovfl)) != 0)
		return (ret);
	if ((ret = __db_add_recovery(dbenv,
	    __ham_copypage_recover, DB_ham_copypage)) != 0)
		return (ret);
	return (0);
}

