/* DO NOT EDIT: automatically built by dist/distrib. */
#ifndef _hash_ext_h_
#define _hash_ext_h_
int __ham_open __P((DB *, DB_INFO *));
int __ham_close __P((DB *));
int __ham_c_iclose __P((DB *, DBC *));
int __ham_expand_table __P((HTAB *));
u_int32_t __ham_call_hash __P((HTAB *, u_int8_t *, int32_t));
int __ham_init_dbt __P((DBT *, u_int32_t, void **, u_int32_t *));
void __ham_c_update
   __P((HASH_CURSOR *, db_pgno_t, u_int32_t, int, int));
int  __ham_hdup __P((DB *, DB *));
int __ham_insdel_log
    __P((DB_LOG *, DB_TXN *, DB_LSN *, u_int32_t,
    u_int32_t, u_int32_t, db_pgno_t, u_int32_t,
    DB_LSN *, const DBT *, const DBT *));
int __ham_insdel_print
   __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
int __ham_insdel_read __P((void *, __ham_insdel_args **));
int __ham_newpage_log
    __P((DB_LOG *, DB_TXN *, DB_LSN *, u_int32_t,
    u_int32_t, u_int32_t, db_pgno_t, DB_LSN *,
    db_pgno_t, DB_LSN *, db_pgno_t, DB_LSN *));
int __ham_newpage_print
   __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
int __ham_newpage_read __P((void *, __ham_newpage_args **));
int __ham_splitmeta_log
    __P((DB_LOG *, DB_TXN *, DB_LSN *, u_int32_t,
    u_int32_t, u_int32_t, u_int32_t, u_int32_t,
    DB_LSN *));
int __ham_splitmeta_print
   __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
int __ham_splitmeta_read __P((void *, __ham_splitmeta_args **));
int __ham_splitdata_log
    __P((DB_LOG *, DB_TXN *, DB_LSN *, u_int32_t,
    u_int32_t, u_int32_t, db_pgno_t, const DBT *,
    DB_LSN *));
int __ham_splitdata_print
   __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
int __ham_splitdata_read __P((void *, __ham_splitdata_args **));
int __ham_replace_log
    __P((DB_LOG *, DB_TXN *, DB_LSN *, u_int32_t,
    u_int32_t, db_pgno_t, u_int32_t, DB_LSN *,
    int32_t, const DBT *, const DBT *, u_int32_t));
int __ham_replace_print
   __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
int __ham_replace_read __P((void *, __ham_replace_args **));
int __ham_newpgno_log
    __P((DB_LOG *, DB_TXN *, DB_LSN *, u_int32_t,
    u_int32_t, u_int32_t, db_pgno_t, db_pgno_t,
    u_int32_t, db_pgno_t, u_int32_t, DB_LSN *,
    DB_LSN *));
int __ham_newpgno_print
   __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
int __ham_newpgno_read __P((void *, __ham_newpgno_args **));
int __ham_ovfl_log
    __P((DB_LOG *, DB_TXN *, DB_LSN *, u_int32_t,
    u_int32_t, db_pgno_t, u_int32_t, db_pgno_t,
    u_int32_t, DB_LSN *));
int __ham_ovfl_print
   __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
int __ham_ovfl_read __P((void *, __ham_ovfl_args **));
int __ham_copypage_log
    __P((DB_LOG *, DB_TXN *, DB_LSN *, u_int32_t,
    u_int32_t, db_pgno_t, DB_LSN *, db_pgno_t,
    DB_LSN *, db_pgno_t, DB_LSN *, const DBT *));
int __ham_copypage_print
   __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
int __ham_copypage_read __P((void *, __ham_copypage_args **));
int __ham_init_print __P((DB_ENV *));
int __ham_init_recover __P((DB_ENV *));
int __ham_pgin __P((db_pgno_t, void *, DBT *));
int __ham_pgout __P((db_pgno_t, void *, DBT *));
int __ham_mswap __P((void *));
#ifdef DEBUG
void __ham_dump_bucket __P((HTAB *, u_int32_t));
#endif
int __ham_add_dup __P((HTAB *, HASH_CURSOR *, DBT *, u_int32_t));
void __ham_move_offpage __P((HTAB *, PAGE *, u_int32_t, db_pgno_t));
u_int32_t __ham_func2 __P((const void *, u_int32_t));
u_int32_t __ham_func3 __P((const void *, u_int32_t));
u_int32_t __ham_func4 __P((const void *, u_int32_t));
u_int32_t __ham_func5 __P((const void *, u_int32_t));
int __ham_item __P((HTAB *, HASH_CURSOR *, db_lockmode_t));
int __ham_item_reset __P((HTAB *, HASH_CURSOR *));
void __ham_item_init __P((HASH_CURSOR *));
int __ham_item_done __P((HTAB *, HASH_CURSOR *, int));
int __ham_item_last __P((HTAB *, HASH_CURSOR *, db_lockmode_t));
int __ham_item_first __P((HTAB *, HASH_CURSOR *, db_lockmode_t));
int __ham_item_prev __P((HTAB *, HASH_CURSOR *, db_lockmode_t));
int __ham_item_next __P((HTAB *, HASH_CURSOR *, db_lockmode_t));
void __ham_putitem __P((PAGE *p, const DBT *, int));
void __ham_reputpair
   __P((PAGE *p, u_int32_t, u_int32_t, const DBT *, const DBT *));
int __ham_del_pair __P((HTAB *, HASH_CURSOR *, int));
int __ham_replpair __P((HTAB *, HASH_CURSOR *, DBT *, u_int32_t));
void __ham_onpage_replace __P((PAGE *, size_t, u_int32_t, int32_t,
    int32_t,  DBT *));
int __ham_split_page __P((HTAB *, u_int32_t, u_int32_t));
int __ham_add_el
   __P((HTAB *, HASH_CURSOR *, const DBT *, const DBT *, int));
void __ham_copy_item __P((HTAB *, PAGE *, u_int32_t, PAGE *));
int __ham_add_ovflpage __P((HTAB *, PAGE *, int, PAGE **));
int __ham_new_page __P((HTAB *, u_int32_t, u_int32_t, PAGE **));
int __ham_del_page __P((DB *, PAGE *));
int __ham_put_page __P((DB *, PAGE *, int32_t));
int __ham_dirty_page __P((HTAB *, PAGE *));
int __ham_get_page __P((DB *, db_pgno_t, PAGE **));
int __ham_overflow_page __P((DB *, u_int32_t, PAGE **));
#ifdef DEBUG
db_pgno_t __bucket_to_page __P((HTAB *, db_pgno_t));
#endif
void __ham_init_ovflpages __P((HTAB *));
int __ham_get_cpage __P((HTAB *, HASH_CURSOR *, db_lockmode_t));
int __ham_next_cpage
   __P((HTAB *, HASH_CURSOR *, db_pgno_t, int, u_int32_t));
void __ham_dpair __P((DB *, PAGE *, u_int32_t));
int __ham_insdel_recover
    __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
int __ham_newpage_recover
    __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
int __ham_replace_recover
   __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
int __ham_newpgno_recover
   __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
int __ham_splitmeta_recover
   __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
int __ham_splitdata_recover
   __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
int __ham_ovfl_recover
    __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
int __ham_copypage_recover
  __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
int __ham_stat __P((DB *, FILE *));
#endif /* _hash_ext_h_ */
