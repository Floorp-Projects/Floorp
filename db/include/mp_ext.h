/* DO NOT EDIT: automatically built by dist/distrib. */
#ifndef _mp_ext_h_
#define _mp_ext_h_
int __memp_bhwrite
    __P((DB_MPOOL *, MPOOLFILE *, BH *, int *, int *));
int __memp_pgread __P((DB_MPOOLFILE *, BH *, int));
int __memp_pgwrite __P((DB_MPOOLFILE *, BH *, int *, int *));
int __memp_pg __P((DB_MPOOLFILE *, BH *, int));
void __memp_bhfree __P((DB_MPOOL *, MPOOLFILE *, BH *, int));
int __memp_fopen __P((DB_MPOOL *, MPOOLFILE *, const char *,
   u_int32_t, int, size_t, int, DB_MPOOL_FINFO *, DB_MPOOLFILE **));
char * __memp_fn __P((DB_MPOOLFILE *));
char * __memp_fns __P((DB_MPOOL *, MPOOLFILE *));
void __memp_dump_region __P((DB_MPOOL *, char *, FILE *));
int __memp_ralloc __P((DB_MPOOL *, size_t, size_t *, void *));
int __memp_ropen
   __P((DB_MPOOL *, const char *, size_t, int, int, u_int32_t));
int __mp_xxx_fd __P((DB_MPOOLFILE *, int *));
#endif /* _mp_ext_h_ */
