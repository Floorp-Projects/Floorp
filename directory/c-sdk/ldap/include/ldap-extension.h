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

/* ldap-extension.h - extensions to the ldap c api specification */

#ifndef _LDAP_EXTENSION_H
#define _LDAP_EXTENSION_H

#ifdef __cplusplus
extern "C" {
#endif

#define LDAP_PORT_MAX           65535           /* API extension */
#define LDAP_VERSION1           1               /* API extension */
#define LDAP_VERSION            LDAP_VERSION2   /* API extension */


/*
 * C LDAP features we support that are not (yet) part of the LDAP C API
 * Internet Draft.  Use the ldap_get_option() call with an option value of
 * LDAP_OPT_API_FEATURE_INFO to retrieve information about a feature.
 *
 * Note that this list is incomplete; it includes only the most widely
 * used extensions.  Also, the version is 1 for all of these for now.
 */
#define LDAP_API_FEATURE_SERVER_SIDE_SORT       1
#define LDAP_API_FEATURE_VIRTUAL_LIST_VIEW      1
#define LDAP_API_FEATURE_PERSISTENT_SEARCH      1
#define LDAP_API_FEATURE_PROXY_AUTHORIZATION    1
#define LDAP_API_FEATURE_X_LDERRNO              1
#define LDAP_API_FEATURE_X_MEMCACHE             1
#define LDAP_API_FEATURE_X_IO_FUNCTIONS         1
#define LDAP_API_FEATURE_X_EXTIO_FUNCTIONS      1
#define LDAP_API_FEATURE_X_DNS_FUNCTIONS        1
#define LDAP_API_FEATURE_X_MEMALLOC_FUNCTIONS   1
#define LDAP_API_FEATURE_X_THREAD_FUNCTIONS     1
#define LDAP_API_FEATURE_X_EXTHREAD_FUNCTIONS   1
#define LDAP_API_FEATURE_X_GETLANGVALUES        1
#define LDAP_API_FEATURE_X_CLIENT_SIDE_SORT     1
#define LDAP_API_FEATURE_X_URL_FUNCTIONS        1
#define LDAP_API_FEATURE_X_FILTER_FUNCTIONS     1

#define LDAP_ROOT_DSE           ""              /* API extension */

#define LDAP_OPT_DESC                   0x01    /*  1 */

#define NULLMSG ((LDAPMessage *)0)

/*built-in SASL methods */
#define LDAP_SASL_EXTERNAL      "EXTERNAL"      /* TLS/SSL extension */

/* possible error codes we can be returned */
#define LDAP_PARTIAL_RESULTS            0x09    /* 9 (UMich LDAPv2 extn) */
#define NAME_ERROR(n)   ((n & 0xf0) == 0x20)

#define LDAP_SORT_CONTROL_MISSING       0x3C    /* 60 (server side sort extn) */
#define LDAP_INDEX_RANGE_ERROR          0x3D    /* 61 (VLV extn) */

/*
 * LDAPv3 server controls we know about
 */
#define LDAP_CONTROL_MANAGEDSAIT        "2.16.840.1.113730.3.4.2"
#define LDAP_CONTROL_SORTREQUEST        "1.2.840.113556.1.4.473"
#define LDAP_CONTROL_SORTRESPONSE       "1.2.840.113556.1.4.474"
#define LDAP_CONTROL_PERSISTENTSEARCH   "2.16.840.1.113730.3.4.3"
#define LDAP_CONTROL_ENTRYCHANGE        "2.16.840.1.113730.3.4.7"
#define LDAP_CONTROL_VLVREQUEST         "2.16.840.1.113730.3.4.9"
#define LDAP_CONTROL_VLVRESPONSE        "2.16.840.1.113730.3.4.10"
#define LDAP_CONTROL_PROXYAUTH          "2.16.840.1.113730.3.4.12" /* version 1
*/
#define LDAP_CONTROL_PROXIEDAUTH        "2.16.840.1.113730.3.4.18" /* version 2
*/

/* Authentication request and response controls */
#define LDAP_CONTROL_AUTH_REQUEST       "2.16.840.1.113730.3.4.16"
#define LDAP_CONTROL_AUTH_RESPONSE      "2.16.840.1.113730.3.4.15"

/* Password information sent back to client */
#define LDAP_CONTROL_PWEXPIRED          "2.16.840.1.113730.3.4.4"
#define LDAP_CONTROL_PWEXPIRING         "2.16.840.1.113730.3.4.5"

/* Suppress virtual/inherited attribute values */
#define LDAP_CONTROL_REAL_ATTRS_ONLY	"2.16.840.1.113730.3.4.17"

/* Only return virtual/inherited attribute values */
#define LDAP_CONTROL_VIRTUAL_ATTRS_ONLY	"2.16.840.1.113730.3.4.19"


LDAP_API(void) LDAP_CALL ldap_ber_free( BerElement *ber, int freebuf );

/*
 * Server side sorting of search results (an LDAPv3 extension --
 * LDAP_API_FEATURE_SERVER_SIDE_SORT)
 */
typedef struct LDAPsortkey {    /* structure for a sort-key */
        char *  sk_attrtype;
        char *  sk_matchruleoid;
        int     sk_reverseorder;
} LDAPsortkey;

LDAP_API(int) LDAP_CALL ldap_create_sort_control( LDAP *ld,
        LDAPsortkey **sortKeyList, const char ctl_iscritical,
        LDAPControl **ctrlp );
LDAP_API(int) LDAP_CALL ldap_parse_sort_control( LDAP *ld,
        LDAPControl **ctrls, unsigned long *result, char **attribute );

LDAP_API(void) LDAP_CALL ldap_free_sort_keylist( LDAPsortkey **sortKeyList );
LDAP_API(int) LDAP_CALL ldap_create_sort_keylist( LDAPsortkey ***sortKeyList,
        const char *string_rep );


/*
 * Virtual list view (an LDAPv3 extension -- LDAP_API_FEATURE_VIRTUAL_LIST_VIEW)
 */
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
 * VLV functions:
 */
LDAP_API(int) LDAP_CALL ldap_create_virtuallist_control( LDAP *ld,
        LDAPVirtualList *ldvlistp, LDAPControl **ctrlp );

LDAP_API(int) LDAP_CALL ldap_parse_virtuallist_control( LDAP *ld,
        LDAPControl **ctrls, unsigned long *target_posp,
        unsigned long *list_sizep, int *errcodep );

/*
 * Routines for creating persistent search controls and for handling
 * "entry changed notification" controls (an LDAPv3 extension --
 * LDAP_API_FEATURE_PERSISTENT_SEARCH)
 */
#define LDAP_CHANGETYPE_ADD             1
#define LDAP_CHANGETYPE_DELETE          2
#define LDAP_CHANGETYPE_MODIFY          4
#define LDAP_CHANGETYPE_MODDN           8
#define LDAP_CHANGETYPE_ANY             (1|2|4|8)
LDAP_API(int) LDAP_CALL ldap_create_persistentsearch_control( LDAP *ld,
        int changetypes, int changesonly, int return_echg_ctls,
        char ctl_iscritical, LDAPControl **ctrlp );
LDAP_API(int) LDAP_CALL ldap_parse_entrychange_control( LDAP *ld,
        LDAPControl **ctrls, int *chgtypep, char **prevdnp,
        int *chgnumpresentp, long *chgnump );

/*
 * Routines for creating Proxied Authorization controls (an LDAPv3
 * extension -- LDAP_API_FEATURE_PROXY_AUTHORIZATION)
 * ldap_create_proxyauth_control() is for the old (version 1) control.
 * ldap_create_proxiedauth_control() is for the newer (version 2) control.
 */
LDAP_API(int) LDAP_CALL ldap_create_proxyauth_control( LDAP *ld,
        const char *dn, const char ctl_iscritical, LDAPControl **ctrlp );
LDAP_API(int) LDAP_CALL ldap_create_proxiedauth_control( LDAP *ld,
        const char *authzid, LDAPControl **ctrlp );

/*
 * Functions to get and set LDAP error information (API extension --
 * LDAP_API_FEATURE_X_LDERRNO )
 *
 * By using LDAP_OPT_THREAD_FN_PTRS, you can arrange for the error info. to
 * be thread-specific.
 */
LDAP_API(int) LDAP_CALL ldap_get_lderrno( LDAP *ld, char **m, char **s );
LDAP_API(int) LDAP_CALL ldap_set_lderrno( LDAP *ld, int e, char *m, char *s );


/*
 * LDAP URL functions and definitions (an API extension --
 * LDAP_API_FEATURE_X_URL_FUNCTIONS)
 */
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

/*
 * possible errors returned by ldap_url_parse()
 */
#define LDAP_URL_ERR_NOTLDAP    1       /* URL doesn't begin with "ldap://" */
#define LDAP_URL_ERR_NODN       2       /* URL has no DN (required) */
#define LDAP_URL_ERR_BADSCOPE   3       /* URL scope string is invalid */
#define LDAP_URL_ERR_MEM        4       /* can't allocate memory space */
#define LDAP_URL_ERR_PARAM      5       /* bad parameter to an URL function */
#define LDAP_URL_UNRECOGNIZED_CRITICAL_EXTENSION	6

/*
 * URL functions:
 */
LDAP_API(int) LDAP_CALL ldap_is_ldap_url( const char *url );
LDAP_API(int) LDAP_CALL ldap_url_parse( const char *url, LDAPURLDesc **ludpp );
LDAP_API(void) LDAP_CALL ldap_free_urldesc( LDAPURLDesc *ludp );
LDAP_API(int) LDAP_CALL ldap_url_search( LDAP *ld, const char *url,
        int attrsonly );
LDAP_API(int) LDAP_CALL ldap_url_search_s( LDAP *ld, const char *url,
        int attrsonly, LDAPMessage **res );
LDAP_API(int) LDAP_CALL ldap_url_search_st( LDAP *ld, const char *url,
        int attrsonly, struct timeval *timeout, LDAPMessage **res );


/*
 * Function to dispose of an array of LDAPMod structures (an API extension).
 * Warning: don't use this unless the mods array was allocated using the
 * same memory allocator as is being used by libldap.
 */
LDAP_API(void) LDAP_CALL ldap_mods_free( LDAPMod **mods, int freemods );

/*
 * SSL option (an API extension):
 */
#define LDAP_OPT_SSL                    0x0A    /* 10 - API extension */

/*
 * Referral hop limit (an API extension):
 */
#define LDAP_OPT_REFERRAL_HOP_LIMIT     0x10    /* 16 - API extension */

/*
 * Rebind callback function (an API extension)
 */
#define LDAP_OPT_REBIND_FN              0x06    /* 6 - API extension */
#define LDAP_OPT_REBIND_ARG             0x07    /* 7 - API extension */
typedef int (LDAP_CALL LDAP_CALLBACK LDAP_REBINDPROC_CALLBACK)( LDAP *ld,
        char **dnp, char **passwdp, int *authmethodp, int freeit, void *arg);
LDAP_API(void) LDAP_CALL ldap_set_rebind_proc( LDAP *ld,
        LDAP_REBINDPROC_CALLBACK *rebindproc, void *arg );

/*
 * Thread function callbacks (an API extension --
 * LDAP_API_FEATURE_X_THREAD_FUNCTIONS).
 */
#define LDAP_OPT_THREAD_FN_PTRS         0x05    /* 5 - API extension */

/*
 * Thread callback functions:
 */
typedef void *(LDAP_C LDAP_CALLBACK LDAP_TF_MUTEX_ALLOC_CALLBACK)( void );
typedef void (LDAP_C LDAP_CALLBACK LDAP_TF_MUTEX_FREE_CALLBACK)( void *m );
typedef int (LDAP_C LDAP_CALLBACK LDAP_TF_MUTEX_LOCK_CALLBACK)( void *m );
typedef int (LDAP_C LDAP_CALLBACK LDAP_TF_MUTEX_UNLOCK_CALLBACK)( void *m );
typedef int (LDAP_C LDAP_CALLBACK LDAP_TF_GET_ERRNO_CALLBACK)( void );
typedef void (LDAP_C LDAP_CALLBACK LDAP_TF_SET_ERRNO_CALLBACK)( int e );
typedef int (LDAP_C LDAP_CALLBACK LDAP_TF_GET_LDERRNO_CALLBACK)(
        char **matchedp, char **errmsgp, void *arg );
typedef void    (LDAP_C LDAP_CALLBACK LDAP_TF_SET_LDERRNO_CALLBACK)( int err,
        char *matched, char *errmsg, void *arg );

/*
 * Structure to hold thread function pointers:
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
 * Extended I/O function callbacks option (an API extension --
 * LDAP_API_FEATURE_X_EXTIO_FUNCTIONS).
 */
#define LDAP_X_OPT_EXTIO_FN_PTRS   (LDAP_OPT_PRIVATE_EXTENSION_BASE + 0x0F00)
        /* 0x4000 + 0x0F00 = 0x4F00 = 20224 - API extension */

/*
 * These extended I/O function callbacks echo the BSD socket API but accept
 * an extra pointer parameter at the end of their argument list that can
 * be used by client applications for their own needs.  For some of the calls,
 * the pointer is a session argument of type struct lextiof_session_private *
 * that is associated with the LDAP session handle (LDAP *).  For others, the
 * pointer is a socket specific struct lextiof_socket_private * argument that
 * is associated with a particular socket (a TCP connection).
 *
 * The lextiof_session_private and lextiof_socket_private structures are not
 * defined by the LDAP C API; users of this extended I/O interface should
 * define these themselves.
 *
 * The combination of the integer socket number (i.e., lpoll_fd, which is
 * the value returned by the CONNECT callback) and the application specific
 * socket argument (i.e., lpoll_socketarg, which is the value set in *sockargpp
 * by the CONNECT callback) must be unique.
 *
 * The types for the extended READ and WRITE callbacks are actually in lber.h.
 *
 * The CONNECT callback gets passed both the session argument (sessionarg)
 * and a pointer to a socket argument (socketargp) so it has the
 * opportunity to set the socket-specific argument.  The CONNECT callback
 * also takes a timeout parameter whose value can be set by calling
 * ldap_set_option( ld, LDAP_X_OPT_..., &val ).  The units used for the
 * timeout parameter are milliseconds.
 *
 * A POLL interface is provided instead of a select() one.  The timeout is
 * in milliseconds.
 *
 * A NEWHANDLE callback function is also provided.  It is called right
 * after the LDAP session handle is created, e.g., during ldap_init().
 * If the NEWHANDLE callback returns anything other than LDAP_SUCCESS,
 * the session handle allocation fails.
 *
 * A DISPOSEHANDLE callback function is also provided.  It is called right
 * before the LDAP session handle and its contents are destroyed, e.g.,
 * during ldap_unbind().
 */

/*
 * Special timeout values for poll and connect:
 */
#define LDAP_X_IO_TIMEOUT_NO_WAIT       0       /* return immediately */
#define LDAP_X_IO_TIMEOUT_NO_TIMEOUT    (-1)    /* block indefinitely */

/* LDAP poll()-like descriptor:
 */
typedef struct ldap_x_pollfd {     /* used by LDAP_X_EXTIOF_POLL_CALLBACK */
    int         lpoll_fd;          /* integer file descriptor / socket */
    struct lextiof_socket_private
                *lpoll_socketarg;
                                   /* pointer socket and for use by */
                                   /* application */
    short       lpoll_events;      /* requested event */
    short       lpoll_revents;     /* returned event */
} LDAP_X_PollFD;

/* Event flags for lpoll_events and lpoll_revents:
 */
#define LDAP_X_POLLIN    0x01  /* regular data ready for reading */
#define LDAP_X_POLLPRI   0x02  /* high priority data available */
#define LDAP_X_POLLOUT   0x04  /* ready for writing */
#define LDAP_X_POLLERR   0x08  /* error occurred -- only in lpoll_revents */
#define LDAP_X_POLLHUP   0x10  /* connection closed -- only in lpoll_revents */
#define LDAP_X_POLLNVAL  0x20  /* invalid lpoll_fd -- only in lpoll_revents */

/* Options passed to LDAP_X_EXTIOF_CONNECT_CALLBACK to modify socket behavior:
 */
#define LDAP_X_EXTIOF_OPT_NONBLOCKING   0x01  /* turn on non-blocking mode */
#define LDAP_X_EXTIOF_OPT_SECURE        0x02  /* turn on 'secure' mode */


/* extended I/O callback function prototypes:
 */
typedef int     (LDAP_C LDAP_CALLBACK LDAP_X_EXTIOF_CONNECT_CALLBACK )(
            const char *hostlist, int port, /* host byte order */
            int timeout /* milliseconds */,
            unsigned long options, /* bitmapped options */
            struct lextiof_session_private *sessionarg,
            struct lextiof_socket_private **socketargp );
typedef int     (LDAP_C LDAP_CALLBACK LDAP_X_EXTIOF_CLOSE_CALLBACK )(
            int s, struct lextiof_socket_private *socketarg );
typedef int     (LDAP_C LDAP_CALLBACK LDAP_X_EXTIOF_POLL_CALLBACK)(
            LDAP_X_PollFD fds[], int nfds, int timeout /* milliseconds */,
            struct lextiof_session_private *sessionarg );
typedef int     (LDAP_C LDAP_CALLBACK LDAP_X_EXTIOF_NEWHANDLE_CALLBACK)(
            LDAP *ld, struct lextiof_session_private *sessionarg );
typedef void    (LDAP_C LDAP_CALLBACK LDAP_X_EXTIOF_DISPOSEHANDLE_CALLBACK)(
            LDAP *ld, struct lextiof_session_private *sessionarg );


/* Structure to hold extended I/O function pointers:
 */
struct ldap_x_ext_io_fns {
        /* lextiof_size should always be set to LDAP_X_EXTIO_FNS_SIZE */
        int                                     lextiof_size;
        LDAP_X_EXTIOF_CONNECT_CALLBACK          *lextiof_connect;
        LDAP_X_EXTIOF_CLOSE_CALLBACK            *lextiof_close;
        LDAP_X_EXTIOF_READ_CALLBACK             *lextiof_read;
        LDAP_X_EXTIOF_WRITE_CALLBACK            *lextiof_write;
        LDAP_X_EXTIOF_POLL_CALLBACK             *lextiof_poll;
        LDAP_X_EXTIOF_NEWHANDLE_CALLBACK        *lextiof_newhandle;
        LDAP_X_EXTIOF_DISPOSEHANDLE_CALLBACK    *lextiof_disposehandle;
        void                                    *lextiof_session_arg;
        LDAP_X_EXTIOF_WRITEV_CALLBACK           *lextiof_writev;
};
#define LDAP_X_EXTIO_FNS_SIZE   sizeof(struct ldap_x_ext_io_fns)

/*
 * Utility functions for parsing space-separated host lists (useful for
 * implementing an extended I/O CONNECT callback function).
 */
struct ldap_x_hostlist_status;
LDAP_API(int) LDAP_CALL ldap_x_hostlist_first( const char *hostlist,
        int defport, char **hostp, int *portp /* host byte order */,
        struct ldap_x_hostlist_status **statusp );
LDAP_API(int) LDAP_CALL ldap_x_hostlist_next( char **hostp,
        int *portp /* host byte order */, struct ldap_x_hostlist_status *status
);
LDAP_API(void) LDAP_CALL ldap_x_hostlist_statusfree(
        struct ldap_x_hostlist_status *status );

/*
 * Client side sorting of entries (an API extension --
 * LDAP_API_FEATURE_X_CLIENT_SIDE_SORT)
 */
/*
 * Client side sorting callback functions:
 */
typedef const struct berval* (LDAP_C LDAP_CALLBACK
        LDAP_KEYGEN_CALLBACK)( void *arg, LDAP *ld, LDAPMessage *entry );
typedef int (LDAP_C LDAP_CALLBACK
        LDAP_KEYCMP_CALLBACK)( void *arg, const struct berval*,
        const struct berval* );
typedef void (LDAP_C LDAP_CALLBACK
        LDAP_KEYFREE_CALLBACK)( void *arg, const struct berval* );
typedef int (LDAP_C LDAP_CALLBACK
        LDAP_CMP_CALLBACK)(const char *val1, const char *val2);
typedef int (LDAP_C LDAP_CALLBACK
        LDAP_VALCMP_CALLBACK)(const char **val1p, const char **val2p);

/*
 * Client side sorting functions:
 */
LDAP_API(int) LDAP_CALL ldap_keysort_entries( LDAP *ld, LDAPMessage **chain,
        void *arg, LDAP_KEYGEN_CALLBACK *gen, LDAP_KEYCMP_CALLBACK *cmp,
        LDAP_KEYFREE_CALLBACK *fre );
LDAP_API(int) LDAP_CALL ldap_multisort_entries( LDAP *ld, LDAPMessage **chain,
        char **attr, LDAP_CMP_CALLBACK *cmp );
LDAP_API(int) LDAP_CALL ldap_sort_entries( LDAP *ld, LDAPMessage **chain,
        char *attr, LDAP_CMP_CALLBACK *cmp );
LDAP_API(int) LDAP_CALL ldap_sort_values( LDAP *ld, char **vals,
        LDAP_VALCMP_CALLBACK *cmp );
LDAP_API(int) LDAP_C LDAP_CALLBACK ldap_sort_strcasecmp( const char **a,
        const char **b );


/*
 * Filter functions and definitions (an API extension --
 * LDAP_API_FEATURE_X_FILTER_FUNCTIONS)
 */
/*
 * Structures, constants, and types for filter utility routines:
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
 * Filter utility functions:
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
 * Friendly mapping structure and routines (an API extension)
 */
typedef struct friendly {
        char    *f_unfriendly;
        char    *f_friendly;
} *FriendlyMap;
LDAP_API(char *) LDAP_CALL ldap_friendly_name( char *filename, char *name,
        FriendlyMap *map );
LDAP_API(void) LDAP_CALL ldap_free_friendlymap( FriendlyMap *map );

/*
 * In Memory Cache (an API extension -- LDAP_API_FEATURE_X_MEMCACHE)
 */
typedef struct ldapmemcache  LDAPMemCache;  /* opaque in-memory cache handle */

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
 * Timeout value for nonblocking connect call
 */
#define LDAP_X_OPT_CONNECT_TIMEOUT    (LDAP_OPT_PRIVATE_EXTENSION_BASE + 0x0F01)
        /* 0x4000 + 0x0F01 = 0x4F01 = 20225 - API extension */

/*
 * Memory allocation callback functions (an API extension --
 * LDAP_API_FEATURE_X_MEMALLOC_FUNCTIONS).  These are global and can
 * not be set on a per-LDAP session handle basis.  Install your own
 * functions by making a call like this:
 *    ldap_set_option( NULL, LDAP_OPT_MEMALLOC_FN_PTRS, &memalloc_fns );
 *
 * look in lber.h for the function typedefs themselves.
 */
#define LDAP_OPT_MEMALLOC_FN_PTRS       0x61    /* 97 - API extension */

struct ldap_memalloc_fns {
        LDAP_MALLOC_CALLBACK    *ldapmem_malloc;
        LDAP_CALLOC_CALLBACK    *ldapmem_calloc;
        LDAP_REALLOC_CALLBACK   *ldapmem_realloc;
        LDAP_FREE_CALLBACK      *ldapmem_free;
};


/*
 * Memory allocation functions (an API extension)
 */
void *ldap_x_malloc( size_t size );
void *ldap_x_calloc( size_t nelem, size_t elsize );
void *ldap_x_realloc( void *ptr, size_t size );
void ldap_x_free( void *ptr );

/*
 * Server reconnect (an API extension).
 */
#define LDAP_OPT_RECONNECT              0x62    /* 98 - API extension */


/*
 * Extra thread callback functions (an API extension --
 * LDAP_API_FEATURE_X_EXTHREAD_FUNCTIONS)
 */
#define LDAP_OPT_EXTRA_THREAD_FN_PTRS  0x65     /* 101 - API extension */

typedef int (LDAP_C LDAP_CALLBACK LDAP_TF_MUTEX_TRYLOCK_CALLBACK)( void *m );
typedef void *(LDAP_C LDAP_CALLBACK LDAP_TF_SEMA_ALLOC_CALLBACK)( void );
typedef void (LDAP_C LDAP_CALLBACK LDAP_TF_SEMA_FREE_CALLBACK)( void *s );
typedef int (LDAP_C LDAP_CALLBACK LDAP_TF_SEMA_WAIT_CALLBACK)( void *s );
typedef int (LDAP_C LDAP_CALLBACK LDAP_TF_SEMA_POST_CALLBACK)( void *s );
typedef void *(LDAP_C LDAP_CALLBACK LDAP_TF_THREADID_CALLBACK)(void);

struct ldap_extra_thread_fns {
        LDAP_TF_MUTEX_TRYLOCK_CALLBACK *ltf_mutex_trylock;
        LDAP_TF_SEMA_ALLOC_CALLBACK *ltf_sema_alloc;
        LDAP_TF_SEMA_FREE_CALLBACK *ltf_sema_free;
        LDAP_TF_SEMA_WAIT_CALLBACK *ltf_sema_wait;
        LDAP_TF_SEMA_POST_CALLBACK *ltf_sema_post;
        LDAP_TF_THREADID_CALLBACK *ltf_threadid_fn;
};

/*
 * Debugging level (an API extension)
 */
#define LDAP_OPT_DEBUG_LEVEL            0x6E    /* 110 - API extension */
/* On UNIX, there's only one copy of ldap_debug */
/* On NT, each dll keeps its own module_ldap_debug, which */
/* points to the process' ldap_debug and needs initializing after load */
#ifdef _WIN32
extern int              *module_ldap_debug;
typedef void (*set_debug_level_fn_t)(int*);
#endif

#ifdef LDAP_DNS
#define LDAP_OPT_DNS                    0x0C    /* 12 - API extension */
#endif

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
LDAP_API(char*) LDAP_CALL ldap_utf8strtok_r( char* src, const char* brk, char**
next);

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
#define LDAP_UTF8NEXT(s) ((0x80 & *(unsigned char*)(s)) ?   ldap_utf8next(s) : ( s)+1)
#define LDAP_UTF8INC(s)  ((0x80 & *(unsigned char*)(s)) ? s=ldap_utf8next(s) : ++s)

#define LDAP_UTF8PREV(s)   ldap_utf8prev(s)
#define LDAP_UTF8DEC(s) (s=ldap_utf8prev(s))

#define LDAP_UTF8COPY(d,s) ((0x80 & *(unsigned char*)(s)) ? ldap_utf8copy(d,s) : ((*(d) = *(s)), 1))
#define LDAP_UTF8GETCC(s) ((0x80 & *(unsigned char*)(s)) ? ldap_utf8getcc (&s) : *s++)
#define LDAP_UTF8GETC(s) ((0x80 & *(unsigned char*)(s)) ? ldap_utf8getcc ((const char**)&s) : *s++)


#ifdef __cplusplus
}
#endif
#endif /* _LDAP_EXTENSION_H */

