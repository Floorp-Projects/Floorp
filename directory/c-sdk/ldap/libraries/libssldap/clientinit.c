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
 * clientinit.c
 */

#if defined(NET_SSL)


#if defined( _WINDOWS )
#include <windows.h>
#include "proto-ntutil.h"
#endif

#include <nspr.h>
#include <plstr.h>
#include <cert.h>
#include <key.h>
#include <ssl.h>
#include <sslproto.h>
#include <ldap.h>
#include <ldap_ssl.h>
#include <nss.h>

/* XXX:mhein The following is a workaround for the redefinition of */
/*	     const problem on OSF.  Fix to be provided by NSS */
/*	     This is a pretty benign workaround for us which */
/*	     should not cause problems in the future even if */
/*	     we forget to take it out :-) */

#ifdef OSF1V4D
#ifndef __STDC__
#  define __STDC__
#endif /* __STDC__ */
#endif /* OSF1V4D */

#ifndef FILE_PATHSEP
#define FILE_PATHSEP '/'
#endif


static PRStatus local_SSLPLCY_Install(void);
static char *ldapssl_strdup ( const char * );
static void ldapssl_free( void ** );

/*
 * This little tricky guy keeps us from initializing twice 
 */
static int		inited = 0;
static char  tokDes[34] = "Internal (Software) Database     ";
static char ptokDes[34] = "Internal (Software) Token        ";


/* IN:					     */
/* string:	/u/mhein/.netscape/mykey3.db */
/* OUT:					     */
/* dir: 	/u/mhein/.netscape/	     */
/* prefix:	my			     */
/* key:		key3.db			     */

static int
splitpath(char *string, char *dir, char *prefix, char *key) {
        char *k;
        char *s;
        char *d = string;
        char *l;
        int  len = 0;


        if (string == NULL)
                return (-1);

        /* goto the end of the string, and walk backwards until */
        /* you get to the first pathseparator */
        len = PL_strlen(string);
        l = string + len - 1;
        while (l != string && *l != '/' && *l != '\\')
                        l--;
        /* search for the .db */
        if ((k = PL_strstr(l, ".db")) != NULL) {
                /* now we are sitting on . of .db */

                /* move backward to the first 'c' or 'k' */
                /* indicating cert or key */
                while (k != l && *k != 'c' && *k != 'k')
                        k--;

                /* move backwards to the first path separator */
                if (k != d && k > d)
                        s = k - 1;
                while (s != d && *s != '/' && *s != '\\')
                        s--;

                /* if we are sitting on top of a path */
                /* separator there is no prefix */
                if (s + 1 == k) {
                        /* we know there is no prefix */
                        prefix = '\0';
                        PL_strcpy(key, k);
                        *k = '\0';
                        PL_strcpy(dir, d);
                } else {
                        /* grab the prefix */
                        PL_strcpy(key, k);
                        *k = '\0';
                        PL_strcpy(prefix, ++s);
                        *s = '\0';
                        PL_strcpy(dir, d);
                }
        } else {
                /* neither *key[0-9].db nor *cert[0=9].db found */
                return (-1);
        }

	return (0);
}


static PRStatus local_SSLPLCY_Install(void)
{
	SECStatus s;

#ifdef NS_DOMESTIC
	s = NSS_SetDomesticPolicy(); 
#elif NS_EXPORT
	s = NSS_SetExportPolicy(); 
#else
	s = PR_FAILURE;
#endif
	return s?PR_FAILURE:PR_SUCCESS;
}



static int
ldapssl_basic_init( const char *certdbpath, const char *keydbpath )
{
	char *confDir = NULL, *certdbPrefix = NULL, *certdbName = NULL;
	char *keyconfDir = NULL, *keydbPrefix = NULL, *keydbName = NULL;
	char *certPath = NULL, *keyPath = NULL;
	static char *secmodname =  "secmod.db";
	int retcode = 0; 
	SECStatus rc;

    /* PR_Init() must to be called before everything else... */
    PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);

    PR_SetConcurrency( 4 );	/* work around for NSPR 3.x I/O hangs */

	/* Get confDir, certdbPrefix and certdbName from certdbpath */
	certPath = ldapssl_strdup( certdbpath );
	confDir = ldapssl_strdup( certdbpath );
	certdbPrefix = ldapssl_strdup( certdbpath );
	certdbName = ldapssl_strdup( certdbpath );
	if (certdbPrefix) {
		*certdbPrefix = '\0';
	}
	splitpath(certPath, confDir, certdbPrefix, certdbName);

	/* Get keyconfDir, keydbPrefix and keydbName from keydbpath */
	keyPath = ldapssl_strdup( keydbpath );
	keyconfDir = ldapssl_strdup( keydbpath );
	keydbPrefix = ldapssl_strdup( keydbpath );
	keydbName = ldapssl_strdup( keydbpath );
	if (keydbPrefix) {
		*keydbPrefix = '\0';
	}
	splitpath(keyPath, keyconfDir, keydbPrefix, keydbName);

	/* Free the variables we no longer need */
	ldapssl_free((void **)&certPath);
	ldapssl_free((void **)&certdbName);
	ldapssl_free((void **)&keyPath);
	ldapssl_free((void **)&keydbName);
	ldapssl_free((void **)&keyconfDir);

	if ((rc = NSS_Initialize(confDir,certdbPrefix,keydbPrefix,
			secmodname, NSS_INIT_READONLY)) != SECSuccess) {
		retcode = -1;
	}

	ldapssl_free((void **)&certdbPrefix);
	ldapssl_free((void **)&keydbPrefix);
	ldapssl_free((void **)&confDir);

	return (retcode);
}



/*
 * Cover  functions for malloc(), calloc(), strdup() and free() that are
 * compatible with the NSS libraries (they seem to use the C runtime
 * library malloc/free so these functions are quite simple right now).
 */
static void *
ldapssl_malloc( size_t size )
{
    void	*p;

    p = malloc( size );
    return p;
}


static void *
ldapssl_calloc( int nelem, size_t elsize )
{
    void	*p;

    p = calloc( nelem, elsize );
    return p;
}


static char *
ldapssl_strdup( const char *s )
{
    char	*scopy;

    if ( NULL == s ) {
	scopy = NULL;
    } else {
	scopy = strdup( s );
    }
    return scopy;
}


static void
ldapssl_free( void **pp )
{
    if ( NULL != pp && NULL != *pp ) {
	free( (void *)*pp );
	*pp = NULL;
    }
}


static char *
buildDBName(const char *basename, const char *dbname)
{
	char		*result;
	PRUint32	len, pathlen, addslash;

	if (basename)
	{
	    if (( len = PL_strlen( basename )) > 3
		&& PL_strcasecmp( ".db", basename + len - 3 ) == 0 ) {
		return (ldapssl_strdup(basename));
	    }
	    
	    pathlen = len;
	    len = pathlen + PL_strlen(dbname) + 1;
	    addslash = ( pathlen > 0 &&
		(( *(basename + pathlen - 1) != FILE_PATHSEP ) || 
		( *(basename + pathlen - 1) != '\\'  )));

	    if ( addslash ) {
		++len;
	    }
	    if (( result = ldapssl_malloc( len )) != NULL ) {
		PL_strcpy( result, basename );
		if ( addslash ) {
		    *(result+pathlen) = FILE_PATHSEP;  /* replaces '\0' */
		    ++pathlen;
		}
		PL_strcpy(result+pathlen, dbname);
	    }
	    
	}


	return result;
}

char *
GetCertDBName(void *alias, int dbVersion)
{
    char		*source;
    char dbname[128];
    
    source = (char *)alias;
    
    if (!source)
    {
	source = "";
    }
    
    sprintf(dbname, "cert%d.db",dbVersion);
    return(buildDBName(source, dbname));


}

/*
 * return database name by appending "dbname" to "path".
 * this code doesn't need to be terribly efficient (not called often).
 */
/* XXXceb this is the old function.  To be removed eventually */
static char *
GetDBName(const char *dbname, const char *path)
{
    char		*result;
    PRUint32	len, pathlen;
    int		addslash;
    
    if ( dbname == NULL ) {
	dbname = "";
    }
    
    if ((path == NULL) || (*path == 0)) {
	result = ldapssl_strdup(dbname);
    } else {
	pathlen = PL_strlen(path);
	len = pathlen + PL_strlen(dbname) + 1;
	addslash = ( path[pathlen - 1] != '/' );
	if ( addslash ) {
	    ++len;
	}
	if (( result = ldapssl_malloc( len )) != NULL ) {
	    PL_strcpy( result, path );
	    if ( addslash ) {
		*(result+pathlen) = '/';  /* replaces '\0' */
		++pathlen;
	    }
	    PL_strcpy(result+pathlen, dbname);
	}
    }
    
    return result;
}

/*
 * Initialize ns/security so it can be used for SSL client authentication.
 * It is safe to call this more than once.
 *
 * If needkeydb == 0, no key database is opened and SSL server authentication
 * is supported but not client authentication.
 *
 * If "certdbpath" is NULL or "", the default cert. db is used (typically
 * ~/.netscape/cert7.db).
 *
 * If "certdbpath" ends with ".db" (case-insensitive compare), then
 * it is assumed to be a full path to the cert. db file; otherwise,
 * it is assumed to be a directory that contains a file called
 * "cert7.db" or "cert.db".
 *
 * If certdbhandle is non-NULL, it is assumed to be a pointer to a
 * SECCertDBHandle structure.  It is fine to pass NULL since this
 * routine will allocate one for you (CERT_GetDefaultDB() can be
 * used to retrieve the cert db handle).
 *
 * If "keydbpath" is NULL or "", the default key db is used (typically
 * ~/.netscape/key3.db).
 *
 * If "keydbpath" ends with ".db" (case-insensitive compare), then
 * it is assumed to be a full path to the key db file; otherwise,
 * it is assumed to be a directory that contains a file called
 * "key3.db" 
 *
 * If certdbhandle is non-NULL< it is assumed to be a pointed to a
 * SECKEYKeyDBHandle structure.  It is fine to pass NULL since this
 * routine will allocate one for you (SECKEY_GetDefaultDB() can be
 * used to retrieve the cert db handle).
 */
int
LDAP_CALL
ldapssl_clientauth_init( const char *certdbpath, void *certdbhandle, 
    const int needkeydb, const char *keydbpath, void *keydbhandle )

{

    int	rc;
     
    /*
     *     LDAPDebug(LDAP_DEBUG_TRACE, "ldapssl_clientauth_init\n",0 ,0 ,0);
     */

    if ( inited ) {
	return( 0 );
    }

    if ((rc = ldapssl_basic_init(certdbpath, keydbpath)) != 0) {
	return (-1);
    }

    if (SSL_OptionSetDefault(SSL_ENABLE_SSL2, PR_FALSE)
	    || SSL_OptionSetDefault(SSL_ENABLE_SSL3, PR_TRUE)) {
	if (( rc = PR_GetError()) >= 0 ) {
	    rc = -1;
	}
	return( rc );
    }



#if defined(NS_DOMESTIC)
    if (local_SSLPLCY_Install() == PR_FAILURE)
      return( -1 );
#elif(NS_EXPORT)
    if (local_SSLPLCY_Install() == PR_FAILURE)
      return( -1 );
#else
    return( -1 );
#endif

    inited = 1;

    return( 0 );

}


/*
 * Initialize ns/security so it can be used for SSL client authentication.
 * It is safe to call this more than once.
 *
 * If needkeydb == 0, no key database is opened and SSL server authentication
 * is supported but not client authentication.
 *
 * If "certdbpath" is NULL or "", the default cert. db is used (typically
 * ~/.netscape/cert7.db).
 *
 * If "certdbpath" ends with ".db" (case-insensitive compare), then
 * it is assumed to be a full path to the cert. db file; otherwise,
 * it is assumed to be a directory that contains a file called
 * "cert7.db" or "cert.db".
 *
 * If certdbhandle is non-NULL, it is assumed to be a pointer to a
 * SECCertDBHandle structure.  It is fine to pass NULL since this
 * routine will allocate one for you (CERT_GetDefaultDB() can be
 * used to retrieve the cert db handle).
 *
 * If "keydbpath" is NULL or "", the default key db is used (typically
 * ~/.netscape/key3.db).
 *
 * If "keydbpath" ends with ".db" (case-insensitive compare), then
 * it is assumed to be a full path to the key db file; otherwise,
 * it is assumed to be a directory that contains a file called
 * "key3.db" 
 *
 * If certdbhandle is non-NULL< it is assumed to be a pointed to a
 * SECKEYKeyDBHandle structure.  It is fine to pass NULL since this
 * routine will allocate one for you (SECKEY_GetDefaultDB() can be
 * used to retrieve the cert db handle).  */
int
LDAP_CALL
ldapssl_advclientauth_init( 
    const char *certdbpath, void *certdbhandle, 
    const int needkeydb, const char *keydbpath, void *keydbhandle,  
    const int needsecmoddb, const char *secmoddbpath,
    const int sslstrength )
{
    int rc = 0;

    if ( inited ) {
	return( 0 );
    }

    /*
     *    LDAPDebug(LDAP_DEBUG_TRACE, "ldapssl_advclientauth_init\n",0 ,0 ,0);
     */

    if ((rc = ldapssl_basic_init(certdbpath, keydbpath)) != 0) {
	return (-1);
    }

#if defined(NS_DOMESTIC)
    if (local_SSLPLCY_Install() == PR_FAILURE)
      return( -1 );
#elif(NS_EXPORT)
    if (local_SSLPLCY_Install() == PR_FAILURE)
      return( -1 );
#else
    return( -1 );
#endif

    inited = 1;
    
    return ( ldapssl_set_strength( NULL, sslstrength ));
}


/*
 * Initialize ns/security so it can be used for SSL client authentication.
 * It is safe to call this more than once.
  */

/* 
 * XXXceb  This is a hack until the new IO functions are done.
 * this function lives in ldapsinit.c
 */
void set_using_pkcs_functions( int val );

int
LDAP_CALL
ldapssl_pkcs_init( const struct ldapssl_pkcs_fns *pfns )
{

    char		*certdbpath, *keydbpath;
    int			rc;
    
    if ( inited ) {
	return( 0 );
    }
/* 
 * XXXceb  This is a hack until the new IO functions are done.
 * this function MUST be called before ldapssl_enable_clientauth.
 * 
 */
    set_using_pkcs_functions( 1 );
    
    /*
     *    LDAPDebug(LDAP_DEBUG_TRACE, "ldapssl_pkcs_init\n",0 ,0 ,0);
     */


    pfns->pkcs_getcertpath( NULL, &certdbpath);
    pfns->pkcs_getkeypath( NULL, &keydbpath);
    ldapssl_basic_init(certdbpath, keydbpath);

    /* this is odd */
    PK11_ConfigurePKCS11(NULL, NULL, tokDes, ptokDes, NULL, NULL, NULL, NULL, 0, 0 );

    if (SSL_OptionSetDefault(SSL_ENABLE_SSL2, PR_FALSE)
	|| SSL_OptionSetDefault(SSL_ENABLE_SSL3, PR_TRUE)) {
	if (( rc = PR_GetError()) >= 0 ) {
	    rc = -1;
	}
	
	return( rc );
    }
    
#if defined(NS_DOMESTIC)
    if (local_SSLPLCY_Install() == PR_FAILURE)
      return( -1 );
#elif(NS_EXPORT)
    if (local_SSLPLCY_Install() == PR_FAILURE)
      return( -1 );
#else
    return( -1 );
#endif

    inited = 1;

    return ( ldapssl_set_strength( NULL, LDAPSSL_AUTH_CERT ));
}


/*
 * ldapssl_client_init() is a server-authentication only version of
 * ldapssl_clientauth_init().
 */
int
LDAP_CALL
ldapssl_client_init(const char* certdbpath, void *certdbhandle )
{
    return( ldapssl_clientauth_init( certdbpath, certdbhandle,
	    0, NULL, NULL ));
}


/*
 * ldapssl_serverauth_init() is a server-authentication only version of
 * ldapssl_clientauth_init().  This function allows the sslstrength
 * to be passed in.  The sslstrength can take one of the following
 * values:
 *	LDAPSSL_AUTH_WEAK: indicate that you accept the server's
 *			   certificate without checking the CA who
 *			   issued the certificate
 *	LDAPSSL_AUTH_CERT: indicates that you accept the server's
 *			   certificate only if you trust the CA who
 *			   issued the certificate
 *	LDAPSSL_AUTH_CNCHECK:
			   indicates that you accept the server's
 *			   certificate only if you trust the CA who
 *			   issued the certificate and if the value
 * 			   of the cn attribute in the DNS hostname
 *			   of the server
 */
int
LDAP_CALL
ldapssl_serverauth_init(const char* certdbpath,
		     void *certdbhandle,
		     const int sslstrength )
{
    if ( ldapssl_set_strength( NULL, sslstrength ) != 0 ) {
	return( -1 );
    }

    return( ldapssl_clientauth_init( certdbpath, certdbhandle,
	    0, NULL, NULL ));
}

#endif /* NET_SSL */
