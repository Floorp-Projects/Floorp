/* DO NOT EDIT: automatically built by dist/distrib. */
#ifndef _os_ext_h_
#define _os_ext_h_
int __db_abspath __P((const char *));
char *__db_strdup __P((const char *));
void *__db_calloc __P((size_t, size_t));
void *__db_malloc __P((size_t));
void *__db_realloc __P((void *, size_t));
int __os_dirlist __P((const char *, char ***, int *));
void __os_dirfree __P((char **, int));
int __db_fileid __P((DB_ENV *, const char *, int, u_int8_t *));
int __db_fsync __P((int));
int __db_mapanon_ok __P((int));
int __db_mapinit __P((void));
int __db_mapregion __P((char *, REGINFO *));
int __db_unmapregion __P((REGINFO *));
int __db_unlinkregion __P((char *, REGINFO *));
int __db_mapfile __P((char *, int, size_t, int, void **));
int __db_unmapfile __P((void *, size_t));
u_int32_t __db_oflags __P((int));
int __db_omode __P((const char *));
int __db_open __P((const char *, u_int32_t, u_int32_t, int, int *));
int __db_close __P((int));
char *__db_rpath __P((const char *));
int __db_read __P((int, void *, size_t, ssize_t *));
int __db_write __P((int, void *, size_t, ssize_t *));
int __os_seek __P((int, size_t, db_pgno_t, u_int32_t, int, int));
int __os_sleep __P((u_long, u_long));
int __os_spin __P((void));
int __os_exists __P((const char *, int *));
int __os_ioinfo
   __P((const char *, int, u_int32_t *, u_int32_t *, u_int32_t *));
int __db_unlink __P((const char *));
#endif /* _os_ext_h_ */
