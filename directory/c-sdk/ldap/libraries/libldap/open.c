/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 * 
 * The contents of this file are subject to the Mozilla Public License Version 
 * 1.1 (the "License"); you may not use this file except in compliance with 
 * the License. You may obtain a copy of the License at 
 * http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 * 
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 * 
 * ***** END LICENSE BLOCK ***** */
/*
 *  Copyright (c) 1995 Regents of the University of Michigan.
 *  All rights reserved.
 */
/*
 *  open.c
 */

#if 0
#ifndef lint 
static char copyright[] = "@(#) Copyright (c) 1995 Regents of the University of Michigan.\nAll rights reserved.\n";
#endif
#endif

#include "ldap-int.h"
#ifdef LDAP_SASLIO_HOOKS
/* Valid for any ANSI C compiler */
#include <limits.h>
#endif

#define VI_PRODUCTVERSION 3

#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK	((unsigned long) 0x7f000001)
#endif

#ifdef LDAP_DEBUG
int	ldap_debug = 0;
#endif

#ifdef _WINDOWS
#define USE_WINDOWS_TLS /* thread local storage */
#endif

/*
 * global defaults for callbacks are stored here.  callers of the API set
 *    these by passing a NULL "ld" to ldap_set_option().  Everything in
 *    nsldapi_ld_defaults can be overridden on a per-ld basis as well (the
 *    memory allocation functions are global to all ld's).
 */
struct ldap                     nsldapi_ld_defaults;
struct ldap_memalloc_fns        nsldapi_memalloc_fns = { 0, 0, 0, 0 };
int				nsldapi_initialized = 0;

#ifdef USE_PTHREADS
#include <pthread.h>
#ifdef VMS
/*
** pthread_self() is not a routine on OpenVMS; it's inline assembler code. 
** Since we need a real address which we can stuff away into a table, we need 
** to make sure that pthread_self maps to the real pthread_self routine (yes,
** we do have one fortunately).
*/
#undef pthread_self
#define pthread_self PTHREAD_SELF
extern pthread_t pthread_self (void);
#endif
static pthread_key_t		nsldapi_key;
static pthread_mutex_t		nsldapi_init_mutex = PTHREAD_MUTEX_INITIALIZER;

struct nsldapi_ldap_error {
        int     le_errno;
        char    *le_matched;
        char    *le_errmsg;
};
#elif defined (USE_WINDOWS_TLS)
static DWORD dwTlsIndex;
struct nsldapi_ldap_error {
	int	le_errno;
	char	*le_matched;
	char	*le_errmsg;
};
#elif defined (_WINDOWS) /* use static tls */
__declspec ( thread ) int	nsldapi_gldaperrno;
__declspec ( thread ) char	*nsldapi_gmatched = NULL;
__declspec ( thread ) char	*nsldapi_gldaperror = NULL; 
#endif /* USE_WINDOWS_TLS */


#ifdef _WINDOWS
#define	LDAP_MUTEX_T	HANDLE
static LDAP_MUTEX_T		nsldapi_init_mutex;

int
pthread_mutex_init( LDAP_MUTEX_T *mp, void *attr)
{
        if ( (*mp = CreateMutex(NULL, FALSE, NULL)) == NULL )
                return( 1 );
        else
                return( 0 );
}

static void *
pthread_mutex_alloc( void )
{
        LDAP_MUTEX_T *mutexp;

        if ( (mutexp = malloc( sizeof(LDAP_MUTEX_T) )) != NULL ) {
                pthread_mutex_init( mutexp, NULL );
        }
        return( mutexp );
}

int
pthread_mutex_destroy( LDAP_MUTEX_T *mp )
{
        if ( !(CloseHandle(*mp)) )
                return( 1 );
        else
                return( 0 );
}

static void
pthread_mutex_free( void *mutexp )
{
        pthread_mutex_destroy( (LDAP_MUTEX_T *) mutexp );
        free( mutexp );
}

int
pthread_mutex_lock( LDAP_MUTEX_T *mp )
{
        if ( (WaitForSingleObject(*mp, INFINITE) != WAIT_OBJECT_0) )
                return( 1 );
        else
                return( 0 );
}

int
pthread_mutex_unlock( LDAP_MUTEX_T *mp )
{
        if ( !(ReleaseMutex(*mp)) )
                return( 1 );
        else
                return( 0 );
}

static int
get_errno( void )
{
	return errno;
}

static void
set_errno( int Errno )
{
	errno = Errno;
}

#ifdef USE_WINDOWS_TLS
static void
set_ld_error( int err, char *matched, char *errmsg, void *dummy )
{
	struct nsldapi_ldap_error *le;
	void *tsd;

	le = TlsGetValue( dwTlsIndex );

	if (le == NULL) {
		tsd = (void *)calloc(1, sizeof(struct nsldapi_ldap_error));
		TlsSetValue( dwTlsIndex, tsd );
	}

	le = TlsGetValue ( dwTlsIndex );

	if (le == NULL)
		return;

	le->le_errno = err;

	if ( le->le_matched != NULL ) {
		ldap_memfree( le->le_matched );
	}
	le->le_matched = matched;

	if ( le->le_errmsg != NULL ) {
		ldap_memfree( le->le_errmsg );
	}
	le->le_errmsg = errmsg;
}

static int
get_ld_error ( char **matched, char **errmsg, void *dummy )
{
	struct nsldapi_ldap_error *le;

	le = TlsGetValue( dwTlsIndex );
	if ( matched != NULL ) {
		*matched = le->le_matched;
	}

	if ( errmsg != NULL ) {
		*errmsg = le->le_errmsg;
	}

	return( le->le_errno );
}
#else
static int
get_ld_error( char **LDMatched, char **LDError, void * Args )
{
	if ( LDMatched != NULL )
	{
		*LDMatched = nsldapi_gmatched;
	}
	if ( LDError != NULL )
	{
		*LDError = nsldapi_gldaperror;
	}
	return nsldapi_gldaperrno;
}

static void
set_ld_error( int LDErrno, char *  LDMatched, char *  LDError,
	void *  Args )
{
	/* Clean up any previous string storage. */
	if ( nsldapi_gmatched != NULL )
	{
		ldap_memfree( nsldapi_gmatched );
	}
	if ( nsldapi_gldaperror != NULL )
	{
		ldap_memfree( nsldapi_gldaperror );
	}

	nsldapi_gldaperrno  = LDErrno;
	nsldapi_gmatched    = LDMatched;
	nsldapi_gldaperror  = LDError;
}
#endif /* USE_WINDOWS_TLS */
#endif /* ! _WINDOWS */

#ifdef USE_PTHREADS
static void *
pthread_mutex_alloc( void )
{
	pthread_mutex_t *mutexp;

	if ( (mutexp = malloc( sizeof(pthread_mutex_t) )) != NULL ) {
		pthread_mutex_init( mutexp, NULL );
	}
	return( mutexp );
}

static void
pthread_mutex_free( void *mutexp )
{
	pthread_mutex_destroy( (pthread_mutex_t *) mutexp );
	free( mutexp );
}

static void
set_ld_error( int err, char *matched, char *errmsg, void *dummy )
{
        struct nsldapi_ldap_error *le;
        void *tsd;

        le = pthread_getspecific( nsldapi_key );

        if (le == NULL) {
                tsd = (void *)calloc(1, sizeof(struct nsldapi_ldap_error));
                pthread_setspecific( nsldapi_key, tsd );
        }

        le = pthread_getspecific( nsldapi_key );

        if (le == NULL)
                return;

        le->le_errno = err;

        if ( le->le_matched != NULL ) {
                ldap_memfree( le->le_matched );
        }
        le->le_matched = matched;

        if ( le->le_errmsg != NULL ) {
                ldap_memfree( le->le_errmsg );
        }
        le->le_errmsg = errmsg;
}

static int
get_ld_error( char **matched, char **errmsg, void *dummy )
{
        struct nsldapi_ldap_error *le;

        le = pthread_getspecific( nsldapi_key );

        if (le == NULL)
                return( LDAP_SUCCESS );

        if ( matched != NULL ) {
                *matched = le->le_matched;
        }
        if ( errmsg != NULL ) {
                *errmsg = le->le_errmsg;
        }
        return( le->le_errno );
}

static void
set_errno( int err )
{
        errno = err;
}

static int
get_errno( void )
{
        return( errno );
}
#endif /* use_pthreads */

#if defined(USE_PTHREADS) || defined(_WINDOWS)
static struct ldap_thread_fns
	nsldapi_default_thread_fns = {
		(void *(*)(void))pthread_mutex_alloc,
		(void (*)(void *))pthread_mutex_free,
		(int (*)(void *))pthread_mutex_lock,
		(int (*)(void *))pthread_mutex_unlock,
                (int (*)(void))get_errno,
                (void (*)(int))set_errno,
                (int (*)(char **, char **, void *))get_ld_error,
                (void (*)(int, char *, char *, void *))set_ld_error,
		0 };

static struct ldap_extra_thread_fns
	nsldapi_default_extra_thread_fns = {
		0, 0, 0, 0, 0,
#ifdef _WINDOWS
		0
#else
		(void *(*)(void))pthread_self
#endif /* _WINDOWS */
		};
#endif /* use_pthreads || _windows */

void
nsldapi_initialize_defaults( void )
{
#ifdef _WINDOWS
	pthread_mutex_init( &nsldapi_init_mutex, NULL );
#endif /* _WINDOWS */

#if defined(USE_PTHREADS) || defined(_WINDOWS)
	pthread_mutex_lock( &nsldapi_init_mutex );

	if ( nsldapi_initialized ) {
		pthread_mutex_unlock( &nsldapi_init_mutex );
		return;
	}
#else
         if ( nsldapi_initialized ) {
                 return;
         }
#endif /* use_pthreads || _windows */

#ifdef USE_PTHREADS
        if ( pthread_key_create(&nsldapi_key, free ) != 0) {
                perror("pthread_key_create");
        }
#elif defined(USE_WINDOWS_TLS)
	dwTlsIndex = TlsAlloc();
#endif /* USE_WINDOWS_TLS */

	memset( &nsldapi_memalloc_fns, 0, sizeof( nsldapi_memalloc_fns ));
	memset( &nsldapi_ld_defaults, 0, sizeof( nsldapi_ld_defaults ));
	nsldapi_ld_defaults.ld_options = LDAP_BITOPT_REFERRALS;
	nsldapi_ld_defaults.ld_version = LDAP_VERSION3;
	nsldapi_ld_defaults.ld_lberoptions = LBER_OPT_USE_DER;
	nsldapi_ld_defaults.ld_refhoplimit = LDAP_DEFAULT_REFHOPLIMIT;

#ifdef LDAP_SASLIO_HOOKS
	/* SASL default option settings */
	nsldapi_ld_defaults.ld_def_sasl_mech = NULL;
	nsldapi_ld_defaults.ld_def_sasl_realm = NULL;
	nsldapi_ld_defaults.ld_def_sasl_authcid = NULL;
	nsldapi_ld_defaults.ld_def_sasl_authzid = NULL;
	/* SASL Security properties */
	nsldapi_ld_defaults.ld_sasl_secprops.max_ssf = UINT_MAX;
	nsldapi_ld_defaults.ld_sasl_secprops.maxbufsize = SASL_MAX_BUFF_SIZE;
	nsldapi_ld_defaults.ld_sasl_secprops.security_flags =
		SASL_SEC_NOPLAINTEXT | SASL_SEC_NOANONYMOUS;
#endif

#if defined( STR_TRANSLATION ) && defined( LDAP_DEFAULT_CHARSET )
	nsldapi_ld_defaults.ld_lberoptions |= LBER_OPT_TRANSLATE_STRINGS;
#if LDAP_CHARSET_8859 == LDAP_DEFAULT_CHARSET
	ldap_set_string_translators( &nsldapi_ld_defaults, ldap_8859_to_t61,
	    ldap_t61_to_8859 );
#endif /* LDAP_CHARSET_8859 == LDAP_DEFAULT_CHARSET */
#endif /* STR_TRANSLATION && LDAP_DEFAULT_CHARSET */

        /* set default connect timeout (in milliseconds) */
        /* this was picked as it is the standard tcp timeout as well */
        nsldapi_ld_defaults.ld_connect_timeout = LDAP_X_IO_TIMEOUT_NO_TIMEOUT;

#if defined(USE_PTHREADS) || defined(_WINDOWS)
        /* load up default platform specific locking routines */
        if (ldap_set_option( &nsldapi_ld_defaults, LDAP_OPT_THREAD_FN_PTRS,
                (void *)&nsldapi_default_thread_fns) != LDAP_SUCCESS) {
		nsldapi_initialized = 0;
		pthread_mutex_unlock( &nsldapi_init_mutex );
                return;
        }

#ifndef _WINDOWS
        /* load up default threadid function */
        if (ldap_set_option( &nsldapi_ld_defaults, LDAP_OPT_EXTRA_THREAD_FN_PTRS,
                (void *)&nsldapi_default_extra_thread_fns) != LDAP_SUCCESS) {
		nsldapi_initialized = 0;
		pthread_mutex_unlock( &nsldapi_init_mutex );
                return;
        }
#endif /* _WINDOWS */
	nsldapi_initialized = 1;
	pthread_mutex_unlock( &nsldapi_init_mutex );
#else
        nsldapi_initialized = 1;
#endif /* use_pthreads || _windows */
}


/*
 * ldap_version - report version levels for important properties
 * This function is deprecated.  Use ldap_get_option( ..., LDAP_OPT_API_INFO,
 *	... ) instead.
 *
 * Example:
 *	LDAPVersion ver;
 *	ldap_version( &ver );
 *  if ( (ver.sdk_version < 100) || (ver.SSL_version < 300) )
 *      fprintf( stderr, "LDAP SDK level insufficient\n" );
 *
 * or:
 *  if ( ldap_version(NULL) < 100 )
 *      fprintf( stderr, "LDAP SDK level insufficient\n" );
 *
 */

int
LDAP_CALL
ldap_version( LDAPVersion *ver )
{
	if ( NULL != ver )
	{
		memset( ver, 0, sizeof(*ver) );
		ver->sdk_version = (int)(VI_PRODUCTVERSION * 100);
		ver->protocol_version = LDAP_VERSION_MAX * 100;
		ver->SSL_version = SSL_VERSION * 100;
		/* 
		 * set security to none by default 
		 */

		ver->security_level = LDAP_SECURITY_NONE;
#if defined(LINK_SSL)
#if defined(NS_DOMESTIC)
		ver->security_level = 128;
#elif defined(NSS_EXPORT)
		ver->security_level = 40;
#endif
#endif

	}
	return (int)(VI_PRODUCTVERSION * 100);
}

/*
 * ldap_open - initialize and connect to an ldap server.  A magic cookie to
 * be used for future communication is returned on success, NULL on failure.
 * "host" may be a space-separated list of hosts or IP addresses
 *
 * Example:
 *	LDAP	*ld;
 *	ld = ldap_open( hostname, port );
 */

LDAP *
LDAP_CALL
ldap_open( const char *host, int port )
{
	LDAP	*ld;

	LDAPDebug( LDAP_DEBUG_TRACE, "ldap_open\n", 0, 0, 0 );

	if (( ld = ldap_init( host, port )) == NULL ) {
		return( NULL );
	}

	LDAP_MUTEX_LOCK( ld, LDAP_CONN_LOCK );
	if ( nsldapi_open_ldap_defconn( ld ) < 0 ) {
		LDAP_MUTEX_UNLOCK( ld, LDAP_CONN_LOCK );
		ldap_ld_free( ld, NULL, NULL, 0 );
		return( NULL );
	}

	LDAP_MUTEX_UNLOCK( ld, LDAP_CONN_LOCK );
	LDAPDebug( LDAP_DEBUG_TRACE, "ldap_open successful, ld_host is %s\n",
		( ld->ld_host == NULL ) ? "(null)" : ld->ld_host, 0, 0 );

	return( ld );
}


/*
 * ldap_init - initialize the LDAP library.  A magic cookie to be used for
 * future communication is returned on success, NULL on failure.
 * "defhost" may be a space-separated list of hosts or IP addresses
 *
 * NOTE: If you want to use IPv6, you must use prldap creating a LDAP handle
 * with prldap_init instead of ldap_init. Or install the NSPR functions
 * by calling prldap_install_routines. (See the nspr samples in examples)
 *
 * Example:
 *	LDAP	*ld;
 *	ld = ldap_init( default_hostname, default_port );
 */
LDAP *
LDAP_CALL
ldap_init( const char *defhost, int defport )
{
	LDAP	*ld;

	if ( !nsldapi_initialized ) {
		nsldapi_initialize_defaults();
	}

	if ( defport < 0 || defport > LDAP_PORT_MAX ) {
	    LDAPDebug( LDAP_DEBUG_ANY,
		    "ldap_init: port %d is invalid (port numbers must range from 1 to %d)\n",
		    defport, LDAP_PORT_MAX, 0 );
#if !defined( macintosh ) && !defined( DOS ) && !defined( BEOS )
	    errno = EINVAL;
#endif
	    return( NULL );
	}

	LDAPDebug( LDAP_DEBUG_TRACE, "ldap_init\n", 0, 0, 0 );

	if ( (ld = (LDAP*)NSLDAPI_MALLOC( sizeof(struct ldap) )) == NULL ) {
		return( NULL );
	}

	/* copy defaults */
	SAFEMEMCPY( ld, &nsldapi_ld_defaults, sizeof( struct ldap ));
	if ( nsldapi_ld_defaults.ld_io_fns_ptr != NULL ) {
		if (( ld->ld_io_fns_ptr = (struct ldap_io_fns *)NSLDAPI_MALLOC(
		    sizeof( struct ldap_io_fns ))) == NULL ) {
			NSLDAPI_FREE( (char *)ld );
			return( NULL );
		}
		/* struct copy */
		*(ld->ld_io_fns_ptr) = *(nsldapi_ld_defaults.ld_io_fns_ptr);
	}

	/* call the new handle I/O callback if one is defined */
	if ( ld->ld_extnewhandle_fn != NULL ) {
		/*
		 * We always pass the session extended I/O argument to
		 * the new handle callback.
		 */
		if ( ld->ld_extnewhandle_fn( ld, ld->ld_ext_session_arg )
		    != LDAP_SUCCESS ) {
			NSLDAPI_FREE( (char*)ld );
			return( NULL );
		}
	}

	/* allocate session-specific resources */
	if (( ld->ld_sbp = ber_sockbuf_alloc()) == NULL ||
	    ( defhost != NULL &&
	    ( ld->ld_defhost = nsldapi_strdup( defhost )) == NULL ) ||
	    ((ld->ld_mutex = (void **) NSLDAPI_CALLOC( LDAP_MAX_LOCK, sizeof(void *))) == NULL )) {
		if ( ld->ld_sbp != NULL ) {
			ber_sockbuf_free( ld->ld_sbp );
		}
		if( ld->ld_mutex != NULL ) {
			NSLDAPI_FREE( ld->ld_mutex );
		}
		NSLDAPI_FREE( (char*)ld );
		return( NULL );
	}

	/* install Sockbuf I/O functions if set in LDAP * */
	if ( ld->ld_extread_fn != NULL || ld->ld_extwrite_fn != NULL ) {
		struct lber_x_ext_io_fns lberiofns;

		memset( &lberiofns, 0, sizeof( lberiofns ));

		lberiofns.lbextiofn_size = LBER_X_EXTIO_FNS_SIZE;
		lberiofns.lbextiofn_read = ld->ld_extread_fn;
		lberiofns.lbextiofn_write = ld->ld_extwrite_fn;
		lberiofns.lbextiofn_writev = ld->ld_extwritev_fn;
		lberiofns.lbextiofn_socket_arg = NULL;
		ber_sockbuf_set_option( ld->ld_sbp, LBER_SOCKBUF_OPT_EXT_IO_FNS,
			(void *)&lberiofns );
	}

	/* allocate mutexes */
	nsldapi_mutex_alloc_all( ld );

	/* set default port */
	ld->ld_defport = ( defport == 0 ) ? LDAP_PORT : defport;

	return( ld );
}


void
nsldapi_mutex_alloc_all( LDAP *ld )
{
	int	i;

	if ( ld != &nsldapi_ld_defaults && ld->ld_mutex != NULL ) {
		for ( i = 0; i<LDAP_MAX_LOCK; i++ ) {
			ld->ld_mutex[i] = LDAP_MUTEX_ALLOC( ld );
			ld->ld_mutex_threadid[i] = (void *) -1; 
			ld->ld_mutex_refcnt[i] = 0; 
		}
	} 
}


void
nsldapi_mutex_free_all( LDAP *ld )
{
	int	i;

	if ( ld != &nsldapi_ld_defaults && ld->ld_mutex != NULL ) {
		for ( i = 0; i<LDAP_MAX_LOCK; i++ ) {
			LDAP_MUTEX_FREE( ld, ld->ld_mutex[i] );
		}
	}
}


/* returns 0 if connection opened and -1 if an error occurs */
int
nsldapi_open_ldap_defconn( LDAP *ld )
{
	LDAPServer	*srv;

	if (( srv = (LDAPServer *)NSLDAPI_CALLOC( 1, sizeof( LDAPServer ))) ==
	    NULL || ( ld->ld_defhost != NULL && ( srv->lsrv_host =
	    nsldapi_strdup( ld->ld_defhost )) == NULL )) {
		LDAP_SET_LDERRNO( ld, LDAP_NO_MEMORY, NULL, NULL );
		return( -1 );
	}
	srv->lsrv_port = ld->ld_defport;

	if (( ld->ld_options & LDAP_BITOPT_SSL ) != 0 ) {
		srv->lsrv_options |= LDAP_SRV_OPT_SECURE;
	}

	if (( ld->ld_defconn = nsldapi_new_connection( ld, &srv, 1, 1, 0 ))
	    == NULL ) {
		if ( ld->ld_defhost != NULL ) {
			NSLDAPI_FREE( srv->lsrv_host );
		}
		NSLDAPI_FREE( (char *)srv );
		return( -1 );
	}
	++ld->ld_defconn->lconn_refcnt;	/* so it never gets closed/freed */

	return( 0 );
}


struct ldap_x_hostlist_status {
	char	*lhs_hostlist;
	char	*lhs_nexthost;
	int	lhs_defport;
};

/*
 * Return the first host and port in hostlist (setting *hostp and *portp).
 * Return value is an LDAP API error code (LDAP_SUCCESS if all goes well).
 * Note that a NULL or zero-length hostlist causes the host "127.0.0.1" to
 * be returned.
 */
int LDAP_CALL
ldap_x_hostlist_first( const char *hostlist, int defport, char **hostp,
    int *portp, struct ldap_x_hostlist_status **statusp )
{

	if ( NULL == hostp || NULL == portp || NULL == statusp ) {
		return( LDAP_PARAM_ERROR );
	}

	if ( NULL == hostlist || *hostlist == '\0' ) {
		*hostp = nsldapi_strdup( "127.0.0.1" );
		if ( NULL == *hostp ) {
			return( LDAP_NO_MEMORY );
		}
		*portp = defport;
		*statusp = NULL;
		return( LDAP_SUCCESS );
	}

	*statusp = NSLDAPI_CALLOC( 1, sizeof( struct ldap_x_hostlist_status ));
	if ( NULL == *statusp ) {
		return( LDAP_NO_MEMORY );
	}
	(*statusp)->lhs_hostlist = nsldapi_strdup( hostlist );
	if ( NULL == (*statusp)->lhs_hostlist ) {
		return( LDAP_NO_MEMORY );
	}
	(*statusp)->lhs_nexthost = (*statusp)->lhs_hostlist;
	(*statusp)->lhs_defport = defport;
	return( ldap_x_hostlist_next( hostp, portp, *statusp ));
}

/*
 * Return the next host and port in hostlist (setting *hostp and *portp).
 * Return value is an LDAP API error code (LDAP_SUCCESS if all goes well).
 * If no more hosts are available, LDAP_SUCCESS is returned but *hostp is set
 * to NULL.
 */
int LDAP_CALL
ldap_x_hostlist_next( char **hostp, int *portp,
	struct ldap_x_hostlist_status *status )
{
	char	*q;
	int		squarebrackets = 0;

	if ( NULL == hostp || NULL == portp ) {
		return( LDAP_PARAM_ERROR );
	}

	if ( NULL == status || NULL == status->lhs_nexthost ) {
		*hostp = NULL;
		return( LDAP_SUCCESS );
	}

	/*
	 * skip past leading '[' if present (IPv6 addresses may be surrounded
	 * with square brackets, e.g., [fe80::a00:20ff:fee5:c0b4]:389
	 */
	if ( status->lhs_nexthost[0] == '[' ) {
		++status->lhs_nexthost;
		squarebrackets = 1;
	}

	/* copy host into *hostp */
	if ( NULL != ( q = strchr( status->lhs_nexthost, ' ' ))) {
		size_t	len = q - status->lhs_nexthost;
		*hostp = NSLDAPI_MALLOC( len + 1 );
		if ( NULL == *hostp ) {
			return( LDAP_NO_MEMORY );
		}
		strncpy( *hostp, status->lhs_nexthost, len );
		(*hostp)[len] = '\0';
		status->lhs_nexthost += ( len + 1 );
	} else {	/* last host */
		*hostp = nsldapi_strdup( status->lhs_nexthost );
		if ( NULL == *hostp ) {
			return( LDAP_NO_MEMORY );
		}
		status->lhs_nexthost = NULL;
	}

	/* 
	 * Look for closing ']' and skip past it before looking for port.
	 */
	if ( squarebrackets && NULL != ( q = strchr( *hostp, ']' ))) {
		*q++ = '\0';
	} else {
		q = *hostp;
	}

	/* determine and set port */
	if ( NULL != ( q = strchr( q, ':' ))) {
		*q++ = '\0';
		*portp = atoi( q );
	} else {
		*portp = status->lhs_defport;
	}

	return( LDAP_SUCCESS );
}


void LDAP_CALL
ldap_x_hostlist_statusfree( struct ldap_x_hostlist_status *status )
{
	if ( NULL != status ) {
		if ( NULL != status->lhs_hostlist ) {
			NSLDAPI_FREE( status->lhs_hostlist );
		}
		NSLDAPI_FREE( status );
	}
}



/*
 * memory allocation functions.  we include these in open.c since every
 *    LDAP application is likely to pull the rest of the code in this file
 *    in anyways.
 */
void *
ldap_x_malloc( size_t size )
{
	return( nsldapi_memalloc_fns.ldapmem_malloc == NULL ?
	    malloc( size ) :
	    nsldapi_memalloc_fns.ldapmem_malloc( size ));
}


void *
ldap_x_calloc( size_t nelem, size_t elsize )
{
	return( nsldapi_memalloc_fns.ldapmem_calloc == NULL ?
	    calloc(  nelem, elsize ) :
	    nsldapi_memalloc_fns.ldapmem_calloc( nelem, elsize ));
}


void *
ldap_x_realloc( void *ptr, size_t size )
{
	return( nsldapi_memalloc_fns.ldapmem_realloc == NULL ?
	    realloc( ptr, size ) :
	    nsldapi_memalloc_fns.ldapmem_realloc( ptr, size ));
}


void
ldap_x_free( void *ptr )
{
	if ( nsldapi_memalloc_fns.ldapmem_free == NULL ) {
		free( ptr );
	} else {
		nsldapi_memalloc_fns.ldapmem_free( ptr );
	}
}


/* if s is NULL, returns NULL */
char *
nsldapi_strdup( const char *s )
{
	char	*p;

	if ( s == NULL ||
	    (p = (char *)NSLDAPI_MALLOC( strlen( s ) + 1 )) == NULL )
		return( NULL );

	strcpy( p, s );

	return( p );
}
