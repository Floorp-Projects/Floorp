/* Do not edit: automatically built by dist/db_gen.sh. */
#ifndef bam_AUTO_H
#define bam_AUTO_H

#define	DB_bam_pg_alloc	(DB_bam_BEGIN + 1)

typedef struct _bam_pg_alloc_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	u_int32_t	fileid;
	DB_LSN 	meta_lsn;
	DB_LSN 	page_lsn;
	db_pgno_t	pgno;
	u_int32_t	ptype;
	db_pgno_t	next;
} __bam_pg_alloc_args;


#define	DB_bam_pg_free	(DB_bam_BEGIN + 2)

typedef struct _bam_pg_free_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	u_int32_t	fileid;
	db_pgno_t	pgno;
	DB_LSN 	meta_lsn;
	DBT	header;
	db_pgno_t	next;
} __bam_pg_free_args;


#define	DB_bam_split	(DB_bam_BEGIN + 3)

typedef struct _bam_split_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	u_int32_t	fileid;
	db_pgno_t	left;
	DB_LSN 	llsn;
	db_pgno_t	right;
	DB_LSN 	rlsn;
	u_int32_t	indx;
	db_pgno_t	npgno;
	DB_LSN 	nlsn;
	DBT	pg;
} __bam_split_args;


#define	DB_bam_rsplit	(DB_bam_BEGIN + 4)

typedef struct _bam_rsplit_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	u_int32_t	fileid;
	db_pgno_t	pgno;
	DBT	pgdbt;
	db_pgno_t	nrec;
	DBT	rootent;
	DB_LSN 	rootlsn;
} __bam_rsplit_args;


#define	DB_bam_adj	(DB_bam_BEGIN + 5)

typedef struct _bam_adj_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	u_int32_t	fileid;
	db_pgno_t	pgno;
	DB_LSN 	lsn;
	u_int32_t	indx;
	u_int32_t	indx_copy;
	u_int32_t	is_insert;
} __bam_adj_args;


#define	DB_bam_cadjust	(DB_bam_BEGIN + 6)

typedef struct _bam_cadjust_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	u_int32_t	fileid;
	db_pgno_t	pgno;
	DB_LSN 	lsn;
	u_int32_t	indx;
	int32_t	adjust;
	int32_t	total;
} __bam_cadjust_args;


#define	DB_bam_cdel	(DB_bam_BEGIN + 7)

typedef struct _bam_cdel_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	u_int32_t	fileid;
	db_pgno_t	pgno;
	DB_LSN 	lsn;
	u_int32_t	indx;
} __bam_cdel_args;


#define	DB_bam_repl	(DB_bam_BEGIN + 8)

typedef struct _bam_repl_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	u_int32_t	fileid;
	db_pgno_t	pgno;
	DB_LSN 	lsn;
	u_int32_t	indx;
	u_int32_t	isdeleted;
	DBT	orig;
	DBT	repl;
	u_int32_t	prefix;
	u_int32_t	suffix;
} __bam_repl_args;

#endif
