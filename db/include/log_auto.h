/* Do not edit: automatically built by dist/db_gen.sh. */
#ifndef log_AUTO_H
#define log_AUTO_H

#define	DB_log_register	(DB_log_BEGIN + 1)

typedef struct _log_register_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	u_int32_t	opcode;
	DBT	name;
	DBT	uid;
	u_int32_t	id;
	DBTYPE	ftype;
} __log_register_args;

#endif
