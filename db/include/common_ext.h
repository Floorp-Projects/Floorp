/* DO NOT EDIT: automatically built by dist/distrib. */
#ifndef _common_ext_h_
#define _common_ext_h_
int __db_appname __P((DB_ENV *,
   APPNAME, const char *, const char *, u_int32_t, int *, char **));
int __db_apprec __P((DB_ENV *, u_int32_t));
int __db_byteorder __P((DB_ENV *, int));
#ifdef __STDC__
void __db_err __P((const DB_ENV *dbenv, const char *fmt, ...));
#else
void __db_err();
#endif
int __db_panic __P((DB *));
int __db_fchk __P((DB_ENV *, const char *, u_int32_t, u_int32_t));
int __db_fcchk
   __P((DB_ENV *, const char *, u_int32_t, u_int32_t, u_int32_t));
int __db_cdelchk __P((const DB *, u_int32_t, int, int));
int __db_cgetchk __P((const DB *, DBT *, DBT *, u_int32_t, int));
int __db_cputchk __P((const DB *,
   const DBT *, DBT *, u_int32_t, int, int));
int __db_delchk __P((const DB *, DBT *, u_int32_t, int));
int __db_getchk __P((const DB *, const DBT *, DBT *, u_int32_t));
int __db_putchk
   __P((const DB *, DBT *, const DBT *, u_int32_t, int, int));
int __db_statchk __P((const DB *, u_int32_t));
int __db_syncchk __P((const DB *, u_int32_t));
int __db_ferr __P((const DB_ENV *, const char *, int));
u_int32_t __db_log2 __P((u_int32_t));
int __db_rattach __P((REGINFO *));
int __db_rdetach __P((REGINFO *));
int __db_runlink __P((REGINFO *, int));
int __db_rgrow __P((REGINFO *, size_t));
int __db_rreattach __P((REGINFO *, size_t));
void __db_shalloc_init __P((void *, size_t));
int __db_shalloc __P((void *, size_t, size_t, void *));
void __db_shalloc_free __P((void *, void *));
size_t __db_shalloc_count __P((void *));
size_t __db_shsizeof __P((void *));
void __db_shalloc_dump __P((void *, FILE *));
int __db_tablesize __P((u_int32_t));
void __db_hashinit __P((void *, u_int32_t));
#endif /* _common_ext_h_ */
