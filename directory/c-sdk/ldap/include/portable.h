/*
 * Copyright (c) 1994 Regents of the University of Michigan.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and that due credit is given
 * to the University of Michigan at Ann Arbor. The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission. This software
 * is provided ``as is'' without express or implied warranty.
 */

#ifndef _PORTABLE_H
#define _PORTABLE_H

/*
 * portable.h for LDAP -- this is where we define common stuff to make
 * life easier on various Unix systems.
 *
 * Unless you are porting LDAP to a new platform, you should not need to
 * edit this file.
 */

#ifndef SYSV
#if defined( hpux ) || defined( sunos5 ) || defined ( sgi ) || defined( SVR4 )
#define SYSV
#endif
#endif

/*
 * under System V, use sysconf() instead of getdtablesize
 */
#if !defined( USE_SYSCONF ) && defined( SYSV )
#define USE_SYSCONF
#endif

/*
 * under System V, daemons should use setsid() instead of detaching from their
 * tty themselves
 */
#if !defined( USE_SETSID ) && defined( SYSV )
#define USE_SETSID
#endif

/*
 * System V has socket options in filio.h
 */
#if !defined( NEED_FILIO ) && defined( SYSV ) && !defined( hpux ) && !defined( AIX )
#define NEED_FILIO
#endif

/*
 * use lockf() under System V
 */
#if !defined( USE_LOCKF ) && ( defined( SYSV ) || defined( aix ))
#define USE_LOCKF
#endif

/*
 * on many systems, we should use waitpid() instead of waitN()
 */
#if !defined( USE_WAITPID ) && ( defined( SYSV ) || defined( sunos4 ) || defined( ultrix ) || defined( aix ))
#define USE_WAITPID
#endif

/*
 * define the wait status argument type
 */
#if ( defined( SunOS ) && SunOS < 40 ) || defined( nextstep )
#define WAITSTATUSTYPE	union wait
#else
#define WAITSTATUSTYPE	int
#endif

/*
 * define the flags for wait
 */
#ifdef sunos5
#define WAIT_FLAGS	( WNOHANG | WUNTRACED | WCONTINUED )
#else
#define WAIT_FLAGS	( WNOHANG | WUNTRACED )
#endif

/*
 * defined the options for openlog (syslog)
 */
#ifdef ultrix
#define OPENLOG_OPTIONS		LOG_PID
#else
#define OPENLOG_OPTIONS		( LOG_PID | LOG_NOWAIT )
#endif

/*
 * some systems don't have the BSD re_comp and re_exec routines
 */
#ifndef NEED_BSDREGEX
#if ( defined( SYSV ) || defined( VMS ) || defined( netbsd ) || defined( freebsd ) || defined( linux )) && !defined(sgi)
#define NEED_BSDREGEX
#endif
#endif

/*
 * many systems do not have the setpwfile() library routine... we just
 * enable use for those systems we know have it.
 */
#ifndef HAVE_SETPWFILE
#if defined( sunos4 ) || defined( ultrix ) || defined( OSF1 )
#define HAVE_SETPWFILE
#endif
#endif

/*
 * Are sys_errlist and sys_nerr declared in stdio.h?
 */
#ifndef SYSERRLIST_IN_STDIO
#if defined( freebsd ) 
#define SYSERRLIST_IN_STDIO
#endif
#endif

/*
 * for select()
 */
#if !defined(WINSOCK) && !defined(_WINDOWS) && !defined(macintosh) && !defined(XP_OS2)
#if defined(hpux) || defined(LINUX2_0)
#include <sys/time.h>
#else
#include <sys/select.h>
#endif
#if !defined(FD_SET)
#define NFDBITS         32
#define FD_SETSIZE      32
#define FD_SET(n, p)    ((p)->fds_bits[(n)/NFDBITS] |= (1 << ((n) % NFDBITS)))
#define FD_CLR(n, p)    ((p)->fds_bits[(n)/NFDBITS] &= ~(1 << ((n) % NFDBITS)))
#define FD_ISSET(n, p)  ((p)->fds_bits[(n)/NFDBITS] & (1 << ((n) % NFDBITS)))
#define FD_ZERO(p)      bzero((char *)(p), sizeof(*(p)))
#endif /* !FD_SET */
#endif /* !WINSOCK && !_WINDOWS && !macintosh */


/*
 * for connect() -- must we block signals when calling connect()?  This
 * is necessary on some buggy UNIXes.
 */
#if !defined(LDAP_CONNECT_MUST_NOT_BE_INTERRUPTED) && \
	( defined(AIX) || defined(IRIX) || defined(HPUX) || defined(SUNOS4) \
	|| defined(SOLARIS))
#define LDAP_CONNECT_MUST_NOT_BE_INTERRUPTED
#endif


/*
 * for signal() -- what do signal handling functions return?
 */
#ifndef SIG_FN
#ifdef sunos5
#   define SIG_FN void          /* signal-catching functions return void */
#else /* sunos5 */
# ifdef BSD
#  if (BSD >= 199006) || defined(NeXT) || defined(OSF1) || defined(sun) || defined(ultrix) || defined(apollo) || defined(POSIX_SIGNALS)
#   define SIG_FN void          /* signal-catching functions return void */
#  else
#   define SIG_FN int           /* signal-catching functions return int */
#  endif
# else /* BSD */
#  define SIG_FN void           /* signal-catching functions return void */
# endif /* BSD */
#endif /* sunos5 */
#endif /* SIG_FN */

/*
 * toupper and tolower macros are different under bsd and sys v
 */
#if defined( SYSV ) && !defined( hpux )
#define TOUPPER(c)	(isascii(c) && islower(c) ? _toupper(c) : c)
#define TOLOWER(c)	(isascii(c) && isupper(c) ? _tolower(c) : c)
#else
#define TOUPPER(c)	(isascii(c) && islower(c) ? toupper(c) : c)
#define TOLOWER(c)	(isascii(c) && isupper(c) ? tolower(c) : c)
#endif

/*
 * put a cover on the tty-related ioctl calls we need to use
 */
#if defined( NeXT ) || (defined(SunOS) && SunOS < 40)
#define TERMIO_TYPE struct sgttyb
#define TERMFLAG_TYPE int
#define GETATTR( fd, tiop )	ioctl((fd), TIOCGETP, (caddr_t)(tiop))
#define SETATTR( fd, tiop )	ioctl((fd), TIOCSETP, (caddr_t)(tiop))
#define GETFLAGS( tio )		(tio).sg_flags
#define SETFLAGS( tio, flags )	(tio).sg_flags = (flags)
#else
#define USE_TERMIOS
#define TERMIO_TYPE struct termios
#define TERMFLAG_TYPE tcflag_t
#define GETATTR( fd, tiop )	tcgetattr((fd), (tiop))
#define SETATTR( fd, tiop )	tcsetattr((fd), TCSANOW /* 0 */, (tiop))
#define GETFLAGS( tio )		(tio).c_lflag
#define SETFLAGS( tio, flags )	(tio).c_lflag = (flags)
#endif

#if ( !defined( HPUX9 )) && ( !defined( sunos4 )) && ( !defined( SNI )) && \
	( !defined( HAVE_TIME_R ))
#define HAVE_TIME_R
#endif

#if defined( sunos5 ) || defined( aix )
#define HAVE_GETPWNAM_R
#define HAVE_GETGRNAM_R
#endif

#if defined(SNI) || defined(LINUX1_2)
int strcasecmp(const char *, const char *);
#ifdef SNI
int strncasecmp(const char *, const char *, int);
#endif /* SNI */
#ifdef LINUX1_2
int strncasecmp(const char *, const char *, size_t);
#endif /* LINUX1_2 */
#endif /* SNI || LINUX1_2 */

#if defined(_WINDOWS) || defined(macintosh) || defined(XP_OS2)
#define GETHOSTBYNAME( n, r, b, l, e )  gethostbyname( n )
#define CTIME( c, b, l )		ctime( c )
#define STRTOK( s1, s2, l )		strtok( s1, s2 )
#else /* UNIX */
#if defined(sgi) || defined(HPUX9) || defined(LINUX1_2) || defined(SCOOS) || \
    defined(UNIXWARE) || defined(SUNOS4) || defined(SNI) || defined(BSDI) || \
    defined(NCR) || defined(OSF1) || defined(NEC) || \
    ( defined(HPUX10) && !defined(_REENTRANT)) || defined(HPUX11) || \
    defined(UnixWare) || defined(LINUX2_0)
#define GETHOSTBYNAME( n, r, b, l, e )  gethostbyname( n )
#elif defined(AIX)
/* Maybe this is for another version of AIX?
   Commenting out for AIX 4.1 for Nova
   Replaced with following to lines, stolen from the #else below
#define GETHOSTBYNAME_BUF_T struct hostent_data
*/
typedef char GETHOSTBYNAME_buf_t [BUFSIZ /* XXX might be too small */];
#define GETHOSTBYNAME_BUF_T GETHOSTBYNAME_buf_t
#define GETHOSTBYNAME( n, r, b, l, e ) \
	(memset (&b, 0, l), gethostbyname_r (n, r, &b) ? NULL : r)
#elif defined(HPUX10)
#define GETHOSTBYNAME_BUF_T struct hostent_data
#define GETHOSTBYNAME( n, r, b, l, e )	nsldapi_compat_gethostbyname_r( n, r, (char *)&b, l, e )
#else
#include <stdio.h> /* BUFSIZ */
typedef char GETHOSTBYNAME_buf_t [BUFSIZ /* XXX might be too small */];
#define GETHOSTBYNAME_BUF_T GETHOSTBYNAME_buf_t
#define GETHOSTBYNAME( n, r, b, l, e )  gethostbyname_r( n, r, b, l, e )
#endif
#if defined(HPUX9) || defined(LINUX1_2) || defined(SUNOS4) || defined(SNI) || \
    defined(SCOOS) || defined(BSDI) || defined(NCR) || \
    defined(NEC) || ( defined(HPUX10) && !defined(_REENTRANT)) 
#define CTIME( c, b, l )		ctime( c )
#elif defined(HPUX10) && defined(_REENTRANT)
#define CTIME( c, b, l )		nsldapi_compat_ctime_r( c, b, l )
#elif defined( IRIX6_2 ) || defined( IRIX6_3 ) || defined(UNIXWARE) \
	|| defined(OSF1V4) || defined(AIX) || defined(UnixWare) || defined(hpux)
#define CTIME( c, b, l )                ctime_r( c, b )
#elif defined( OSF1V3 )
#define CTIME( c, b, l )		(ctime_r( c, b, l ) ? NULL : b)
#else
#define CTIME( c, b, l )		ctime_r( c, b, l )
#endif
#if defined(hpux9) || defined(LINUX1_2) || defined(SUNOS4) || defined(SNI) || \
    defined(SCOOS) || defined(BSDI) || defined(NCR) || \
    defined(NEC) || defined(LINUX2_0)
#define STRTOK( s1, s2, l )		strtok( s1, s2 )
#else
#define HAVE_STRTOK_R
char *strtok_r(char *, const char *, char **);
#define STRTOK( s1, s2, l )		(char *)strtok_r( s1, s2, l )
#endif /* STRTOK */
#endif /* UNIX */

#if defined( ultrix ) || defined( nextstep )
extern char *strdup();
#endif /* ultrix || nextstep */

#if defined( sunos4 ) || defined( OSF1 )
#define	BSD_TIME	1	/* for servers/slapd/log.h */
#endif /* sunos4 || osf */

#ifdef SOLARIS
#include <netinet/in.h>
#include <arpa/inet.h>	/* for inet_addr() */
#endif /* SOLARIS */

#ifdef SUNOS4
#include <pcfs/pc_dir.h>	/* for toupper() */
int fprintf(FILE *, char *, ...);
int fseek(FILE *, long, int);
int fread(char *, int, int, FILE *);
int fclose(FILE *);
int fflush(FILE *);
int rewind(FILE *);
void *memmove(void *, const void *, size_t);
int strcasecmp(char *, char *);
int strncasecmp(char *, char *, int);
time_t time(time_t *);
void perror(char *);
int fputc(char, FILE *);
int fputs(char *, FILE *);
int LDAP_CALL re_exec(char *);
int socket(int, int, int);
void bzero(char *, int);
unsigned long inet_addr(char *);
char * inet_ntoa(struct in_addr);
int getdtablesize();
int connect(int, struct sockaddr *, int);
#endif /* SUNOS4 */

/* #if defined(SUNOS4) || defined(SNI) */
#if defined(SUNOS4)
int select(int, fd_set *, fd_set *, fd_set *, struct timeval *);
#endif /* SUNOS4 || SNI */

/*
 * SAFEMEMCPY is an overlap-safe copy from s to d of n bytes
 */
#ifdef macintosh
#define SAFEMEMCPY( d, s, n )	BlockMoveData( (Ptr)s, (Ptr)d, n )
#else /* macintosh */
#ifdef sunos4
#define SAFEMEMCPY( d, s, n )	bcopy( s, d, n )
#else /* sunos4 */
#define SAFEMEMCPY( d, s, n )	memmove( d, s, n )
#endif /* sunos4 */
#endif /* macintosh */

#ifdef _WINDOWS

#define strcasecmp strcmpi
#define strncasecmp _strnicmp
#define bzero(a, b) memset( a, 0, b )
#define getpid _getpid
#define ioctl ioctlsocket
#define sleep(a) Sleep( a*1000 )

#define EMSGSIZE WSAEMSGSIZE
#define EWOULDBLOCK WSAEWOULDBLOCK
#define EHOSTUNREACH WSAEHOSTUNREACH

#ifndef MAXPATHLEN
#define MAXPATHLEN _MAX_PATH
#endif

#define DS_MAX_NT_SOCKET_CONNECTIONS 2000

#elif defined(XP_OS2)

#define strcasecmp strcmpi
#define strncasecmp strnicmp
#define bzero(a, b) memset( a, 0, b )
#include "dirent.h"
#include <string.h> /*for strcmpi()*/
#include <time.h>   /*for ctime()*/

#endif

#endif /* _PORTABLE_H */
