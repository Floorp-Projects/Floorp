/* Do not edit: automatically built by dist/db_gen.sh. */
#ifndef txn_AUTO_H
#define txn_AUTO_H

#define	DB_txn_regop	(DB_txn_BEGIN + 1)

typedef struct _txn_regop_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	u_int32_t	opcode;
} __txn_regop_args;


#define	DB_txn_ckp	(DB_txn_BEGIN + 2)

typedef struct _txn_ckp_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	DB_LSN 	ckp_lsn;
	DB_LSN 	last_ckp;
} __txn_ckp_args;

#endif
