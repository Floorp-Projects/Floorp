/* DO NOT EDIT: automatically built by dist/distrib. */
#ifndef _clib_ext_h_
#define _clib_ext_h_
#ifdef __STDC__
void err __P((int eval, const char *, ...));
#else
void err();
#endif
#ifdef __STDC__
void errx __P((int eval, const char *, ...));
#else
void errx();
#endif
#ifdef __STDC__
void warn __P((const char *, ...));
#else
void warn();
#endif
#ifdef __STDC__
void warnx __P((const char *, ...));
#else
void warnx();
#endif
#ifndef HAVE_GETCWD
char *getcwd __P((char *, size_t));
#endif
void get_long __P((char *, long, long, long *));
#ifndef HAVE_GETOPT
int getopt __P((int, char * const *, const char *));
#endif
#ifndef HAVE_MEMCMP
int memcmp __P((const void *, const void *, size_t));
#endif
#ifndef HAVE_MEMCPY
void *memcpy __P((void *, const void *, size_t));
#endif
#ifndef HAVE_MEMMOVE
void *memmove __P((void *, const void *, size_t));
#endif
#ifndef HAVE_MEMCPY
void *memcpy __P((void *, const void *, size_t));
#endif
#ifndef HAVE_MEMMOVE
void *memmove __P((void *, const void *, size_t));
#endif
#ifndef HAVE_RAISE
int raise __P((int));
#endif
#ifndef HAVE_SNPRINTF
#ifdef __STDC__
int snprintf __P((char *, size_t, const char *, ...));
#else
int snprintf();
#endif
#endif
#ifndef HAVE_STRERROR
char *strerror __P((int));
#endif
#ifndef HAVE_STRSEP
char *strsep __P((char **, const char *));
#endif
#ifndef HAVE_VSNPRINTF
int vsnprintf();
#endif
#endif /* _clib_ext_h_ */
