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

#if !defined(LDAP_SSL_H)
#define LDAP_SSL_H

/* ldap_ssl.h - prototypes for LDAP over SSL functions */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * these three defines resolve the SSL strength 
 * setting auth weak, diables all cert checking
 * the CNCHECK tests for the man in the middle hack
 */ 
#define LDAPSSL_AUTH_WEAK       0
#define LDAPSSL_AUTH_CERT       1
#define LDAPSSL_AUTH_CNCHECK    2


/*
 * Initialize LDAP library for SSL
 */
LDAP * LDAP_CALL ldapssl_init( const char *defhost, int defport,
	int defsecure );

/*
 * Install I/O routines to make SSL over LDAP possible.
 * Use this after ldap_init() or just use ldapssl_init() instead.
 */
int LDAP_CALL ldapssl_install_routines( LDAP *ld );


/* The next four functions initialize the security code for SSL
 * The first one ldapssl_client_init() does initialization for SSL only
 * The next one supports server authentication using clientauth_init()
 * and allows the caller to specify the ssl strength to use in order to
 * verify the servers's certificate.
 * The next one supports ldapssl_clientauth_init() intializes security 
 * for SSL for client authentication.  The third function initializes
 * security for doing SSL with client authentication, and PKCS, that is, 
 * the third function initializes the security module database (secmod.db).
 * The parameters are as follows:
 * const char *certdbpath - path to the cert file.  This can be a shortcut 
 *     to the directory name, if so cert7.db will be postfixed to the string.
 * void *certdbhandle - Normally this is NULL.  This memory will need 
 *     to be freed.
 * int needkeydb - boolean.  Must be !=0 if client Authentification 
 *     is required
 * char *keydbpath - path to the key database.  This can be a shortcut 
 *     to the directory name, if so key3.db will be postfixed to the string.
 * void *keydbhandle - Normally this is NULL, This memory will need 
 *     to be freed 
 * int needsecmoddb - boolean.  Must be !=0 to assure that the correct 
 *     security module is loaded into memory
 * char *secmodpath - path to the secmod.  This can be a shortcut to the
 *    directory name, if so secmod.db will be postfixed to the string.
 *
 *  These three functions are mutually exclusive.  You can only call 
 *     one.  This means that, for a given process, you must call the
 *     appropriate initialization function for the life of the process.
 */


/*
 * Initialize the secure parts (Security and SSL) of the runtime for use
 * by a client application.  This is only called once.
 */

int LDAP_CALL ldapssl_client_init(
    const char *certdbpath, void *certdbhandle );

/*
 * Initialize the secure parts (Security and SSL) of the runtime for use
 * by a client application using server authentication.  This is only
 * called once.
 *
 * ldapssl_serverauth_init() is a server-authentication only version of
 * ldapssl_clientauth_init().  This function allows the sslstrength
 * to be passed in.  The sslstrength can take one of the following
 * values:
 *
 *      LDAPSSL_AUTH_WEAK: indicate that you accept the server's 
 *                         certificate without checking the CA who
 *                         issued the certificate
 *      LDAPSSL_AUTH_CERT: indicates that you accept the server's 
 *                         certificate only if you trust the CA who
 *                         issued the certificate
 *      LDAPSSL_AUTH_CNCHECK:
 *                         indicates that you accept the server's
 *                         certificate only if you trust the CA who
 *                         issued the certificate and if the value
 *                         of the cn attribute is the DNS hostname
 *                         of the server.  If this option is selected,
 *			   please ensure that the "defhost" parameter
 *			   passed to ldapssl_init() consist of only 
 *			   one hostname and not a list of hosts.
 *			   Furthermore, the port number must be passed
 *			   via the "defport" parameter, and cannot
 *			   be passed via a host:port option. 
 */

int LDAP_CALL ldapssl_serverauth_init(
    const char *certdbpath, void *certdbhandle, const int sslstrength );

/*
 * Initialize the secure parts (Security and SSL) of the runtime for use
 * by a client application that may want to do SSL client authentication.
 */

int LDAP_CALL ldapssl_clientauth_init( 
    const char *certdbpath, void *certdbhandle, 
    const int needkeydb, const char *keydbpath, void *keydbhandle );

/*
 * Initialize the secure parts (Security and SSL) of the runtime for use
 * by a client application that may want to do SSL client authentication.
 *
 * Please see the description of the sslstrength value in the
 * ldapssl_serverauth_init() function above and note the potential
 * problems which can be caused by passing in wrong host & portname 
 * values.  The same warning applies to the ldapssl_advclientauth_init()
 * function.
 */

int LDAP_CALL ldapssl_advclientauth_init( 
    const char *certdbpath, void *certdbhandle, 
    const int needkeydb, const char *keydbpath, void *keydbhandle,  
    const int needsecmoddb, const char *secmoddbpath, 
    const int sslstrength );



/*
 * get a meaningful error string back from the security library
 * this function should be called, if ldap_err2string doesn't 
 * identify the error code.
 */
const char * LDAP_CALL ldapssl_err2string( const int prerrno );


/*
 * Enable SSL client authentication on the given ld.
 */
int LDAP_CALL ldapssl_enable_clientauth( LDAP *ld, char *keynickname,
	char *keypasswd, char *certnickname );

#ifdef __cplusplus
}
#endif
#endif /* !defined(LDAP_SSL_H) */





