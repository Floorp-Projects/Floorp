/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   This file should contain prototypes of all public functions for 
   the network module in the client library.  Also all NET lib
   related public data structures should be in here.

   This file will be included automatically when source includes "xp.h".
   By the time this file is included, all global typedefs have been 
   executed.
*/

#if defined(CookieManagement)
/* #define TRUST_LABELS 1 */
#endif


/* make sure we only include this once */
#ifndef _NET_PROTO_H_
#define _NET_PROTO_H_

#include "xp_core.h"
#include "prio.h"
#include "ntypes.h"
#include "shistele.h"
#include "xp_list.h"
#include "msgtypes.h"

#ifdef XP_UNIX
#include <sys/param.h> /* for MAXPATHLEN */
#endif

/* larubbio */
#include "mcom_db.h"
/* #include "jri.h" */

/*
    prototypes for external structures
*/
typedef struct _net_MemoryCacheObject net_MemoryCacheObject;

/* ------------------------------------------------------------------------ */
/* ============= Typedefs for the network module structures =============== */ 

#define VIEW_SOURCE_URL_PREFIX  "view-source:"
#define NETHELP_URL_PREFIX      "nethelp:"

/* spider begin */
#define MARIMBA_URL_PREFIX      "castanet:"
/* spider end */

/* Format out codes
 *
 * Notice that there is no wildcard for format out.
 * you must register a wildcard format_in and
 * regular format_out pair for every format_out
 * listed or used in the application.  That way
 * there can never be a mime pair that doesn't
 * match.
 */
#define SET_PRESENT_BIT(X,B)    ((FO_Present_Types) ( (int)X | B ))
#define CLEAR_PRESENT_BIT(X,B)  ((FO_Present_Types) ( (int)X & ~(B) ))
#define CLEAR_CACHE_BIT(X)      CLEAR_PRESENT_BIT(X,FO_CACHE_ONLY)

/*  If you add a FO_TYPE below for the client, please tell a windows
 *      front end person that is working on your product immediately.
 *      GAB
 */
#define FO_PRESENT              1
#define FO_INTERNAL_IMAGE       2
#define FO_OUT_TO_PROXY_CLIENT  3
#define FO_VIEW_SOURCE          4
#define FO_SAVE_AS              5
#define FO_SAVE_AS_TEXT         6
#define FO_SAVE_AS_POSTSCRIPT   7
#define FO_QUOTE_MESSAGE        8   /* very much like FO_SAVE_AS_TEXT */
#define FO_MAIL_TO              9
#define FO_OLE_NETWORK          10
#define FO_MULTIPART_IMAGE      11
#define FO_EMBED                12
#define FO_PRINT                13
#define FO_PLUGIN               14
#define FO_APPEND_TO_FOLDER     15  /* used by the mail/news code */
#define FO_DO_JAVA              16
#define FO_BYTERANGE            17
#define FO_EDT_SAVE_IMAGE       18
#define FO_EDIT                 19
#define FO_LOAD_HTML_HELP_MAP_FILE 20
#define FO_SAVE_TO_NEWS_DB      21 /* used by off-line news for art. caching */
#define FO_OPEN_DRAFT           22 /* used by the mail/news code */
#define FO_PRESENT_INLINE       23 /* like FO_PRESENT, but doesn't blow away */
#define FO_OBJECT               24 /* see layobj.c */
#define FO_FONT                 25 /* Downloadable Fonts */
#define FO_SOFTWARE_UPDATE      26 /* Software updates */
#define FO_JAVASCRIPT_CONFIG    27
#define FO_QUOTE_HTML_MESSAGE   28
#define FO_CMDLINE_ATTACHMENTS  29 /* for initializing compose window with attachments via commandline */
#define FO_MAIL_MESSAGE_TO      30  /* for S/MIME */
#define FO_LOCATION_INDEPENDENCE 31 /* Location independence stuff */
#define FO_RDF          32
#define FO_CRAWL_PAGE           33 /* for crawling HTML pages */
#define FO_CRAWL_RESOURCE       34 /* for crawling images and resources */
#define FO_ROBOTS_TXT           35 /* web robots control */
#define FO_XMLCSS               36 /* CSS for formatting XML */
#define FO_XMLHTML              37 /* HTML inclusions in XML */
#define FO_NGLAYOUT             38 /* NGLayout streams */
#define FO_AUTOUPDATE           39 
#define FO_LASTFORMAT           40 /* If you create another FO_Format, bump this value! */

/* bitfield detectable CACHE FO's
 */
#define FO_CACHE_ONLY                   64
#define FO_CACHE_AND_PRESENT            (FO_CACHE_ONLY | FO_PRESENT)
#define FO_CACHE_AND_INTERNAL_IMAGE     (FO_CACHE_ONLY | FO_INTERNAL_IMAGE)
#define FO_CACHE_AND_OUT_TO_PROXY_CLIENT (FO_CACHE_ONLY | \
                                          FO_OUT_TO_PROXY_CLIENT)
#define FO_CACHE_AND_VIEW_SOURCE        (FO_CACHE_ONLY | FO_VIEW_SOURCE)
#define FO_CACHE_AND_SAVE_AS            (FO_CACHE_ONLY | FO_SAVE_AS)
#define FO_CACHE_AND_SAVE_AS_TEXT       (FO_CACHE_ONLY | FO_SAVE_AS_TEXT)
#define FO_CACHE_AND_SAVE_AS_POSTSCRIPT (FO_CACHE_ONLY | FO_SAVE_AS_POSTSCRIPT)
#define FO_CACHE_AND_QUOTE_MESSAGE      (FO_CACHE_ONLY | FO_QUOTE_MESSAGE)
#define FO_CACHE_AND_MAIL_TO            (FO_CACHE_ONLY | FO_MAIL_TO)
#define FO_CACHE_AND_MAIL_MESSAGE_TO    (FO_CACHE_ONLY | FO_MAIL_MESSAGE_TO)
#define FO_CACHE_AND_OLE_NETWORK        (FO_CACHE_ONLY | FO_OLE_NETWORK)
#define FO_CACHE_AND_MULTIPART_IMAGE    (FO_CACHE_ONLY | FO_MULTIPART_IMAGE)
#define FO_CACHE_AND_EMBED              (FO_CACHE_ONLY | FO_EMBED)
#define FO_CACHE_AND_PRINT              (FO_CACHE_ONLY | FO_PRINT)
#define FO_CACHE_AND_PLUGIN             (FO_CACHE_ONLY | FO_PLUGIN)
#define FO_CACHE_AND_APPEND_TO_FOLDER   (FO_CACHE_ONLY | FO_APPEND_TO_FOLDER)
#define FO_CACHE_AND_DO_JAVA            (FO_CACHE_ONLY | FO_DO_JAVA)
#define FO_CACHE_AND_BYTERANGE          (FO_CACHE_ONLY | FO_BYTERANGE)
#define FO_CACHE_AND_EDIT               (FO_CACHE_ONLY | FO_EDIT)
#define FO_CACHE_AND_LOAD_HTML_HELP_MAP_FILE    (FO_CACHE_ONLY | FO_LOAD_HTML_HELP_MAP_FILE)
#define FO_CACHE_AND_PRESENT_INLINE     (FO_CACHE_ONLY | FO_PRESENT_INLINE)
#define FO_CACHE_AND_OBJECT             (FO_CACHE_ONLY | FO_OBJECT)
#define FO_CACHE_AND_FONT               (FO_CACHE_ONLY | FO_FONT)
#define FO_CACHE_AND_JAVASCRIPT_CONFIG   (FO_CACHE_ONLY | FO_JAVASCRIPT_CONFIG)
#define FO_CACHE_AND_SOFTUPDATE   (FO_CACHE_ONLY | FO_SOFTWARE_UPDATE)
#define FO_CACHE_AND_RDF   (FO_CACHE_ONLY | FO_RDF)
#define FO_CACHE_AND_XMLCSS   (FO_CACHE_ONLY | FO_XMLCSS)
#define FO_CACHE_AND_XMLHTML   (FO_CACHE_ONLY | FO_XMLHTML)
#define FO_CACHE_AND_CRAWL_PAGE         (FO_CACHE_ONLY | FO_CRAWL_PAGE)
#define FO_CACHE_AND_CRAWL_RESOURCE     (FO_CACHE_ONLY | FO_CRAWL_RESOURCE)
#define FO_CACHE_AND_ROBOTS_TXT         (FO_CACHE_ONLY | FO_ROBOTS_TXT)
#define FO_CACHE_AND_NGLAYOUT           (FO_CACHE_ONLY | FO_NGLAYOUT)
#define FO_CACHE_AND_AUTOUPDATE         (FO_CACHE_ONLY | FO_AUTOUPDATE)

/* bitfield detectable ONLY_FROM_CACHE FO's
 */
#define FO_ONLY_FROM_CACHE                      128
#define FO_ONLY_FROM_CACHE_AND_PRESENT          (FO_ONLY_FROM_CACHE | \
                                                 FO_PRESENT)
#define FO_ONLY_FROM_CACHE_AND_INTERNAL_IMAGE   (FO_ONLY_FROM_CACHE | \
                                                 FO_INTERNAL_IMAGE)
#define FO_ONLY_FROM_CACHE_AND_OUT_TO_PROXY_CLIENT  (FO_ONLY_FROM_CACHE | \
                                                     FO_OUT_TO_PROXY_CLIENT)
#define FO_ONLY_FROM_CACHE_AND_VIEW_SOURCE      (FO_ONLY_FROM_CACHE | \
                                                 FO_VIEW_SOURCE)
#define FO_ONLY_FROM_CACHE_AND_SAVE_AS          (FO_ONLY_FROM_CACHE | \
                                                 FO_SAVE_AS)
#define FO_ONLY_FROM_CACHE_AND_SAVE_AS_TEXT     (FO_ONLY_FROM_CACHE | \
                                                 FO_SAVE_AS_TEXT)
#define FO_ONLY_FROM_CACHE_AND_SAVE_AS_POSTSCRIPT (FO_ONLY_FROM_CACHE | \
                                                   FO_SAVE_AS_POSTSCRIPT)
#define FO_ONLY_FROM_CACHE_AND_QUOTE_MESSAGE     (FO_ONLY_FROM_CACHE | \
                                                  FO_QUOTE_MESSAGE)
#define FO_ONLY_FROM_CACHE_AND_MAIL_TO          (FO_ONLY_FROM_CACHE | \
                                                 FO_MAIL_TO)
#define FO_ONLY_FROM_CACHE_AND_MAIL_MESSAGE_TO  (FO_ONLY_FROM_CACHE | \
                                                 FO_MAIL_MESSAGE_TO)
#define FO_ONLY_FROM_CACHE_AND_OLE_NETWORK      (FO_ONLY_FROM_CACHE | \
                                                 FO_OLE_NETWORK)
#define FO_ONLY_FROM_CACHE_AND_MULTIPART_IMAGE  (FO_ONLY_FROM_CACHE | \
                                                 FO_MULTIPART_IMAGE)
#define FO_ONLY_FROM_CACHE_AND_EMBED            (FO_ONLY_FROM_CACHE | FO_EMBED)

#define FO_ONLY_FROM_CACHE_AND_PRINT            (FO_ONLY_FROM_CACHE | FO_PRINT)

#define FO_ONLY_FROM_CACHE_AND_PLUGIN           (FO_ONLY_FROM_CACHE | FO_PLUGIN)

#define FO_ONLY_FROM_CACHE_AND_APPEND_TO_FOLDER (FO_ONLY_FROM_CACHE | \
                                                 FO_APPEND_TO_FOLDER)
#define FO_ONLY_FROM_CACHE_AND_DO_JAVA          (FO_ONLY_FROM_CACHE | FO_DO_JAVA)

#define FO_ONLY_FROM_CACHE_AND_BYTERANGE        (FO_ONLY_FROM_CACHE | FO_BYTERANGE)

#define FO_ONLY_FROM_CACHE_AND_EDIT             (FO_ONLY_FROM_CACHE | FO_EDIT)
#define FO_ONLY_FROM_CACHE_AND_LOAD_HTML_HELP_MAP_FILE      (FO_ONLY_FROM_CACHE | FO_LOAD_HTML_HELP_MAP_FILE)
#define FO_ONLY_FROM_CACHE_AND_PRESENT_INLINE               (FO_ONLY_FROM_CACHE | FO_PRESENT_INLINE)
#define FO_ONLY_FROM_CACHE_AND_NGLAYOUT         (FO_ONLY_FROM_CACHE | FO_NGLAYOUT)

#define MAX_FORMATS_OUT (FO_LASTFORMAT | FO_ONLY_FROM_CACHE)+1

typedef void
Net_GetUrlExitFunc (URL_Struct *URL_s, int status, MWContext *window_id);

typedef void
Net_GetUrlProgressFunc (URL_Struct *URL_s, int status, MWContext *window_id);


/* A structure to store all headers that can be passed into HTTPLOAD. 
 * Used for supporting URLConnection object of Java
 */
typedef struct _NET_AllHeaders { 
    char **key;                 /* The Key portion of each HTTP header */
    char **value;               /* The value portion of each HTTP header */
    uint32 empty_index;         /* pointer to the next free slot */
    uint32 max_index;           /* maximum no of free slots allocated */
} NET_AllHeaders;

/* Forward declaration to make compiler happy on OSF1 */
#if defined (OSF1)
#ifdef XP_CPLUSPLUS
class MSG_Pane; 
#endif
#endif

/* larubbio */
/* moved from mkextcac, this is used for 197 SAR caches, 
 * and external cache
 */
typedef struct _ExtCacheDBInfo {
    DB      *database;
    char    *filename;
    char    *path;
    char    *name;
    Bool    queried_this_session;
    int     type;
    uint32  DiskCacheSize; 
    uint32  NumberInDiskCache;
    uint32  MaxSize;
    void    *logFile;

} ExtCacheDBInfo;

typedef enum {
    Normal_priority,
    Prefetch_priority,              /* passed in to prefetch a URL in the background */
    CurrentlyPrefetching_priority   /* a prefetch url currently loading */
} URLLoadPriority;

/* passing in structure 
 *
 * use this structure to pass a URL and associated data
 * into the load_URL() routine
 *
 * This structure can then be used to look up associated
 * data after the transfer has taken place
 *
 *  WARNING!!  If you ever change this structure, there is a good chance
 *  you will have to also change History_entry, and the functions
 *  SHIST_CreateHistoryEntry and SHIST_CreateURLStructFromHistoryEntry which
 *  copy URLs to and from the history.  You may also need to change the
 *  net_CacheObject structure in mkcache.c, and the code therein which copies
 *  URLs to and from the memory and disk caches.
 */
struct URL_Struct_ {
    URLLoadPriority priority;       /* load priority */
    char *  address;            /* the URL part */

    char * username; /* Put username/password info into the URL struct without */
    char * password; /* using the address field.  The problem was that the user's password
                      * became visible on screen in a number of cases. Bug 35243, 39418 */

    char *  destination;        /* for copy and move methods */
    char *  IPAddressString;    /* ASCII string of IP address.
                                 * If this field exists, then NET_BeginConnect
                                 * should use it rather than the hostname in
                                 * the address.
                                 * This is here for Java DNS security firedrill
                                 *  see jar or jsw...
                                 */
    int     method;             /* post or get or ... */
    char *  referer;            /* What document points to this url */
    char *  post_data;          /* data to be posted */
    int32   post_data_size;     /* size of post_data */
    char *  range_header;       /* requested ranges (bytes usually) */
    char *  post_headers;       /* Additional headers for the post. 
                                 * Should end with \n */
    NET_ReloadMethod
            force_reload;       /* forced refresh? either NORMAL or SUPER */
    char *  http_headers;       /* headers that can be passed into
                                 * HTTPLoad.  Used to support
                                 * proxying
                                 */
    char  * window_target;      /* named window support */
    Chrome * window_chrome;     /* pointer to a Chrome structure that
                                 * if non-null specifies the characteristics
                                 * of the target window named in 
                                 * the window_target variable
                                 */

    /* attributes of the URL that are assembled
     * during and after transfer of the data
     */
    int      server_status;               /* status code returned by server */
    int32    content_length;              /* length of the incoming data */
    int32    real_content_length;         /* real length of the partial file */
    char   * content_type;                /* the content type of incoming 
                                           * data 
                                           */
    char   * content_name;                /* passed in name for object */
    char   * transfer_encoding;           /* first encoding of incoming data */
    char   * content_encoding;            /* encoding of incoming data */

    char   *x_mac_creator, *x_mac_type;   /* If we somehow know the Mac-types
                                             of this document, these are they.
                                             This is only of interest on Macs,
                                             and currently only happens with
                                             mail/news attachments. */

    char   * charset;                     /* charset of the data */
    char   * boundary;                    /* mixed multipart boundary */
    char   * redirecting_url;             /* the Location: of the redirect */
    char   * authenticate;                /* auth scheme */
    char   * proxy_authenticate;          /* proxy auth scheme */
    char   * protection_template;         /* name of server auth template */
    uint32   refresh;                     /* refresh interval */
    char   * refresh_url;                 /* url to refresh */
    char   * wysiwyg_url;                 /* URL for WYSIWYG printing/saving */
    time_t   expires;
    time_t   last_modified;
    time_t   server_date;                 /* Date: header returned by 
                                           * HTTP servers
                                           */
    char   * cache_file;                  
                                           /* if non-NULL then it contains
                                           * the filename of its local cache 
                                           * file containing it's data
                                           */
#ifdef NU_CACHE
    void*    cache_object;
#else
    net_MemoryCacheObject * memory_copy;  /* if non-NULL then it contains 
                                           * a structure with
                                           * a pointer to a list of 
                                           * memory segments
                                           * containing it's data
                                           */
#endif

    void   * fe_data;                     /* random front end data to be passed
                                           * up the line
                                           */

#if defined(XP_WIN) || defined(XP_OS2)
    void *ncapi_data;   /* Window ncapi data structure
                 * to track loads by URL and not context
                 */
#endif /* XP_WIN */
    uint32 localIP;                       /* a local ip address that should be
                                           * bound to the socket. */
               
    int     history_num;                  /* == add this to history list 
                                           * dflt TRUE 
                                           */

    uint32   position;                    /* for plug-in streams */
    uint32   low_range;                    
    uint32   high_range;                  /* these range tags represent
                                           * the byte offsets that
                                           * this URL contains.
                                           * if high_range is non-zero
                                           * then this URL represents
                                           * just a subset of the URL.
                                           */

    int32   position_tag;                 /* tag number of last position */

    SHIST_SavedData savedData;            /* layout data */

    Net_GetUrlExitFunc *pre_exit_fn;      /* If set, called just before the
                                             exit routine that was passed
                                             in to NET_GetURL. */

    /* Security information */
    int     security_on;                  /* is security on? */
    /* Security info on a connection is opaque */
    unsigned char *sec_info;
    unsigned char *redirect_sec_info;
    
    void   *sec_private;                  /* private libsec info */

    char   *error_msg;                   /* what went wrong */

    char  **files_to_post;               /* zero terminated array
                                          * of char * filenames
                                          * that need to be posted
                                          */
    char **post_to;               /* Only meaningful if files_to_post is set.  If post_to is non-NULL, it is a NULL-terminated 
                                            array of the URLs to post the files in files_to_post to.  If NULL, files_to_post along with 
                                            address(a directory) will be used to decide where to post the files (like before). */
  XP_Bool *add_crlf; /* Only meaningful if files_to_post is non-NULL.  If set, it specifies for each file in files_to_post,
                        whether or not to make all line endings be crlf. */
    uint32    auto_scroll;                /* set this if you want
                                           * the window to autoscroll
                                           * to the bottom at all times
                                           * The value represents
                                           * the number of lines to
                                           * buffer
                                           */
    PRPackedBool
            bypassProxy,                /* should we bypass the proxy (if any) */
            dontAllowDiffHostRedirect,  /* Do we want to allow a redirect from host A to host B */      
            post_data_is_file,          /* is the post_data field a filename? */
            address_modified,           /* was the address modified? */
            resize_reload,              /* persistent NET_RESIZE_RELOAD flag */
            use_local_copy,             /* ignore force reload */
            is_directory,               /* filled in upon return in to indicate
                                         * the url points to a directory 
                                         */
            preset_content_type,         /* set this if you want to pass in
                                          * a known content-type 
                                          */
            is_binary,                   /* should mailto and save-as be 
                                          * wary of this object 
                                          */
            is_active,                   /* is it an active stream */
            is_netsite,                  /* did it come from Netsite? */
            intr_on_load,                /* set this to interrupt any
                                          * existing transfers for the window
                                          * when this URL begins to load
                                          */
            internal_url,                 /* set to true if this
                                           * is a for internal use
                                           * only url.  If it's not
                                           * set for an internal
                                           * URL the netlib will
                                           * return an unrecognized url
                                           * error
                                           */
            ext_cache_file,               /* the cache file referenced
                                           * is from an external cache
                                           */

            must_cache,                   /* overrides cache logic, used by plugins */

            dont_cache,                   /* This gets set by a mime
                                           * header and tells us not
                                           * to cache the document
                                           */
            can_reuse_connection,         /* set when the connetion is reusable
                                           * as in HTTP/1.1 or NG.
                                           */
            server_can_do_byteranges,     /* can the server do byteranges */
            server_can_do_restart,        /* can the (ftp) server do restarts? */
   
            load_background,              /* Load in the "background".
                                             Suppress thermo, progress, etc. */
            mailto_post,                  /* is this a mailto: post? */
            allow_content_change,         /* Set to TRUE if we are allowed to change the
                                             content that it is displayed to the user.
                                             Currently only used for IMAP partial fetches. */
            content_modified,             /* TRUE if the content of this URL has been modified
                                             internally.  Used for IMAP partial fetches. */
            open_new_window_specified,    /* TRUE if the invoker of the URL has specifically
                                             set the open_new_window bit - otherwise, msg_GetURL
                                             will clear it. */
            open_new_window;              /* TRUE if the invoker of the URL wants a new window 
                                             to open */
#ifdef XP_CPLUSPLUS
    class MSG_Pane                *msg_pane; 
#else
    struct MSG_Pane               *msg_pane; 
#endif

    NET_AllHeaders all_headers;           /* All headers that can be passed into
                                           * HTTPLOAD. Used to support 
                                           * URLConnection object of Java
                                           */
    int ref_count;          /* how many people have a pointer to me */
    ExtCacheDBInfo *SARCache;                         /* a pointer to the structure telling
                                           * us which cache this is in, if it's 
                                           * NULL it's in the normal navigator
                                           * cache
                                           */ /* larubbio */
    int      owner_id;                    /* unique ID for each library:
                                                NPL (plugins) => 0x0000BAC0;
                                           */
    void    *owner_data;                  /* private data owned by whomever created the URL_Struct */           
    char    *page_services_url;
    char    *privacy_policy_url;
    char    *etag;                        /* HTTP/1.1 Etag */
    char    *origin_url;                  /* original referrer of javascript: URL */
#ifdef TRUST_LABELS
    XP_List *TrustList;                   /* the list of trust labels that came in the header */
#endif
};

#ifndef NU_CACHE /* Not on my branch you don't */
/*  silly macro's (methods) to access the URL Struct that nobody uses
 */
#define NET_URLStruct_Address(S)          S->address
#define NET_URLStruct_AddressModified(S)  S->address_modified
#define NET_URLStruct_PostData(S)         S->post_data
#define NET_URLStruct_Method(S)           S->method
#define NET_URLStruct_ContentLength(S)    S->content_length
#define NET_URLStruct_ContentType(S)      S->content_type

#endif
/* stream functions
 */
typedef unsigned int
(*MKStreamWriteReadyFunc) (NET_StreamClass *stream);

#define MAX_WRITE_READY (((unsigned) (~0) << 1) >> 1)   /* must be <= than MAXINT!!!!! */

typedef int 
(*MKStreamWriteFunc) (NET_StreamClass *stream, const char *str, int32 len);

typedef void 
(*MKStreamCompleteFunc) (NET_StreamClass *stream);

typedef void 
(*MKStreamAbortFunc) (NET_StreamClass *stream, int status);

/* streamclass function
 */
struct _NET_StreamClass {

    char      * name;          /* Just for diagnostics */

    MWContext * window_id;     /* used for progress messages, etc. */

    void      * data_object;   /* a pointer to whatever
                                * structure you wish to have
                                * passed to the routines below
                                * during writes, etc...
                                * 
                                * this data object should hold
                                * the document, document
                                * structure or a pointer to the
                                * document.
                                */

    MKStreamWriteReadyFunc  is_write_ready;   /* checks to see if the stream is ready
                                               * for writing.  Returns 0 if not ready
                                               * or the number of bytes that it can
                                               * accept for write
                                               */
    MKStreamWriteFunc       put_block;        /* writes a block of data to the stream */
    MKStreamCompleteFunc    complete;         /* normal end */
    MKStreamAbortFunc       abort;            /* abnormal end */

    Bool                    is_multipart;    /* is the stream part of a multipart sequence */
};

/*DSR040197 - new and improved position for BEGIN_PROTOS*/
XP_BEGIN_PROTOS
/*
 * this defines the prototype for
 * stream converter routines.
 */
typedef NET_StreamClass * NET_Converter (FO_Present_Types rep_out,      /* the output format */
                                         void * data_obj,     /* saved from registration */
                                         URL_Struct * URL_s,  /* the URL info */
                                         MWContext * context);/* window id */
   
/*
 * The ContentInfo structure.
 *
 * Currently, we support the following attributes:
 *
 * 1. Type: This identifies what kind of data is in the file.
 * 2. Encoding: Identifies any compression or otherwise content-independent
 *    transformation which has been applied to the file (uuencode, etc.)
 * 3. Language: Identifies the language a text document is in.
 * 4. Description: A text string describing the file.
 *
 * Multiple items are separated with a comma, e.g.
 * encoding="x-gzip, x-uuencode"
 */

typedef struct {
    char *type;
    char *encoding;
    char *language;
    char *desc;
    char *icon;
    char *alt_text;
    Bool  is_default; /* is it the default because a type couldn't be found? */
    void *fe_data;   /* used by fe to save ext. viewer/save to disk options */
} NET_cinfo;

/* this is a list item struct that holds extension lists and
 * cinfo structs.
 *
 * call cinfo_MasterListPointer to return 
 * the master list held by the net library
 */
typedef struct _cdata {
    int            num_exts;
    char         **exts;
    NET_cinfo      ci;
    Bool       is_external;  /* did this entry come from an external file? */
    Bool       is_modified;  /* Is this entry modified externally; default = False */
    Bool       is_local;    /* Is personal copy*/
    Bool       is_new;  /* indicate if this is 
                   added by user via helper */
    char*      src_string;  /* For output use */
    char*      pref_name; /* If the mimetype came from preferences */
} NET_cdataStruct;


#ifdef XP_UNIX
/* This is only used in UNIX */

typedef struct _mdata {
    char *contenttype;
    char *command;
    char *testcommand;
    char *label;
    char *printcommand;
    int32 stream_buffer_size;
    Bool  test_succeed;
    char* xmode;
    char*   src_string;  /* For output use */
    Bool  is_local;
    Bool  is_modified;
} NET_mdataStruct;
#endif



/* Defines for various MIME content-types and encodings.
   Whenever you type in a content-type, you should use one of these defines
   instead, to help catch typos, and make central management of them easier.
 */

#define AUDIO_WILDCARD                      "audio/*"
#define IMAGE_WILDCARD                      "image/*"

#define APPLICATION_APPLEFILE               "application/applefile"
#define APPLICATION_BINHEX                  "application/mac-binhex40"
#define APPLICATION_MACBINARY               "application/x-macbinary"
#define APPLICATION_COMPRESS                "application/x-compress"
#define APPLICATION_COMPRESS2               "application/compress"
#define APPLICATION_FORTEZZA_CKL            "application/x-fortezza-ckl"
#define APPLICATION_FORTEZZA_KRL            "application/x-fortezza-krl"
#define APPLICATION_GZIP                    "application/x-gzip"
#define APPLICATION_GZIP2                   "application/gzip"
#define APPLICATION_HTTP_INDEX              "application/http-index"
#define APPLICATION_JAVASCRIPT              "application/x-javascript"
#define APPLICATION_NETSCAPE_REVOCATION     "application/x-netscape-revocation"
#define APPLICATION_NS_PROXY_AUTOCONFIG     "application/x-ns-proxy-autoconfig"
#define APPLICATION_NS_JAVASCRIPT_AUTOCONFIG        "application/x-javascript-config"
#define APPLICATION_OCTET_STREAM            "application/octet-stream"
#define APPLICATION_PGP                     "application/pgp"
#define APPLICATION_PGP2                    "application/x-pgp-message"
#define APPLICATION_POSTSCRIPT              "application/postscript"
#define APPLICATION_PRE_ENCRYPTED           "application/pre-encrypted"
#define APPLICATION_UUENCODE                "application/x-uuencode"
#define APPLICATION_UUENCODE2               "application/x-uue"
#define APPLICATION_UUENCODE3               "application/uuencode"
#define APPLICATION_UUENCODE4               "application/uue"
#define APPLICATION_X509_CA_CERT            "application/x-x509-ca-cert"
#define APPLICATION_X509_SERVER_CERT        "application/x-x509-server-cert"
#define APPLICATION_X509_EMAIL_CERT         "application/x-x509-email-cert"
#define APPLICATION_X509_USER_CERT          "application/x-x509-user-cert"
#define APPLICATION_X509_CRL                "application/x-pkcs7-crl"
#define APPLICATION_XPKCS7_MIME             "application/x-pkcs7-mime"
#define APPLICATION_XPKCS7_SIGNATURE        "application/x-pkcs7-signature"
#define APPLICATION_WWW_FORM_URLENCODED     "application/x-www-form-urlencoded"
#define APPLICATION_OLEOBJECT               "application/oleobject"
#define APPLICATION_OLEOBJECT2              "application/x-oleobject"
#define APPLICATION_JAVAARCHIVE             "application/java-archive"
#define APPLICATION_MARIMBA                 "application/marimba"
#define APPLICATION_XMARIMBA                "application/x-marimba"

#define AUDIO_BASIC                         "audio/basic"

#define IMAGE_GIF                           "image/gif"
#define IMAGE_JPG                           "image/jpeg"
#define IMAGE_PJPG                          "image/pjpeg"
#define IMAGE_PNG                           "image/png"
#define IMAGE_PPM                           "image/x-portable-pixmap"
#define IMAGE_XBM                           "image/x-xbitmap"
#define IMAGE_XBM2                          "image/x-xbm"
#define IMAGE_XBM3                          "image/xbm"

#define MESSAGE_EXTERNAL_BODY               "message/external-body"
#define MESSAGE_NEWS                        "message/news"
#define MESSAGE_RFC822                      "message/rfc822"

#define MULTIPART_ALTERNATIVE               "multipart/alternative"
#define MULTIPART_APPLEDOUBLE               "multipart/appledouble"
#define MULTIPART_DIGEST                    "multipart/digest"
#define MULTIPART_HEADER_SET                "multipart/header-set"
#define MULTIPART_MIXED                     "multipart/mixed"
#define MULTIPART_PARALLEL                  "multipart/parallel"
#define MULTIPART_SIGNED                    "multipart/signed"
#define MULTIPART_RELATED                   "multipart/related"
#define SUN_ATTACHMENT                      "x-sun-attachment"

#define TEXT_ENRICHED                       "text/enriched"
#define TEXT_CALENDAR                       "text/calendar"
#define TEXT_HTML                           "text/html"
#define TEXT_MDL                            "text/mdl"
#define TEXT_PLAIN                          "text/plain"
#define TEXT_RICHTEXT                       "text/richtext"
#define TEXT_VCARD                          "text/x-vcard"
#define TEXT_CSS                            "text/css"
#define TEXT_JSSS                           "text/jsss"
#define TEXT_XML                            "text/xml"

#define VIDEO_MPEG                          "video/mpeg"

/* x-uuencode-apple-single. QuickMail made me do this. */
#define UUENCODE_APPLE_SINGLE               "x-uuencode-apple-single"

/* The standard MIME message-content-encoding values:
 */
#define ENCODING_7BIT                       "7bit"
#define ENCODING_8BIT                       "8bit"
#define ENCODING_BINARY                     "binary"
#define ENCODING_BASE64                     "base64"
#define ENCODING_QUOTED_PRINTABLE           "quoted-printable"

/* Some nonstandard encodings.  Note that the names are TOTALLY RANDOM,
   and code that looks for these in network-provided data must look for
   all the possibilities.
 */
#define ENCODING_COMPRESS                   "x-compress"
#define ENCODING_COMPRESS2                  "compress"
#define ENCODING_ZLIB                       "x-zlib"
#define ENCODING_ZLIB2                      "zlib"
#define ENCODING_GZIP                       "x-gzip"
#define ENCODING_GZIP2                      "gzip"
#define ENCODING_DEFLATE                    "x-deflate"
#define ENCODING_DEFLATE2                   "deflate"
#define ENCODING_UUENCODE                   "x-uuencode"
#define ENCODING_UUENCODE2                  "x-uue"
#define ENCODING_UUENCODE3                  "uuencode"
#define ENCODING_UUENCODE4                  "uue"

/* Some names of parameters that various MIME headers include.
 */
#define PARAM_PROTOCOL                      "protocol"
#define PARAM_MICALG                        "micalg"
#define PARAM_MICALG_MD2                    "rsa-md2"
#define PARAM_MICALG_MD5                    "rsa-md5"
#define PARAM_MICALG_SHA1                   "sha1"
#define PARAM_MICALG_SHA1_2                 "sha-1"
#define PARAM_MICALG_SHA1_3                 "rsa-sha1"
#define PARAM_MICALG_SHA1_4                 "rsa-sha-1"
#define PARAM_MICALG_SHA1_5                 "rsa-sha"
#define PARAM_X_MAC_CREATOR                 "x-mac-creator"
#define PARAM_X_MAC_TYPE                    "x-mac-type"


/* This is like text/html, but also implies that the charset is that of
   the window.  This type should not escape to the outside world!
 */
#define INTERNAL_PARSER                     "internal/parser"

/* This type represents a document whose type was unknown.  Note that this
   is not the same as "application/octet-stream".  UNKNOWN_CONTENT_TYPE is
   an internal marker used to distinguish between documents *without* a
   specified type, and documents which had their type explicitly specified
   as octet_stream.

   This type should not be allowed to escape to the outside world -- if we
   feed a content type to an external consumer, we must translate this type
   to application/octet-stream first.
 */
#define UNKNOWN_CONTENT_TYPE               "application/x-unknown-content-type"

/* I have no idea what these are... */
#define MAGNUS_INTERNAL_UNKNOWN             "magnus-internal/unknown"
/* pre-encrypted file that has passed the cache */
#define INTERNAL_PRE_ENCRYPTED              "internal/pre-encrypted"
/* html file that needs to be pre-encrypted */
#define INTERNAL_PRE_ENC_HTML               "internal/preenc-html"

#define NET_AUTH_FAILED_DONT_DISPLAY        1
#define NET_AUTH_FAILED_DISPLAY_DOCUMENT    2
#define NET_AUTH_SUCCEEDED                  3
#define NET_RETRY_WITH_AUTH                 4
#define NET_WAIT_FOR_AUTH                   5

#ifdef  XP_UNIX
#define NET_COMMAND_NETSCAPE        "internal"
#define NET_COMMAND_PLUGIN      "plugin:"
#define NET_COMMAND_SAVE_TO_DISK    "save"
#define NET_COMMAND_SAVE_BY_NETSCAPE    "plugin-error"
#define NET_COMMAND_UNKNOWN     "prompt"
#define NET_COMMAND_DELETED     "deleted"
#define NET_MOZILLA_FLAGS       "x-mozilla-flags"
#endif

/* this is the interface struct for password auth with
   the front-end dialog. this is a hack to bridge the
   new and old parts of netlib XXX */
typedef struct _NET_AuthClosure {
  char * msg;
  char * user;
  char * pass;
  void * _private;
} NET_AuthClosure;


/* the entry file info structure contains information about
 * an ftp directory entry broken down into components
 */
typedef struct _NET_FileEntryInfo {
    char      *  filename;
    NET_cinfo *  cinfo;
    char      *  content_type;
    int          special_type;
    time_t       date;
    int32        size;
    Bool         display;    /* show this entry? */
    int32        permissions;
} NET_FileEntryInfo;

#define NET_FILE_TYPE        0
#define NET_DIRECTORY        1
#define NET_SYM_LINK         2
#define NET_SYM_LINK_TO_DIR  3
#define NET_SYM_LINK_TO_FILE 4

/* bit wise or'able permissions */
#define NET_READ_PERMISSION     4
#define NET_WRITE_PERMISSION    2
#define NET_EXECUTE_PERMISSION  1

typedef struct _HTTPIndexParserData HTTPIndexParserData;

/* define HTML help mapping file parser */
#define HTML_HELP_ID_FOUND 999

/* ====================================================================== */
/* Prototypes for NetLib */

/* set this to see debug messages if compiled with -DDEBUG
 */
extern int MKLib_trace_flag;

/* load the HTML help mapping file and search for
 * the id or text to load a specific document
 */
extern void
NET_GetHTMLHelpFileFromMapFile(MWContext *context,
                               char *map_file_url,
                               char *id,
                               char *search_text);

/* print a text string in place of the NET_FileEntryInfo special_type int */
extern char * NET_PrintFileType(int special_type);

/* streamclass parser for application/http-index-format */
extern NET_StreamClass *
NET_HTTPIndexFormatToHTMLConverter(int         format_out,
                               void       *data_object,
                               URL_Struct *URL_s,
                               MWContext  *window_id);

/* parser routines to parse application/http-index-format
 */
extern HTTPIndexParserData * NET_HTTPIndexParserInit(void);

extern void NET_HTTPIndexParserFree(HTTPIndexParserData *obj);

extern int
NET_HTTPIndexParserPut(HTTPIndexParserData *obj, char *data, int32 len);

extern int
NET_HTTPIndexParserSort(HTTPIndexParserData *data_obj, int sort_method);

extern int32 NET_HTTPIndexParserGetTotalFiles(HTTPIndexParserData *data_obj);

extern NET_FileEntryInfo *
NET_HTTPIndexParserGetFileNum(HTTPIndexParserData *data_obj, int32 num);

extern char * NET_HTTPIndexParserGetTextMessage(HTTPIndexParserData *data_obj);

extern char * NET_HTTPIndexParserGetHTMLMessage(HTTPIndexParserData *data_obj);

extern char * NET_HTTPIndexParserGetBaseURL(HTTPIndexParserData *data_obj);

/* the view source converter.  Data_obj should be the
 * content type of the object
 */
extern NET_StreamClass * 
net_ColorHTMLStream (int         format_out,
                     void       *data_obj,
                     URL_Struct *URL_s,
                     MWContext  *window_id);

/* socks support function
 *
 * if set to NULL socks is off.  If set to non null string the string will be
 * used as the socks host and socks will be used for all connections.
 *
 * returns 0 on hostname error (gethostbyname returning bad)
 * 1 otherwise
 */
extern int NET_SetSocksHost(char * host);

/* set the number of seconds for the tcp connect timeout
 *
 * this timeout is used to end connections that do not
 * timeout on there own
 */
extern void NET_SetTCPConnectTimeout(uint32 seconds);

/* Is there a registered converter for the passed mime_type  */
extern XP_Bool NET_HaveConverterForMimeType(char *content_type);

/* builds an outgoing stream and returns a stream class structure
 * containing a stream function table
 */
PR_EXTERN(NET_StreamClass *)
           NET_StreamBuilder (
           FO_Present_Types  format_out,
           URL_Struct *      anchor,
           MWContext *       window_id);


/* bit flags for determining what we want to parse from the URL
 */
#define GET_ALL_PARTS              127
#define GET_PASSWORD_PART           64
#define GET_USERNAME_PART           32
#define GET_PROTOCOL_PART          16
#define GET_HOST_PART               8
#define GET_PATH_PART               4
#define GET_HASH_PART               2
#define GET_SEARCH_PART             1


 /* return codes used by NET_BeginConnection
 * and MSG_BeginOpenFolder
 */
#define MK_WAITING_FOR_CONNECTION   100
#define MK_CONNECTED                101

/* This function assumes a url that includes the protocol.
 * Parses and returns peices of URL as defined by the
 * bit flags in "wanted" which are defined above.
 *
 * Note: GET_HOST_PART returns the port number if one exists.
 * always mallocs a returned string that must be freed
 */
extern char * NET_ParseURL (const char *url, int wanted);

/* decode % escaped hex codes into character values,
 * modifies the parameter, returns the same buffer
 */
extern char * NET_UnEscape (char * str);

/* decode % escaped hex codes into character values,
 * modifies the parameter buffer, returns the length of the result
 * (result may contain \0's).
 */
extern int NET_UnEscapeCnt (char * str);

/* Like UnEscapeCnt but uses len to bound str, does not assume NUL termination
 */
extern int32  NET_UnEscapeBytes (char * str, int32 len);

/* a converter for plain text
 */
PUBLIC NET_StreamClass *
NET_PlainTextConverter   (FO_Present_Types format_out,
                          void       *data_obj,
                          URL_Struct *URL_s,
                          MWContext  *window_id);


MODULE_PRIVATE NET_StreamClass *
NET_ProxyAutoConfig(int fmt, void *data_obj, URL_Struct *URL_s, MWContext *w);

/* set the host to be used as an SMTP relay
 */
extern void NET_SetMailRelayHost(char * host);


/* Silly utility routine to send a message without user interaction. */

extern int
NET_SendMessageUnattended(MWContext* context, char* to, char* subject,
                          char* otherheaders, char* body);



/* add coordinates to the URL address
 * in the form url?x,y
 *
 * any # or ? data previously on the URL is stripped
 */
extern int NET_AddCoordinatesToURLStruct(URL_Struct * url_struct, int32 x_coord, int32 y_coord);


/* take a Layout generated LO_FormSubmitData_struct
 * and use it to add post data to the URL Structure
 * generated by CreateURLStruct
 *
 * DOES NOT Generate the URL Struct, it must be created prior to
 * calling this function
 *
 * returns 0 on failure and 1 on success
 */
extern int NET_AddLOSubmitDataToURLStruct(LO_FormSubmitData * sub_data, 
                                          URL_Struct * url_struct);

typedef enum {
    IdententifyViaUniqueID,
    IdentintifyViaEmailAddress,
    DoNotIdentifyMe
} IdentifyMeEnum;

/* set the method that the user wishes to identify themselves
 * with.
 * Default is DoNotIdentify unless this is called at startup
 */
extern void NET_SetIdentifyMeType(IdentifyMeEnum method);

/* set the users cookie prefs */
typedef enum {
    NET_SilentCookies,
    NET_WhineAboutCookies,
    NET_DontAcceptNewCookies,
    NET_DisableCookies
} NET_CookiePrefsEnum;

/* sets users inline cookie prefs */
typedef enum {
    NET_AllowForeignCookies,
    NET_DoNotAllowForeignCookies
} NET_ForeignCookiesEnum;

typedef enum {
    NET_Accept,
    NET_DontAcceptForeign,
    NET_DontUse
} NET_CookieBehaviorEnum;


/* Register the cookie preferences callback functions. Only called once from mkgeturl. */
extern void NET_RegisterCookiePrefCallbacks(void);

/* Save the cookies. libnet ignores whatever filename is specified, for now.
 */
extern int NET_SaveCookies(char *filename);

/* Start an anonymous list of cookies */
extern void NET_AnonymizeCookies();

/* Restore original list of cookies */
extern void NET_UnanonymizeCookies();

/* Should referer by supressed for anonymity sake */
extern Bool NET_SupressRefererForAnonymity();

#if defined(CookieManagement)
extern void NET_DisplayCookieInfoAsHTML(MWContext *context);
extern void NET_DisplayCookieInfoOfSiteAsHTML(MWContext *context, char * URLName);
extern int NET_CookiePermission(char* URLName);
extern int NET_CookieCount(char * URLName);
#endif

#if defined(SingleSignon)
extern void SI_DisplaySignonInfoAsHTML(MWContext *context);
#endif

/* returns a malloc'd string containing a unique id 
 * generated from the sec random stuff.
 */
extern char * NET_GetUniqueIdString(void);

/* pass in TRUE to disable disk caching
 * of SSL documents.
 * pass in FALSE to enable disk cacheing
 * of SSL documents
 */
extern void NET_DontDiskCacheSSL(XP_Bool set);

/* removes the specified number of objects from the
 * cache taking care to remove only the oldest objects.
 */
extern int32
NET_RemoveDiskCacheObjects(uint32 remove_num);

/* cleans up the cache directory by listing every
 * file and deleting the ones it doesn't know about
 * that begin with the cacheing prefix
 *
 * on entry:  Set
 *  dir_name: equal to the name of the directory where cache files reside
 *    prefix:   equal to the file prefix that cache files use. 
 *            (i.e. "cache" if all files begin with cacheXXXX)
 *
 *  remember that cache file names were generated by WH_TempName with
 *  the xpCache enum
 *
 */
extern int NET_CleanupCacheDirectory(char * dir_name, const char * prefix);

/* locks or unlocks a cache file.  The
 * cache lock is only good for a single
 * session
 * set=TRUE locks a file
 * set=FALSE unlocks a file
 */
extern Bool NET_ChangeCacheFileLock(URL_Struct *URL_s, Bool set);

/* Find an actively-loading cache file for URL_s in context, and copy the first
 * nbytes of it to a new cache file.  Return a cache converter stream by which
 * the caller can append to the cloned cache file.
 */
extern NET_StreamClass *
NET_CloneWysiwygCacheFile(MWContext *context, URL_Struct *URL_s, 
              uint32 nbytes, const char * wysiwyg_url,
              const char * base_href);

/* returns TRUE if the url is in the disk cache
 */
extern Bool NET_IsURLInDiskCache(URL_Struct *URL_s);

/* Returns TRUE if the URL is currently in the
 * memory cache and false otherwise.
 */
extern Bool NET_IsURLInMemCache(URL_Struct *URL_s);

/* returns TRUE if the URL->address passed in
 * is a local file URL
 */
extern XP_Bool NET_IsLocalFileURL(char *address);


/* read the Cache File allocation table.
 */
extern void NET_ReadCacheFAT(char * cachefatfile, XP_Bool stat_files);

/* unload the disk cache FAT list to disk
 *
 * set final_call to true if this is the last call to
 * this function before cleaning up the cache and shutting down.
 *
 * Front ends should never set final_call to TRUE, this will
 * be done by the cache cleanup code when the netlib is being
 * shutdown.
 */
extern void NET_WriteCacheFAT(char *filename, Bool final_call);

/* removes all cache files from the cache directory
 * and deletes the cache FAT
 */
extern int NET_DestroyCacheDirectory(char * dir_name, char * prefix);

/* set the size of the Memory cache in bytes
 * (The size should almost never be breached except when set exceptionally low)
 * Set it to 0 if you want cacheing turned off
 * completely
 *
 *
 * if you want to free up a bunch of memory
 * set the size to zero and then set it back
 * to some normal value.  
 */
extern void NET_SetMemoryCacheSize(int32 new_size);

/* set the size of the Disk cache.
 * Set it to 0 if you want cacheing turned off
 * completely
 */
extern void NET_SetDiskCacheSize(int32 new_size);

/* returns the number of bytes currently in use by the Memory cache
 */
extern int32 NET_GetMemoryCacheSize(void);

/* returns the ceiling on the memory cache
 */
extern int32 NET_GetMaxMemoryCacheSize(void);
    
/* returns the number of bytes currently in use by the disk cache
 */
extern int32 NET_GetDiskCacheSize(void);

/* returns the number of files currently in the
 * disk cache
 */
extern uint32 NET_NumberOfFilesInDiskCache();

/* remove the last memory cache object if one exists
 * returns the total size of the memory cache in bytes
 * after performing the operation
 */
extern int32 NET_RemoveLastMemoryCacheObject(void);

/* remove the last disk cache object if one exists
 * returns the total size of the disk cache in bytes
 * after performing the operation
 */


/* larubbio 197 SAR cache functions */
/* returns and creates a handle to the specified SAR cache 
 * the user is responsible for freeing the return struct
 */
extern ExtCacheDBInfo * CACHE_GetCache(ExtCacheDBInfo *db);

/* 
 * Adds the specified file to the cache.
 */
extern Bool CACHE_Put(char *filename, URL_Struct *url_s);

/* returns 0 if the url is not found, non-zero otherwise.  It
 * fills in URL_Struct as needed
 */
extern int CACHE_FindURLInCache(URL_Struct *URL_s, MWContext *ctxt);

/* Flushes the caches data to disk */
extern void CACHE_FlushCache(ExtCacheDBInfo *db_info);

/* Closes the SARcache database */
extern int CACHE_CloseCache(ExtCacheDBInfo *db);

/* Emptys all the URL's from a cache */
extern int CACHE_EmptyCache(ExtCacheDBInfo *db);

/* Removes the URL from the cache */
extern void NET_RemoveURLFromCache(URL_Struct *URL_s);

/* Removes the cache from the list of cache's being managed */
extern int CACHE_RemoveCache(ExtCacheDBInfo *db);

/* Returns an already opened ExtCache struct */
extern ExtCacheDBInfo * CACHE_GetCacheStruct(char * path, char * filename, char * name);

/* Returns the platform specific path to a cache file */
extern char * CACHE_GetCachePath(char * filename);

/* Returns the list of managed caches */
extern XP_List * CACHE_GetManagedCacheList();

/* Saves the cache struct to the DB */
extern void CACHE_SaveCacheInfoToDB(ExtCacheDBInfo *db_info);

/* Writes any changes to the cache's properties to disk */
extern void CACHE_UpdateCache(ExtCacheDBInfo *db_info);

extern int32 NET_RemoveLastDiskCacheObject(void);

/*--------------------------------------------------------------
 * cdata routines to manipulate extention to mime type mapping
 */

/* return the master cdata List pointer
 *
 * use the normal XP_List routines to iterate through
 */
extern XP_List * cinfo_MasterListPointer(void);

/* get the presumed content-type of the filename given
 */
extern NET_cinfo *NET_cinfo_find_type(char *uri);

/* return a cinfo from a mime_type
 */
extern NET_cinfo * NET_cinfo_find_info_by_type(char *mime_type);

/* remove an item from the cdata list
 */
extern void NET_cdataRemove(NET_cdataStruct *cd);

/* create a cdata struct
 *
 * does not fill anything in
 */
extern NET_cdataStruct * NET_cdataCreate(void);

/* fills in cdata structure
 *
 * if cdata for a given type do not exist, create a new one
 */
extern void NET_cdataCommit(char * mimeType, char * cdataString);

/* returns an extension (without the ".") for
 * a given mime type.
 *
 * If no match is found NULL is returned.
 *
 * Do not free the returned char *
 */
extern char * NET_cinfo_find_ext(char *mime_type);

/* check to see if this is an old mime type */
extern XP_Bool NET_IsOldMimeTypes (XP_List *masterList);

#ifdef XP_UNIX
extern XP_List * mailcap_MasterListPointer(void);
extern NET_mdataStruct *NET_mdataCreate(void);
extern void NET_mdataAdd(NET_mdataStruct *md);
extern void NET_mdataRemove(NET_mdataStruct *md);
extern void NET_mdataFree(NET_mdataStruct *md);
extern NET_mdataStruct* NET_mdataExist(NET_mdataStruct *old_md );
#endif


/* toggles netlib trace messages on and off
 */
PUBLIC void NET_ToggleTrace(void);

/* Makes a relative URL into an absolute one.
 *
 * If an absolute url is passed in it will be copied and returned.
 *
 * Always returns a malloc'd string or NULL on out of memory error
 */
PUBLIC char *
NET_MakeAbsoluteURL(char * absolute_url, char * relative_url);

/* FE_SetupAsyncSelect
 *
 * This function will setup the messages needed for windows Async functionality
 */


/* encode illegal characters into % escaped hex codes.
 *
 * mallocs and returns a string that must be freed
 */
PUBLIC char * NET_Escape (const char * str, int mask);

/* Like NET_Escape, but if out_len is non-null, return result string length
 * in *out_len, and uses len instead of NUL termination.
 */
PUBLIC char * NET_EscapeBytes (const char * str, int32 len, int mask,
                   int32 * out_len);

/* return the size of a string if it were to be escaped.
 */
extern int32 NET_EscapedSize (const char * str, int mask);

/*
 * valid mask values for NET_Escape() and NET_EscapedSize().
 */
#define URL_XALPHAS     (unsigned char) 1
#define URL_XPALPHAS    (unsigned char) 2
#define URL_PATH        (unsigned char) 4



#if !defined(XP_UNIX)

/* these routines are depricated and should be removed shortely */

extern int 
FE_SetupAsyncSelect(unsigned int sock, MWContext * context);

/* Start an async DNS lookup
 */
extern int 
FE_StartAsyncDNSLookup(MWContext *context, char * host_port, void ** hoststruct_ptr_ptr, int sock);
#endif

extern void NET_DownloadAutoAdminCfgFile();

#ifdef MOZ_LI

/* LDAP METHOD IDs for URL_s->method */
#define LDAP_SEARCH_METHOD  100 /* Address book search */
#define LDAP_LI_SEARCH_METHOD   101 /* Get LDAPMessage* results from a LDAP URL to search */
#define LDAP_LI_ADD_METHOD  102 /* Add an entry given LDAPMods */
#define LDAP_LI_MOD_METHOD  103 /* Modify an entry given LDAPMods */
#define LDAP_LI_DEL_METHOD  104 /* Delete an entry given the DN */
#define LDAP_LI_PUTFILE_METHOD  105 /* Put a file into a given DN and attribute, given a local path */
#define LDAP_LI_GETFILE_METHOD  106 /* Retreive an attribute into a local file, given DN and attribute */
#define LDAP_LI_ADDGLM_METHOD 107 /* Add an entry, then return the result
                                   *  of a search for the modified entry's
                                   *  modifyTimeStamp attribute
                                   */
#define LDAP_LI_MODGLM_METHOD 108 /* Modify an entry, then return the result
                                   *  of a search for the modified entry's
                                   *  modifyTimeStamp attribute
                                   */
#define LDAP_LI_GETLASTMOD_METHOD 109 /* Return an entry's modifyTimeStamp attribute.
                                       * Any attributes specified in the URL will be ignored
                                       */
#define LDAP_LI_BIND_METHOD 110     /* Bind as a particular user DN to an ldap server. 
                                     */
#endif

/*
 * NET_GetURL is called to begin the transfer of a URL
 *
 * URL_s:         see above
 * output_format: is a variable that specifies what to
 *        do with the outgoing data stream.
 * window_id:     is an identifier that will be used to
 *        determine where status messages and displayed data
 *        will be sent.
 * exit_routine:  is a callback function that will be
 *        invoked by the network library at the completion of a
 *        transfer or in case of error.
 *
 * Returns negative if the entire GET operation completes
 * in a single call or an error causes the transfer to
 * not happen.  In that case the exit_routine will be called
 * before returning from NET_GetURL.  A negative response
 * does not carry any other meaning than finished.  The status
 * passed into the exit_routine relays the success or failure
 * of the actual load and contains a meaningfull error status code.
 */

extern int
NET_GetURL (URL_Struct * URL_s,
        FO_Present_Types output_format,
        MWContext * context,
        Net_GetUrlExitFunc* exit_routine);
PUBLIC int
NET_GetURLQuick (URL_Struct * URL_s,
        FO_Present_Types output_format,
        MWContext * context,
        Net_GetUrlExitFunc* exit_routine);

/* URL methods
 */
#define URL_GET_METHOD    0
#define URL_POST_METHOD   1
#define URL_HEAD_METHOD   2
#define URL_PUT_METHOD    3
#define URL_DELETE_METHOD 4
#define URL_MKDIR_METHOD  5
#define URL_MOVE_METHOD   6
#define URL_INDEX_METHOD  7
#define URL_GETPROPERTIES_METHOD 8
#define URL_COPY_METHOD 9

/* LDAP METHOD IDs for URL_s->method */
#define LDAP_SEARCH_METHOD  100 /* Address book search */
#define LDAP_LI_GET_METHOD  101 /* Get LDAPMessage* results from a LDAP URL to search */
#define LDAP_LI_ADD_METHOD  102 /* Add an entry given LDAPMods */
#define LDAP_LI_MOD_METHOD  103 /* Modify an entry given LDAPMods */
#define LDAP_LI_DEL_METHOD  104 /* Delete an entry given the DN */
#define LDAP_LI_PUTFILE_METHOD  105 /* Put a file into a given DN and attribute, given a local path */
#define LDAP_LI_GETFILE_METHOD  106 /* Retreive an attribute into a local file, given DN and attribute */
#define LDAP_LI_ADDGLM_METHOD 107 /* Add an entry, then return the result
                                   *  of a search for the modified entry's
                                   *  modifyTimeStamp attribute
                                   */
#define LDAP_LI_MODGLM_METHOD 108 /* Modify an entry, then return the result
                                   *  of a search for the modified entry's
                                   *  modifyTimeStamp attribute
                                   */
#define LDAP_LI_GETLASTMOD_METHOD 109 /* Return an entry's modifyTimeStamp attribute.
                                       * Any attributes specified in the URL will be ignored
                                       */
#define LDAP_LI_BIND_METHOD 110     /* Bind as a particular user DN to an ldap server. 
                                     */

/*
 * NET_ProcessNet should be called repeatedly from the
 * clients main event loop to process network activity.
 * When there are active connections in progress
 * NET_ProcessNet will return one(1). If there are no
 * active connections open NET_ProcessNet will return
 * zero(0).
 */
PUBLIC int NET_ProcessNet(PRFileDesc *ready_fd, int fd_type);
#define NET_UNKNOWN_FD     0
#define NET_LOCAL_FILE_FD  1
#define NET_SOCKET_FD      2
#define NET_EVERYTIME_TYPE 3

/* call PR_Poll and call Netlib if necessary
 *
 * return FALSE if no sockets to process
 *
 * Should be called in OnIdle loop to service netlib 
 */
PUBLIC XP_Bool NET_PollSockets(void);

/* NET_InterruptWindow  interrupts all in progress transfers
 * that were initiated with the same window_id
 *
 * FE_GetContextID() is used internally to compare the
 * unique window_id's of the in progress transfers and
 * the window_id passed in
 */
extern int NET_InterruptWindow(MWContext * window_id);

/* Does the same thing as interruptWindow, but it
 * doesn't complain about reentrancy since it is
 * expected when calling this routine.
 *
 * NOTE that you can interrupt the stream that
 * you are calling from
 */
extern int NET_SilentInterruptWindow(MWContext * window_id);

/*
 *  NET_InterruptStream kills just stream associated with an URL.
 */
extern int NET_InterruptStream (URL_Struct *nurl);

/* set the netlib active entry coresponding to the
 * passed in URL to the busy state passed in.
 * TRUE to set busy, FALSE to set non-busy
 *
 * returns TRUE if found and set
 * returns FALSE if entry not found
 */
Bool NET_SetActiveEntryBusyStatus(URL_Struct *nurl, Bool set_busy);

/* interrupt a LoadURL action by using a socket number as
 * an index.  NOTE: that this cannot interrupt non-socket
 * based operations such as file or memory cache loads
 *
 * returns -1 on error.
 */
extern int NET_InterruptSocket (PRFileDesc *socket);

/* check for any active URL transfers in progress for the given
 * window Id
 *
 * It is possible that there exist URL's on the wait queue that
 * will not be returned by this function.  It is recommended
 * that you call NET_InterruptWindow(MWContext * window_id)
 * when FALSE is returned to make sure that the wait queue is
 * cleared as well.
 */
extern Bool NET_AreThereActiveConnectionsForWindow(MWContext * window_id);

extern Bool NET_AreThereActiveConnections();


#ifdef XP_MAC
/* pchen - Fix bug #72831. Same as
 * NET_AreThereActiveConnectionsForWindow except that it will
 * return false if there is an active connection that is busy.
 */
extern Bool NET_AreThereNonBusyActiveConnectionsForWindow(MWContext * window_id);
#endif /* XP_MAC */

/* check for network activity in general (not local disk or memory).
 */
extern Bool NET_HasNetworkActivity(MWContext * window_id, Bool waiting, Bool background);

/* NET_CreateURLStruct is used to build the URL structure
 * and to fill it with data.
 *
 * NET_FreeURLStruct should be called to free the returned
 * structure after the transfer is completed. ( this should
 * probably take place in the exit_routine passed as the callback
 * function to NET_GetURL)
 */
extern URL_Struct * NET_CreateURLStruct (const char *url, NET_ReloadMethod force_reload);

/* Append Key, Value pair into AllHeaders list of URL_struct
 */
extern Bool NET_AddToAllHeaders(URL_Struct * URL_s, char *name, char *value);

/* Set the option IPAddressString field
 * This field overrides the hostname when doing the connect, 
 * and allows Java to specify a spelling of an IP number.
 * returns 0 on success, non-zero for failure (out of memory)
 */
extern int NET_SetURLIPAddressString (URL_Struct * URL_s, const char *ip_string);

/* Free's the contents of the structure as well as
 * the structure
 */
extern void NET_FreeURLStruct (URL_Struct * URL_s);

/* Let someone keep a pointer to the URL struct without them
 * having to worry about it going away out from under them
 */
extern URL_Struct * NET_HoldURLStruct (URL_Struct * URL_s);

/* Macro sugar to complement NET_HoldURLStruct */
#define NET_DropURLStruct NET_FreeURLStruct

/* returns a text string explaining the negative status code
 * returned from mkgeturl
 */
extern const char * NET_ExplainError(int code);

/* returns true if the URL is a secure URL address
 */
extern Bool NET_IsURLSecure(char * address);

/*
 *  NewMKStream
 *  Utility to create a new initialized NET_StreamClass object
 */
extern NET_StreamClass * NET_NewStream(char *, 
                                       MKStreamWriteFunc, 
                                       MKStreamCompleteFunc, 
                                       MKStreamAbortFunc, 
                                       MKStreamWriteReadyFunc,
                                       void *, 
                                       MWContext *);

/* this should really be a FE function
 */ 
extern void NET_RegisterConverters(char * personal_file, char * global_file);
extern XP_List *NET_GetRegConverterList(FO_Present_Types iFormatOut);
extern void *NET_GETDataObject(XP_List *list, char *pMimeType, void** obj);
extern void NET_CleanupMailCapList(char* filename);

/*  Register a routine to convert between format_in and format_out
 */
/*  Reroute XP handling of streams for windows.
 */
extern void NET_RegContentTypeConverter (char * format_in,
                                              FO_Present_Types format_out,
                                              void          * data_obj,
                                              NET_Converter * converter_func,
                                              XP_Bool          bAutomated);

/*  This function found in the windows front end.
 */
extern void NET_RegisterContentTypeConverter (char          * format_in,
                                              FO_Present_Types format_out,
                                              void          * data_obj,
                                              NET_Converter * converter_func);

extern void NET_DeregisterContentTypeConverter (char          * format_in,
                                                FO_Present_Types format_out);

extern void
NET_RegisterExternalViewerCommand(char * format_in,
                        char * system_command,
                                unsigned int stream_block_size);

/* register a routine to decode a stream
 */
extern void NET_RegisterEncodingConverter (char           * encoding_in,
                                           void           * data_obj,
                                           NET_Converter  * converter_func);

#ifdef NEW_DECODERS
/* removes all registered converters of any kind
 */
extern void NET_ClearAllConverters(void);
#endif /* NEW_DECODERS */


#ifdef XP_UNIX
/* register an external program to act as a content-encoding
 * converter
 */
extern void NET_RegisterExternalDecoderCommand (char * format_in,
                                                char * format_out,
                                                char * system_command);
#endif /* XP_UNIX */



/* set the NNTP server hostname
 */
extern void NET_SetNewsHost(const char * hostname);

extern NET_StreamClass * NET_CacheConverter (FO_Present_Types format_out,
                                             void       *converter_obj,
                                             URL_Struct *URL_s,
                                             MWContext  *window_id);

/* init types loads standard types so that a config file
 * is not required and can also read
 * a mime types file if filename is passed in non empty and
 * non null
 *
 * on each call to NET_InitFileFormatTypes the extension list
 * will be completely free'd and rebuilt.
 *
 * if you pass in NULL filename pointers they will not be
 * attempted to be read
 */
extern int NET_InitFileFormatTypes(char * personal_file, char *global_file);
extern void NET_cdataAdd(NET_cdataStruct *cd);
extern NET_cdataStruct* NET_cdataExist(NET_cdataStruct *old_cd );
extern void NET_cdataFree(NET_cdataStruct *cd);

#ifdef XP_UNIX
extern void NET_CleanupFileFormat(char *filename);
#else
extern void NET_CleanupFileFormat(void);
#endif

/* reads HTTP cookies from disk
 *
 * on entry pass in the name of the file to read
 *
 * returns 0 on success -1 on failure.
 *
 */
extern int NET_ReadCookies(char * filename);

/* removes all cookies structs from the cookie list */
extern void
NET_RemoveAllCookies();

/* initialize the netlibrary
 */
extern int NET_InitNetLib(int socket_buffer_size, int max_number_of_connections);

/*
finish the netlib startup stuff - needed if li is on
*/
extern void NET_FinishInitNetLib();

/* reads a mailcap file and adds entries to the
 * external viewers converter list
 */  
extern int NET_ProcessMailcapFile (char *file, Bool is_local);

/* removes all external viewer commands
 */
extern void NET_ClearExternalVieweraConverters(void);

/* shutdown the netlibrary, cancel all
 * conections and free all
 * memory
 */
extern void NET_ShutdownNetLib(void);

/* set the maximum allowable number of open connections at any one
 * time
 */
extern void NET_ChangeMaxNumberOfConnections(int max_number_of_connections);

/* set the maximum allowable number of open connections at any one
 * time on a per context basis
 */
extern void NET_ChangeMaxNumberOfConnectionsPerContext(int max_number_of_connections);

/* allocate memory for the TCP socket
 * buffer
 * this is an optional call
 */
extern int NET_ChangeSocketBufferSize (int size);

typedef enum {  
        FTP_PROXY = 0,
        GOPHER_PROXY,
        HTTP_PROXY,
        HTTPS_PROXY,
        NEWS_PROXY,
        WAIS_PROXY,
        NO_PROXY,
        PROXY_AUTOCONF_URL
} NET_ProxyType;

typedef enum {
        PROXY_STYLE_UNSET = 0, /* do not change these without notifying macfe */
        PROXY_STYLE_MANUAL = 1,
        PROXY_STYLE_AUTOMATIC = 2,
        PROXY_STYLE_NONE = 3
} NET_ProxyStyle;

/* set (or unset) a proxy server
 *
 *  call with a host[:port] string or an http url
 *  of the form http://host[:port]/
 *
 * if type is NO_PROXY send a comma deliminated string
 * of host and port numbers of the form
 *  host[:port]{,host[:port]}*
 *
 *  if type is PROXY_AUTOCONFIG_URL it sets the URL pointing to
 *  a new-style automatic proxy configuration file.
 *
 *  A call to function NET_SelectProxyStyle() should be made after
 *  *ALL* the calls to these functions have been done.
 *
 *  you can unset values by setting the type and passing
 *  NULL as the host_port string
 */
PUBLIC void NET_SetProxyServer(NET_ProxyType type, const char * host_port);


/*
 * Select proxy style;
 * one of:
 *
 *      PROXY_STYLE_MANUAL      -- for old style proxies
 *      PROXY_STYLE_AUTOMATIC   -- for new style proxies
 *      PROXY_STYLE_NONE        -- for no proxies at all
 *
 * This function should be called only *AFTER* all the calls to
 * NET_SetProxyServer() function have been made.
 *
 */
PUBLIC void
NET_SelectProxyStyle(NET_ProxyStyle style);


/*
 * Force to reload automatic proxy config.
 * Has any effect only if proxy style has been set to
 * PROXY_STYLE_AUTOMATIC.
 *
 */
PUBLIC void
NET_ReloadProxyConfig(MWContext *window_id);


/*
 * NET_SetNoProxyFailover
 * Sets a flag that indicates that proxy failover should not
 * be done.  This is used by the Enterprise Kit code, where
 * the customer might configure the client to use specific
 * proxies.  In these cases, failover can cause different
 * proxies, or no proxies to be used.
 */
PUBLIC void
NET_SetNoProxyFailover(void);


/*
 * Returns a pointer to a NULL-terminted buffer which contains
 * the text of the proxy autoconfig file.
 *
 * NULL if not enabled.
 *
 */
PUBLIC char * NET_GetProxyConfigSource(void);



typedef enum {
    CU_CHECK_PER_SESSION,
    CU_CHECK_ALL_THE_TIME,
    CU_NEVER_CHECK
} CacheUseEnum;

/* set the way the cache objects should
 * be checked
 */
extern void NET_SetCacheUseMethod(CacheUseEnum method);

/* set the number of articles shown in news at any one time
 */
extern void NET_SetNumberOfNewsArticlesInListing(int32 number);


/* Set whether to cache XOVER lines in an attempt to make news go faster
 */
extern void NET_SetCacheXOVER(XP_Bool value);


/* Tell netlib to clean up any strictly temporary cache files it has for XOVER
   data.  (This is called by msglib when the news context is destroyed.) */
extern void NET_CleanTempXOVERCache(void);


/* Register decoder functions for the standard MIME encodings.  */
PUBLIC void NET_RegisterMIMEDecoders (void);

/* Create a netlib stream for interpreting Mocha.  */
PUBLIC NET_StreamClass *
NET_CreateMochaConverter(FO_Present_Types format_out,
                         void             *data_object,
                         URL_Struct       *URL_struct,
                         MWContext        *window_id);

/* check the date and set off the timebomb if necessary
 *
 * Calls FE_Alert with a message and then disables
 * all network calls to hosts not in our domains,
 * except for FTP connects.
 */
extern Bool NET_CheckForTimeBomb(MWContext *context);

/* parse a mime header.
 * the outputFormat is passed in and passed to NET_SetCookieStringFromHttp
 *
 * Both name and value strings will be modified during
 * parsing.  If you need to retain your strings make a
 * copy before passing them in.
 *
 * Values will be placed in the URL struct.
 *
 * returns TRUE if it found a valid header
 * and FALSE if it didn't
 */
PUBLIC Bool
NET_ParseMimeHeader(FO_Present_Types outputFormat,
                    MWContext  *context,
                    URL_Struct *URL_s,
                    char       *name,
                    char       *value,
                    XP_Bool    is_http);


/* scans a line for references to URL's and turns them into active
 * links.  If the output size is exceeded the line will be
 * truncated.  "output" must be at least "output_size" characters
 * long.  If urls_only_p is FALSE, then also tweak citations (lines starting
 * with ">").
 *
 * The MSG_Pane* parameter is used to determine preferences for fonts
 * used in citations.  If it is NULL, then boring defaults are used.
 */

extern int NET_ScanForURLs(MSG_Pane* pane, const char *input, int32 input_size,
                           char *output, int output_size, XP_Bool urls_only_p);


/* Takes an arbitrary chunk of HTML, and returns another chunk which has had
   any URLs turned into references.  The result is in a new string that
   must be free'd using XP_FREE(). */

extern char* NET_ScanHTMLForURLs(const char* input);



/* escapes all '<', '>' and '&' characters in a string
 * returns a string that must be freed
 */
extern char * NET_EscapeHTML(const char * string);


/* escapes doubles quotes in a url, to protect
 * the html page embedding the url.
 */
extern char * NET_EscapeDoubleQuote(const char * string);


/* register a newsrc file mapping
 */
extern Bool NET_RegisterNewsrcFile(char *filename, 
                                   char *hostname, 
                                   Bool  is_secure,
                                   Bool  is_newsgroups_file);

/* removes a newshost to file mapping.  Removes both
 * the newsrc and the newsgroups mapping
 * this function does not remove the actual files,
 * that is left up to the caller
 */
extern void NET_UnregisterNewsHost(const char *host, Bool is_secure);

/* user has removed a news host from the UI. 
 * be sure it has also been removed from the
 * connection cache so we renegotiate news host
 * capabilities if we try to use it again
 */
extern void NET_OnNewsHostDeleted (const char *host);

/* map a filename and security status into a filename
 *
 * returns NULL on error or no mapping.
 */
extern char * NET_MapNewsrcHostToFilename(char *host, 
                                          Bool  is_secure,
                                          Bool  is_newsgroups_file);

/* save newsrc file mappings from memory onto disk
 */
extern Bool NET_SaveNewsrcFileMappings(void);

/* read newsrc file mappings from disk into memory
 */
extern Bool NET_ReadNewsrcFileMappings(void);

/* free newsrc file mappings in the memory
 */
extern void NET_FreeNewsrcFileMappings(void);

/* returns a malloc'd string that has a bunch of netlib
 * status info in it.
 *
 * please free the pointer when done.
 */
extern char * NET_PrintNetlibStatus(void);

/* add an external URL type prefix
 *
 * this prefix will be used to see if a URL is an absolute
 * or a relative URL.
 *
 * you should just pass in the prefix of the URL
 *
 * for instance "http" or "ftp" or "foobar".
 *
 * do not pass in a colon or double slashes.
 */
extern void NET_AddExternalURLType(char * type);

/* remove an external URL type prefix
 */
extern void NET_DelExternalURLType(char * type);

/*
 *  NET_SetNewContext changes the context associated with a URL Struct.
 *  can also change the url exit routine, which will cause the old one
 *  to be called with a status of MK_CHANGING_CONTEXT
 */
extern int NET_SetNewContext(URL_Struct *URL_s, MWContext * new_context, Net_GetUrlExitFunc *exit_routine);

/* returns true if the stream is safe for setting up a new
 * context and using a separate window.
 *
 * This will return FALSE in multipart/mixed cases where
 * it is impossible to use separate windows for the
 * same stream.
 */
extern Bool NET_IsSafeForNewContext(URL_Struct *URL_s);

/* set the pop3 option to leave a copy of old
 * mail on the server
 */
extern void NET_LeavePopMailOnServer(Bool do_it);

/* Set the pop3 option of the biggest message to download.  If negative,
 * then there is no limit (this should be the default). */
extern void NET_SetPopMsgSizeLimit(int32 size);

/* Return what the current message size limit is (again, negative means no
 * limit.) */
extern int32 NET_GetPopMsgSizeLimit(void);

/* set the pop account username
 */
extern void NET_SetPopUsername(const char *username);

/* get the pop account username
 */
extern const char * NET_GetPopUsername(void);

/* set the pop account password
 */
extern void NET_SetPopPassword(const char *password);

/* get the pop account password
 */
extern const char * NET_GetPopPassword(void);

/* call this to enable/disable FTP PASV mode.
 * Note: PASV mode is on by default
 */
extern void NET_UsePASV(Bool do_it);

#if defined(XP_MAC) || defined(XP_UNIX)
/* call this with TRUE to enable the sending of the email
 * address as the default user "anonymous" password.
 * The default is OFF.  
 */
extern void NET_SendEmailAddressAsFTPPassword(Bool do_it);
#endif

/* begins the upload of the contents of a directory.
 * local_directory and remote_directory can both be NULL.
 * if either are NULL then it will prompt the user for them.
 */
extern void
NET_PublishDirectory(MWContext *context, 
                     char *local_directory, 
                     char *remote_directory);

/* upload a set of local files (array of char*)
 * all files must have full path name
 * first file is primary html document,
 * others are included images or other files
 * in the same directory as main file
 */
extern void
NET_PublishFiles(MWContext *context, 
                 char **files_to_publish,
                 char *remote_directory);
/* NET_PublishFiles is obsolete and should be removed (hardts).  Use Net_PublishFilesTo. */
/* files_to_publish and publish_to will be freed by NET_PublishFilesTo. */
extern void
NET_PublishFilesTo(MWContext *context, 
                 char **files_to_publish,
                 char **publish_to,  /* Absolute URLs of the location to 
                                      * publish the files to. */
                 XP_Bool *add_crlf, /* For each file in files_to_publish, should every line 
                                       end in a CRLF. */
                 char *base_url,       /* Directory to publish to, or the destination 
                                                * URL of the root HTML document. */
                 char *username,  /* may be NULL, don't pass in through base_url. */
                 char *password,  /* may be NULL, don't pass in through base_url. */
                 Net_GetUrlExitFunc *exit_func, /* Called after publishing files is complete. 
                                                                            Can be NULL. */
                 void *fe_data);        /* Will be added as the fe_data field of the URL_Struct.  
                                                    So, it can be used as an argument that exit_func() can get 
                                                    ahold of. */

/* assemble username, password, and ftp:// or http://
 * URL into ftp://user:password@/blah  format for uploading
 * user_name or password are optional, allowing
 * construction of ftp://user@/blah for security reasons
*/
extern Bool
NET_MakeUploadURL( char **full_location, char *location, 
                   char *user_name, char *password );

/* extract the username, password, and reassembled URL
 * string (e.g.: ftp://user:pass@neon -> ftp://neon)
 * from an ftp:// or http:// URL
*/
extern Bool
NET_ParseUploadURL( char *full_location, char **location, 
                    char **user_name, char **password );


/* opens a directory and puts all the files therein into a
 * null terminated array of filenames
 * Returns NULL on error
 */
extern char **
NET_AssembleAllFilesInDirectory(MWContext *context, char * local_dir_name);

/* takes a local and a remote directory and loads all of the
 * files in the directory into a URL struct that it creates
 * and calls FE_GetURL to start the upload
 */
extern void
NET_UploadDirectory(MWContext *context, char *local_dir, char *remote_dir_url);


/* convert + to space within str.
 */
extern void NET_PlusToSpace(char *str);

/* url_type return types */
#define FILE_TYPE_URL           1
#define FTP_TYPE_URL            2
#define GOPHER_TYPE_URL         3
#define HTTP_TYPE_URL           4
#define MAILTO_TYPE_URL         5
#define NEWS_TYPE_URL           6
#define RLOGIN_TYPE_URL         7
#define TELNET_TYPE_URL         8
#define TN3270_TYPE_URL         9
#define WAIS_TYPE_URL           10
#define ABOUT_TYPE_URL          11
#define FILE_CACHE_TYPE_URL     12
#ifdef NU_CACHE
#define NU_CACHE_TYPE_URL       13
#endif
#define MEMORY_CACHE_TYPE_URL   13 /* Will go away later */
#define SECURE_HTTP_TYPE_URL    14
#define INTERNAL_IMAGE_TYPE_URL 15
#define URN_TYPE_URL            16
#define POP3_TYPE_URL           17
#define MAILBOX_TYPE_URL        18
#define INTERNAL_NEWS_TYPE_URL  19
#define SECURITY_TYPE_URL       20
#define MOCHA_TYPE_URL          21
#define VIEW_SOURCE_TYPE_URL    22
#define HTML_DIALOG_HANDLER_TYPE_URL 23
#define HTML_PANEL_HANDLER_TYPE_URL 24
#define INTERNAL_SECLIB_TYPE_URL 25
#define MSG_SEARCH_TYPE_URL     26
#define IMAP_TYPE_URL           27
#define LDAP_TYPE_URL           28
#define SECURE_LDAP_TYPE_URL    29
#define WYSIWYG_TYPE_URL        30
#define ADDRESS_BOOK_TYPE_URL   31
#define CLASSID_TYPE_URL        32
#define JAVA_TYPE_URL           33
#define DATA_TYPE_URL           34
#define NETHELP_TYPE_URL        35
#define NFS_TYPE_URL            36
#define MARIMBA_TYPE_URL        37
#define INTERNAL_CERTLDAP_TYPE_URL 38
#define ADDRESS_BOOK_LDAP_TYPE_URL 39
#define LDAP_REPLICATION_TYPE_URL  40
#define LDAP_QUERY_DSE_TYPE_URL    41
#define CALLBACK_TYPE_URL       42
#define SOCKSTUB_TYPE_URL         43

#define LAST_URL_TYPE 43  /* defines the max number of URL types there are */

/* A usefull function to test URL types (mkutils.cpp)
 * Unfortunately, we can't get to return values
 * We are most interested in FILE_TYPE_URL, which is 1
*/
extern int NET_URL_Type(const char *URL);

/* CM these functions added to \libnet\MKPARSE.C: */
/* Returns TRUE if URL type is HTTP_TYPE_URL or SECURE_HTTP_TYPE_URL */
extern Bool NET_IsHTTP_URL(const char *URL);

/* Return values for NET_MakeRelativeURL */
enum {
    NET_URL_SAME_DIRECTORY,       /* Base and URL type only filename is different */
    NET_URL_SAME_DEVICE,          /* Base and URL are on same device, different directory */
    NET_URL_NOT_ON_SAME_DEVICE,   /* One is remote, other is local, or on different local drives */
    NET_URL_FAIL                 /* Failed for lack of params, etc. */
};

/* Converts absolute URL into an URL relative to the base URL
 * BasePath must be an HTTP: or FILE: URL type
 * absolute_url should be absolute url of the same type,
 *  but we pass it through NET_MakeAbsoluteURL so we can use
 *  relative input - ASSUMES SAME TYPE!
 * If relative_url is not supplied, tests are done,
 *   but no relative URL string is created
 * Caller must XP_FREE the string
*/
int NET_MakeRelativeURL( char *base_url,
                         char *absolute_url,
                         char **relative_url );

/* Extract the filename from a source URL 
 *  and construct add it the same path as 
 *  a base URL (replacing file in base)
 * BasePath should be an HTTP: or FILE: URL type,
 *  but we don't check it.
 * src_url may be a relative URL,
 *  (if its an HTTP, and base_url is FILE,
 *   this creates the filename for saving
 *  remote image locally.)
 * If relative_url is supplied,
 *  the new relative URL is returned
 *  (just the "file" portion of src_url)
 * Caller must XP_FREE the string(s)
*/
char * NET_MakeTargetURL( char *base_url,
                          char *src_url,
                          char **relative_url );

/*
 * call this function on navigator initialization and whenever the pref
 * changes.  PR_TRUE means that the warning dialog will be generated,
 * and PR_FALSE means that the warning dialog will NOT be generated.
 */
extern void NET_WarnOnMailtoPost(PRBool warn);

/* Is the user off-line - uses the network.online preference */
extern XP_Bool NET_IsOffline();

#ifndef SOCKET_ERRNO
#define SOCKET_INACTIVE 0   /* value that determines if a socket is inactive */
#define SOCKET_INVALID  -1
#define SOCKET_ERRNO  (int)PR_GetError()      /* normal socket errno */
#endif


XP_END_PROTOS

#endif /* _NET_PROTO_H_ */
