/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef _LDAPINT_H
#define _LDAPINT_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#ifdef hpux
#include <strings.h>
#include <time.h>
#endif /* hpux */

#ifdef _WINDOWS
#  define FD_SETSIZE		256	/* number of connections we support */
#  define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <time.h>
#elif defined(macintosh)
#include "ldap-macos.h"
# include <time.h>
#elif defined(XP_OS2)
#include <os2sock.h>
#else /* _WINDOWS */
# include <sys/time.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <netdb.h>
#if !defined(hpux) && !defined(SUNOS4) && !defined(LINUX1_2) && !defined(LINUX2_0)
# include <sys/select.h>
#endif /* !defined(hpux) and others */
#endif /* _WINDOWS */

#if defined(BSDI) || defined(LINUX1_2) || defined(SNI) || defined(IRIX)
#include <arpa/inet.h>
#endif /* BSDI */

#if defined(IRIX)
#include <bstring.h>
#endif /* IRIX */

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
#include <sys/ioctl.h>		/* to get FIONBIO for ioctl() call */
#endif /* NEED_FILIO */
#endif /* LDAP_ASYNC_IO */

#ifdef USE_SYSCONF
#  include <unistd.h>
#endif /* USE_SYSCONF */

#if !defined(_WINDOWS) && !defined(macintosh) && !defined(LINUX2_0)
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

typedef enum { LDAP_CACHE_LOCK, LDAP_MEMCACHE_LOCK, LDAP_MSGID_LOCK,
LDAP_REQ_LOCK, LDAP_RESP_LOCK, LDAP_ABANDON_LOCK, LDAP_CTRL_LOCK,
LDAP_OPTION_LOCK, LDAP_ERR_LOCK, LDAP_CONN_LOCK, LDAP_SELECT_LOCK,
LDAP_RESULT_LOCK, LDAP_PEND_LOCK, LDAP_MAX_LOCK } LDAPLock;

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
	struct ldapreq	*lr_refnext;	/* next referral spawned */
	struct ldapreq	*lr_prev;	/* previous request */
	struct ldapreq	*lr_next;	/* next request */
} LDAPRequest;

typedef struct ldappend {
	void		*lp_sema;	/* semaphore to post */
	int		lp_msgid;	/* message id */
	LDAPMessage	*lp_result;	/* result storage */
	struct ldappend	*lp_prev;	/* previous pending */
	struct ldappend	*lp_next;	/* next pending */
} LDAPPend;

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

	/* do not mess with the rest though */
	char		*ld_defhost;	/* full name of default server */
	int		ld_defport;	/* port of default server */
	BERTranslateProc ld_lber_encode_translate_proc;
	BERTranslateProc ld_lber_decode_translate_proc;
	LDAPConn	*ld_defconn;	/* default connection */
	LDAPConn	*ld_conns;	/* list of all server connections */
	void		*ld_selectinfo;	/* platform specifics for select */
	int		ld_selectreadcnt;  /* count of read sockets */
	int		ld_selectwritecnt; /* count of write sockets */
	LDAP_REBINDPROC_CALLBACK *ld_rebind_fn;
	void		*ld_rebind_arg;

	/* function pointers, etc. for io */
	struct ldap_io_fns	ld_io;
#define ld_read_fn		ld_io.liof_read
#define ld_write_fn		ld_io.liof_write
#define ld_select_fn		ld_io.liof_select
#define ld_socket_fn		ld_io.liof_socket
#define ld_ioctl_fn		ld_io.liof_ioctl
#define ld_connect_fn		ld_io.liof_connect
#define ld_close_fn		ld_io.liof_close
#define ld_ssl_enable_fn	ld_io.liof_ssl_enable

	/* function pointers, etc. for DNS */
	struct ldap_dns_fns	ld_dnsfn;
#define ld_dns_extradata	ld_dnsfn.lddnsfn_extradata
#define ld_dns_bufsize		ld_dnsfn.lddnsfn_bufsize
#define ld_dns_gethostbyname_fn	ld_dnsfn.lddnsfn_gethostbyname
#define ld_dns_gethostbyaddr_fn	ld_dnsfn.lddnsfn_gethostbyaddr

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
#define ld_mutex_trylock_fn		ld_thread2.ltf_mutex_trylock
#define ld_sema_alloc_fn		ld_thread2.ltf_sema_alloc
#define ld_sema_free_fn			ld_thread2.ltf_sema_free
#define ld_sema_wait_fn			ld_thread2.ltf_sema_wait
#define ld_sema_post_fn			ld_thread2.ltf_sema_post
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
#define LDAP_MUTEX_TRYLOCK( ld, i ) \
	( (ld)->ld_mutex_trylock_fn == NULL ) ? 0 : \
		(ld)->ld_mutex_trylock_fn( (ld)->ld_mutex[i] )
#define LDAP_MUTEX_LOCK( ld, i ) \
	if ( (ld)->ld_mutex_lock_fn != NULL ) { \
		(ld)->ld_mutex_lock_fn( (ld)->ld_mutex[i] ); \
	}
#define LDAP_MUTEX_UNLOCK( ld, i ) \
	if ( (ld)->ld_mutex_unlock_fn != NULL ) { \
		(ld)->ld_mutex_unlock_fn( (ld)->ld_mutex[i] ); \
	}

/* Backword compatibility locks */
#define LDAP_MUTEX_BC_LOCK( ld, i ) \
	if( (ld)->ld_mutex_trylock_fn == NULL ) { \
		if ( (ld)->ld_mutex_lock_fn != NULL ) { \
			(ld)->ld_mutex_lock_fn( (ld)->ld_mutex[i] ); \
		} \
	}
#define LDAP_MUTEX_BC_UNLOCK( ld, i ) \
	if( (ld)->ld_mutex_trylock_fn == NULL ) { \
		if ( (ld)->ld_mutex_unlock_fn != NULL ) { \
			(ld)->ld_mutex_unlock_fn( (ld)->ld_mutex[i] ); \
		} \
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
#define NSLDAPI_MALLOC( size )		nsldapi_malloc( size )
#define NSLDAPI_CALLOC( nelem, elsize )	nsldapi_calloc( nelem, elsize )
#define NSLDAPI_REALLOC( ptr, size )	nsldapi_realloc( ptr, size )
#define NSLDAPI_FREE( ptr )		nsldapi_free( ptr )


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
int nsldapi_next_line_tokens( char **bufp, long *blenp, char ***toksp );
void nsldapi_free_strarray( char **sap );

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
int nsldapi_open_ldap_connection( LDAP *ld, Sockbuf *sb, char *host,
	int defport, char **krbinstancep, int async, int secure );
int nsldapi_open_ldap_defconn( LDAP *ld );
void *nsldapi_malloc( size_t size );
void *nsldapi_calloc( size_t nelem, size_t elsize );
void *nsldapi_realloc( void *ptr, size_t size );
void nsldapi_free( void *ptr );
char *nsldapi_strdup( const char *s );

/*
 * in os-ip.c
 */
int nsldapi_connect_to_host( LDAP *ld, Sockbuf *sb, char *host,
	unsigned long address, int port, int async, int secure );
void nsldapi_close_connection( LDAP *ld, Sockbuf *sb );

int nsldapi_do_ldap_select( LDAP *ld, struct timeval *timeout );
void *nsldapi_new_select_info( void );
void nsldapi_free_select_info( void *vsip );
void nsldapi_mark_select_write( LDAP *ld, Sockbuf *sb );
void nsldapi_mark_select_read( LDAP *ld, Sockbuf *sb );
void nsldapi_mark_select_clear( LDAP *ld, Sockbuf *sb );
int nsldapi_is_read_ready( LDAP *ld, Sockbuf *sb );
int nsldapi_is_write_ready( LDAP *ld, Sockbuf *sb );

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
	int async );
int nsldapi_send_server_request( LDAP *ld, BerElement *ber, int msgid,
	LDAPRequest *parentreq, LDAPServer *srvlist, LDAPConn *lc,
	char *bindreqdn, int bind );
LDAPConn *nsldapi_new_connection( LDAP *ld, LDAPServer **srvlistp, int use_ldsb,
	int connect, int bind );
LDAPRequest *nsldapi_find_request_by_msgid( LDAP *ld, int msgid );
void nsldapi_free_request( LDAP *ld, LDAPRequest *lr, int free_conn );
void nsldapi_free_connection( LDAP *ld, LDAPConn *lc, int force, int unbind );
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
int ldap_build_search_req( LDAP *ld, char *base, int scope,
	char *filter, char **attrs, int attrsonly, LDAPControl **serverctrls,
	LDAPControl **clientctrls, struct timeval *timeoutp, int sizelimit,
	int msgid, BerElement **berp );

/*
 * in unbind.c
 */
int ldap_ld_free( LDAP *ld, int close );
int nsldapi_send_unbind( LDAP *ld, Sockbuf *sb );

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
int nsldapi_url_parse( char *url, LDAPURLDesc **ludpp, int dn_required );


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

#endif /* _LDAPINT_H */
