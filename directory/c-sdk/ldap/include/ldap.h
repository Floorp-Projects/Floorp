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
/* ldap.h - general header file for libldap */
#ifndef _LDAP_H
#define _LDAP_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined( XP_OS2 ) 
#include "os2sock.h"
#elif defined (WIN32) || defined (_WIN32) || defined( _CONSOLE ) 
#include <windows.h>
#  if defined( _WINDOWS )
#  include <winsock.h>
#  endif
#elif defined(macintosh)
#include <utime.h>
#include "macsocket.h"
#else
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#endif

#ifdef _AIX
#include <sys/select.h>
#endif /* _AIX */

#include "lber.h"

#define LDAP_PORT       389
#define LDAPS_PORT      636
#define LDAP_PORT_MAX	65535
#define LDAP_VERSION1   1
#define LDAP_VERSION2   2
#define LDAP_VERSION3   3
#define LDAP_VERSION    LDAP_VERSION2	/* default should stay as LDAPv2 */
#define LDAP_VERSION_MAX	LDAP_VERSION3

#define LDAP_ROOT_DSE		""
#define LDAP_NO_ATTRS		"1.1"
#define LDAP_ALL_USER_ATTRS	"*"

#define LDAP_OPT_DESC                   1
#define LDAP_OPT_DEREF                  2
#define LDAP_OPT_SIZELIMIT              3
#define LDAP_OPT_TIMELIMIT              4
#define LDAP_OPT_THREAD_FN_PTRS         5
#define LDAP_OPT_REBIND_FN              6
#define LDAP_OPT_REBIND_ARG             7
#define LDAP_OPT_REFERRALS              8
#define LDAP_OPT_RESTART                9
#define LDAP_OPT_SSL			10
#define LDAP_OPT_IO_FN_PTRS		11
#define LDAP_OPT_CACHE_FN_PTRS          13
#define LDAP_OPT_CACHE_STRATEGY         14
#define LDAP_OPT_CACHE_ENABLE           15
#define LDAP_OPT_REFERRAL_HOP_LIMIT	16
#define LDAP_OPT_PROTOCOL_VERSION	17
#define LDAP_OPT_SERVER_CONTROLS	18
#define LDAP_OPT_CLIENT_CONTROLS	19
#define LDAP_OPT_PREFERRED_LANGUAGE	20
#define LDAP_OPT_ERROR_NUMBER		49
#define LDAP_OPT_ERROR_STRING		50

/* for on/off options */
#define LDAP_OPT_ON     ((void *)1)
#define LDAP_OPT_OFF    ((void *)0)

extern int ldap_debug;
/* On UNIX, there's only one copy of ldap_debug */
/* On NT, each dll keeps its own module_ldap_debug, which */
/* points to the process' ldap_debug and needs initializing after load */
#ifdef _WIN32
extern int		*module_ldap_debug;
typedef void (*set_debug_level_fn_t)(int*);
#endif

typedef struct ldap     LDAP;           /* opaque connection handle */
typedef struct ldapmsg  LDAPMessage;    /* opaque result/entry handle */

#define NULLMSG ((LDAPMessage *)0)

/* structure representing an LDAP modification */
typedef struct ldapmod {
	int             mod_op;         /* kind of mod + form of values*/
#define LDAP_MOD_ADD            0x00
#define LDAP_MOD_DELETE         0x01
#define LDAP_MOD_REPLACE        0x02
#define LDAP_MOD_BVALUES        0x80
	char            *mod_type;      /* attribute name to modify */
	union {
		char            **modv_strvals;
		struct berval   **modv_bvals;
	} mod_vals;                     /* values to add/delete/replace */
#define mod_values      mod_vals.modv_strvals
#define mod_bvalues     mod_vals.modv_bvals
} LDAPMod;

/*
 * thread function callbacks
 */
typedef void *(LDAP_C LDAP_CALLBACK LDAP_TF_MUTEX_ALLOC_CALLBACK)( void );
typedef void (LDAP_C LDAP_CALLBACK LDAP_TF_MUTEX_FREE_CALLBACK)( void * );
typedef int (LDAP_C LDAP_CALLBACK LDAP_TF_MUTEX_LOCK_CALLBACK)( void * );
typedef int (LDAP_C LDAP_CALLBACK LDAP_TF_MUTEX_UNLOCK_CALLBACK)( void * );
typedef int (LDAP_C LDAP_CALLBACK LDAP_TF_GET_ERRNO_CALLBACK)( void );
typedef void (LDAP_C LDAP_CALLBACK LDAP_TF_SET_ERRNO_CALLBACK)( int  );
typedef int (LDAP_C LDAP_CALLBACK LDAP_TF_GET_LDERRNO_CALLBACK)( char **, 
	char **, void * );
typedef void    (LDAP_C LDAP_CALLBACK LDAP_TF_SET_LDERRNO_CALLBACK)( int, 
	char *, char *, void * );

/*
 * structure to hold thread function pointers
 */
struct ldap_thread_fns {
	LDAP_TF_MUTEX_ALLOC_CALLBACK *ltf_mutex_alloc;
	LDAP_TF_MUTEX_FREE_CALLBACK *ltf_mutex_free;
	LDAP_TF_MUTEX_LOCK_CALLBACK *ltf_mutex_lock;
	LDAP_TF_MUTEX_UNLOCK_CALLBACK *ltf_mutex_unlock;
	LDAP_TF_GET_ERRNO_CALLBACK *ltf_get_errno;
	LDAP_TF_SET_ERRNO_CALLBACK *ltf_set_errno;
	LDAP_TF_GET_LDERRNO_CALLBACK *ltf_get_lderrno;
	LDAP_TF_SET_LDERRNO_CALLBACK *ltf_set_lderrno;
	void    *ltf_lderrno_arg;
};




/*
 * I/O function callbacks.  Note that types for the read and write callbacks
 * are actually in lber.h
 */
typedef int	(LDAP_C LDAP_CALLBACK LDAP_IOF_SELECT_CALLBACK)( int, fd_set *, 
	fd_set *, fd_set *, struct timeval * );
typedef LBER_SOCKET (LDAP_C LDAP_CALLBACK LDAP_IOF_SOCKET_CALLBACK)( int, 
	int, int );
typedef int	(LDAP_C LDAP_CALLBACK LDAP_IOF_IOCTL_CALLBACK)( LBER_SOCKET, 
	int, ... );
typedef int	(LDAP_C LDAP_CALLBACK LDAP_IOF_CONNECT_CALLBACK )( LBER_SOCKET, 
	struct sockaddr *, int );
typedef int	(LDAP_C LDAP_CALLBACK LDAP_IOF_CLOSE_CALLBACK )( LBER_SOCKET );
typedef int	(LDAP_C LDAP_CALLBACK LDAP_IOF_SSL_ENABLE_CALLBACK )( LBER_SOCKET );


/*
 * structure to hold I/O function pointers
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
 * structure for holding ldapv3 controls
 */
typedef struct ldapcontrol {
    char            *ldctl_oid;
    struct berval   ldctl_value;
    char            ldctl_iscritical;
} LDAPControl, *PLDAPControl;

/*
 * structure for ldap friendly mapping routines
 */

typedef struct friendly {
	char    *f_unfriendly;
	char    *f_friendly;
} *FriendlyMap;

/*
 * structure for a sort-key 
 */

typedef struct LDAPsortkey {
	char *  sk_attrtype;
	char *  sk_matchruleoid;
	int     sk_reverseorder;
} LDAPsortkey;

/*
 * structures for ldap getfilter routines
 */

typedef struct ldap_filt_info {
	char                    *lfi_filter;
	char                    *lfi_desc;
	int                     lfi_scope;      /* LDAP_SCOPE_BASE, etc */
	int                     lfi_isexact;    /* exact match filter? */
	struct ldap_filt_info   *lfi_next;
} LDAPFiltInfo;

#define LDAP_FILT_MAXSIZ        1024

typedef struct ldap_filt_list LDAPFiltList; /* opaque filter list handle */
typedef struct ldap_filt_desc LDAPFiltDesc; /* opaque filter desc handle */

/*
 * structure that describes a VirtualListViewRequest control.
 * note that ldvlist_index and ldvlist_size are only relevant to
 * ldap_create_virtuallist_control() if ldvlist_attrvalue is NULL.
 */
typedef struct ldapvirtuallist {
    unsigned long   ldvlist_before_count;       /* # entries before target */
    unsigned long   ldvlist_after_count;        /* # entries after target */
    char            *ldvlist_attrvalue;         /* jump to this value */
    unsigned long   ldvlist_index;              /* list offset */
    unsigned long   ldvlist_size;               /* number of items in vlist */
    void            *ldvlist_extradata;         /* for use by application */
} LDAPVirtualList;



/*
 * types for ldap URL handling
 */
typedef struct ldap_url_desc {
    char                *lud_host;
    int                 lud_port;
    char                *lud_dn;
    char                **lud_attrs;
    int                 lud_scope;
    char                *lud_filter;
    unsigned long       lud_options;
#define LDAP_URL_OPT_SECURE     0x01
    char        *lud_string;    /* for internal use only */
} LDAPURLDesc;
#define NULLLDAPURLDESC ((LDAPURLDesc *)NULL)

/* Version reporting */
typedef struct _LDAPVersion {
	int                     sdk_version;    /* Version of the SDK, * 100 */
	int                     protocol_version; /* Highest protocol version
												 supported by the SDK, * 100 */
	int                     SSL_version;    /* SSL version if this SDK
											   version supports it, * 100 */
	int                     security_level; /* highest level available */
	int                     reserved[4];
} LDAPVersion;
#define LDAP_SECURITY_NONE      0

#define LDAP_URL_ERR_NOTLDAP    1       /* URL doesn't begin with "ldap://" */
#define LDAP_URL_ERR_NODN       2       /* URL has no DN (required) */
#define LDAP_URL_ERR_BADSCOPE   3       /* URL scope string is invalid */
#define LDAP_URL_ERR_MEM        4       /* can't allocate memory space */
#define LDAP_URL_ERR_PARAM	5	/* bad parameter to an URL function */

/* possible result types a server can return */
#define LDAP_RES_BIND                   0x61L	/* 97 */
#define LDAP_RES_SEARCH_ENTRY           0x64L	/* 100 */
#define LDAP_RES_SEARCH_RESULT          0x65L	/* 101 */
#define LDAP_RES_MODIFY                 0x67L	/* 103 */
#define LDAP_RES_ADD                    0x69L	/* 105 */
#define LDAP_RES_DELETE                 0x6bL	/* 107 */
#define LDAP_RES_MODRDN                 0x6dL	/* 109 */
#define LDAP_RES_RENAME			0x6dL	/* same as LDAP_RES_MODRDN */
#define LDAP_RES_COMPARE                0x6fL	/* 111 */
#define LDAP_RES_SEARCH_REFERENCE       0x73L	/* 115 */
#define LDAP_RES_EXTENDED               0x78L	/* 120 */
#define LDAP_RES_ANY                    (-1L)

/* authentication methods available */
#define LDAP_AUTH_NONE          0x00L
#define LDAP_AUTH_SIMPLE        0x80L
#define LDAP_AUTH_SASL		0xa3L

/* supported SASL methods */
#define LDAP_SASL_SIMPLE	0	/* special value used for simple bind */
#define LDAP_SASL_EXTERNAL	"EXTERNAL"


/* search scopes */
#define LDAP_SCOPE_BASE         0x00
#define LDAP_SCOPE_ONELEVEL     0x01
#define LDAP_SCOPE_SUBTREE      0x02

/* alias dereferencing */
#define LDAP_DEREF_NEVER        0
#define LDAP_DEREF_SEARCHING    1
#define LDAP_DEREF_FINDING      2
#define LDAP_DEREF_ALWAYS       3

/* size/time limits */
#define LDAP_NO_LIMIT           0

/* allowed values for "all" ldap_result() parameter */
#define LDAP_MSG_ONE		0
#define LDAP_MSG_ALL		1
#define LDAP_MSG_RECEIVED	2

/* possible error codes we can be returned */
#define LDAP_SUCCESS                    0x00	/* 0 */
#define LDAP_OPERATIONS_ERROR           0x01	/* 1 */
#define LDAP_PROTOCOL_ERROR             0x02	/* 2 */
#define LDAP_TIMELIMIT_EXCEEDED         0x03	/* 3 */
#define LDAP_SIZELIMIT_EXCEEDED         0x04	/* 4 */
#define LDAP_COMPARE_FALSE              0x05	/* 5 */
#define LDAP_COMPARE_TRUE               0x06	/* 6 */
#define LDAP_AUTH_METHOD_NOT_SUPPORTED  0x07	/* 7 */
#define LDAP_STRONG_AUTH_NOT_SUPPORTED  LDAP_AUTH_METHOD_NOT_SUPPORTED
#define LDAP_STRONG_AUTH_REQUIRED       0x08	/* 8 */
#define LDAP_PARTIAL_RESULTS            0x09	/* 9 (UMich LDAPv2 extn) */
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

#define NAME_ERROR(n)   ((n & 0xf0) == 0x20)

#define LDAP_INAPPROPRIATE_AUTH         0x30	/* 48 */
#define LDAP_INVALID_CREDENTIALS        0x31	/* 49 */
#define LDAP_INSUFFICIENT_ACCESS        0x32	/* 50 */
#define LDAP_BUSY                       0x33	/* 51 */
#define LDAP_UNAVAILABLE                0x34	/* 52 */
#define LDAP_UNWILLING_TO_PERFORM       0x35	/* 53 */
#define LDAP_LOOP_DETECT                0x36	/* 54 */

#define LDAP_SORT_CONTROL_MISSING       0x3C	/* 60 */

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
 * LDAPv3 server controls we know about
 */
#define LDAP_CONTROL_MANAGEDSAIT	"2.16.840.1.113730.3.4.2"
#define LDAP_CONTROL_SORTREQUEST	"1.2.840.113556.1.4.473"
#define LDAP_CONTROL_SORTRESPONSE	"1.2.840.113556.1.4.474"
#define LDAP_CONTROL_PERSISTENTSEARCH	"2.16.840.1.113730.3.4.3"
#define LDAP_CONTROL_ENTRYCHANGE	"2.16.840.1.113730.3.4.7"
#define LDAP_CONTROL_VLVREQUEST    	"2.16.840.1.113730.3.4.9"
#define LDAP_CONTROL_VLVRESPONSE	"2.16.840.1.113730.3.4.10"
/* Password information sent back to client */
#define LDAP_CONTROL_PWEXPIRED		"2.16.840.1.113730.3.4.4"
#define LDAP_CONTROL_PWEXPIRING		"2.16.840.1.113730.3.4.5"

/*
 * Client controls we know about
 */
#define LDAP_CONTROL_REFERRALS		"1.2.840.113556.1.4.616"

/* function prototypes for ldap library */
#ifndef LDAP_API
#if defined( _WINDOWS ) || defined( _WIN32 )
#define LDAP_API(rt) rt
#else /* _WINDOWS */
#define LDAP_API(rt) rt
#endif /* _WINDOWS */
#endif /* LDAP_API */

typedef int (LDAP_CALL LDAP_CALLBACK LDAP_REBINDPROC_CALLBACK)( LDAP *ld, 
	char **dnp, char **passwdp, int *authmethodp, int freeit, void *arg);

typedef const struct berval* (LDAP_C LDAP_CALLBACK
	LDAP_KEYGEN_CALLBACK)( void *arg, LDAP *ld, LDAPMessage *entry );
typedef int (LDAP_C LDAP_CALLBACK
	LDAP_KEYCMP_CALLBACK)( void *arg, const struct berval*, const struct berval* );
typedef void (LDAP_C LDAP_CALLBACK
	LDAP_KEYFREE_CALLBACK)( void *arg, const struct berval* );

typedef int (LDAP_C LDAP_CALLBACK LDAP_CMP_CALLBACK)(const char*, 
	const char*);

typedef int (LDAP_C LDAP_CALLBACK LDAP_VALCMP_CALLBACK)(const char**, 
	const char**);


typedef int (LDAP_C LDAP_CALLBACK LDAP_CANCELPROC_CALLBACK)( void *cl );

LDAP_API(LDAP *) LDAP_CALL ldap_open( const char *host, int port );
LDAP_API(LDAP *) LDAP_CALL ldap_init( const char *defhost, int defport );
LDAP_API(int) LDAP_CALL ldap_set_option( LDAP *ld, int option, void *optdata );
LDAP_API(int) LDAP_CALL ldap_get_option( LDAP *ld, int option, void *optdata );
LDAP_API(int) LDAP_CALL ldap_unbind( LDAP *ld );
LDAP_API(int) LDAP_CALL ldap_unbind_s( LDAP *ld );
LDAP_API(int) LDAP_CALL ldap_version( LDAPVersion *ver );

/*
 * perform ldap operations and obtain results
 */
LDAP_API(int) LDAP_CALL ldap_abandon( LDAP *ld, int msgid );
LDAP_API(int) LDAP_CALL ldap_add( LDAP *ld, const char *dn, LDAPMod **attrs );
LDAP_API(int) LDAP_CALL ldap_add_s( LDAP *ld, const char *dn, LDAPMod **attrs );
LDAP_API(void) LDAP_CALL ldap_set_rebind_proc( LDAP *ld, 
	LDAP_REBINDPROC_CALLBACK *rebindproc, void *arg );
LDAP_API(int) LDAP_CALL ldap_simple_bind( LDAP *ld, const char *who,
	const char *passwd );
LDAP_API(int) LDAP_CALL ldap_simple_bind_s( LDAP *ld, const char *who,
	const char *passwd );
LDAP_API(int) LDAP_CALL ldap_modify( LDAP *ld, const char *dn, LDAPMod **mods );
LDAP_API(int) LDAP_CALL ldap_modify_s( LDAP *ld, const char *dn, 
	LDAPMod **mods );
LDAP_API(int) LDAP_CALL ldap_modrdn( LDAP *ld, const char *dn, 
	const char *newrdn );
LDAP_API(int) LDAP_CALL ldap_modrdn_s( LDAP *ld, const char *dn, 
	const char *newrdn );
LDAP_API(int) LDAP_CALL ldap_modrdn2( LDAP *ld, const char *dn, 
	const char *newrdn, int deleteoldrdn );
LDAP_API(int) LDAP_CALL ldap_modrdn2_s( LDAP *ld, const char *dn, 
	const char *newrdn, int deleteoldrdn);
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
LDAP_API(int) LDAP_CALL ldap_result( LDAP *ld, int msgid, int all,
	struct timeval *timeout, LDAPMessage **result );
LDAP_API(int) LDAP_CALL ldap_msgfree( LDAPMessage *lm );
LDAP_API(void) LDAP_CALL ldap_mods_free( LDAPMod **mods, int freemods );
LDAP_API(int) LDAP_CALL ldap_msgid( LDAPMessage *lm );
LDAP_API(int) LDAP_CALL ldap_msgtype( LDAPMessage *lm );

/*
 * parse/create controls
 */
LDAP_API(int) LDAP_CALL ldap_create_sort_control( LDAP *ld,
	LDAPsortkey **sortKeyList, const char ctl_iscritical,
	LDAPControl **ctrlp );
LDAP_API(int) LDAP_CALL ldap_parse_sort_control( LDAP *ld,
	LDAPControl **ctrls, unsigned long *result, char **attribute );
LDAP_API(int) LDAP_CALL ldap_controls_count( LDAPControl **ctrls );

/*
 * parse/deal with results and errors returned
 */
LDAP_API(int) LDAP_CALL ldap_get_lderrno( LDAP *ld, char **m, char **s );
LDAP_API(int) LDAP_CALL ldap_set_lderrno( LDAP *ld, int e, char *m, char *s );
LDAP_API(int) LDAP_CALL ldap_result2error( LDAP *ld, LDAPMessage *r, 
	int freeit );
LDAP_API(char *) LDAP_CALL ldap_err2string( int err );
LDAP_API(void) LDAP_CALL ldap_perror( LDAP *ld, const char *s );
LDAP_API(LDAPMessage *) LDAP_CALL ldap_first_entry( LDAP *ld, 
	LDAPMessage *chain );
LDAP_API(LDAPMessage *) LDAP_CALL ldap_next_entry( LDAP *ld, 
	LDAPMessage *entry );
LDAP_API(int) LDAP_CALL ldap_count_entries( LDAP *ld, LDAPMessage *chain );
LDAP_API(char *) LDAP_CALL ldap_get_dn( LDAP *ld, LDAPMessage *entry );
LDAP_API(char *) LDAP_CALL ldap_dn2ufn( const char *dn );
LDAP_API(char **) LDAP_CALL ldap_explode_dn( const char *dn, 
	const int notypes );
LDAP_API(char **) LDAP_CALL ldap_explode_rdn( const char *dn, 
	const int notypes );
LDAP_API(char **) LDAP_CALL ldap_explode_dns( const char *dn );
LDAP_API(char *) LDAP_CALL ldap_first_attribute( LDAP *ld, LDAPMessage *entry,
	BerElement **ber );
LDAP_API(char *) LDAP_CALL ldap_next_attribute( LDAP *ld, LDAPMessage *entry,
	BerElement *ber );
LDAP_API(void) LDAP_CALL ldap_ber_free( BerElement *ber, int freebuf );
LDAP_API(char **) LDAP_CALL ldap_get_values( LDAP *ld, LDAPMessage *entry,
	const char *target );
LDAP_API(struct berval **) LDAP_CALL ldap_get_values_len( LDAP *ld,
	LDAPMessage *entry, const char *target );
LDAP_API(char **) LDAP_CALL ldap_get_lang_values( LDAP *ld, LDAPMessage *entry,
	const char *target, char **type );
LDAP_API(struct berval **) LDAP_CALL ldap_get_lang_values_len( LDAP *ld,
	LDAPMessage *entry, const char *target, char **type );
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
	const char *mechanism, struct berval *cred, LDAPControl **serverctrls,
	LDAPControl **clientctrls, int *msgidp );
LDAP_API(int) LDAP_CALL ldap_sasl_bind_s( LDAP *ld, const char *dn,
	const char *mechanism, struct berval *cred, LDAPControl **serverctrls,
	LDAPControl **clientctrls, struct berval **servercredp );
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
	const char *attr, struct berval *bvalue, LDAPControl **serverctrls,
	LDAPControl **clientctrls, int *msgidp );
LDAP_API(int) LDAP_CALL ldap_compare_ext_s( LDAP *ld, const char *dn,
	const char *attr, struct berval *bvalue, LDAPControl **serverctrls,
	LDAPControl **clientctrls );
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
	const char *requestoid, struct berval *requestdata,
	LDAPControl **serverctrls, LDAPControl **clientctrls, int *msgidp );
LDAP_API(int) LDAP_CALL ldap_extended_operation_s( LDAP *ld,
	const char *requestoid, struct berval *requestdata,
	LDAPControl **serverctrls, LDAPControl **clientctrls,
	char **retoidp, struct berval **retdatap );

/* 
 * Routines for manipulating sort key lists, used for making sort controls
 */

LDAP_API(void) LDAP_CALL ldap_free_sort_keylist (LDAPsortkey **sortKeyList);
LDAP_API(int) LDAP_CALL ldap_create_sort_keylist (LDAPsortkey ***sortKeyList,char *string_rep);


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

/*
 * virtual list view support
 */

LDAP_API(int) LDAP_CALL ldap_create_virtuallist_control( LDAP *ld, 
        LDAPVirtualList *ldvlistp, LDAPControl **ctrlp );

LDAP_API(int) LDAP_CALL ldap_parse_virtuallist_control( LDAP *ld,
        LDAPControl **ctrls, unsigned long *target_posp, 
	unsigned long *list_sizep, int *errcodep );

/*
 * entry sorting routines
 */
LDAP_API(int) LDAP_CALL ldap_keysort_entries( LDAP *ld, LDAPMessage **chain,
	void *arg, LDAP_KEYGEN_CALLBACK *gen, LDAP_KEYCMP_CALLBACK *cmp, LDAP_KEYFREE_CALLBACK *fre);
LDAP_API(int) LDAP_CALL ldap_multisort_entries( LDAP *ld, LDAPMessage **chain,
	char **attr, LDAP_CMP_CALLBACK *cmp);
LDAP_API(int) LDAP_CALL ldap_sort_entries( LDAP *ld, LDAPMessage **chain, 
	char *attr, LDAP_CMP_CALLBACK *cmp);
LDAP_API(int) LDAP_CALL ldap_sort_values( LDAP *ld, char **vals, 
	LDAP_VALCMP_CALLBACK *cmp);
LDAP_API(int) LDAP_C LDAP_CALLBACK ldap_sort_strcasecmp( const char **a, 
	const char **b );

/*
 * getfilter routines
 */
LDAP_API(LDAPFiltDesc *) LDAP_CALL ldap_init_getfilter( char *fname );
LDAP_API(LDAPFiltDesc *) LDAP_CALL ldap_init_getfilter_buf( char *buf, 
	long buflen );
LDAP_API(LDAPFiltInfo *) LDAP_CALL ldap_getfirstfilter( LDAPFiltDesc *lfdp,
	char *tagpat, char *value );
LDAP_API(LDAPFiltInfo *) LDAP_CALL ldap_getnextfilter( LDAPFiltDesc *lfdp );
LDAP_API(int) LDAP_CALL ldap_set_filter_additions( LDAPFiltDesc *lfdp, 
	char *prefix, char *suffix );
LDAP_API(int) LDAP_CALL ldap_create_filter( char *buf, unsigned long buflen,
	char *pattern, char *prefix, char *suffix, char *attr,
	char *value, char **valwords );
LDAP_API(void) LDAP_CALL ldap_getfilter_free( LDAPFiltDesc *lfdp );

/*
 * friendly routines
 */
LDAP_API(char *) LDAP_CALL ldap_friendly_name( char *filename, char *name,
	FriendlyMap *map );
LDAP_API(void) LDAP_CALL ldap_free_friendlymap( FriendlyMap *map );

/*
 * ldap url routines
 */
LDAP_API(int) LDAP_CALL ldap_is_ldap_url( char *url );
LDAP_API(int) LDAP_CALL ldap_url_parse( char *url, LDAPURLDesc **ludpp );
LDAP_API(void) LDAP_CALL ldap_free_urldesc( LDAPURLDesc *ludp );
LDAP_API(int) LDAP_CALL ldap_url_search( LDAP *ld, char *url, int attrsonly );
LDAP_API(int) LDAP_CALL ldap_url_search_s( LDAP *ld, char *url, int attrsonly,
	LDAPMessage **res );
LDAP_API(int) LDAP_CALL ldap_url_search_st( LDAP *ld, char *url, int attrsonly,
	struct timeval *timeout, LDAPMessage **res );

/*
 * routines for creating persistent search controls and for handling
 * "entry changed notification" controls (an LDAPv3 extension).
 */
#define LDAP_CHANGETYPE_ADD		1
#define LDAP_CHANGETYPE_DELETE		2
#define LDAP_CHANGETYPE_MODIFY		4
#define LDAP_CHANGETYPE_MODDN		8
#define LDAP_CHANGETYPE_ANY		(1|2|4|8)
LDAP_API(int) LDAP_CALL ldap_create_persistentsearch_control( LDAP *ld, 
    int changetypes, int changesonly, int return_echg_ctls,
    char ctl_iscritical, LDAPControl **ctrlp );
LDAP_API(int) LDAP_CALL ldap_parse_entrychange_control( LDAP *ld,
	LDAPControl **ctrls, int *chgtypep, char **prevdnp,
	int *chgnumpresentp, long *chgnump );

typedef struct ldapmemcache  LDAPMemCache;  /* opaque in-memory cache handle */

/*
 * cacheing routines
 */
LDAP_API(int) LDAP_CALL ldap_memcache_init( unsigned long ttl,
	unsigned long size, char **baseDNs, struct ldap_thread_fns *thread_fns, 
	LDAPMemCache **cachep );
LDAP_API(int) LDAP_CALL ldap_memcache_set( LDAP *ld, LDAPMemCache *cache );
LDAP_API(int) LDAP_CALL ldap_memcache_get( LDAP *ld, LDAPMemCache **cachep );
LDAP_API(void) LDAP_CALL ldap_memcache_flush( LDAPMemCache *cache, char *dn,
	int scope );
LDAP_API(void) LDAP_CALL ldap_memcache_destroy( LDAPMemCache *cache );
LDAP_API(void) LDAP_CALL ldap_memcache_update( LDAPMemCache *cache );


/*
 * DNS resolver callbacks.  for now, we only use gethostbyname()
 */
typedef struct LDAPHostEnt {
    char	*ldaphe_name;		/* official name of host */
    char	**ldaphe_aliases;	/* alias list */
    int		ldaphe_addrtype;	/* host address type */
    int		ldaphe_length;		/* length of address */
    char	**ldaphe_addr_list;	/* list of addresses from name server */
} LDAPHostEnt;

typedef LDAPHostEnt * (LDAP_C LDAP_CALLBACK LDAP_DNSFN_GETHOSTBYNAME)(
	const char *name, LDAPHostEnt *result, char *buffer,
	int buflen, int *statusp, void *extradata );
typedef LDAPHostEnt * (LDAP_C LDAP_CALLBACK LDAP_DNSFN_GETHOSTBYADDR)(
	const char *addr, int length, int type, LDAPHostEnt *result,
	char *buffer, int buflen, int *statusp, void *extradata );

struct ldap_dns_fns {
	void				*lddnsfn_extradata;
	int				lddnsfn_bufsize;
	LDAP_DNSFN_GETHOSTBYNAME	*lddnsfn_gethostbyname;
	LDAP_DNSFN_GETHOSTBYADDR	*lddnsfn_gethostbyaddr;
};

#define LDAP_OPT_DNS_FN_PTRS		96	/* Netscape extension */

/************** the functions in this section are experimental ***************/

/*
 * libldap memory allocation callback functions.  These are global and can
 * not be set on a per-LDAP session handle basis.  Install your own
 * functions by making a call like this:
 *    ldap_set_option( NULL, LDAP_OPT_MEMALLOC_FN_PTRS, &memalloc_fns );
 *
 * look in lber.h for the function typedefs themselves.
 */
struct ldap_memalloc_fns {
	LDAP_MALLOC_CALLBACK	*ldapmem_malloc;
	LDAP_CALLOC_CALLBACK	*ldapmem_calloc;
	LDAP_REALLOC_CALLBACK	*ldapmem_realloc;
	LDAP_FREE_CALLBACK	*ldapmem_free;
};
#define LDAP_OPT_MEMALLOC_FN_PTRS	97	/* Netscape extension */

#define LDAP_OPT_RECONNECT		98	/* Netscape extension */

#define LDAP_OPT_EXTRA_THREAD_FN_PTRS  101	/* Netscape extension */

typedef int (LDAP_C LDAP_CALLBACK LDAP_TF_MUTEX_TRYLOCK_CALLBACK)( void * );
typedef void *(LDAP_C LDAP_CALLBACK LDAP_TF_SEMA_ALLOC_CALLBACK)( void );
typedef void (LDAP_C LDAP_CALLBACK LDAP_TF_SEMA_FREE_CALLBACK)( void * );
typedef int (LDAP_C LDAP_CALLBACK LDAP_TF_SEMA_WAIT_CALLBACK)( void * );
typedef int (LDAP_C LDAP_CALLBACK LDAP_TF_SEMA_POST_CALLBACK)( void * );

struct ldap_extra_thread_fns {
        LDAP_TF_MUTEX_TRYLOCK_CALLBACK *ltf_mutex_trylock;
        LDAP_TF_SEMA_ALLOC_CALLBACK *ltf_sema_alloc;
        LDAP_TF_SEMA_FREE_CALLBACK *ltf_sema_free;
        LDAP_TF_SEMA_WAIT_CALLBACK *ltf_sema_wait;
        LDAP_TF_SEMA_POST_CALLBACK *ltf_sema_post;
};



#define LDAP_OPT_ASYNC_CONNECT          99      /* Netscape extension */
#define LDAP_OPT_ASYNC_RECONNECT_FN_PTR 100     /* Netscape extension */


/* 
 * this function sets the connect status of the ld so that a client 
 * can do dns and connect, and then tell the sdk to ignore the connect phase 
 */ 

LDAP_API(int) LDAP_CALL ldap_set_connected( LDAP *ld, 
					    const int currentstatus );   

/* 
 * callback definition for reconnect request from a referral 
 */ 

typedef int( LDAP_C LDAP_CALLBACK LDAP_ASYNC_RECONNECT)( LBER_SOCKET, 
							 struct sockaddr *, 
							 int ); 

struct ldap_async_connect_fns 
{ 
    LDAP_ASYNC_RECONNECT *lac_reconnect; 
}; 

 
/************************ end of experimental section ************************/


/********** the functions, etc. below are unsupported at this time ***********/
#ifdef LDAP_DNS
#define LDAP_OPT_DNS			12
#endif

#define LDAP_OPT_HOST_NAME		48	/* MSFT extension */
#define LDAP_OPT_RETURN_REFERRALS	51	/* MSFT extension */


/*
 * generalized bind
 */
LDAP_API(int) LDAP_CALL ldap_bind( LDAP *ld, const char *who, 
	const char *passwd, int authmethod );
LDAP_API(int) LDAP_CALL ldap_bind_s( LDAP *ld, const char *who, 
	const char *cred, int method );

/*
 * experimental DN format support
 */
LDAP_API(int) LDAP_CALL ldap_is_dns_dn( const char *dn );


/*
 * user friendly naming/searching routines
 */
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

/*
 * UTF-8 routines (should these move into libnls?)
 */
/* number of bytes in character */
LDAP_API(int) LDAP_CALL ldap_utf8len( const char* );
/* find next character */
LDAP_API(char*) LDAP_CALL ldap_utf8next( char* );
/* find previous character */
LDAP_API(char*) LDAP_CALL ldap_utf8prev( char* );
/* copy one character */
LDAP_API(int) LDAP_CALL ldap_utf8copy( char* dst, const char* src );
/* total number of characters */
LDAP_API(size_t) LDAP_CALL ldap_utf8characters( const char* );
/* get one UCS-4 character, and move *src to the next character */
LDAP_API(unsigned long) LDAP_CALL ldap_utf8getcc( const char** src );
/* UTF-8 aware strtok_r() */
LDAP_API(char*) LDAP_CALL ldap_utf8strtok_r( char* src, const char* brk, char** next);

/* like isalnum(*s) in the C locale */
LDAP_API(int) LDAP_CALL ldap_utf8isalnum( char* s );
/* like isalpha(*s) in the C locale */
LDAP_API(int) LDAP_CALL ldap_utf8isalpha( char* s );
/* like isdigit(*s) in the C locale */
LDAP_API(int) LDAP_CALL ldap_utf8isdigit( char* s );
/* like isxdigit(*s) in the C locale */
LDAP_API(int) LDAP_CALL ldap_utf8isxdigit(char* s );
/* like isspace(*s) in the C locale */
LDAP_API(int) LDAP_CALL ldap_utf8isspace( char* s );

#define LDAP_UTF8LEN(s)  ((0x80 & *(unsigned char*)(s)) ?   ldap_utf8len (s) : 1)
#define LDAP_UTF8NEXT(s) ((0x80 & *(unsigned char*)(s)) ?   ldap_utf8next(s) : (s)+1)
#define LDAP_UTF8INC(s)  ((0x80 & *(unsigned char*)(s)) ? s=ldap_utf8next(s) : ++s)

#define LDAP_UTF8PREV(s)   ldap_utf8prev(s)
#define LDAP_UTF8DEC(s) (s=ldap_utf8prev(s))

#define LDAP_UTF8COPY(d,s) ((0x80 & *(unsigned char*)(s)) ? ldap_utf8copy(d,s) : ((*(d) = *(s)), 1))
#define LDAP_UTF8GETCC(s) ((0x80 & *(unsigned char*)(s)) ? ldap_utf8getcc (&s) : *s++)
#define LDAP_UTF8GETC(s) ((0x80 & *(unsigned char*)(s)) ? ldap_utf8getcc ((const char**)&s) : *s++)

/*
 * functions that have been replaced by new improved ones
 */
/* use ldap_create_filter() instead of ldap_build_filter() */
LDAP_API(void) LDAP_CALL ldap_build_filter( char *buf, unsigned long buflen,
	char *pattern, char *prefix, char *suffix, char *attr,
	char *value, char **valwords );
/* use ldap_set_filter_additions() instead of ldap_setfilteraffixes() */
LDAP_API(void) LDAP_CALL ldap_setfilteraffixes( LDAPFiltDesc *lfdp, 
	char *prefix, char *suffix );

/* cache strategies */
#define LDAP_CACHE_CHECK                0
#define LDAP_CACHE_POPULATE             1
#define LDAP_CACHE_LOCALDB              2

typedef int (LDAP_C LDAP_CALLBACK LDAP_CF_BIND_CALLBACK)( LDAP *, int,
	unsigned long, const char *, struct berval *, int );
typedef int (LDAP_C LDAP_CALLBACK LDAP_CF_UNBIND_CALLBACK)( LDAP *, int,
	unsigned long );
typedef int (LDAP_C LDAP_CALLBACK LDAP_CF_SEARCH_CALLBACK)( LDAP *,
	int, unsigned long, const char *, int, const char LDAP_CALLBACK *,
	char **, int );
typedef int (LDAP_C LDAP_CALLBACK LDAP_CF_COMPARE_CALLBACK)( LDAP *, int,
	unsigned long, const char *, const char *, struct berval * );
typedef int (LDAP_C LDAP_CALLBACK LDAP_CF_ADD_CALLBACK)( LDAP *, int,
	unsigned long, const char *, LDAPMod ** );
typedef int (LDAP_C LDAP_CALLBACK LDAP_CF_DELETE_CALLBACK)( LDAP *, int,
	unsigned long, const char * );
typedef int (LDAP_C LDAP_CALLBACK LDAP_CF_MODIFY_CALLBACK)( LDAP *, int,
	unsigned long, const char *, LDAPMod ** );
typedef int (LDAP_C LDAP_CALLBACK LDAP_CF_MODRDN_CALLBACK)( LDAP *, int,
	unsigned long, const char *, const char *, int );
typedef int (LDAP_C LDAP_CALLBACK LDAP_CF_RESULT_CALLBACK)( LDAP *, int,
	int, struct timeval *, LDAPMessage ** );
typedef int (LDAP_C LDAP_CALLBACK LDAP_CF_FLUSH_CALLBACK)( LDAP *,
	const char *, const char * );

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

/*********************** end of unsupported functions ************************/

#ifdef __cplusplus
}
#endif
#endif /* _LDAP_H */
