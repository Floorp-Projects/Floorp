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

/* OK, this stuff broke the Macintosh Dogbert build. Please fix it.
** The files you included do not exist for client builds on macintosh.
** XXXmcs: noted.
*/
#if defined(XP_MAC) && defined(MOZILLA_CLIENT)
#define VI_PRODUCTVERSION 3 /* fixme */
#else
#include "sdkver.h"
#endif

#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK	((unsigned long) 0x7f000001)
#endif

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN  64
#endif

#ifdef LDAP_DEBUG
int	ldap_debug;
#endif

extern int SSL_IsDomestic(void);

/*
 * global defaults for callbacks are stored here.  callers of the API set
 *    these by passing a NULL "ld" to ldap_set_option().  Everything in
 *    nsldapi_ld_defaults can be overridden on a per-ld basis as well (the
 *    memory allocation functions are global to all ld's).
 */
struct ldap                     nsldapi_ld_defaults;
struct ldap_memalloc_fns        nsldapi_memalloc_fns = { 0, 0, 0, 0 };
int				nsldapi_initialized = 0;


void
nsldapi_initialize_defaults( void )
{

	if ( nsldapi_initialized ) {
		return;
	}

	nsldapi_initialized = 1;
	memset( &nsldapi_memalloc_fns, 0, sizeof( nsldapi_memalloc_fns ));
	memset( &nsldapi_ld_defaults, 0, sizeof( nsldapi_ld_defaults ));
	nsldapi_ld_defaults.ld_options = LDAP_BITOPT_REFERRALS;
	nsldapi_ld_defaults.ld_version = LDAP_VERSION;
	nsldapi_ld_defaults.ld_lberoptions = LBER_OPT_USE_DER;
	nsldapi_ld_defaults.ld_refhoplimit = LDAP_DEFAULT_REFHOPLIMIT;

#if defined( STR_TRANSLATION ) && defined( LDAP_DEFAULT_CHARSET )
	nsldapi_ld_defaults.ld_lberoptions |= LBER_OPT_TRANSLATE_STRINGS;
#if LDAP_CHARSET_8859 == LDAP_DEFAULT_CHARSET
	ldap_set_string_translators( &nsldapi_ld_defaults, ldap_8859_to_t61,
	    ldap_t61_to_8859 );
#endif /* LDAP_CHARSET_8859 == LDAP_DEFAULT_CHARSET */
#endif /* STR_TRANSLATION && LDAP_DEFAULT_CHARSET */
}


/*
 * ldap_version - report version levels for important properties
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
#if defined(LINK_SSL)
		ver->security_level = SSL_IsDomestic() ? 128 : 40;
#else
		ver->security_level = LDAP_SECURITY_NONE;
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
		ldap_ld_free( ld, 0 );
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
 * Example:
 *	LDAP	*ld;
 *	ld = ldap_init( default_hostname, default_port );
 */
LDAP *
LDAP_CALL
ldap_init( const char *defhost, int defport )
{
	LDAP	*ld;
	int	i;

	if ( !nsldapi_initialized ) {
		nsldapi_initialize_defaults();
	}

	if ( defport < 0 || defport > LDAP_PORT_MAX ) {
	    LDAPDebug( LDAP_DEBUG_ANY,
		    "ldap_init: port %d is invalid (port numbers must range from 1 to %d)\n",
		    defport, LDAP_PORT_MAX, 0 );
#if !defined( macintosh ) && !defined( DOS )
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

	if (( ld->ld_selectinfo = nsldapi_new_select_info()) == NULL ||
	    ( ld->ld_sbp = ber_sockbuf_alloc()) == NULL ||
	    ( defhost != NULL &&
	    ( ld->ld_defhost = nsldapi_strdup( defhost )) == NULL ) ||
	    ((ld->ld_mutex = (void **) NSLDAPI_CALLOC( LDAP_MAX_LOCK, sizeof(void *))) == NULL )) {
		if ( ld->ld_sbp != NULL ) {
			ber_sockbuf_free( ld->ld_sbp );
		}
		if ( ld->ld_selectinfo != NULL ) {
			nsldapi_free_select_info( ld->ld_selectinfo );
		}
		if( ld->ld_mutex != NULL )
			NSLDAPI_FREE( ld->ld_mutex );
		NSLDAPI_FREE( (char*)ld );
		return( NULL );
	}

	for( i=0; i<LDAP_MAX_LOCK; i++ )
		ld->ld_mutex[i] = LDAP_MUTEX_ALLOC( ld );
	ld->ld_defport = ( defport == 0 ) ? LDAP_PORT : defport;

	return( ld );
}


int
nsldapi_open_ldap_connection( LDAP *ld, Sockbuf *sb, char *host, int defport,
	char **krbinstancep, int async, int secure )
{
	int 			rc = 0, port;
	char			*p, *q, *r;
	char			*curhost, hostname[ 2*MAXHOSTNAMELEN ];

	LDAPDebug( LDAP_DEBUG_TRACE, "nsldapi_open_ldap_connection\n", 0, 0,
	    0 );

	defport = htons( (unsigned short)defport );

	if ( host != NULL && *host != '\0' ) {
		for ( p = host; p != NULL && *p != '\0'; p = q ) {
			if (( q = strchr( p, ' ' )) != NULL ) {
				strncpy( hostname, p, q - p );
				hostname[ q - p ] = '\0';
				curhost = hostname;
				while ( *q == ' ' ) {
				    ++q;
				}
			} else {
				curhost = p;	/* avoid copy if possible */
				q = NULL;
			}

			if (( r = strchr( curhost, ':' )) != NULL ) {
			    if ( curhost != hostname ) {
				strcpy( hostname, curhost );	/* now copy */
				r = hostname + ( r - curhost );
				curhost = hostname;
			    }
			    *r++ = '\0';
			    port = htons( (short)atoi( r ));
			} else {
			    port = defport;   
			}

			if (( rc = nsldapi_connect_to_host( ld, sb, curhost, 0L,
			    port, async, secure )) != -1 ) {
				break;
			}
		}
	} else {
		rc = nsldapi_connect_to_host( ld, sb, NULL, htonl( INADDR_LOOPBACK ),
		    defport, async, secure );
	}

	if ( rc == -1 ) {
		return( rc );
	}

	if ( krbinstancep != NULL ) {
#ifdef KERBEROS
		if (( *krbinstancep = nsldapi_host_connected_to( sb )) != NULL &&
		    ( p = strchr( *krbinstancep, '.' )) != NULL ) {
			*p = '\0';
		}
#else /* KERBEROS */
		krbinstancep = NULL;
#endif /* KERBEROS */
	}

	return( 0 );
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

#ifdef LDAP_SSLIO_HOOKS
	if (( ld->ld_options & LDAP_BITOPT_SSL ) != 0 ) {
		srv->lsrv_options |= LDAP_SRV_OPT_SECURE;
	}
#endif

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


/*
 * memory allocation functions.  we include these in open.c since every
 *    LDAP application is likely to pull the rest of the code in this file
 *    in anyways.
 */
void *
nsldapi_malloc( size_t size )
{
	return( nsldapi_memalloc_fns.ldapmem_malloc == NULL ?
	    malloc( size ) :
	    nsldapi_memalloc_fns.ldapmem_malloc( size ));
}


void *
nsldapi_calloc( size_t nelem, size_t elsize )
{
	return( nsldapi_memalloc_fns.ldapmem_calloc == NULL ?
	    calloc(  nelem, elsize ) :
	    nsldapi_memalloc_fns.ldapmem_calloc( nelem, elsize ));
}


void *
nsldapi_realloc( void *ptr, size_t size )
{
	return( nsldapi_memalloc_fns.ldapmem_realloc == NULL ?
	    realloc( ptr, size ) :
	    nsldapi_memalloc_fns.ldapmem_realloc( ptr, size ));
}


void
nsldapi_free( void *ptr )
{
	if ( nsldapi_memalloc_fns.ldapmem_free == NULL ) {
		free( ptr );
	} else {
		nsldapi_memalloc_fns.ldapmem_free( ptr );
	}
}


char *
nsldapi_strdup( const char *s )
{
	char	*p;

	if ( (p = (char *)NSLDAPI_MALLOC( strlen( s ) + 1 )) == NULL )
		return( NULL );

	strcpy( p, s );

	return( p );
}
