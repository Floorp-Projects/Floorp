/* DO NOT EDIT: automatically built by dist/distrib. */
#ifndef _txn_ext_h_
#define _txn_ext_h_
int __txn_regop_log
    __P((DB_LOG *, DB_TXN *, DB_LSN *, u_int32_t,
    u_int32_t));
int __txn_regop_print
   __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
int __txn_regop_read __P((void *, __txn_regop_args **));
int __txn_ckp_log
    __P((DB_LOG *, DB_TXN *, DB_LSN *, u_int32_t,
    DB_LSN *, DB_LSN *));
int __txn_ckp_print
   __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
int __txn_ckp_read __P((void *, __txn_ckp_args **));
int __txn_init_print __P((DB_ENV *));
int __txn_init_recover __P((DB_ENV *));
int __txn_regop_recover
    __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
int __txn_ckp_recover __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
#endif /* _txn_ext_h_ */
