/* DO NOT EDIT: automatically built by dist/distrib. */
#ifndef _lock_ext_h_
#define _lock_ext_h_
int __lock_is_locked
   __P((DB_LOCKTAB *, u_int32_t, DBT *, db_lockmode_t));
void __lock_printlock __P((DB_LOCKTAB *, struct __db_lock *, int));
int __lock_getobj  __P((DB_LOCKTAB *,
    u_int32_t, const DBT *, u_int32_t type, DB_LOCKOBJ **));
int __lock_validate_region __P((DB_LOCKTAB *));
int __lock_grow_region __P((DB_LOCKTAB *, int, size_t));
void __lock_dump_region __P((DB_LOCKTAB *, char *, FILE *));
int __lock_cmp __P((const DBT *, DB_LOCKOBJ *));
int __lock_locker_cmp __P((u_int32_t, DB_LOCKOBJ *));
u_int32_t __lock_ohash __P((const DBT *));
u_int32_t __lock_lhash __P((DB_LOCKOBJ *));
u_int32_t __lock_locker_hash __P((u_int32_t));
#endif /* _lock_ext_h_ */
