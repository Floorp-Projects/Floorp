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

/* ldap-deprecated.h - deprecated functions and declarations
 *
 * A deprecated API is an API that we recommend you no longer use,
 * due to improvements in the LDAP C SDK. While deprecated APIs are
 * currently still implemented, they may be removed in future
 * implementations, and we recommend using other APIs.
 *
 * This file contain functions and declarations which have
 * outlived their usefullness and have been deprecated.  In many
 * cases functions and declarations have been replaced with newer
 * extended functions.  In no way should applications rely on the
 * declarations and defines within this files as they can and will
 * disappear without any notice.
 */

#ifndef _LDAP_DEPRECATED_H
#define _LDAP_DEPRECATED_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * establish an ldap session
 */
LDAP_API(LDAP *) LDAP_CALL ldap_open( const char *host, int port );

/*
 * Authentication methods:
 */
#define LDAP_AUTH_NONE          0x00L
#define LDAP_AUTH_SIMPLE        0x80L
#define LDAP_AUTH_SASL          0xa3L
LDAP_API(int) LDAP_CALL ldap_bind( LDAP *ld, const char *who,
        const char *passwd, int authmethod );
LDAP_API(int) LDAP_CALL ldap_bind_s( LDAP *ld, const char *who,
        const char *cred, int method );

LDAP_API(int) LDAP_CALL ldap_modrdn( LDAP *ld, const char *dn,
        const char *newrdn );
LDAP_API(int) LDAP_CALL ldap_modrdn_s( LDAP *ld, const char *dn,
        const char *newrdn );
LDAP_API(int) LDAP_CALL ldap_modrdn2( LDAP *ld, const char *dn,
        const char *newrdn, int deleteoldrdn );
LDAP_API(int) LDAP_CALL ldap_modrdn2_s( LDAP *ld, const char *dn,
        const char *newrdn, int deleteoldrdn);

LDAP_API(void) LDAP_CALL ldap_perror( LDAP *ld, const char *s );
LDAP_API(int) LDAP_CALL ldap_result2error( LDAP *ld, LDAPMessage *r,
        int freeit );

/*
 * Preferred language and get_lang_values (an API extension --
 * LDAP_API_FEATURE_X_GETLANGVALUES)
 *
 * The following two APIs are deprecated
 */

#define LDAP_OPT_PREFERRED_LANGUAGE     0x14    /* 20 - API extension */
LDAP_API(char **) LDAP_CALL ldap_get_lang_values( LDAP *ld, LDAPMessage *entry,
        const char *target, char **type );
LDAP_API(struct berval **) LDAP_CALL ldap_get_lang_values_len( LDAP *ld,
        LDAPMessage *entry, const char *target, char **type );

/*
 * Asynchronous I/O (an API extension).
 */
/*
 * This option enables completely asynchronous IO.  It works by using ioctl()
 * on the fd, (or tlook())
 */
#define LDAP_OPT_ASYNC_CONNECT          0x63    /* 99 - API extension */

/*
 * functions and definitions that have been replaced by new improved ones
 */
/*
 * Use ldap_get_option() with LDAP_OPT_API_INFO and an LDAPAPIInfo structure
 * instead of ldap_version().
 */
typedef struct _LDAPVersion {
        int sdk_version;      /* Version of the SDK, * 100 */
        int protocol_version; /* Highest protocol version supported, * 100 */
        int SSL_version;      /* SSL version if this SDK supports it, * 100 */
        int security_level;   /* highest level available */
        int reserved[4];
} LDAPVersion;
#define LDAP_SECURITY_NONE      0
LDAP_API(int) LDAP_CALL ldap_version( LDAPVersion *ver );

/* use ldap_create_filter() instead of ldap_build_filter() */
LDAP_API(void) LDAP_CALL ldap_build_filter( char *buf, unsigned long buflen,
        char *pattern, char *prefix, char *suffix, char *attr,
        char *value, char **valwords );
/* use ldap_set_filter_additions() instead of ldap_setfilteraffixes() */
LDAP_API(void) LDAP_CALL ldap_setfilteraffixes( LDAPFiltDesc *lfdp,
        char *prefix, char *suffix );

/* older result types a server can return -- use LDAP_RES_MODDN instead */
#define LDAP_RES_MODRDN                 LDAP_RES_MODDN
#define LDAP_RES_RENAME                 LDAP_RES_MODDN

/* older error messages */
#define LDAP_AUTH_METHOD_NOT_SUPPORTED  LDAP_STRONG_AUTH_NOT_SUPPORTED

/*
 * Generalized cache callback interface:
 */
#define LDAP_OPT_CACHE_FN_PTRS          0x0D    /* 13 - API extension */
#define LDAP_OPT_CACHE_STRATEGY         0x0E    /* 14 - API extension */
#define LDAP_OPT_CACHE_ENABLE           0x0F    /* 15 - API extension */

/* cache strategies */
#define LDAP_CACHE_CHECK                0
#define LDAP_CACHE_POPULATE             1
#define LDAP_CACHE_LOCALDB              2

typedef int (LDAP_C LDAP_CALLBACK LDAP_CF_BIND_CALLBACK)( LDAP *ld, int msgid,
        unsigned long tag, const char *dn, const struct berval *creds,
        int method);
typedef int (LDAP_C LDAP_CALLBACK LDAP_CF_UNBIND_CALLBACK)( LDAP *ld,
        int unused0, unsigned long unused1 );
typedef int (LDAP_C LDAP_CALLBACK LDAP_CF_SEARCH_CALLBACK)( LDAP *ld,
        int msgid, unsigned long tag, const char *base, int scope,
        const char LDAP_CALLBACK *filter, char **attrs, int attrsonly );
typedef int (LDAP_C LDAP_CALLBACK LDAP_CF_COMPARE_CALLBACK)( LDAP *ld,
        int msgid, unsigned long tag, const char *dn, const char *attr,
        const struct berval *value );
typedef int (LDAP_C LDAP_CALLBACK LDAP_CF_ADD_CALLBACK)( LDAP *ld,
        int msgid, unsigned long tag, const char *dn, LDAPMod **attrs );
typedef int (LDAP_C LDAP_CALLBACK LDAP_CF_DELETE_CALLBACK)( LDAP *ld,
        int msgid, unsigned long tag, const char *dn );
typedef int (LDAP_C LDAP_CALLBACK LDAP_CF_MODIFY_CALLBACK)( LDAP *ld,
        int msgid, unsigned long tag, const char *dn, LDAPMod **mods );
typedef int (LDAP_C LDAP_CALLBACK LDAP_CF_MODRDN_CALLBACK)( LDAP *ld,
        int msgid, unsigned long tag, const char *dn, const char *newrdn,
        int deleteoldrdn );
typedef int (LDAP_C LDAP_CALLBACK LDAP_CF_RESULT_CALLBACK)( LDAP *ld,
        int msgid, int all, struct timeval *timeout, LDAPMessage **result );
typedef int (LDAP_C LDAP_CALLBACK LDAP_CF_FLUSH_CALLBACK)( LDAP *ld,
        const char *dn, const char *filter );

struct ldap_cache_fns {
        void    *lcf_private;
        LDAP_CF_BIND_CALLBACK *lcf_bind;
        LDAP_CF_UNBIND_CALLBACK *lcf_unbind;
        LDAP_CF_SEARCH_CALLBACK *lcf_search;
        LDAP_CF_COMPARE_CALLBACK *lcf_compare;
        LDAP_CF_ADD_CALLBACK *lcf_add;
        LDAP_CF_DELETE_CALLBACK *lcf_delete;
        LDAP_CF_MODIFY_CALLBACK *lcf_modify;
        LDAP_CF_MODRDN_CALLBACK *lcf_modrdn;
        LDAP_CF_RESULT_CALLBACK *lcf_result;
        LDAP_CF_FLUSH_CALLBACK *lcf_flush;
};

LDAP_API(int) LDAP_CALL ldap_cache_flush( LDAP *ld, const char *dn,
        const char *filter );


#ifdef __cplusplus
}
#endif
#endif /* _LDAP_DEPRECATED_H */
