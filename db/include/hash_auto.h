/* Do not edit: automatically built by dist/db_gen.sh. */
#ifndef ham_AUTO_H
#define ham_AUTO_H

#define	DB_ham_insdel	(DB_ham_BEGIN + 1)

typedef struct _ham_insdel_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	u_int32_t	opcode;
	u_int32_t	fileid;
	db_pgno_t	pgno;
	u_int32_t	ndx;
	DB_LSN 	pagelsn;
	DBT	key;
	DBT	data;
} __ham_insdel_args;


#define	DB_ham_newpage	(DB_ham_BEGIN + 2)

typedef struct _ham_newpage_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	u_int32_t	opcode;
	u_int32_t	fileid;
	db_pgno_t	prev_pgno;
	DB_LSN 	prevlsn;
	db_pgno_t	new_pgno;
	DB_LSN 	pagelsn;
	db_pgno_t	next_pgno;
	DB_LSN 	nextlsn;
} __ham_newpage_args;


#define	DB_ham_splitmeta	(DB_ham_BEGIN + 3)

typedef struct _ham_splitmeta_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	u_int32_t	fileid;
	u_int32_t	bucket;
	u_int32_t	ovflpoint;
	u_int32_t	spares;
	DB_LSN 	metalsn;
} __ham_splitmeta_args;


#define	DB_ham_splitdata	(DB_ham_BEGIN + 4)

typedef struct _ham_splitdata_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	u_int32_t	fileid;
	u_int32_t	opcode;
	db_pgno_t	pgno;
	DBT	pageimage;
	DB_LSN 	pagelsn;
} __ham_splitdata_args;


#define	DB_ham_replace	(DB_ham_BEGIN + 5)

typedef struct _ham_replace_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	u_int32_t	fileid;
	db_pgno_t	pgno;
	u_int32_t	ndx;
	DB_LSN 	pagelsn;
	int32_t	off;
	DBT	olditem;
	DBT	newitem;
	u_int32_t	makedup;
} __ham_replace_args;


#define	DB_ham_newpgno	(DB_ham_BEGIN + 6)

typedef struct _ham_newpgno_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	u_int32_t	opcode;
	u_int32_t	fileid;
	db_pgno_t	pgno;
	db_pgno_t	free_pgno;
	u_int32_t	old_type;
	db_pgno_t	old_pgno;
	u_int32_t	new_type;
	DB_LSN 	pagelsn;
	DB_LSN 	metalsn;
} __ham_newpgno_args;


#define	DB_ham_ovfl	(DB_ham_BEGIN + 7)

typedef struct _ham_ovfl_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	u_int32_t	fileid;
	db_pgno_t	start_pgno;
	u_int32_t	npages;
	db_pgno_t	free_pgno;
	u_int32_t	ovflpoint;
	DB_LSN 	metalsn;
} __ham_ovfl_args;


#define	DB_ham_copypage	(DB_ham_BEGIN + 8)

typedef struct _ham_copypage_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	u_int32_t	fileid;
	db_pgno_t	pgno;
	DB_LSN 	pagelsn;
	db_pgno_t	next_pgno;
	DB_LSN 	nextlsn;
	db_pgno_t	nnext_pgno;
	DB_LSN 	nnextlsn;
	DBT	page;
} __ham_copypage_args;

#endif
