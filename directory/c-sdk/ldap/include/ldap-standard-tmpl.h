/* This file is a template.  The generated file is ldap-standard.h>
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

/* ldap-standard.h - standards base header file for libldap */
/* This file contain the defines and function prototypes matching */
/* very closely to the latest LDAP C API draft */
 
#ifndef _LDAP_STANDARD_H
#define _LDAP_STANDARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ldap-platform.h"

#include "lber.h"

#define LDAP_PORT       	389
#define LDAPS_PORT      	636
#define LDAP_VERSION2   	2
#define LDAP_VERSION3   	3
#define LDAP_VERSION_MIN	LDAP_VERSION1
#define LDAP_VERSION_MAX	LDAP_VERSION3

#define LDAP_VENDOR_VERSION	{{LDAP_VENDOR_VERSION}}	/* version # * 100 */
#define LDAP_VENDOR_NAME	"{{LDAP_VENDOR_NAME}}"
/*
 * The following will be an RFC number once the LDAP C API Internet Draft
 * is published as a Proposed Standard RFC.  For now we use 2000 + the
 * draft revision number (currently 5) since we are close to compliance
 * with revision 5 of the draft.
 */
#define LDAP_API_VERSION	2005

/* special values that may appear in the attributes field of a SearchRequest.
 */
#define LDAP_NO_ATTRS		"1.1"
#define LDAP_ALL_USER_ATTRS	"*"

/*
 * Standard options (used with ldap_set_option() and ldap_get_option):
 */
#define LDAP_OPT_API_INFO               0x00	/*  0 */
#define LDAP_OPT_DEREF                  0x02	/*  2 */
#define LDAP_OPT_SIZELIMIT              0x03	/*  3 */
#define LDAP_OPT_TIMELIMIT              0x04	/*  4 */
#define LDAP_OPT_REFERRALS              0x08	/*  8 */
#define LDAP_OPT_RESTART                0x09	/*  9 */
#define LDAP_OPT_PROTOCOL_VERSION	0x11	/* 17 */
#define LDAP_OPT_SERVER_CONTROLS	0x12	/* 18 */
#define LDAP_OPT_CLIENT_CONTROLS	0x13	/* 19 */
#define LDAP_OPT_API_FEATURE_INFO	0x15	/* 21 */
#define LDAP_OPT_HOST_NAME		0x30	/* 48 */
#define LDAP_OPT_ERROR_NUMBER		0x31	/* 49 */
#define LDAP_OPT_ERROR_STRING		0x32	/* 50 */
#define LDAP_OPT_MATCHED_DN		0x33	/* 51 */

/*
 * Well-behaved private and experimental extensions will use option values
 * between 0x4000 (16384) and 0x7FFF (32767) inclusive.
 */
#define LDAP_OPT_PRIVATE_EXTENSION_BASE	0x4000	/* to 0x7FFF inclusive */

/* for on/off options */
#define LDAP_OPT_ON     ((void *)1)
#define LDAP_OPT_OFF    ((void *)0)

typedef struct ldap     LDAP;           /* opaque connection handle */
typedef struct ldapmsg  LDAPMessage;    /* opaque result/entry handle */

/* structure representing an LDAP modification */
typedef struct ldapmod {
	int             mod_op;         /* kind of mod + form of values*/
#define LDAP_MOD_ADD            0x00
#define LDAP_MOD_DELETE         0x01
#define LDAP_MOD_REPLACE        0x02
#define LDAP_MOD_BVALUES        0x80
	char            *mod_type;      /* attribute name to modify */
	union mod_vals_u {
		char            **modv_strvals;
		struct berval   **modv_bvals;
	} mod_vals;                     /* values to add/delete/replace */
#define mod_values      mod_vals.modv_strvals
#define mod_bvalues     mod_vals.modv_bvals
} LDAPMod;


/*
 * structure for holding ldapv3 controls
 */
typedef struct ldapcontrol {
    char            *ldctl_oid;
    struct berval   ldctl_value;
    char            ldctl_iscritical;
} LDAPControl;


/*
 * LDAP API information.  Can be retrieved by using a sequence like:
 *
 *    LDAPAPIInfo ldai;
 *    ldai.ldapai_info_version = LDAP_API_INFO_VERSION;
 *    if ( ldap_get_option( NULL, LDAP_OPT_API_INFO, &ldia ) == 0 ) ...
 */
#define LDAP_API_INFO_VERSION		1
typedef struct ldapapiinfo {
    int  ldapai_info_version;     /* version of this struct (1) */
    int  ldapai_api_version;      /* revision of API supported */
    int  ldapai_protocol_version; /* highest LDAP version supported */
    char **ldapai_extensions;     /* names of API extensions */
    char *ldapai_vendor_name;     /* name of supplier */
    int  ldapai_vendor_version;   /* supplier-specific version times 100 */
} LDAPAPIInfo;


/*
 * LDAP API extended features info.  Can be retrieved by using a sequence like:
 *
 *    LDAPAPIFeatureInfo ldfi;
 *    ldfi.ldapaif_info_version = LDAP_FEATURE_INFO_VERSION;
 *    ldfi.ldapaif_name = "VIRTUAL_LIST_VIEW";
 *    if ( ldap_get_option( NULL, LDAP_OPT_API_FEATURE_INFO, &ldfi ) == 0 ) ...
 */
#define LDAP_FEATURE_INFO_VERSION	1
typedef struct ldap_apifeature_info {
    int   ldapaif_info_version;	/* version of this struct (1) */
    char  *ldapaif_name;	/* name of supported feature */
    int   ldapaif_version;	/* revision of supported feature */
} LDAPAPIFeatureInfo;


/* possible result types a server can return */
#define LDAP_RES_BIND                   0x61L	/* 97 */
#define LDAP_RES_SEARCH_ENTRY           0x64L	/* 100 */
#define LDAP_RES_SEARCH_RESULT          0x65L	/* 101 */
#define LDAP_RES_MODIFY                 0x67L	/* 103 */
#define LDAP_RES_ADD                    0x69L	/* 105 */
#define LDAP_RES_DELETE                 0x6BL	/* 107 */
#define LDAP_RES_MODDN			0x6DL	/* 109 */
#define LDAP_RES_COMPARE                0x6FL	/* 111 */
#define LDAP_RES_SEARCH_REFERENCE       0x73L	/* 115 */
#define LDAP_RES_EXTENDED               0x78L	/* 120 */

/* Special values for ldap_result() "msgid" parameter */
#define LDAP_RES_ANY                (-1)
#define LDAP_RES_UNSOLICITED		0

/* built-in SASL methods */
#define LDAP_SASL_SIMPLE	0	/* special value used for simple bind */

/* search scopes */
#define LDAP_SCOPE_BASE         0x00
#define LDAP_SCOPE_ONELEVEL     0x01
#define LDAP_SCOPE_SUBTREE      0x02

/* alias dereferencing */
#define LDAP_DEREF_NEVER        0x00
#define LDAP_DEREF_SEARCHING    0x01
#define LDAP_DEREF_FINDING      0x02
#define LDAP_DEREF_ALWAYS       0x03

/* predefined size/time limits */
#define LDAP_NO_LIMIT           0

/* allowed values for "all" ldap_result() parameter */
#define LDAP_MSG_ONE		0x00
#define LDAP_MSG_ALL		0x01
#define LDAP_MSG_RECEIVED	0x02

/* possible error codes we can be returned */
#define LDAP_SUCCESS                    0x00	/* 0 */
#define LDAP_OPERATIONS_ERROR           0x01	/* 1 */
#define LDAP_PROTOCOL_ERROR             0x02	/* 2 */
#define LDAP_TIMELIMIT_EXCEEDED         0x03	/* 3 */
#define LDAP_SIZELIMIT_EXCEEDED         0x04	/* 4 */
#define LDAP_COMPARE_FALSE              0x05	/* 5 */
#define LDAP_COMPARE_TRUE               0x06	/* 6 */
#define LDAP_STRONG_AUTH_NOT_SUPPORTED  0x07	/* 7 */
#define LDAP_STRONG_AUTH_REQUIRED       0x08	/* 8 */
#define LDAP_REFERRAL                   0x0a	/* 10 - LDAPv3 */
#define LDAP_ADMINLIMIT_EXCEEDED	0x0b	/* 11 - LDAPv3 */
#define LDAP_UNAVAILABLE_CRITICAL_EXTENSION  0x0c /* 12 - LDAPv3 */
#define LDAP_CONFIDENTIALITY_REQUIRED	0x0d	/* 13 */
#define LDAP_SASL_BIND_IN_PROGRESS	0x0e	/* 14 - LDAPv3 */

#define LDAP_NO_SUCH_ATTRIBUTE          0x10	/* 16 */
#define LDAP_UNDEFINED_TYPE             0x11	/* 17 */
#define LDAP_INAPPROPRIATE_MATCHING     0x12	/* 18 */
#define LDAP_CONSTRAINT_VIOLATION       0x13	/* 19 */
#define LDAP_TYPE_OR_VALUE_EXISTS       0x14	/* 20 */
#define LDAP_INVALID_SYNTAX             0x15	/* 21 */

#define LDAP_NO_SUCH_OBJECT             0x20	/* 32 */
#define LDAP_ALIAS_PROBLEM              0x21	/* 33 */
#define LDAP_INVALID_DN_SYNTAX          0x22	/* 34 */
#define LDAP_IS_LEAF                    0x23	/* 35 (not used in LDAPv3) */
#define LDAP_ALIAS_DEREF_PROBLEM        0x24	/* 36 */

#define LDAP_INAPPROPRIATE_AUTH         0x30	/* 48 */
#define LDAP_INVALID_CREDENTIALS        0x31	/* 49 */
#define LDAP_INSUFFICIENT_ACCESS        0x32	/* 50 */
#define LDAP_BUSY                       0x33	/* 51 */
#define LDAP_UNAVAILABLE                0x34	/* 52 */
#define LDAP_UNWILLING_TO_PERFORM       0x35	/* 53 */
#define LDAP_LOOP_DETECT                0x36	/* 54 */

#define LDAP_NAMING_VIOLATION           0x40	/* 64 */
#define LDAP_OBJECT_CLASS_VIOLATION     0x41	/* 65 */
#define LDAP_NOT_ALLOWED_ON_NONLEAF     0x42	/* 66 */
#define LDAP_NOT_ALLOWED_ON_RDN         0x43	/* 67 */
#define LDAP_ALREADY_EXISTS             0x44	/* 68 */
#define LDAP_NO_OBJECT_CLASS_MODS       0x45	/* 69 */
#define LDAP_RESULTS_TOO_LARGE          0x46	/* 70 - CLDAP */
#define LDAP_AFFECTS_MULTIPLE_DSAS      0x47	/* 71 */

#define LDAP_OTHER                      0x50	/* 80 */
#define LDAP_SERVER_DOWN                0x51	/* 81 */
#define LDAP_LOCAL_ERROR                0x52	/* 82 */
#define LDAP_ENCODING_ERROR             0x53	/* 83 */
#define LDAP_DECODING_ERROR             0x54	/* 84 */
#define LDAP_TIMEOUT                    0x55	/* 85 */
#define LDAP_AUTH_UNKNOWN               0x56	/* 86 */
#define LDAP_FILTER_ERROR               0x57	/* 87 */
#define LDAP_USER_CANCELLED             0x58	/* 88 */
#define LDAP_PARAM_ERROR                0x59	/* 89 */
#define LDAP_NO_MEMORY                  0x5a	/* 90 */
#define LDAP_CONNECT_ERROR              0x5b	/* 91 */
#define LDAP_NOT_SUPPORTED              0x5c	/* 92 - LDAPv3 */
#define LDAP_CONTROL_NOT_FOUND		0x5d	/* 93 - LDAPv3 */
#define LDAP_NO_RESULTS_RETURNED	0x5e	/* 94 - LDAPv3 */
#define LDAP_MORE_RESULTS_TO_RETURN	0x5f	/* 95 - LDAPv3 */
#define LDAP_CLIENT_LOOP		0x60	/* 96 - LDAPv3 */
#define LDAP_REFERRAL_LIMIT_EXCEEDED	0x61	/* 97 - LDAPv3 */

/*
 * LDAPv3 unsolicited notification messages we know about
 */
#define LDAP_NOTICE_OF_DISCONNECTION	"1.3.6.1.4.1.1466.20036"

/*
 * Client controls we know about
 */
#define LDAP_CONTROL_REFERRALS		"1.2.840.113556.1.4.616"

/*
 * Initializing an ldap sesssion, set session handle options, and
 * closing an ldap session functions
 *
 * NOTE: If you want to use IPv6, you must use prldap creating a LDAP handle
 * with prldap_init instead of ldap_init. Or install the NSPR functions
 * by calling prldap_install_routines. (See the nspr samples in examples)
 */
LDAP_API(LDAP *) LDAP_CALL ldap_init( const char *defhost, int defport );
LDAP_API(int) LDAP_CALL ldap_set_option( LDAP *ld, int option,
	const void *optdata );
LDAP_API(int) LDAP_CALL ldap_get_option( LDAP *ld, int option, void *optdata );
LDAP_API(int) LDAP_CALL ldap_unbind( LDAP *ld );
LDAP_API(int) LDAP_CALL ldap_unbind_s( LDAP *ld );

/*
 * perform ldap operations
 */
LDAP_API(int) LDAP_CALL ldap_abandon( LDAP *ld, int msgid );
LDAP_API(int) LDAP_CALL ldap_add( LDAP *ld, const char *dn, LDAPMod **attrs );
LDAP_API(int) LDAP_CALL ldap_add_s( LDAP *ld, const char *dn, LDAPMod **attrs );
LDAP_API(int) LDAP_CALL ldap_simple_bind( LDAP *ld, const char *who,
	const char *passwd );
LDAP_API(int) LDAP_CALL ldap_simple_bind_s( LDAP *ld, const char *who,
	const char *passwd );
LDAP_API(int) LDAP_CALL ldap_modify( LDAP *ld, const char *dn, LDAPMod **mods );
LDAP_API(int) LDAP_CALL ldap_modify_s( LDAP *ld, const char *dn, 
	LDAPMod **mods );
LDAP_API(int) LDAP_CALL ldap_compare( LDAP *ld, const char *dn,
	const char *attr, const char *value );
LDAP_API(int) LDAP_CALL ldap_compare_s( LDAP *ld, const char *dn, 
	const char *attr, const char *value );
LDAP_API(int) LDAP_CALL ldap_delete( LDAP *ld, const char *dn );
LDAP_API(int) LDAP_CALL ldap_delete_s( LDAP *ld, const char *dn );
LDAP_API(int) LDAP_CALL ldap_search( LDAP *ld, const char *base, int scope,
	const char *filter, char **attrs, int attrsonly );
LDAP_API(int) LDAP_CALL ldap_search_s( LDAP *ld, const char *base, int scope,
	const char *filter, char **attrs, int attrsonly, LDAPMessage **res );
LDAP_API(int) LDAP_CALL ldap_search_st( LDAP *ld, const char *base, int scope,
	const char *filter, char **attrs, int attrsonly,
	struct timeval *timeout, LDAPMessage **res );

/*
 * obtain result from ldap operation
 */
LDAP_API(int) LDAP_CALL ldap_result( LDAP *ld, int msgid, int all,
	struct timeval *timeout, LDAPMessage **result );

/*
 * peeking inside LDAP Messages and deallocating LDAP Messages
 */
LDAP_API(int) LDAP_CALL ldap_msgfree( LDAPMessage *lm );
LDAP_API(int) LDAP_CALL ldap_msgid( LDAPMessage *lm );
LDAP_API(int) LDAP_CALL ldap_msgtype( LDAPMessage *lm );


/*
 * Routines to parse/deal with results and errors returned
 */
LDAP_API(char *) LDAP_CALL ldap_err2string( int err );
LDAP_API(LDAPMessage *) LDAP_CALL ldap_first_entry( LDAP *ld, 
	LDAPMessage *chain );
LDAP_API(LDAPMessage *) LDAP_CALL ldap_next_entry( LDAP *ld, 
	LDAPMessage *entry );
LDAP_API(int) LDAP_CALL ldap_count_entries( LDAP *ld, LDAPMessage *chain );
LDAP_API(char *) LDAP_CALL ldap_get_dn( LDAP *ld, LDAPMessage *entry );
LDAP_API(char *) LDAP_CALL ldap_dn2ufn( const char *dn );
LDAP_API(char **) LDAP_CALL ldap_explode_dn( const char *dn, 
	const int notypes );
LDAP_API(char **) LDAP_CALL ldap_explode_rdn( const char *rdn, 
	const int notypes );
LDAP_API(char *) LDAP_CALL ldap_first_attribute( LDAP *ld, LDAPMessage *entry,
	BerElement **ber );
LDAP_API(char *) LDAP_CALL ldap_next_attribute( LDAP *ld, LDAPMessage *entry,
	BerElement *ber );
LDAP_API(char **) LDAP_CALL ldap_get_values( LDAP *ld, LDAPMessage *entry,
	const char *target );
LDAP_API(struct berval **) LDAP_CALL ldap_get_values_len( LDAP *ld,
	LDAPMessage *entry, const char *target );
LDAP_API(int) LDAP_CALL ldap_count_values( char **vals );
LDAP_API(int) LDAP_CALL ldap_count_values_len( struct berval **vals );
LDAP_API(void) LDAP_CALL ldap_value_free( char **vals );
LDAP_API(void) LDAP_CALL ldap_value_free_len( struct berval **vals );
LDAP_API(void) LDAP_CALL ldap_memfree( void *p );


/*
 * LDAPv3 extended operation calls
 */
/*
 * Note: all of the new asynchronous calls return an LDAP error code,
 * not a message id.  A message id is returned via the int *msgidp
 * parameter (usually the last parameter) if appropriate.
 */
LDAP_API(int) LDAP_CALL ldap_abandon_ext( LDAP *ld, int msgid,
	LDAPControl **serverctrls, LDAPControl **clientctrls );
LDAP_API(int) LDAP_CALL ldap_add_ext( LDAP *ld, const char *dn, LDAPMod **attrs,
	LDAPControl **serverctrls, LDAPControl **clientctrls, int *msgidp );
LDAP_API(int) LDAP_CALL ldap_add_ext_s( LDAP *ld, const char *dn,
	LDAPMod **attrs, LDAPControl **serverctrls, LDAPControl **clientctrls );
LDAP_API(int) LDAP_CALL ldap_sasl_bind( LDAP *ld, const char *dn,
	const char *mechanism, const struct berval *cred,
	LDAPControl **serverctrls, LDAPControl **clientctrls, int *msgidp );
LDAP_API(int) LDAP_CALL ldap_sasl_bind_s( LDAP *ld, const char *dn,
	const char *mechanism, const struct berval *cred,
	LDAPControl **serverctrls, LDAPControl **clientctrls,
	struct berval **servercredp );
LDAP_API(int) LDAP_CALL ldap_modify_ext( LDAP *ld, const char *dn,
	LDAPMod **mods, LDAPControl **serverctrls, LDAPControl **clientctrls,
	int *msgidp );
LDAP_API(int) LDAP_CALL ldap_modify_ext_s( LDAP *ld, const char *dn,
	LDAPMod **mods, LDAPControl **serverctrls, LDAPControl **clientctrls );
LDAP_API(int) LDAP_CALL ldap_rename( LDAP *ld, const char *dn,
	const char *newrdn, const char *newparent, int deleteoldrdn,
	LDAPControl **serverctrls, LDAPControl **clientctrls, int *msgidp );
LDAP_API(int) LDAP_CALL ldap_rename_s( LDAP *ld, const char *dn,
	const char *newrdn, const char *newparent, int deleteoldrdn,
	LDAPControl **serverctrls, LDAPControl **clientctrls );
LDAP_API(int) LDAP_CALL ldap_compare_ext( LDAP *ld, const char *dn,
	const char *attr, const struct berval *bvalue,
	LDAPControl **serverctrls, LDAPControl **clientctrls, int *msgidp );
LDAP_API(int) LDAP_CALL ldap_compare_ext_s( LDAP *ld, const char *dn,
	const char *attr, const struct berval *bvalue,
	LDAPControl **serverctrls, LDAPControl **clientctrls );
LDAP_API(int) LDAP_CALL ldap_delete_ext( LDAP *ld, const char *dn,
	LDAPControl **serverctrls, LDAPControl **clientctrls, int *msgidp );
LDAP_API(int) LDAP_CALL ldap_delete_ext_s( LDAP *ld, const char *dn,
	LDAPControl **serverctrls, LDAPControl **clientctrls );
LDAP_API(int) LDAP_CALL ldap_search_ext( LDAP *ld, const char *base,
	int scope, const char *filter, char **attrs, int attrsonly,
	LDAPControl **serverctrls, LDAPControl **clientctrls,
	struct timeval *timeoutp, int sizelimit, int *msgidp );
LDAP_API(int) LDAP_CALL ldap_search_ext_s( LDAP *ld, const char *base,
	int scope, const char *filter, char **attrs, int attrsonly,
	LDAPControl **serverctrls, LDAPControl **clientctrls,
	struct timeval *timeoutp, int sizelimit, LDAPMessage **res );
LDAP_API(int) LDAP_CALL ldap_extended_operation( LDAP *ld,
	const char *requestoid, const struct berval *requestdata,
	LDAPControl **serverctrls, LDAPControl **clientctrls, int *msgidp );
LDAP_API(int) LDAP_CALL ldap_extended_operation_s( LDAP *ld,
	const char *requestoid, const struct berval *requestdata,
	LDAPControl **serverctrls, LDAPControl **clientctrls,
	char **retoidp, struct berval **retdatap );
LDAP_API(int) LDAP_CALL ldap_unbind_ext( LDAP *ld, LDAPControl **serverctrls,
	LDAPControl **clientctrls );


/*
 * LDAPv3 extended parsing / result handling calls
 */
LDAP_API(int) LDAP_CALL ldap_parse_sasl_bind_result( LDAP *ld,
	LDAPMessage *res, struct berval **servercredp, int freeit );
LDAP_API(int) LDAP_CALL ldap_parse_result( LDAP *ld, LDAPMessage *res,
	int *errcodep, char **matcheddnp, char **errmsgp, char ***referralsp,
	LDAPControl ***serverctrlsp, int freeit );
LDAP_API(int) LDAP_CALL ldap_parse_extended_result( LDAP *ld, LDAPMessage *res,
	char **retoidp, struct berval **retdatap, int freeit );
LDAP_API(LDAPMessage *) LDAP_CALL ldap_first_message( LDAP *ld,
	LDAPMessage *res );
LDAP_API(LDAPMessage *) LDAP_CALL ldap_next_message( LDAP *ld,	
	LDAPMessage *msg );
LDAP_API(int) LDAP_CALL ldap_count_messages( LDAP *ld, LDAPMessage *res );
LDAP_API(LDAPMessage *) LDAP_CALL ldap_first_reference( LDAP *ld,
	LDAPMessage *res );
LDAP_API(LDAPMessage *) LDAP_CALL ldap_next_reference( LDAP *ld,
	LDAPMessage *ref );
LDAP_API(int) LDAP_CALL ldap_count_references( LDAP *ld, LDAPMessage *res );
LDAP_API(int) LDAP_CALL ldap_parse_reference( LDAP *ld, LDAPMessage *ref,
	char ***referralsp, LDAPControl ***serverctrlsp, int freeit );
LDAP_API(int) LDAP_CALL ldap_get_entry_controls( LDAP *ld, LDAPMessage *entry,
	LDAPControl ***serverctrlsp );
LDAP_API(void) LDAP_CALL ldap_control_free( LDAPControl *ctrl );
LDAP_API(void) LDAP_CALL ldap_controls_free( LDAPControl **ctrls );

#ifdef __cplusplus
}
#endif
#endif /* _LDAP_STANDARD_H */
