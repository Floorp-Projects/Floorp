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

/*
 * ldapsinit.c
 */

#if defined(NET_SSL)

#if defined( _WINDOWS )
#include <windows.h>
#endif

/* XXX:mhein The following is a workaround for the redefinition of */
/*           const problem on OSF.  Fix to be provided by NSS */
/*           This is a pretty benign workaround for us which */
/*           should not cause problems in the future even if */
/*           we forget to take it out :-) */

#ifdef OSF1V4D
#ifndef __STDC__
#  define __STDC__
#endif /* __STDC__ */
#endif /* OSF1V4D */

#include <errno.h>
#include <nspr.h>
#include <cert.h>
#include <key.h>
#include <ssl.h>
#include <sslproto.h>
#include <sslerr.h>
#include <prnetdb.h>

#include <ldap.h>
#include <ldap_ssl.h>
#include <ldappr.h>
#include <pk11func.h>

/*
 * Data structure to hold the standard NSPR I/O function pointers set by
 * libprldap.   We save them in our session data structure so we can call
 * them from our own I/O functions (we add functionality to support SSL
 * while using libprldap's functions as much as possible).
 */
typedef struct ldapssl_std_functions {
    LDAP_X_EXTIOF_CLOSE_CALLBACK		*lssf_close_fn;
    LDAP_X_EXTIOF_CONNECT_CALLBACK		*lssf_connect_fn;
    LDAP_X_EXTIOF_DISPOSEHANDLE_CALLBACK	*lssf_disposehdl_fn;
} LDAPSSLStdFunctions;



/*
 * LDAP session data structure.
 */
typedef struct ldapssl_session_info {
    int			lssei_using_pcks_fns;
    char		*lssei_certnickname;
    char        	*lssei_keypasswd;
    LDAPSSLStdFunctions	lssei_std_functions;
} LDAPSSLSessionInfo;


/*
 * LDAP socket data structure.
 */
typedef struct ldapssl_socket_info {
    LDAPSSLSessionInfo	*soi_sessioninfo;	/* session info */
} LDAPSSLSocketInfo;


/* 
 * XXXceb  This is a hack until the new IO functions are done.
 * this function MUST be called before ldapssl_enable_clientauth.
 * right now, this function is called in ldapssl_pkcs_init();
 */

static int using_pkcs_functions = 0;

void set_using_pkcs_functions( int val )
{
    using_pkcs_functions = val;
}


/* 
 * External functions... this function currently lives in clientinit.c
 */
int get_ssl_strength( void );


/*
 * Utility functions:
 */
static void ldapssl_free_session_info( LDAPSSLSessionInfo **ssipp );
static void ldapssl_free_socket_info( LDAPSSLSocketInfo **soipp );


/*
 *  SSL Stuff 
 */

static int ldapssl_AuthCertificate(void *certdbarg, PRFileDesc *fd,
	PRBool checkSig, PRBool isServer);

/*
 * client auth stuff
 */
static SECStatus get_clientauth_data( void *sessionarg, PRFileDesc *prfd,
	CERTDistNames *caNames,  CERTCertificate **pRetCert,
	SECKEYPrivateKey **pRetKey );
static SECStatus get_keyandcert( LDAPSSLSessionInfo *ssip,
	CERTCertificate **pRetCert, SECKEYPrivateKey **pRetKey,
	char **errmsgp );
static SECStatus check_clientauth_nicknames_and_passwd( LDAP *ld,
	LDAPSSLSessionInfo *ssip );
static char *get_keypassword( PK11SlotInfo *slot, PRBool retry,
	void *sessionarg );

/*
 * Like ldap_init(), except also install I/O routines from libsec so we
 * can support SSL.  If defsecure is non-zero, SSL is enabled for the
 * default connection as well.
 */
LDAP *
LDAP_CALL
ldapssl_init( const char *defhost, int defport, int defsecure )
{
    LDAP	*ld;


    if (0 ==defport)
	defport = LDAPS_PORT;
    
    if (( ld = ldap_init( defhost, defport )) == NULL ) {
	return( NULL );
    }

    if ( ldapssl_install_routines( ld ) < 0 || ldap_set_option( ld,
		LDAP_OPT_SSL, defsecure ? LDAP_OPT_ON : LDAP_OPT_OFF ) != 0 ) {
	PR_SetError( PR_UNKNOWN_ERROR, EINVAL );  /* XXXmcs: just a guess! */
	ldap_unbind( ld );
	return( NULL );
    }

    return( ld );
}


static int
ldapssl_close(int s, struct lextiof_socket_private *socketarg)
{
    PRLDAPSocketInfo	soi;
    LDAPSSLSocketInfo	*ssoip;
    LDAPSSLSessionInfo	*sseip;

    memset( &soi, 0, sizeof(soi));
    soi.soinfo_size = PRLDAP_SOCKETINFO_SIZE;
    if ( prldap_get_socket_info( s, socketarg, &soi ) != LDAP_SUCCESS ) {
	return( -1 );
    }

    ssoip = (LDAPSSLSocketInfo *)soi.soinfo_appdata;
    sseip = ssoip->soi_sessioninfo;

    ldapssl_free_socket_info( (LDAPSSLSocketInfo **)&soi.soinfo_appdata );

    return( (*(sseip->lssei_std_functions.lssf_close_fn))( s, socketarg ));
}


static int
do_ldapssl_connect(const char *hostlist, int defport, int timeout,
	unsigned long options, struct lextiof_session_private *sessionarg,
	struct lextiof_socket_private **socketargp, int clientauth )
{
    int			intfd = -1;
    PRBool		secure;
    PRLDAPSessionInfo	sei;
    PRLDAPSocketInfo	soi;
    LDAPSSLSocketInfo	*ssoip = NULL;
    LDAPSSLSessionInfo	*sseip;
    PRFileDesc		*sslfd = NULL;

    /*
     * Determine if secure option is set.  Also, clear secure bit in options
     * the we pass to the standard connect() function (since it doesn't know
     * how to handle the secure option).
     */
    if ( 0 != ( options & LDAP_X_EXTIOF_OPT_SECURE )) {
	secure = PR_TRUE;
	options &= ~LDAP_X_EXTIOF_OPT_SECURE;
    } else {
	secure = PR_FALSE;
    }

    /*
     * Retrieve session info. so we can store a pointer to our session info.
     * in our socket info. later.
     */
    memset( &sei, 0, sizeof(sei));
    sei.seinfo_size = PRLDAP_SESSIONINFO_SIZE;
    if ( prldap_get_session_info( NULL, sessionarg, &sei ) != LDAP_SUCCESS ) {
	return( -1 );
    }
    sseip = (LDAPSSLSessionInfo *)sei.seinfo_appdata;
    
    /*
     * Call the standard connect() callback to make the TCP connection.  If it
     * succeeds, *socketargp is set.
     */
    intfd = (*(sseip->lssei_std_functions.lssf_connect_fn))( hostlist, defport,
		timeout, options, sessionarg, socketargp );
    if ( intfd < 0 ) {
	return( intfd );
    }

    /*
     * Retrieve socket info. so we have the PRFileDesc.
     */
    memset( &soi, 0, sizeof(soi));
    soi.soinfo_size = PRLDAP_SOCKETINFO_SIZE;
    if ( prldap_get_socket_info( intfd, *socketargp, &soi ) != LDAP_SUCCESS ) {
	goto close_socket_and_exit_with_error;
    }

    /*
     * Allocate a structure to hold our socket-specific data.
     */
    if ( NULL == ( ssoip = PR_Calloc( 1, sizeof( LDAPSSLSocketInfo )))) {
	goto close_socket_and_exit_with_error;
    }
    ssoip->soi_sessioninfo = sseip;

    /*
     * Add SSL layer and let the standard NSPR to LDAP layer and enable SSL.
     */
    if (( sslfd = SSL_ImportFD( NULL, soi.soinfo_prfd )) == NULL ) {
	goto close_socket_and_exit_with_error;
    }

    if ( SSL_OptionSet( sslfd, SSL_SECURITY, secure ) != SECSuccess ||
		SSL_OptionSet( sslfd, SSL_HANDSHAKE_AS_CLIENT, secure )
		!= SECSuccess || ( secure && SSL_ResetHandshake( sslfd,
		PR_FALSE ) != SECSuccess )) {
	goto close_socket_and_exit_with_error;
    }

    /*
     * Set hostname which will be retrieved (depending on ssl strength) when
     * using client or server auth.
     */
    if ( SSL_SetURL( sslfd, hostlist ) != SECSuccess ) {
	goto close_socket_and_exit_with_error;
    }

    /*
     * Let the standard NSPR to LDAP layer know about the new socket and
     * our own socket-specific data.
     */
    soi.soinfo_prfd = sslfd;
    soi.soinfo_appdata = (void *)ssoip;
    if ( prldap_set_socket_info( intfd, *socketargp, &soi ) != LDAP_SUCCESS ) {
	goto close_socket_and_exit_with_error;
    }
    sslfd = NULL;	/* so we don't close the socket twice upon error */

    /*
     * Install certificate hook function.
     */
    SSL_AuthCertificateHook( soi.soinfo_prfd,
			     (SSLAuthCertificate)ldapssl_AuthCertificate, 
			     (void *)CERT_GetDefaultCertDB());

    if ( SSL_GetClientAuthDataHook( soi.soinfo_prfd,
		get_clientauth_data, clientauth ? sseip : NULL ) != 0 ) {
	goto close_socket_and_exit_with_error;
    }

    return( intfd );	/* success */

close_socket_and_exit_with_error:
    if ( NULL != sslfd ) {
	PR_Close( sslfd );
    }
    if ( NULL != ssoip ) {
	ldapssl_free_socket_info( &ssoip );
    }
    if ( intfd >= 0 && NULL != *socketargp ) {
	(*(sseip->lssei_std_functions.lssf_close_fn))( intfd, *socketargp );
    }
    return( -1 );
}


static int
ldapssl_connect(const char *hostlist, int defport, int timeout,
	unsigned long options, struct lextiof_session_private *sessionarg,
	struct lextiof_socket_private **socketargp )
{
    return( do_ldapssl_connect( hostlist, defport, timeout, options,
		sessionarg, socketargp, 0 ));
}


static int
ldapssl_clientauth_connect(const char *hostlist, int defport, int timeout,
	unsigned long options, struct lextiof_session_private *sessionarg,
	struct lextiof_socket_private **socketargp )
{
    return( do_ldapssl_connect( hostlist, defport, timeout, options,
		sessionarg, socketargp, 1 ));
}


static void
ldapssl_disposehandle(LDAP *ld, struct lextiof_session_private *sessionarg)
{
    PRLDAPSessionInfo				sei;
    LDAPSSLSessionInfo				*sseip;
    LDAP_X_EXTIOF_DISPOSEHANDLE_CALLBACK	*disposehdl_fn;

    memset( &sei, 0, sizeof( sei ));
    sei.seinfo_size = PRLDAP_SESSIONINFO_SIZE;
    if ( prldap_get_session_info( ld, NULL, &sei ) == LDAP_SUCCESS ) {
	sseip = (LDAPSSLSessionInfo *)sei.seinfo_appdata;
	disposehdl_fn = sseip->lssei_std_functions.lssf_disposehdl_fn;
	ldapssl_free_session_info( &sseip );
	(*disposehdl_fn)( ld, sessionarg );
    }
}


/*
 * Install I/O routines from libsec and NSPR into libldap to allow libldap
 * to do SSL.
 *
 * We rely on libprldap to provide most of the functions, and then we override
 * a few of them to support SSL.
 */
int
LDAP_CALL
ldapssl_install_routines( LDAP *ld )
{
    struct ldap_x_ext_io_fns	iofns;
    LDAPSSLSessionInfo		*ssip;
    PRLDAPSessionInfo		sei;

    /* install standard NSPR functions */
    if ( prldap_install_routines(
		ld,
		1 /* shared -- we have to assume it is */ )
		!= LDAP_SUCCESS ) {
	return( -1 );
    }

    /*
     * Allocate our own session information.
     */
    if ( NULL == ( ssip = (LDAPSSLSessionInfo *)PR_Calloc( 1,
		sizeof( LDAPSSLSessionInfo )))) {
	ldap_set_lderrno( ld, LDAP_NO_MEMORY, NULL, NULL );
	return( -1 );
    }
    ssip->lssei_using_pcks_fns = using_pkcs_functions;

    /*
     * override a few functions, saving a pointer to the standard function
     * in each case so we can call it from our SSL savvy functions.
     */
    memset( &iofns, 0, sizeof(iofns));
    iofns.lextiof_size = LDAP_X_EXTIO_FNS_SIZE;
    if ( ldap_get_option( ld, LDAP_X_OPT_EXTIO_FN_PTRS, (void *)&iofns ) < 0 ) {
	ldapssl_free_session_info( &ssip );
	return( -1 );
    }

    /* override socket, connect, and ioctl */
    ssip->lssei_std_functions.lssf_connect_fn = iofns.lextiof_connect;
    iofns.lextiof_connect = ldapssl_connect;
    ssip->lssei_std_functions.lssf_close_fn = iofns.lextiof_close;
    iofns.lextiof_close = ldapssl_close;
    ssip->lssei_std_functions.lssf_disposehdl_fn = iofns.lextiof_disposehandle;
    iofns.lextiof_disposehandle = ldapssl_disposehandle;

    if ( ldap_set_option( ld, LDAP_X_OPT_EXTIO_FN_PTRS, (void *)&iofns ) < 0 ) {
	ldapssl_free_session_info( &ssip );
	return( -1 );
    }

    /*
     * Store session info. for later retrieval.
     */
    sei.seinfo_size = PRLDAP_SESSIONINFO_SIZE;
    sei.seinfo_appdata = (void *)ssip;
    if ( prldap_set_session_info( ld, NULL, &sei ) != LDAP_SUCCESS ) {
	return( -1 );
    }

    return( 0 );
}


int
LDAP_CALL
ldapssl_enable_clientauth( LDAP *ld, char *keynickname,
        char *keypasswd, char *certnickname )
{
    struct ldap_x_ext_io_fns	iofns;
    LDAPSSLSessionInfo		*ssip;
    PRLDAPSessionInfo		sei;

    /*
     * Check parameters
     */
    if ( certnickname == NULL || keypasswd == NULL ) {
	ldap_set_lderrno( ld, LDAP_PARAM_ERROR, NULL, NULL );
	return( -1 );
    }

    /*
     * Update session info. data structure.
     */
    sei.seinfo_size = PRLDAP_SESSIONINFO_SIZE;
    if ( prldap_get_session_info( ld, NULL, &sei ) != LDAP_SUCCESS ) {
	return( -1 );
    }
    ssip = (LDAPSSLSessionInfo *)sei.seinfo_appdata;
    if ( NULL == ssip ) {
	ldap_set_lderrno( ld, LDAP_PARAM_ERROR, NULL, NULL );
	return( -1 );
    }
    ssip->lssei_certnickname = PL_strdup( certnickname );
    ssip->lssei_keypasswd = PL_strdup( keypasswd );

    if ( NULL == ssip->lssei_certnickname || NULL == ssip->lssei_keypasswd ) {
	ldap_set_lderrno( ld, LDAP_NO_MEMORY, NULL, NULL );
	return( -1 );
    }

    if ( check_clientauth_nicknames_and_passwd( ld, ssip ) != SECSuccess ) {
	return( -1 );
    }

    /*
     * replace standard SSL CONNECT function with client auth aware one
     */
    memset( &iofns, 0, sizeof(iofns));
    iofns.lextiof_size = LDAP_X_EXTIO_FNS_SIZE;
    if ( ldap_get_option( ld, LDAP_X_OPT_EXTIO_FN_PTRS, (void *)&iofns )
		!= 0 ) {
	return( -1 );
    }

    if ( iofns.lextiof_connect != ldapssl_connect ) {
	/* standard SSL setup has not done */
	ldap_set_lderrno( ld, LDAP_PARAM_ERROR, NULL, NULL );
	return( -1 );
    }

    iofns.lextiof_connect = ldapssl_clientauth_connect;

    if ( ldap_set_option( ld, LDAP_X_OPT_EXTIO_FN_PTRS, (void *)&iofns )
		!= 0 ) {
	return( -1 );
    }

    return( 0 );
}


static void
ldapssl_free_session_info( LDAPSSLSessionInfo **ssipp )
{
    if ( NULL != ssipp && NULL != *ssipp ) {
	if ( NULL != (*ssipp)->lssei_certnickname ) {
	    PL_strfree( (*ssipp)->lssei_certnickname );
	    (*ssipp)->lssei_certnickname = NULL;
	}
	if ( NULL != (*ssipp)->lssei_keypasswd ) {
	    PL_strfree( (*ssipp)->lssei_keypasswd );
	    (*ssipp)->lssei_keypasswd = NULL;
	}
	PR_Free( *ssipp );
	*ssipp = NULL;
    }
}


static void
ldapssl_free_socket_info( LDAPSSLSocketInfo **soipp )
{
    if ( NULL != soipp && NULL != *soipp ) {
	PR_Free( *soipp );
	*soipp = NULL;
    }
}


/* this function provides cert authentication.  This is called during 
 * the SSL_Handshake process.  Once the cert has been retrieved from
 * the server, the it is checked, using VerifyCertNow(), then 
 * the cn is checked against the host name, set with SSL_SetURL()
 */

/*
 * XXXceb NOTE, this needs to be fixed to check for the MITM hack 980623
 */

static int
ldapssl_AuthCertificate(void *certdbarg, PRFileDesc *fd, PRBool checkSig,
	PRBool isServer)
{
    SECStatus          rv = SECFailure;
    CERTCertDBHandle * handle;
    CERTCertificate * cert;
    SECCertUsage certUsage;
    char * hostname    = (char *)0;
    
    if (!certdbarg || !socket)
	return rv;

    if (LDAPSSL_AUTH_WEAK == get_ssl_strength() )
      return SECSuccess;

    handle = (CERTCertDBHandle *)certdbarg;

    if ( isServer ) {
	certUsage = certUsageSSLClient;
    } else {
	certUsage = certUsageSSLServer;
    }
    cert = SSL_PeerCertificate( fd );
    
    rv = CERT_VerifyCertNow(handle, cert, checkSig, certUsage, NULL);

    if ( rv != SECSuccess || isServer )
	return rv;
  
    if ( LDAPSSL_AUTH_CNCHECK == get_ssl_strength() )
      {
	/* cert is OK.  This is the client side of an SSL connection.
	 * Now check the name field in the cert against the desired hostname.
	 * NB: This is our only defense against Man-In-The-Middle (MITM) 
	 * attacks!
	 */

	hostname = SSL_RevealURL( fd );

	if (hostname && hostname[0]) {
	  rv = CERT_VerifyCertName(cert, hostname);
	} else  {
	  rv = SECFailure;
     	}
	if (rv != SECSuccess)
	  PORT_SetError(SSL_ERROR_BAD_CERT_DOMAIN);
      }

    return((int)rv);
}


/*
 * called during SSL client auth. when server wants our cert and key.
 * returns: SECSuccess if we succeeded and set *pRetCert and *pRetKey,
 *			SECFailure otherwise.
 * if SECFailure is returned SSL will proceed without sending a cert.
 */

static SECStatus
get_clientauth_data( void *sessionarg, PRFileDesc *prfd,
        CERTDistNames *caNames,  CERTCertificate **pRetCert,
        SECKEYPrivateKey **pRetKey )

{
    LDAPSSLSessionInfo	*ssip;

    if (( ssip = (LDAPSSLSessionInfo *)sessionarg ) == NULL ) {
	return( SECFailure );       /* client auth. not enabled */
    }

    return( get_keyandcert( ssip, pRetCert, pRetKey, NULL ));
}

static SECStatus
get_keyandcert( LDAPSSLSessionInfo *ssip,
	CERTCertificate **pRetCert, SECKEYPrivateKey **pRetKey,
	char **errmsgp )
{
    CERTCertificate	*cert;
    SECKEYPrivateKey	*key;

    if (( cert = PK11_FindCertFromNickname( ssip->lssei_certnickname, NULL ))
		== NULL ) {
	if ( errmsgp != NULL ) {
	    *errmsgp = "unable to find certificate";
	}
	return( SECFailure );
    }

    if (!ssip->lssei_using_pcks_fns)
    {
	PK11_SetPasswordFunc( get_keypassword );
    }
    


    if (( key = PK11_FindKeyByAnyCert( cert, (void *)ssip )) == NULL ) {
	CERT_DestroyCertificate( cert );
	if ( errmsgp != NULL ) {
	    *errmsgp = "bad key or key password";
	}
	return( SECFailure );
    }

    *pRetCert = cert;
    *pRetKey = key;
    return( SECSuccess );
}


/* 
 * This function returns the password to NSS.
 * This function is enable through PK11_SetPasswordFunc
 * only if pkcs functions are not being used.
 */ 

static char *
get_keypassword( PK11SlotInfo *slot, PRBool retry, void *sessionarg )
{
    LDAPSSLSessionInfo	*ssip;

    if ( retry)
      return (NULL);

    ssip = (LDAPSSLSessionInfo *)sessionarg;
    if ( NULL == ssip ) {
	return( NULL );
    }

    return( PL_strdup(ssip->lssei_keypasswd) );
}


/*
 * performs some basic checks on clientauth cert and key/password
 *
 * XXXmcs: could perform additional checks... see servers/slapd/ssl.c
 *	1) check expiration
 *	2) check that public key in cert matches private key
 * see ns/netsite/ldap/servers/slapd/ssl.c:slapd_ssl_init() for example code.
 */
static SECStatus
check_clientauth_nicknames_and_passwd( LDAP *ld, LDAPSSLSessionInfo *ssip )
{
    char		*errmsg = NULL;
    CERTCertificate	*cert = NULL;
    SECKEYPrivateKey	*key = NULL;
    SECStatus		rv;

    rv = get_keyandcert( ssip, &cert, &key, &errmsg );

    if ( rv != SECSuccess ) {
    	if ( errmsg != NULL ) {
	    errmsg = strdup( errmsg );
	}
	ldap_set_lderrno( ld, LDAP_PARAM_ERROR, NULL, errmsg );
	return( rv );
    }

    if ( cert != NULL ) {
	CERT_DestroyCertificate( cert );
    }
    if ( key != NULL ) {
	SECKEY_DestroyPrivateKey( key );
    }
    return( SECSuccess );
}


/* there are patches and kludges.  this is both.  force some linkers to 
 * link this stuff in
 */
int stubs_o_stuff( void )
{
    PRExplodedTime exploded;
    PLArenaPool pool;
  
    const char *name ="t";
    PRUint32 size = 0, align = 0;

    PR_ImplodeTime( &exploded );
    PL_InitArenaPool( &pool, name, size, align);
    PR_Cleanup();
    PR_fprintf((PRFileDesc*)stderr, "Bad IDEA!!");

    return 0;

}
#endif /* NET_SSL */
