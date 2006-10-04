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

/* ldap-to-be-deprecated.h - functions and declaration which will be
 * deprecated in a future release.
 * 
 * A deprecated API is an API that we recommend you no longer use,
 * due to improvements in the LDAP C SDK. While deprecated APIs are
 * currently still implemented, they may be removed in future
 * implementations, and we recommend using other APIs.
 *
 * This header file will act as a first warning before moving functions
 * into an unsupported/deprecated state.  If your favorite application
 * depend on any declaration and defines, and there is a good reason
 * for not porting to new functions, Speak up now or they may disappear
 * in a future release
 */

#ifndef _LDAP_TOBE_DEPRECATED_H
#define _LDAP_TOBE_DEPRECATED_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * I/O function callbacks option (an API extension --
 * LDAP_API_FEATURE_X_IO_FUNCTIONS).
 * Use of the extended I/O functions instead is recommended
 */
#define LDAP_OPT_IO_FN_PTRS		0x0B	/* 11 - API extension */

/*
 * I/O callback functions (note that types for the read and write callbacks
 * are actually in lber.h):
 */
typedef int	(LDAP_C LDAP_CALLBACK LDAP_IOF_SELECT_CALLBACK)( int nfds,
	fd_set *readfds, fd_set *writefds, fd_set *errorfds,
	struct timeval *timeout );
typedef LBER_SOCKET (LDAP_C LDAP_CALLBACK LDAP_IOF_SOCKET_CALLBACK)(
	int domain, int type, int protocol );
typedef int	(LDAP_C LDAP_CALLBACK LDAP_IOF_IOCTL_CALLBACK)( LBER_SOCKET s, 
	int option, ... );
typedef int	(LDAP_C LDAP_CALLBACK LDAP_IOF_CONNECT_CALLBACK )(
	LBER_SOCKET s, struct sockaddr *name, int namelen );
typedef int	(LDAP_C LDAP_CALLBACK LDAP_IOF_CLOSE_CALLBACK )(
	LBER_SOCKET s );
typedef int	(LDAP_C LDAP_CALLBACK LDAP_IOF_SSL_ENABLE_CALLBACK )(
	LBER_SOCKET s );

/*
 * Structure to hold I/O function pointers:
 */
struct ldap_io_fns {
	LDAP_IOF_READ_CALLBACK *liof_read;
	LDAP_IOF_WRITE_CALLBACK *liof_write;
	LDAP_IOF_SELECT_CALLBACK *liof_select;
	LDAP_IOF_SOCKET_CALLBACK *liof_socket;
	LDAP_IOF_IOCTL_CALLBACK *liof_ioctl;
	LDAP_IOF_CONNECT_CALLBACK *liof_connect;
	LDAP_IOF_CLOSE_CALLBACK *liof_close;
	LDAP_IOF_SSL_ENABLE_CALLBACK *liof_ssl_enable;
};

/*
 * DNS resolver callbacks (an API extension --LDAP_API_FEATURE_X_DNS_FUNCTIONS).
 * Note that gethostbyaddr() is not currently used.
 */
#define LDAP_OPT_DNS_FN_PTRS            0x60    /* 96 - API extension */

typedef struct LDAPHostEnt {
    char        *ldaphe_name;           /* official name of host */
    char        **ldaphe_aliases;       /* alias list */
    int         ldaphe_addrtype;        /* host address type */
    int         ldaphe_length;          /* length of address */
    char        **ldaphe_addr_list;     /* list of addresses from name server */
} LDAPHostEnt;

typedef LDAPHostEnt * (LDAP_C LDAP_CALLBACK LDAP_DNSFN_GETHOSTBYNAME)(
        const char *name, LDAPHostEnt *result, char *buffer,
        int buflen, int *statusp, void *extradata );
typedef LDAPHostEnt * (LDAP_C LDAP_CALLBACK LDAP_DNSFN_GETHOSTBYADDR)(
        const char *addr, int length, int type, LDAPHostEnt *result,
        char *buffer, int buflen, int *statusp, void *extradata );
typedef int (LDAP_C LDAP_CALLBACK LDAP_DNSFN_GETPEERNAME)(
        LDAP *ld, struct sockaddr *netaddr, char *buffer, int buflen);

struct ldap_dns_fns {
        void                            *lddnsfn_extradata;
        int                             lddnsfn_bufsize;
        LDAP_DNSFN_GETHOSTBYNAME        *lddnsfn_gethostbyname;
        LDAP_DNSFN_GETHOSTBYADDR        *lddnsfn_gethostbyaddr;
	LDAP_DNSFN_GETPEERNAME          *lddnsfn_getpeername;
};

/*
 * experimental DN format support
 */
LDAP_API(char **) LDAP_CALL ldap_explode_dns( const char *dn );
LDAP_API(int) LDAP_CALL ldap_is_dns_dn( const char *dn );


/*
 * user friendly naming/searching routines
 */
typedef int (LDAP_C LDAP_CALLBACK LDAP_CANCELPROC_CALLBACK)( void *cl );
LDAP_API(int) LDAP_CALL ldap_ufn_search_c( LDAP *ld, char *ufn,
        char **attrs, int attrsonly, LDAPMessage **res,
        LDAP_CANCELPROC_CALLBACK *cancelproc, void *cancelparm );
LDAP_API(int) LDAP_CALL ldap_ufn_search_ct( LDAP *ld, char *ufn,
        char **attrs, int attrsonly, LDAPMessage **res,
        LDAP_CANCELPROC_CALLBACK *cancelproc, void *cancelparm,
        char *tag1, char *tag2, char *tag3 );
LDAP_API(int) LDAP_CALL ldap_ufn_search_s( LDAP *ld, char *ufn,
        char **attrs, int attrsonly, LDAPMessage **res );
LDAP_API(LDAPFiltDesc *) LDAP_CALL ldap_ufn_setfilter( LDAP *ld, char *fname );
LDAP_API(void) LDAP_CALL ldap_ufn_setprefix( LDAP *ld, char *prefix );
LDAP_API(int) LDAP_C ldap_ufn_timeout( void *tvparam );

/*
 * utility routines
 */
LDAP_API(int) LDAP_CALL ldap_charray_add( char ***a, char *s );
LDAP_API(int) LDAP_CALL ldap_charray_merge( char ***a, char **s );
LDAP_API(void) LDAP_CALL ldap_charray_free( char **array );
LDAP_API(int) LDAP_CALL ldap_charray_inlist( char **a, char *s );
LDAP_API(char **) LDAP_CALL ldap_charray_dup( char **a );
LDAP_API(char **) LDAP_CALL ldap_str2charray( char *str, char *brkstr );
LDAP_API(int) LDAP_CALL ldap_charray_position( char **a, char *s );

/* from ldap_ssl.h - the pkcs function and declaration */
typedef int (LDAP_C LDAP_CALLBACK LDAP_PKCS_GET_TOKEN_CALLBACK)(void *context, char **tokenname);
typedef int (LDAP_C LDAP_CALLBACK LDAP_PKCS_GET_PIN_CALLBACK)(void *context, const char *tokenname, char **tokenpin);
typedef int (LDAP_C LDAP_CALLBACK LDAP_PKCS_GET_CERTPATH_CALLBACK)(void *context, char **certpath);
typedef int (LDAP_C LDAP_CALLBACK LDAP_PKCS_GET_KEYPATH_CALLBACK)(void *context,char **keypath);
typedef int (LDAP_C LDAP_CALLBACK LDAP_PKCS_GET_MODPATH_CALLBACK)(void *context, char **modulepath);
typedef int (LDAP_C LDAP_CALLBACK LDAP_PKCS_GET_CERTNAME_CALLBACK)(void *context, char **certname);
typedef int (LDAP_C LDAP_CALLBACK LDAP_PKCS_GET_DONGLEFILENAME_CALLBACK)(void *context, char **filename);

#define PKCS_STRUCTURE_ID 1
struct ldapssl_pkcs_fns {
    int local_structure_id;
    void *local_data;
    LDAP_PKCS_GET_CERTPATH_CALLBACK *pkcs_getcertpath;
    LDAP_PKCS_GET_CERTNAME_CALLBACK *pkcs_getcertname;
    LDAP_PKCS_GET_KEYPATH_CALLBACK *pkcs_getkeypath;
    LDAP_PKCS_GET_MODPATH_CALLBACK *pkcs_getmodpath;
    LDAP_PKCS_GET_PIN_CALLBACK *pkcs_getpin;
    LDAP_PKCS_GET_TOKEN_CALLBACK *pkcs_gettokenname;
    LDAP_PKCS_GET_DONGLEFILENAME_CALLBACK *pkcs_getdonglefilename;

};

LDAP_API(int) LDAP_CALL ldapssl_pkcs_init( const struct ldapssl_pkcs_fns *pfns);

#ifdef __cplusplus
}
#endif
#endif /* _LDAP_TOBE_DEPRECATED_H */
