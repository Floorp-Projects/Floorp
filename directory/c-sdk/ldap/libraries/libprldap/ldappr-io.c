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
 * Extended I/O callback functions for libldap that use
 * NSPR (Netscape Portable Runtime) I/O.
 *
 * High level strategy: we use the socket-specific arg to hold our own data
 * structure that includes the NSPR file handle (PRFileDesc *), among other
 * useful information.  We use the default argument to hold an LDAP session
 * handle specific data structure.
 */

#include "ldappr-int.h"

#define PRLDAP_POLL_ARRAY_GROWTH  5  /* grow arrays 5 elements at a time */

/*
 * Local function prototypes:
 */
static PRIntervalTime prldap_timeout2it( int ms_timeout, int ms_maxtimeout );
static int LDAP_CALLBACK prldap_read( int s, void *buf, int bufsize,
	struct lextiof_socket_private *socketarg );
static int LDAP_CALLBACK prldap_write( int s, const void *buf, int len,
	struct lextiof_socket_private *socketarg );
static int LDAP_CALLBACK prldap_poll( LDAP_X_PollFD fds[], int nfds,
	int timeout, struct lextiof_session_private *sessionarg );
static int LDAP_CALLBACK prldap_connect( const char *hostlist, int defport,
	int timeout, unsigned long options,
	struct lextiof_session_private *sessionarg,
	struct lextiof_socket_private **socketargp );
static int LDAP_CALLBACK prldap_close( int s,
	struct lextiof_socket_private *socketarg );
static int LDAP_CALLBACK prldap_newhandle( LDAP *ld,
	struct lextiof_session_private *sessionarg );
static void LDAP_CALLBACK prldap_disposehandle( LDAP *ld,
	struct lextiof_session_private *sessionarg );
static int LDAP_CALLBACK prldap_shared_newhandle( LDAP *ld,
	struct lextiof_session_private *sessionarg );
static void LDAP_CALLBACK prldap_shared_disposehandle( LDAP *ld,
	struct lextiof_session_private *sessionarg );
static PRLDAPIOSessionArg *prldap_session_arg_alloc( void );
static void prldap_session_arg_free( PRLDAPIOSessionArg **prsesspp );
static void prldap_socket_arg_free( PRLDAPIOSocketArg **prsockpp );
static void *prldap_safe_realloc( void *ptr, PRUint32 size );



/*
 * Local macros:
 */
/* given a socket-specific arg, return the corresponding PRFileDesc * */
#define PRLDAP_GET_PRFD( socketarg )	\
		(((PRLDAPIOSocketArg *)(socketarg))->prsock_prfd)

/*
 * Static variables.
 */
static int prldap_default_io_max_timeout = LDAP_X_IO_TIMEOUT_NO_TIMEOUT;


/*
 * Install NSPR I/O functions into ld (if ld is NULL, they are installed
 * as the default functions for new LDAP * handles).
 *
 * Returns 0 if all goes well and -1 if not.
 */
int
prldap_install_io_functions( LDAP *ld, int shared )
{
    struct ldap_x_ext_io_fns	iofns;

    memset( &iofns, 0, sizeof(iofns));
    iofns.lextiof_size = LDAP_X_EXTIO_FNS_SIZE;
    iofns.lextiof_read = prldap_read;
    iofns.lextiof_write = prldap_write;
    iofns.lextiof_poll = prldap_poll;
    iofns.lextiof_connect = prldap_connect;
    iofns.lextiof_close = prldap_close;
    if ( shared ) {
	iofns.lextiof_newhandle = prldap_shared_newhandle;
	iofns.lextiof_disposehandle = prldap_shared_disposehandle;
    } else {
	iofns.lextiof_newhandle = prldap_newhandle;
	iofns.lextiof_disposehandle = prldap_disposehandle;
    }
    if ( NULL != ld ) {
	/*
	 * If we are dealing with a real ld, we allocate the session specific
	 * data structure now.  If not allocated here, it will be allocated
	 * inside prldap_newhandle() or prldap_shared_newhandle().
	 */
	if ( NULL ==
		( iofns.lextiof_session_arg = prldap_session_arg_alloc())) {
	    ldap_set_lderrno( ld, LDAP_NO_MEMORY, NULL, NULL );
	    return( -1 );
	}
    } else {
	iofns.lextiof_session_arg = NULL;
    }

    if ( ldap_set_option( ld, LDAP_X_OPT_EXTIO_FN_PTRS, &iofns ) != 0 ) {
	prldap_session_arg_free(
		(PRLDAPIOSessionArg **) &iofns.lextiof_session_arg );
	return( -1 );
    }

    return( 0 );
}


static PRIntervalTime
prldap_timeout2it( int ms_timeout, int ms_maxtimeout )
{
    PRIntervalTime	prit;

    if ( LDAP_X_IO_TIMEOUT_NO_WAIT == ms_timeout ) {
	prit = PR_INTERVAL_NO_WAIT;
    } else if ( LDAP_X_IO_TIMEOUT_NO_TIMEOUT == ms_timeout ) {
	prit = PR_INTERVAL_NO_TIMEOUT;
    } else {
	prit = PR_MillisecondsToInterval( ms_timeout );
    }

    /* cap at maximum I/O timeout */
    if ( LDAP_X_IO_TIMEOUT_NO_WAIT == ms_maxtimeout ) {
	prit = LDAP_X_IO_TIMEOUT_NO_WAIT;
    } else if ( LDAP_X_IO_TIMEOUT_NO_TIMEOUT != ms_maxtimeout ) {
	if ( LDAP_X_IO_TIMEOUT_NO_TIMEOUT == ms_timeout ||
		    ms_timeout > ms_maxtimeout ) {
	    prit = PR_MillisecondsToInterval( ms_maxtimeout );
	}
    }

#ifdef PRLDAP_DEBUG
    if ( PR_INTERVAL_NO_WAIT == prit ) {
	fprintf( stderr, "prldap_timeout2it: NO_WAIT\n" );
    } else if ( PR_INTERVAL_NO_TIMEOUT == prit ) {
	fprintf( stderr, "prldap_timeout2it: NO_TIMEOUT\n" );
    } else {
	fprintf( stderr, "prldap_timeout2it: %dms\n",
		PR_IntervalToMilliseconds(prit));
    }
#endif /* PRLDAP_DEBUG */

    return( prit );
}


static int LDAP_CALLBACK
prldap_read( int s, void *buf, int bufsize,
	struct lextiof_socket_private *socketarg )
{
    PRIntervalTime	prit;

    prit = prldap_timeout2it( LDAP_X_IO_TIMEOUT_NO_TIMEOUT,
			socketarg->prsock_io_max_timeout );
    return( PR_Recv( PRLDAP_GET_PRFD(socketarg), buf, bufsize, 0, prit ));
}


static int LDAP_CALLBACK
prldap_write( int s, const void *buf, int len,
	struct lextiof_socket_private *socketarg )
{
    PRIntervalTime	prit;
    char		*ptr = (char *)buf;
    int			rest = len;

    prit = prldap_timeout2it( LDAP_X_IO_TIMEOUT_NO_TIMEOUT,
			socketarg->prsock_io_max_timeout );

    while ( rest > 0 ) {
	int rval;
	if ( rest > PRLDAP_MAX_SEND_SIZE ) {
	    len = PRLDAP_MAX_SEND_SIZE;
	} else {
	    len = rest;
	}
	/*
	 * Note the 4th parameter (flags) to PR_Send() has been obsoleted and
	 * must always be 0
	 */
	rval = PR_Send( PRLDAP_GET_PRFD(socketarg), ptr, len, 0, prit );
	if ( 0 > rval ) {
	    return rval;
	}

	if ( 0 == rval ) {
	    break;
	}

	ptr += rval;
	rest -= rval;
    }

    return (int)( ptr - (char *)buf );
}


struct prldap_eventmap_entry {
    PRInt16	evm_nspr;	/* corresponding NSPR PR_Poll() event */
    int		evm_ldap;	/* LDAP poll event */
};

static struct prldap_eventmap_entry prldap_eventmap[] = {
    { PR_POLL_READ,	LDAP_X_POLLIN },
    { PR_POLL_EXCEPT,	LDAP_X_POLLPRI },
    { PR_POLL_WRITE,	LDAP_X_POLLOUT },
    { PR_POLL_ERR,	LDAP_X_POLLERR },
    { PR_POLL_HUP,	LDAP_X_POLLHUP },
    { PR_POLL_NVAL,	LDAP_X_POLLNVAL },
};

#define PRLDAP_EVENTMAP_ENTRIES	\
	sizeof(prldap_eventmap)/sizeof(struct prldap_eventmap_entry )

static int LDAP_CALLBACK
prldap_poll( LDAP_X_PollFD fds[], int nfds, int timeout,
	struct lextiof_session_private *sessionarg )
{
    PRLDAPIOSessionArg	*prsessp = sessionarg;
    PRPollDesc		*pds;
    int			i, j, rc;

    if ( NULL == prsessp ) {
	prldap_set_system_errno( EINVAL );
	return( -1 );
    }

    /* allocate or resize NSPR poll descriptor array */
    if ( prsessp->prsess_pollds_count < nfds ) {
	pds = prldap_safe_realloc( prsessp->prsess_pollds,
		( nfds + PRLDAP_POLL_ARRAY_GROWTH )
		* sizeof( PRPollDesc ));
	if ( NULL == pds ) {
	    prldap_set_system_errno( prldap_prerr2errno());
	    return( -1 );
	}
	prsessp->prsess_pollds = pds;
	prsessp->prsess_pollds_count = nfds + PRLDAP_POLL_ARRAY_GROWTH;
    } else {
	pds = prsessp->prsess_pollds;
    }

    /* populate NSPR poll info. based on LDAP info. */
    for ( i = 0; i < nfds; ++i ) {
	if ( NULL == fds[i].lpoll_socketarg ) {
	    pds[i].fd = NULL;
	} else {
	    pds[i].fd = PRLDAP_GET_PRFD( fds[i].lpoll_socketarg );
	}
	pds[i].in_flags = pds[i].out_flags = 0;
	if ( fds[i].lpoll_fd >= 0 ) {
	    for ( j = 0; j < PRLDAP_EVENTMAP_ENTRIES; ++j ) {
		if (( fds[i].lpoll_events & prldap_eventmap[j].evm_ldap )
		    != 0 ) {
			pds[i].in_flags |= prldap_eventmap[j].evm_nspr;
		}
	    }
	}
	fds[i].lpoll_revents = 0;	/* clear revents */
    }

    /* call PR_Poll() to do the real work */
    rc = PR_Poll( pds, nfds,
	    prldap_timeout2it( timeout, prsessp->prsess_io_max_timeout ));

    /* populate LDAP info. based on NSPR results */
    for ( i = 0; i < nfds; ++i ) {
	if ( pds[i].fd != NULL ) {
	    for ( j = 0; j < PRLDAP_EVENTMAP_ENTRIES; ++j ) {
		if (( pds[i].out_flags & prldap_eventmap[j].evm_nspr )
			!= 0 ) {
		    fds[i].lpoll_revents |= prldap_eventmap[j].evm_ldap;
		}
	    }
	}
    }

    return( rc );
}


/*
 * Utility function to try one TCP connect()
 * Returns 1 if successful and -1 if not.  Sets the NSPR fd inside prsockp.
 */
static int
prldap_try_one_address( struct lextiof_socket_private *prsockp,
    PRNetAddr *addrp, int port, int timeout, unsigned long options )
{
    /*
     * Set up address and open a TCP socket:
     */
    if ( PR_SUCCESS != PR_SetNetAddr( PR_IpAddrNull, /* don't touch IP addr. */
		PRLDAP_DEFAULT_ADDRESS_FAMILY, (PRUint16)port, addrp )) { 
	return( -1 );
    }

    if (( prsockp->prsock_prfd = PR_OpenTCPSocket(
		PRLDAP_DEFAULT_ADDRESS_FAMILY )) == NULL ) {
	return( -1 );
    }

    /*
     * Set nonblocking option if requested:
     */
    if ( 0 != ( options & LDAP_X_EXTIOF_OPT_NONBLOCKING )) {
	PRSocketOptionData	optdata;

	optdata.option = PR_SockOpt_Nonblocking;
	optdata.value.non_blocking = PR_TRUE;
	if ( PR_SetSocketOption( prsockp->prsock_prfd, &optdata )
		    != PR_SUCCESS ) {
	    prldap_set_system_errno( prldap_prerr2errno());
	    PR_Close( prsockp->prsock_prfd );
	    return( -1 );
	}
    }

#ifdef PRLDAP_DEBUG
    {
	char	buf[ 256 ], *p, *fmtstr;

	if ( PR_SUCCESS != PR_NetAddrToString( addrp, buf, sizeof(buf ))) {
		strcpy( buf, "conversion failed!" );
	}
	if ( strncmp( buf, "::ffff:", 7 ) == 0 ) {
		/* IPv4 address mapped into IPv6 address space */
		p = buf + 7;
		fmtstr = "prldap_try_one_address(): Trying %s:%d...\n";
	} else {
		p = buf;
		fmtstr = "prldap_try_one_address(): Trying [%s]:%d...\n";
	}
	fprintf( stderr, fmtstr, p, PR_ntohs( addrp->ipv6.port ));
    }
#endif /* PRLDAP_DEBUG */

    /*
     * Try to open the TCP connection itself:
     */
    if ( PR_SUCCESS != PR_Connect( prsockp->prsock_prfd, addrp,
                prldap_timeout2it( timeout, prsockp->prsock_io_max_timeout ))
                && PR_IN_PROGRESS_ERROR != PR_GetError() ) {
	PR_Close( prsockp->prsock_prfd );
	prsockp->prsock_prfd = NULL;
	return( -1 );
    }

#ifdef PRLDAP_DEBUG
    fputs( "prldap_try_one_address(): Connected.\n", stderr );
#endif /* PRLDAP_DEBUG */

    /*
     * Success.  Return a valid file descriptor (1 is always valid)
     */
    return( 1 );
}


/*
 * XXXmcs: At present, this code ignores the timeout when doing DNS lookups.
 */
static int LDAP_CALLBACK
prldap_connect( const char *hostlist, int defport, int timeout,
	unsigned long options, struct lextiof_session_private *sessionarg,
	struct lextiof_socket_private **socketargp )
{
    int					rc, parse_err, port;
    char				*host, hbuf[ PR_NETDB_BUF_SIZE ];
    struct ldap_x_hostlist_status	*status;
    struct lextiof_socket_private	*prsockp;
    PRNetAddr				addr;
    PRHostEnt				hent;

    if ( 0 != ( options & LDAP_X_EXTIOF_OPT_SECURE )) {
	prldap_set_system_errno( EINVAL );
	return( -1 );
    }

    if ( NULL == ( prsockp = prldap_socket_arg_alloc( sessionarg ))) {
	prldap_set_system_errno( prldap_prerr2errno());
	return( -1 );
    }

    rc = -1;	/* pessimistic */
    for ( parse_err = ldap_x_hostlist_first( hostlist, defport, &host, &port,
		&status );
		rc < 0 && LDAP_SUCCESS == parse_err && NULL != host;
		parse_err = ldap_x_hostlist_next( &host, &port, status )) {

	if ( PR_SUCCESS == PR_StringToNetAddr( host, &addr )) {
		
		if ( PRLDAP_DEFAULT_ADDRESS_FAMILY == PR_AF_INET6 &&
				PR_AF_INET == PR_NetAddrFamily( &addr )) {
			PRUint32	ipv4ip = addr.inet.ip;
			memset( &addr, 0, sizeof(addr));
			PR_ConvertIPv4AddrToIPv6( ipv4ip, &addr.ipv6.ip );
			addr.ipv6.family = PR_AF_INET6;
			
		}
	    rc = prldap_try_one_address( prsockp, &addr, port,
			timeout, options );
	} else {
	    if ( PR_SUCCESS == PR_GetIPNodeByName( host,
			PRLDAP_DEFAULT_ADDRESS_FAMILY, PR_AI_DEFAULT | PR_AI_ALL, hbuf, 
			sizeof( hbuf ), &hent )) {
		PRIntn enumIndex = 0;

		while ( rc < 0 && ( enumIndex = PR_EnumerateHostEnt(
			    enumIndex, &hent, (PRUint16)port, &addr )) > 0 ) {
		    rc = prldap_try_one_address( prsockp, &addr, port,
				timeout, options );
		}
	    }
	}

	ldap_memfree( host );
    }

    ldap_x_hostlist_statusfree( status );

    if ( rc < 0 ) {
	prldap_set_system_errno( prldap_prerr2errno());
	prldap_socket_arg_free( &prsockp );
    } else {
	*socketargp = prsockp;
    }

    return( rc );
}


static int LDAP_CALLBACK
prldap_close( int s, struct lextiof_socket_private *socketarg )
{
    int		rc;

    rc = 0;
    if ( PR_Close( PRLDAP_GET_PRFD(socketarg)) != PR_SUCCESS ) {
	rc = -1;
	prldap_set_system_errno( prldap_prerr2errno());
    }
    prldap_socket_arg_free( &socketarg );

    return( rc );
}


/*
 * LDAP session handle creation callback.
 *
 * Allocate a session argument if not already done, and then call the
 * thread's new handle function.
 */
static int LDAP_CALLBACK
prldap_newhandle( LDAP *ld, struct lextiof_session_private *sessionarg )
{

    if ( NULL == sessionarg ) {
	struct ldap_x_ext_io_fns	iofns;

	memset( &iofns, 0, sizeof(iofns));
	iofns.lextiof_size = LDAP_X_EXTIO_FNS_SIZE;
	if ( ldap_get_option( ld, LDAP_X_OPT_EXTIO_FN_PTRS,
		(void *)&iofns ) < 0 ) {
	    return( ldap_get_lderrno( ld, NULL, NULL ));
	}
	if ( NULL ==
		( iofns.lextiof_session_arg = prldap_session_arg_alloc())) {
	    return( LDAP_NO_MEMORY );
	}
	if ( ldap_set_option( ld, LDAP_X_OPT_EXTIO_FN_PTRS,
		    (void *)&iofns ) < 0 ) {
	    return( ldap_get_lderrno( ld, NULL, NULL ));
	}
    }

    return( LDAP_SUCCESS );
}


/* only called/installed if shared is non-zero. */
static int LDAP_CALLBACK
prldap_shared_newhandle( LDAP *ld, struct lextiof_session_private *sessionarg )
{
    int		rc;

    if (( rc = prldap_newhandle( ld, sessionarg )) == LDAP_SUCCESS ) {
	rc = prldap_thread_new_handle( ld, sessionarg );
    }

    return( rc );
}


static void LDAP_CALLBACK
prldap_disposehandle( LDAP *ld, struct lextiof_session_private *sessionarg )
{
    prldap_session_arg_free( &sessionarg );
}


/* only called/installed if shared is non-zero */
static void LDAP_CALLBACK
prldap_shared_disposehandle( LDAP *ld,
	struct lextiof_session_private *sessionarg )
{
    prldap_thread_dispose_handle( ld, sessionarg );
    prldap_disposehandle( ld, sessionarg );
}


/*
 * Allocate a session argument.
 */
static PRLDAPIOSessionArg *
prldap_session_arg_alloc( void )
{
    PRLDAPIOSessionArg		*prsessp;

    prsessp = PR_Calloc( 1, sizeof( PRLDAPIOSessionArg ));

    if ( NULL != prsessp ) {
	/* copy global defaults to the new session handle */
	prsessp->prsess_io_max_timeout = prldap_default_io_max_timeout;
    }

    return( prsessp );
}


static void
prldap_session_arg_free( PRLDAPIOSessionArg **prsesspp )
{
    if ( NULL != prsesspp && NULL != *prsesspp ) {
	if ( NULL != (*prsesspp)->prsess_pollds ) {
	    PR_Free( (*prsesspp)->prsess_pollds );
	    (*prsesspp)->prsess_pollds = NULL;
	}
	PR_Free( *prsesspp );
	*prsesspp = NULL;
    }
}


/*
 * Given an LDAP session handle, retrieve a session argument.
 * Returns an LDAP error code.
 */
int
prldap_session_arg_from_ld( LDAP *ld, PRLDAPIOSessionArg **sessargpp )
{
    struct ldap_x_ext_io_fns	iofns;

    if ( NULL == ld || NULL == sessargpp ) {
	/* XXXmcs: NULL ld's are not supported */
	ldap_set_lderrno( ld, LDAP_PARAM_ERROR, NULL, NULL );
	return( LDAP_PARAM_ERROR );
    }

    memset( &iofns, 0, sizeof(iofns));
    iofns.lextiof_size = LDAP_X_EXTIO_FNS_SIZE;
    if ( ldap_get_option( ld, LDAP_X_OPT_EXTIO_FN_PTRS, (void *)&iofns ) < 0 ) {
	return( ldap_get_lderrno( ld, NULL, NULL ));
    }

    if ( NULL == iofns.lextiof_session_arg ) {
	ldap_set_lderrno( ld, LDAP_LOCAL_ERROR, NULL, NULL );
	return( LDAP_LOCAL_ERROR );
    }

    *sessargpp = iofns.lextiof_session_arg;
    return( LDAP_SUCCESS );
}


/*
 * Allocate a socket argument.
 */
PRLDAPIOSocketArg *
prldap_socket_arg_alloc( PRLDAPIOSessionArg *sessionarg )
{
    PRLDAPIOSocketArg		*prsockp;

    prsockp = PR_Calloc( 1, sizeof( PRLDAPIOSocketArg ));

    if ( NULL != prsockp && NULL != sessionarg ) {
	/* copy socket defaults from the session */
	prsockp->prsock_io_max_timeout = sessionarg->prsess_io_max_timeout;
    }

    return( prsockp );
}


static void
prldap_socket_arg_free( PRLDAPIOSocketArg **prsockpp )
{
    if ( NULL != prsockpp && NULL != *prsockpp ) {
	PR_Free( *prsockpp );
	*prsockpp = NULL;
    }
}


static void *
prldap_safe_realloc( void *ptr, PRUint32 size )
{
    void	*p;

    if ( NULL == ptr ) {
	p = PR_Malloc( size );
    } else {
	p = PR_Realloc( ptr, size );
    }

    return( p );
}



/* returns an LDAP result code */
int
prldap_set_io_max_timeout( PRLDAPIOSessionArg *prsessp, int io_max_timeout )
{
    int	rc = LDAP_SUCCESS;	/* optimistic */

    if ( NULL == prsessp ) {
	prldap_default_io_max_timeout = io_max_timeout;
    } else {
	prsessp->prsess_io_max_timeout = io_max_timeout;
    }

    return( rc );
}


/* returns an LDAP result code */
int
prldap_get_io_max_timeout( PRLDAPIOSessionArg *prsessp, int *io_max_timeoutp )
{
    int	rc = LDAP_SUCCESS;	/* optimistic */

    if ( NULL == io_max_timeoutp ) {
	rc = LDAP_PARAM_ERROR;
    } else if ( NULL == prsessp ) {
	*io_max_timeoutp = prldap_default_io_max_timeout;
    } else {
	*io_max_timeoutp = prsessp->prsess_io_max_timeout;
    }

    return( rc );
}

/* Check if NSPR layer has been installed for a LDAP session.
 * Simply check whether prldap_connect() I/O function is installed 
 */
PRBool
prldap_is_installed( LDAP *ld )
{
    struct ldap_x_ext_io_fns iofns;

    /* Retrieve current I/O functions */
    memset( &iofns, 0, sizeof(iofns));
    iofns.lextiof_size = LDAP_X_EXTIO_FNS_SIZE;
    if ( ld == NULL || ldap_get_option( ld, LDAP_X_OPT_EXTIO_FN_PTRS, (void *)&iofns )
	 != 0 ||  iofns.lextiof_connect != prldap_connect ) 
    {
	return( PR_FALSE );
    }
    
    return( PR_TRUE );
}

