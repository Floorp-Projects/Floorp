/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 */
#ifndef _LDAPINT_H
#define _LDAPINT_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#ifdef hpux
#include <strings.h>
#endif /* hpux */

#ifdef _WINDOWS
#  if defined(FD_SETSIZE)
#  else
#    define FD_SETSIZE 256 /* number of connections we support */
#  endif
#  define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <portable.h>
#elif defined(macintosh)
#include "ldap-macos.h"
#elif defined(XP_OS2)
#include <os2sock.h>
#else /* _WINDOWS */
# include <sys/time.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <netdb.h>
#if !defined(hpux) && !defined(SUNOS4)
# include <sys/select.h>
#endif /* !defined(hpux) and others */
#endif /* _WINDOWS */

#if defined(IRIX)
#include <bstring.h>
#endif /* IRIX */

#define NSLBERI_LBER_INT_FRIEND
#ifdef macintosh
#include "lber-int.h"
#else /* macintosh */
#include "../liblber/lber-int.h"
#endif /* macintosh */

#include "ldap.h"
#include "ldaprot.h"
#include "ldaplog.h"
#include "portable.h"

#ifdef LDAP_ASYNC_IO
#ifdef NEED_FILIO
#include <sys/filio.h>		/* to get FIONBIO for ioctl() call */
#else /* NEED_FILIO */
#if !defined( _WINDOWS) && !defined (macintosh)
#include <sys/ioctl.h>		/* to get FIONBIO for ioctl() call */
#endif /* _WINDOWS && macintosh */
#endif /* NEED_FILIO */
#endif /* LDAP_ASYNC_IO */

#ifdef USE_SYSCONF
#  include <unistd.h>
#endif /* USE_SYSCONF */

#ifdef LDAP_SASLIO_HOOKS
#include <sasl.h>
#define SASL_MAX_BUFF_SIZE	65536
#define SASL_MIN_BUFF_SIZE	4096
#endif

#if !defined(_WINDOWS) && !defined(macintosh) && !defined(BSDI)
#define NSLDAPI_HAVE_POLL	1
#endif

/* SSL version, or 0 if not built with SSL */
#if defined(NET_SSL)
#  define SSL_VERSION 3
#else
#  define SSL_VERSION 0
#endif


#define LDAP_URL_URLCOLON	"URL:"
#define LDAP_URL_URLCOLON_LEN	4

#define LDAP_LDAP_REF_STR	LDAP_URL_PREFIX
#define LDAP_LDAP_REF_STR_LEN	LDAP_URL_PREFIX_LEN
#define LDAP_LDAPS_REF_STR	LDAPS_URL_PREFIX
#define LDAP_LDAPS_REF_STR_LEN	LDAPS_URL_PREFIX_LEN

/* default limit on nesting of referrals */
#define LDAP_DEFAULT_REFHOPLIMIT	5
#ifdef LDAP_DNS
#define LDAP_DX_REF_STR		"dx://"
#define LDAP_DX_REF_STR_LEN	5
#endif /* LDAP_DNS */

typedef enum { 
    LDAP_CACHE_LOCK, 
    LDAP_MEMCACHE_LOCK, 
    LDAP_MSGID_LOCK,
    LDAP_REQ_LOCK, 
    LDAP_RESP_LOCK, 
    LDAP_ABANDON_LOCK, 
    LDAP_CTRL_LOCK,
    LDAP_OPTION_LOCK, 
    LDAP_ERR_LOCK, 
    LDAP_CONN_LOCK, 
    LDAP_IOSTATUS_LOCK,		/* serializes access to ld->ld_iostatus */
    LDAP_RESULT_LOCK, 
    LDAP_PEND_LOCK, 
    LDAP_THREADID_LOCK, 
#ifdef LDAP_SASLIO_HOOKS
    LDAP_SASL_LOCK, 
#endif
    LDAP_MAX_LOCK 
} LDAPLock;

/*
 * This structure represents both ldap messages and ldap responses.
 * These are really the same, except in the case of search responses,
 * where a response has multiple messages.
 */

struct ldapmsg {
	int		lm_msgid;	/* the message id */
	int		lm_msgtype;	/* the message type */
	BerElement	*lm_ber;	/* the ber encoded message contents */
	struct ldapmsg	*lm_chain;	/* for search - next msg in the resp */
	struct ldapmsg	*lm_next;	/* next response */
	int		lm_fromcache;	/* memcache: origin of message */
};

/*
 * structure for tracking LDAP server host, ports, DNs, etc.
 */
typedef struct ldap_server {
	char			*lsrv_host;
	char			*lsrv_dn;	/* if NULL, use default */
	int			lsrv_port;
	unsigned long		lsrv_options;	/* boolean options */
#define LDAP_SRV_OPT_SECURE	0x01
	struct ldap_server	*lsrv_next;
} LDAPServer;

/*
 * structure for representing an LDAP server connection
 */
typedef struct ldap_conn {
	Sockbuf			*lconn_sb;
	BerElement		*lconn_ber;  /* non-NULL if in midst of msg. */
	int			lconn_version;	/* LDAP protocol version */
	int			lconn_refcnt;
	unsigned long		lconn_lastused;	/* time */
	int			lconn_status;
#define LDAP_CONNST_NEEDSOCKET		1
#define LDAP_CONNST_CONNECTING		2
#define LDAP_CONNST_CONNECTED		3
#define LDAP_CONNST_DEAD		4
	LDAPServer		*lconn_server;
	char			*lconn_binddn;	/* DN of last successful bind */
	int			lconn_bound;	/* has a bind been done? */
	char			*lconn_krbinstance;
	struct ldap_conn	*lconn_next;
} LDAPConn;


/*
 * structure used to track outstanding requests
 */
typedef struct ldapreq {
	int		lr_msgid;	/* the message id */
	int		lr_status;	/* status of request */
#define LDAP_REQST_INPROGRESS	1
#define LDAP_REQST_CHASINGREFS	2
#define LDAP_REQST_NOTCONNECTED	3
#define LDAP_REQST_WRITING	4
#define LDAP_REQST_CONNDEAD	5	/* associated conn. has failed */
	int		lr_outrefcnt;	/* count of outstanding referrals */
	int		lr_origid;	/* original request's message id */
	int		lr_parentcnt;	/* count of parent requests */
	int		lr_res_msgtype;	/* result message type */
	int		lr_res_errno;	/* result LDAP errno */
	char		*lr_res_error;	/* result error string */
	char		*lr_res_matched;/* result matched DN string */
	BerElement	*lr_ber;	/* ber encoded request contents */
	LDAPConn	*lr_conn;	/* connection used to send request */
	char		*lr_binddn;	/* request is a bind for this DN */
	struct ldapreq	*lr_parent;	/* request that spawned this referral */
	struct ldapreq	*lr_child;	/* list of requests we spawned */
	struct ldapreq	*lr_sibling;	/* next referral spawned */
	struct ldapreq	*lr_prev;	/* ld->ld_requests previous request */
	struct ldapreq	*lr_next;	/* ld->ld_requests next request */
} LDAPRequest;

typedef struct ldappend {
	void		*lp_sema;	/* semaphore to post */
	int		lp_msgid;	/* message id */
	LDAPMessage	*lp_result;	/* result storage */
	struct ldappend	*lp_prev;	/* previous pending */
	struct ldappend	*lp_next;	/* next pending */
} LDAPPend;

/*
 * forward declaration for I/O status structure (defined in os-ip.c)
 */
typedef struct nsldapi_iostatus_info NSLDAPIIOStatus;

/*
 * old extended IO structure (before writev callback was added)
 */
struct ldap_x_ext_io_fns_rev0 {
        int                                     lextiof_size;
        LDAP_X_EXTIOF_CONNECT_CALLBACK          *lextiof_connect;
        LDAP_X_EXTIOF_CLOSE_CALLBACK            *lextiof_close;
        LDAP_X_EXTIOF_READ_CALLBACK             *lextiof_read;
        LDAP_X_EXTIOF_WRITE_CALLBACK            *lextiof_write;
        LDAP_X_EXTIOF_POLL_CALLBACK             *lextiof_poll;
        LDAP_X_EXTIOF_NEWHANDLE_CALLBACK        *lextiof_newhandle;
        LDAP_X_EXTIOF_DISPOSEHANDLE_CALLBACK    *lextiof_disposehandle;
        void                                    *lextiof_session_arg;
};
#define LDAP_X_EXTIO_FNS_SIZE_REV0   sizeof(struct ldap_x_ext_io_fns_rev0)


/*
 * structure representing an ldap connection
 */
struct ldap {
	struct sockbuf	*ld_sbp;	/* pointer to socket desc. & buffer */
	char		*ld_host;
	int		ld_version;	/* LDAP protocol version */
	char		ld_lberoptions;
	int		ld_deref;

	int		ld_timelimit;
	int		ld_sizelimit;

	struct ldap_filt_desc	*ld_filtd;	/* from getfilter for ufn searches */
	char		*ld_ufnprefix;	/* for incomplete ufn's */

	int		ld_errno;
	char		*ld_error;
	char		*ld_matched;
	int		ld_msgid;

	/* do not mess with these */
	LDAPRequest	*ld_requests;	/* list of outstanding requests */
	LDAPMessage	*ld_responses;	/* list of outstanding responses */
	int		*ld_abandoned;	/* array of abandoned requests */
	char		*ld_cldapdn;	/* DN used in connectionless search */

	/* it is OK to change these next four values directly */
	int		ld_cldaptries;	/* connectionless search retry count */
	int		ld_cldaptimeout;/* time between retries */
	int		ld_refhoplimit;	/* limit on referral nesting */
	unsigned long	ld_options;	/* boolean options */

#define LDAP_BITOPT_REFERRALS	0x80000000
#define LDAP_BITOPT_SSL		0x40000000
#define LDAP_BITOPT_DNS		0x20000000
#define LDAP_BITOPT_RESTART	0x10000000
#define LDAP_BITOPT_RECONNECT	0x08000000
#define LDAP_BITOPT_ASYNC       0x04000000
#define LDAP_BITOPT_NOREBIND    0x02000000

	/* do not mess with the rest though */
	char		*ld_defhost;	/* full name of default server */
	int		ld_defport;	/* port of default server */
	BERTranslateProc ld_lber_encode_translate_proc;
	BERTranslateProc ld_lber_decode_translate_proc;
	LDAPConn	*ld_defconn;	/* default connection */
	LDAPConn	*ld_conns;	/* list of all server connections */
	NSLDAPIIOStatus	*ld_iostatus;	/* status info. about network sockets */
	LDAP_REBINDPROC_CALLBACK *ld_rebind_fn;
	void		*ld_rebind_arg;

	/* function pointers, etc. for extended I/O */
	struct ldap_x_ext_io_fns ld_ext_io_fns;
#define ld_extio_size		ld_ext_io_fns.lextiof_size
#define ld_extclose_fn		ld_ext_io_fns.lextiof_close
#define ld_extconnect_fn	ld_ext_io_fns.lextiof_connect
#define ld_extread_fn		ld_ext_io_fns.lextiof_read
#define ld_extwrite_fn		ld_ext_io_fns.lextiof_write
#define ld_extwritev_fn         ld_ext_io_fns.lextiof_writev
#define ld_extpoll_fn		ld_ext_io_fns.lextiof_poll
#define ld_extnewhandle_fn	ld_ext_io_fns.lextiof_newhandle
#define ld_extdisposehandle_fn	ld_ext_io_fns.lextiof_disposehandle
#define ld_ext_session_arg	ld_ext_io_fns.lextiof_session_arg

	/* allocated pointer for older I/O functions */
	struct ldap_io_fns	*ld_io_fns_ptr;
#define NSLDAPI_USING_CLASSIC_IO_FUNCTIONS( ld ) ((ld)->ld_io_fns_ptr != NULL)

	/* function pointers, etc. for DNS */
	struct ldap_dns_fns	ld_dnsfn;
#define ld_dns_extradata	ld_dnsfn.lddnsfn_extradata
#define ld_dns_bufsize		ld_dnsfn.lddnsfn_bufsize
#define ld_dns_gethostbyname_fn	ld_dnsfn.lddnsfn_gethostbyname
#define ld_dns_gethostbyaddr_fn	ld_dnsfn.lddnsfn_gethostbyaddr
#define ld_dns_getpeername_fn	ld_dnsfn.lddnsfn_getpeername

	/* function pointers, etc. for threading */
	struct ldap_thread_fns	ld_thread;
#define ld_mutex_alloc_fn	ld_thread.ltf_mutex_alloc
#define ld_mutex_free_fn	ld_thread.ltf_mutex_free
#define ld_mutex_lock_fn	ld_thread.ltf_mutex_lock
#define ld_mutex_unlock_fn	ld_thread.ltf_mutex_unlock
#define ld_get_errno_fn		ld_thread.ltf_get_errno
#define ld_set_errno_fn		ld_thread.ltf_set_errno
#define ld_get_lderrno_fn	ld_thread.ltf_get_lderrno
#define ld_set_lderrno_fn	ld_thread.ltf_set_lderrno
#define ld_lderrno_arg		ld_thread.ltf_lderrno_arg
	void			**ld_mutex;

	/* function pointers, etc. for caching */
	int			ld_cache_on;
	int			ld_cache_strategy;
	struct ldap_cache_fns	ld_cache;
#define ld_cache_config		ld_cache.lcf_config
#define ld_cache_bind		ld_cache.lcf_bind
#define ld_cache_unbind		ld_cache.lcf_unbind
#define ld_cache_search		ld_cache.lcf_search
#define ld_cache_compare	ld_cache.lcf_compare
#define ld_cache_add		ld_cache.lcf_add
#define ld_cache_delete		ld_cache.lcf_delete
#if 0
#define ld_cache_rename		ld_cache.lcf_rename
#endif
#define ld_cache_modify		ld_cache.lcf_modify
#define ld_cache_modrdn		ld_cache.lcf_modrdn
#define ld_cache_abandon	ld_cache.lcf_abandon
#define ld_cache_result		ld_cache.lcf_result
#define ld_cache_flush		ld_cache.lcf_flush
#define ld_cache_arg		ld_cache.lcf_arg

	/* ldapv3 controls */
	LDAPControl		**ld_servercontrols;
	LDAPControl		**ld_clientcontrols;

	/* Preferred language */
	char            *ld_preferred_language;

	/* MemCache */
	LDAPMemCache	*ld_memcache;

	/* Pending results */
	LDAPPend	*ld_pend;	/* list of pending results */

	/* extra thread function pointers */
	struct ldap_extra_thread_fns	ld_thread2;

	/* With the 4.0 version of the LDAP SDK */
	/* the extra thread functions except for */
	/* the ld_threadid_fn has been disabled */
	/* Look at the release notes for the full */
	/* explanation */
#define ld_mutex_trylock_fn		ld_thread2.ltf_mutex_trylock
#define ld_sema_alloc_fn		ld_thread2.ltf_sema_alloc
#define ld_sema_free_fn			ld_thread2.ltf_sema_free
#define ld_sema_wait_fn			ld_thread2.ltf_sema_wait
#define ld_sema_post_fn			ld_thread2.ltf_sema_post
#define ld_threadid_fn			ld_thread2.ltf_threadid_fn

	/* extra data for mutex handling in referrals */
	void 			*ld_mutex_threadid[LDAP_MAX_LOCK];
	unsigned long		ld_mutex_refcnt[LDAP_MAX_LOCK];

	/* connect timeout value (milliseconds) */
	int				ld_connect_timeout;

#ifdef LDAP_SASLIO_HOOKS
	/* SASL default option settings */
	char			*ld_def_sasl_mech;
	char			*ld_def_sasl_realm;
	char			*ld_def_sasl_authcid;
	char			*ld_def_sasl_authzid;
	/* SASL Security properties */
	struct sasl_security_properties ld_sasl_secprops;
	/* prldap shadow io functions */
	struct ldap_x_ext_io_fns ld_sasl_io_fns;
#endif
};

/* allocate/free mutex */
#define LDAP_MUTEX_ALLOC( ld ) \
	(((ld)->ld_mutex_alloc_fn != NULL) ? (ld)->ld_mutex_alloc_fn() : NULL)

/* allocate/free mutex */
#define LDAP_MUTEX_FREE( ld, m ) \
	if ( (ld)->ld_mutex_free_fn != NULL && m != NULL ) { \
		(ld)->ld_mutex_free_fn( m ); \
	}

/* enter/exit critical sections */
/*
 * The locks assume that the locks are thread safe.  XXXmcs: which means???
 *
 * Note that we test for both ld_mutex_lock_fn != NULL AND ld_mutex != NULL.
 * This is necessary because there is a window in ldap_init() between the
 * time we set the ld_mutex_lock_fn pointer and the time we allocate the
 * mutexes in which external code COULD be called which COULD make a call to
 * something like ldap_get_option(), which uses LDAP_MUTEX_LOCK().  The
 * libprldap code does this in its newhandle callback (prldap_newhandle).
 */

#define LDAP_MUTEX_LOCK(ld, lock) \
    if ((ld)->ld_mutex_lock_fn != NULL && ld->ld_mutex != NULL) { \
        if ((ld)->ld_threadid_fn != NULL) { \
            if ((ld)->ld_mutex_threadid[lock] == (ld)->ld_threadid_fn()) { \
                (ld)->ld_mutex_refcnt[lock]++; \
            } else { \
                (ld)->ld_mutex_lock_fn(ld->ld_mutex[lock]); \
                (ld)->ld_mutex_threadid[lock] = ld->ld_threadid_fn(); \
                (ld)->ld_mutex_refcnt[lock] = 1; \
            } \
        } else { \
            (ld)->ld_mutex_lock_fn(ld->ld_mutex[lock]); \
        } \
    } 

#define LDAP_MUTEX_UNLOCK(ld, lock) \
    if ((ld)->ld_mutex_lock_fn != NULL && ld->ld_mutex != NULL) { \
        if ((ld)->ld_threadid_fn != NULL) { \
            if ((ld)->ld_mutex_threadid[lock] == (ld)->ld_threadid_fn()) { \
                (ld)->ld_mutex_refcnt[lock]--; \
                if ((ld)->ld_mutex_refcnt[lock] <= 0) { \
                    (ld)->ld_mutex_threadid[lock] = (void *) -1; \
                    (ld)->ld_mutex_refcnt[lock] = 0; \
                    (ld)->ld_mutex_unlock_fn(ld->ld_mutex[lock]); \
                } \
            } \
        } else { \
            ld->ld_mutex_unlock_fn(ld->ld_mutex[lock]); \
        } \
    }

/* Backward compatibility locks */
#define LDAP_MUTEX_BC_LOCK( ld, i ) \
	/* the ld_mutex_trylock_fn is always set to NULL */ \
	/* in setoption.c as the extra thread functions were */ \
	/* turned off in the 4.0 SDK.  This check will  */ \
	/* always be true */ \
	if( (ld)->ld_mutex_trylock_fn == NULL ) { \
		LDAP_MUTEX_LOCK( ld, i ) ; \
	}
#define LDAP_MUTEX_BC_UNLOCK( ld, i ) \
	/* the ld_mutex_trylock_fn is always set to NULL */ \
	/* in setoption.c as the extra thread functions were */ \
	/* turned off in the 4.0 SDK.  This check will  */ \
	/* always be true */ \
	if( (ld)->ld_mutex_trylock_fn == NULL ) { \
		LDAP_MUTEX_UNLOCK( ld, i ) ; \
	}

/* allocate/free semaphore */
#define LDAP_SEMA_ALLOC( ld ) \
	(((ld)->ld_sema_alloc_fn != NULL) ? (ld)->ld_sema_alloc_fn() : NULL)
#define LDAP_SEMA_FREE( ld, m ) \
	if ( (ld)->ld_sema_free_fn != NULL && m != NULL ) { \
		(ld)->ld_sema_free_fn( m ); \
	}

/* wait/post binary semaphore */
#define LDAP_SEMA_WAIT( ld, lp ) \
	if ( (ld)->ld_sema_wait_fn != NULL ) { \
		(ld)->ld_sema_wait_fn( lp->lp_sema ); \
	}
#define LDAP_SEMA_POST( ld, lp ) \
	if ( (ld)->ld_sema_post_fn != NULL ) { \
		(ld)->ld_sema_post_fn( lp->lp_sema ); \
	}
#define POST( ld, y, z ) \
	/* the ld_mutex_trylock_fn is always set to NULL */ \
	/* in setoption.c as the extra thread functions were */ \
	/* turned off in the 4.0 SDK.  This check will  */ \
	/* always be false */ \
	if( (ld)->ld_mutex_trylock_fn != NULL ) { \
		nsldapi_post_result( ld, y, z ); \
	}

/* get/set errno */
#ifndef macintosh
#define LDAP_SET_ERRNO( ld, e ) \
	if ( (ld)->ld_set_errno_fn != NULL ) { \
		(ld)->ld_set_errno_fn( e ); \
	} else { \
		errno = e; \
	}
#define LDAP_GET_ERRNO( ld ) \
	(((ld)->ld_get_errno_fn != NULL) ? \
		(ld)->ld_get_errno_fn() : errno)
#else /* macintosh */
#define LDAP_SET_ERRNO( ld, e ) \
	if ( (ld)->ld_set_errno_fn != NULL ) { \
		(ld)->ld_set_errno_fn( e ); \
	}
#define LDAP_GET_ERRNO( ld ) \
	(((ld)->ld_get_errno_fn != NULL) ? \
		(ld)->ld_get_errno_fn() : 0)
#endif


/* get/set ldap-specific errno */
#define LDAP_SET_LDERRNO( ld, e, m, s )	ldap_set_lderrno( ld, e, m, s )
#define LDAP_GET_LDERRNO( ld, m, s ) ldap_get_lderrno( ld, m, s )

/*
 * your standard "mimimum of two values" macro
 */
#define NSLDAPI_MIN(a, b)	(((a) < (b)) ? (a) : (b))

/*
 * handy macro to check whether LDAP struct is set up for CLDAP or not
 */
#define LDAP_IS_CLDAP( ld )	( ld->ld_sbp->sb_naddr > 0 )

/*
 * handy macro to check errno "e" for an "in progress" sort of error
 */
#if defined(macintosh) || defined(_WINDOWS)
#define NSLDAPI_ERRNO_IO_INPROGRESS( e )  ((e) == EWOULDBLOCK || (e) == EAGAIN)
#else
#ifdef EAGAIN
#define NSLDAPI_ERRNO_IO_INPROGRESS( e )  ((e) == EWOULDBLOCK || (e) == EINPROGRESS || (e) == EAGAIN)
#else /* EAGAIN */
#define NSLDAPI_ERRNO_IO_INPROGRESS( e )  ((e) == EWOULDBLOCK || (e) == EINPROGRESS) 
#endif /* EAGAIN */
#endif /* macintosh || _WINDOWS*/

/*
 * macro to return the LDAP protocol version we are using
 */
#define NSLDAPI_LDAP_VERSION( ld )	( (ld)->ld_defconn == NULL ? \
					(ld)->ld_version : \
					(ld)->ld_defconn->lconn_version )

/*
 * Structures used for handling client filter lists.
 */
#define LDAP_FILT_MAXSIZ	1024

struct ldap_filt_list {
    char			*lfl_tag;
    char			*lfl_pattern;
    char			*lfl_delims;
    struct ldap_filt_info	*lfl_ilist;
    struct ldap_filt_list	*lfl_next;
};

struct ldap_filt_desc {
	LDAPFiltList		*lfd_filtlist;
	LDAPFiltInfo		*lfd_curfip;
	LDAPFiltInfo		lfd_retfi;
	char			lfd_filter[ LDAP_FILT_MAXSIZ ];
	char			*lfd_curval;
	char			*lfd_curvalcopy;
	char			**lfd_curvalwords;
	char			*lfd_filtprefix;
	char			*lfd_filtsuffix;
};

/*
 * "internal" globals used to track defaults and memory allocation callbacks:
 *    (the actual definitions are in open.c)
 */
extern struct ldap			nsldapi_ld_defaults;
extern struct ldap_memalloc_fns		nsldapi_memalloc_fns;
extern int				nsldapi_initialized;


/*
 * Memory allocation done in liblber should all go through one of the
 * following macros. This is so we can plug-in alternative memory
 * allocators, etc. as the need arises.
 */
#define NSLDAPI_MALLOC( size )		ldap_x_malloc( size )
#define NSLDAPI_CALLOC( nelem, elsize )	ldap_x_calloc( nelem, elsize )
#define NSLDAPI_REALLOC( ptr, size )	ldap_x_realloc( ptr, size )
#define NSLDAPI_FREE( ptr )		ldap_x_free( ptr )


/*
 * macros used to check validity of data structures and parameters
 */
#define NSLDAPI_VALID_LDAP_POINTER( ld ) \
	( (ld) != NULL )

#define NSLDAPI_VALID_LDAPMESSAGE_POINTER( lm ) \
	( (lm) != NULL )

#define NSLDAPI_VALID_LDAPMESSAGE_ENTRY_POINTER( lm ) \
	( (lm) != NULL && (lm)->lm_msgtype == LDAP_RES_SEARCH_ENTRY )

#define NSLDAPI_VALID_LDAPMESSAGE_REFERENCE_POINTER( lm ) \
	( (lm) != NULL && (lm)->lm_msgtype == LDAP_RES_SEARCH_REFERENCE )

#define NSLDAPI_VALID_LDAPMESSAGE_BINDRESULT_POINTER( lm ) \
	( (lm) != NULL && (lm)->lm_msgtype == LDAP_RES_BIND )

#define NSLDAPI_VALID_LDAPMESSAGE_EXRESULT_POINTER( lm ) \
	( (lm) != NULL && (lm)->lm_msgtype == LDAP_RES_EXTENDED )

#define NSLDAPI_VALID_LDAPMOD_ARRAY( mods ) \
	( (mods) != NULL )

#define NSLDAPI_VALID_NONEMPTY_LDAPMOD_ARRAY( mods ) \
	( (mods) != NULL && (mods)[0] != NULL )

#define NSLDAPI_IS_SEARCH_ENTRY( code ) \
	((code) == LDAP_RES_SEARCH_ENTRY)

#define NSLDAPI_IS_SEARCH_RESULT( code ) \
	((code) == LDAP_RES_SEARCH_RESULT)

#define NSLDAPI_SEARCH_RELATED_RESULT( code ) \
	(NSLDAPI_IS_SEARCH_RESULT( code ) || NSLDAPI_IS_SEARCH_ENTRY( code ))

/*
 * in bind.c
 */
char *nsldapi_get_binddn( LDAP *ld );

/*
 * in cache.c
 */
void nsldapi_add_result_to_cache( LDAP *ld, LDAPMessage *result );

/*
 * in dsparse.c
 */
int ldap_next_line_tokens( char **bufp, long *blenp, char ***toksp );
void ldap_free_strarray( char **sap );

/*
 * in error.c
 */
int nsldapi_parse_result( LDAP *ld, int msgtype, BerElement *rber,
    int *errcodep, char **matchednp, char **errmsgp, char ***referralsp,
    LDAPControl ***serverctrlsp );

/*
 * in open.c
 */
void nsldapi_initialize_defaults( void );
void nsldapi_mutex_alloc_all( LDAP *ld );
void nsldapi_mutex_free_all( LDAP *ld );
int nsldapi_open_ldap_defconn( LDAP *ld );
char *nsldapi_strdup( const char *s );  /* if s is NULL, returns NULL */

/*
 * in os-ip.c
 */
int nsldapi_connect_to_host( LDAP *ld, Sockbuf *sb, const char *host,
	int port, int secure, char **krbinstancep );
void nsldapi_close_connection( LDAP *ld, Sockbuf *sb );

int nsldapi_iostatus_poll( LDAP *ld, struct timeval *timeout );
void nsldapi_iostatus_free( LDAP *ld );
int nsldapi_iostatus_interest_write( LDAP *ld, Sockbuf *sb );
int nsldapi_iostatus_interest_read( LDAP *ld, Sockbuf *sb );
int nsldapi_iostatus_interest_clear( LDAP *ld, Sockbuf *sb );
int nsldapi_iostatus_is_read_ready( LDAP *ld, Sockbuf *sb );
int nsldapi_iostatus_is_write_ready( LDAP *ld, Sockbuf *sb );
int nsldapi_install_lber_extiofns( LDAP *ld, Sockbuf *sb );
int nsldapi_install_compat_io_fns( LDAP *ld, struct ldap_io_fns *iofns );

/*
 * if referral.c
 */
int nsldapi_parse_reference( LDAP *ld, BerElement *rber, char ***referralsp,
	LDAPControl ***serverctrlsp );


/*
 * in result.c
 */
int ldap_msgdelete( LDAP *ld, int msgid );
int nsldapi_result_nolock( LDAP *ld, int msgid, int all, int unlock_permitted,
    struct timeval *timeout, LDAPMessage **result );
int nsldapi_wait_result( LDAP *ld, int msgid, int all, struct timeval *timeout,
    LDAPMessage **result );
int nsldapi_post_result( LDAP *ld, int msgid, LDAPMessage *result );

/*
 * in request.c
 */
int nsldapi_send_initial_request( LDAP *ld, int msgid, unsigned long msgtype,
	char *dn, BerElement *ber );
int nsldapi_alloc_ber_with_options( LDAP *ld, BerElement **berp );
void nsldapi_set_ber_options( LDAP *ld, BerElement *ber );
int nsldapi_ber_flush( LDAP *ld, Sockbuf *sb, BerElement *ber, int freeit,
	int async, int epipe_handler );
int nsldapi_send_server_request( LDAP *ld, BerElement *ber, int msgid,
	LDAPRequest *parentreq, LDAPServer *srvlist, LDAPConn *lc,
	char *bindreqdn, int bind );
LDAPConn *nsldapi_new_connection( LDAP *ld, LDAPServer **srvlistp, int use_ldsb,
	int connect, int bind );
LDAPRequest *nsldapi_find_request_by_msgid( LDAP *ld, int msgid );
void nsldapi_free_request( LDAP *ld, LDAPRequest *lr, int free_conn );
void nsldapi_free_connection( LDAP *ld, LDAPConn *lc,
	LDAPControl **serverctrls, LDAPControl **clientctrls,
	int force, int unbind );
void nsldapi_dump_connection( LDAP *ld, LDAPConn *lconns, int all );
void nsldapi_dump_requests_and_responses( LDAP *ld );
int nsldapi_chase_v2_referrals( LDAP *ld, LDAPRequest *lr, char **errstrp,
	int *totalcountp, int *chasingcountp );
int nsldapi_chase_v3_refs( LDAP *ld, LDAPRequest *lr, char **refs,
	int is_reference, int *totalcountp, int *chasingcountp );
int nsldapi_append_referral( LDAP *ld, char **referralsp, char *s );
void nsldapi_connection_lost_nolock( LDAP *ld, Sockbuf *sb );

/*
 * in search.c
 */
int nsldapi_build_search_req( LDAP *ld, const char *base, int scope,
	const char *filter, char **attrs, int attrsonly,
	LDAPControl **serverctrls, LDAPControl **clientctrls,
	int timelimit, int sizelimit, int msgid, BerElement **berp );

int ldap_put_filter( BerElement *ber, char *str );

/*
 * in unbind.c
 */
int ldap_ld_free( LDAP *ld, LDAPControl **serverctrls,
	LDAPControl **clientctrls, int close );
int nsldapi_send_unbind( LDAP *ld, Sockbuf *sb, LDAPControl **serverctrls,
	LDAPControl **clientctrls );

#ifdef LDAP_DNS
/*
 * in getdxbyname.c
 */
char **nsldapi_getdxbyname( char *domain );

#endif /* LDAP_DNS */

/*
 * in unescape.c
 */
void nsldapi_hex_unescape( char *s );

/*
 * in reslist.c
 */
LDAPMessage *ldap_delete_result_entry( LDAPMessage **list, LDAPMessage *e );
void ldap_add_result_entry( LDAPMessage **list, LDAPMessage *e );

/*
 * in compat.c
 */
#ifdef hpux
char *nsldapi_compat_ctime_r( const time_t *clock, char *buf, int buflen );
struct hostent *nsldapi_compat_gethostbyname_r( const char *name,
	struct hostent *result, char *buffer, int buflen, int *h_errnop );
#endif /* hpux */

/*
 * in control.c
 */
int nsldapi_put_controls( LDAP *ld, LDAPControl **ctrls, int closeseq,
	BerElement *ber );
int nsldapi_get_controls( BerElement *ber, LDAPControl ***controlsp );
int nsldapi_dup_controls( LDAP *ld, LDAPControl ***ldctrls,
	LDAPControl **newctrls );
int nsldapi_build_control( char *oid, BerElement *ber, int freeber,
    char iscritical, LDAPControl **ctrlp );


/*
 * in url.c
 */
int nsldapi_url_parse( const char *inurl, LDAPURLDesc **ludpp,
	int dn_required );


/*
 * in charset.c
 *
 * If we ever want to expose character set translation functionality to
 * users of libldap, all of these prototypes will need to be moved to ldap.h
 */
#ifdef STR_TRANSLATION
void ldap_set_string_translators( LDAP *ld,
        BERTranslateProc encode_proc, BERTranslateProc decode_proc );
int ldap_translate_from_t61( LDAP *ld, char **bufp,
        unsigned long *lenp, int free_input );
int ldap_translate_to_t61( LDAP *ld, char **bufp,
        unsigned long *lenp, int free_input );
void ldap_enable_translation( LDAP *ld, LDAPMessage *entry,
        int enable );
#ifdef LDAP_CHARSET_8859
int ldap_t61_to_8859( char **bufp, unsigned long *buflenp,
        int free_input );
int ldap_8859_to_t61( char **bufp, unsigned long *buflenp,
        int free_input );
#endif /* LDAP_CHARSET_8859 */
#endif /* STR_TRANSLATION */

/*
 * in memcache.h
 */
int ldap_memcache_createkey( LDAP *ld, const char *base, int scope,
	const char *filter, char **attrs, int attrsonly,
	LDAPControl **serverctrls, LDAPControl **clientctrls,
	unsigned long *keyp );
int ldap_memcache_result( LDAP *ld, int msgid, unsigned long key );
int ldap_memcache_new( LDAP *ld, int msgid, unsigned long key,
	const char *basedn );
int ldap_memcache_append( LDAP *ld, int msgid, int bLast, LDAPMessage *result );
int ldap_memcache_abandon( LDAP *ld, int msgid );

/*
 * in sbind.c
 */
void nsldapi_handle_reconnect( LDAP *ld );

#endif /* _LDAPINT_H */
