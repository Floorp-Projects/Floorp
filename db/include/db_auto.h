/* Do not edit: automatically built by dist/db_gen.sh. */
#ifndef db_AUTO_H
#define db_AUTO_H

#define	DB_db_addrem	(DB_db_BEGIN + 1)

typedef struct _db_addrem_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	u_int32_t	opcode;
	u_int32_t	fileid;
	db_pgno_t	pgno;
	u_int32_t	indx;
	size_t	nbytes;
	DBT	hdr;
	DBT	dbt;
	DB_LSN 	pagelsn;
} __db_addrem_args;


#define	DB_db_split	(DB_db_BEGIN + 2)

typedef struct _db_split_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	u_int32_t	opcode;
	u_int32_t	fileid;
	db_pgno_t	pgno;
	DBT	pageimage;
	DB_LSN 	pagelsn;
} __db_split_args;


#define	DB_db_big	(DB_db_BEGIN + 3)

typedef struct _db_big_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	u_int32_t	opcode;
	u_int32_t	fileid;
	db_pgno_t	pgno;
	db_pgno_t	prev_pgno;
	db_pgno_t	next_pgno;
	DBT	dbt;
	DB_LSN 	pagelsn;
	DB_LSN 	prevlsn;
	DB_LSN 	nextlsn;
} __db_big_args;


#define	DB_db_ovref	(DB_db_BEGIN + 4)

typedef struct _db_ovref_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	u_int32_t	fileid;
	db_pgno_t	pgno;
	int32_t	adjust;
	DB_LSN 	lsn;
} __db_ovref_args;


#define	DB_db_relink	(DB_db_BEGIN + 5)

typedef struct _db_relink_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	u_int32_t	fileid;
	db_pgno_t	pgno;
	DB_LSN 	lsn;
	db_pgno_t	prev;
	DB_LSN 	lsn_prev;
	db_pgno_t	next;
	DB_LSN 	lsn_next;
} __db_relink_args;


#define	DB_db_addpage	(DB_db_BEGIN + 6)

typedef struct _db_addpage_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	u_int32_t	fileid;
	db_pgno_t	pgno;
	DB_LSN 	lsn;
	db_pgno_t	nextpgno;
	DB_LSN 	nextlsn;
} __db_addpage_args;


#define	DB_db_debug	(DB_db_BEGIN + 7)

typedef struct _db_debug_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	DBT	op;
	u_int32_t	fileid;
	DBT	key;
	DBT	data;
	u_int32_t	arg_flags;
} __db_debug_args;


#define	DB_db_noop	(DB_db_BEGIN + 8)

typedef struct _db_noop_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	u_int32_t	fileid;
	db_pgno_t	pgno;
	DB_LSN 	prevlsn;
} __db_noop_args;

#endif
