
/*  W3 Copyright statement 
Copyright 1995 by: Massachusetts Institute of Technology (MIT), INRIA</H2>

This W3C software is being provided by the copyright holders under the
following license. By obtaining, using and/or copying this software,
you agree that you have read, understood, and will comply with the
following terms and conditions: 

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee or royalty is hereby
granted, provided that the full text of this NOTICE appears on
<EM>ALL</EM> copies of the software and documentation or portions
thereof, including modifications, that you make. 

<B>THIS SOFTWARE IS PROVIDED "AS IS," AND COPYRIGHT HOLDERS MAKE NO
REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED.  BY WAY OF EXAMPLE,
BUT NOT LIMITATION, COPYRIGHT HOLDERS MAKE NO REPRESENTATIONS OR
WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE OR
THAT THE USE OF THE SOFTWARE OR DOCUMENTATION WILL NOT INFRINGE ANY
THIRD PARTY PATENTS, COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS.
COPYRIGHT HOLDERS WILL BEAR NO LIABILITY FOR ANY USE OF THIS SOFTWARE
OR DOCUMENTATION.

The name and trademarks of copyright holders may NOT be used
in advertising or publicity pertaining to the software without
specific, written prior permission.  Title to copyright in this
software and any associated documentation will at all times remain
with copyright holders. 
*/
/*                                        W3C Reference Library libwww General Purpose Macros
                                  GENERAL PURPOSE MACROS
                                             
 */
/*
**      (c) COPYRIGHT MIT 1995.
**      Please first read the full copyright statement in the file COPYRIGH.
*/
/*

   This module is a part of the W3C Reference Library. See also the system dependent file
   sysdep module for system specific information.
   
 */
#ifndef HTUTILS_H
#define HTUTILS_H

#include "xp_core.h"
/*

DEBUG MESSAGE CONTROL

   This is the global flag for setting the WWWTRACEoptions. The verbose mode is no longer
   a simple boolean but a bit field so that it is possible to see parts of the output
   messages.
   
 */
#ifndef DEBUG
#define DEBUG   /* No one ever turns this off as trace is too important */
#endif
/*

  Definition of the Global Trace Flag
  
   The global trace flag variable is available everywhere.
   
 */
#ifdef DEBUG
#ifdef WWW_WIN_DLL
extern int *            WWW_TraceFlag;   /* In DLLs, we need the indirection */
#define WWWTRACE        (*WWW_TraceFlag)
#else
extern int              WWW_TraceFlag;       /* Global flag for all W3 trace */
#define WWWTRACE        (WWW_TraceFlag)
#endif /* WWW_WIN_DLL */
#else
#define WWWTRACE        0
#endif /* DEBUG */
/*

   The WWWTRACEdefine outputs messages if verbose mode is active according to the
   following rules:
   
 */
typedef enum _HTTraceFlags {
    SHOW_UTIL_TRACE     = 0x1,                          /*                 1 */
    SHOW_APP_TRACE      = 0x2,                          /*                10 */
    SHOW_CACHE_TRACE    = 0x4,                          /*               100 */
    SHOW_SGML_TRACE     = 0x8,                          /*              1000 */
    SHOW_BIND_TRACE     = 0x10,                         /*            1.0000 */
    SHOW_THREAD_TRACE   = 0x20,                         /*           10.0000 */
    SHOW_STREAM_TRACE   = 0x40,                         /*          100.0000 */
    SHOW_PROTOCOL_TRACE = 0x80,                         /*         1000.0000 */
    SHOW_MEM_TRACE      = 0x100,                        /*       1.0000.0000 */
    SHOW_URI_TRACE      = 0x200,                        /*      10.0000.0000 */
    SHOW_AUTH_TRACE     = 0x400,                        /*     100.0000.0000 */
    SHOW_ANCHOR_TRACE   = 0x800,                        /*    1000.0000.0000 */
    SHOW_PICS_TRACE     = 0x1000,                       /*  1.0000.0000.0000 */
    SHOW_CORE_TRACE     = 0x2000,                       /* 10.0000.0000.0000 */
    SHOW_ALL_TRACE      = 0x3FFF                        /* 11.1111.1111.1111 */
} HTTraceFlags;
/*

   The flags are made so that they can serve as a group flag for correlated trace
   messages, e.g. showing messages for SGML and HTML at the same time.
   
 */
#define UTIL_TRACE      (WWWTRACE & SHOW_UTIL_TRACE)
#define APP_TRACE       (WWWTRACE & SHOW_APP_TRACE)
#define CACHE_TRACE     (WWWTRACE & SHOW_CACHE_TRACE)
#define SGML_TRACE      (WWWTRACE & SHOW_SGML_TRACE)
#define BIND_TRACE      (WWWTRACE & SHOW_BIND_TRACE)
#define THD_TRACE       (WWWTRACE & SHOW_THREAD_TRACE)
#define STREAM_TRACE    (WWWTRACE & SHOW_STREAM_TRACE)
#define PROT_TRACE      (WWWTRACE & SHOW_PROTOCOL_TRACE)
#define MEM_TRACE       (WWWTRACE & SHOW_MEM_TRACE)
#define URI_TRACE       (WWWTRACE & SHOW_URI_TRACE)
#define AUTH_TRACE      (WWWTRACE & SHOW_AUTH_TRACE)
#define ANCH_TRACE      (WWWTRACE & SHOW_ANCHOR_TRACE)
#define PICS_TRACE      (WWWTRACE & SHOW_PICS_TRACE)
#define CORE_TRACE      (WWWTRACE & SHOW_CORE_TRACE)
/*

  Destination for Trace Messages
  
   You can send trace messages to various destinations depending on the type of your
   application. By default, on Unix the messages are sent to stderr using fprintf() and if
   we are on Windows and have a windows applications then register a HTTraceCallback
   function. This is done with HTTrace_setCallback. It tells HTTrace to call a
   HTTraceCallback. If your compiler has problems with va_list, then you may forget about
   registering the callback and instead macro HTTrace as follows: #define HTTrace
   MyAppSpecificTrace
   
 */
typedef int (*HTTraceCallback)();  /* jhines--7/9/97 */
/* typedef int HTTraceCallback(const char * fmt, va_list pArgs); */

extern void HTTrace_setCallback(HTTraceCallback * pCall);
extern HTTraceCallback * HTTrace_getCallback(void);

extern int HTTrace(const char * fmt, ...);
/*

MACROS FOR FUNCTION DECLARATIONS

 */
/* #define PUBLIC             */     /* Accessible outside this module     */
/* #define PRIVATE static     */     /* Accessible only within this module */
/*

OFTEN USED INTERGER MACROS

  Min and Max functions
  
 */
#ifndef HTMIN
#define HTMIN(a,b) ((a) <= (b) ? (a) : (b))
#define HTMAX(a,b) ((a) >= (b) ? (a) : (b))
#endif
/*

  Double abs function
  
 */
#ifndef HTDABS
#define HTDABS(a) ((a) < 0.0 ? (-(a)) : (a))
#endif
/*

RETURN CODES FOR PROTOCOL MODULES AND STREAMS

   Theese are the codes returned from the protocol modules, and the stream modules.
   Success are (>=0) and failure are (<0)
   
 */
#define HT_OK                   0       /* Generic success */
#define HT_ALL                  1       /* Used by Net Manager */

#define HT_CONTINUE             29991   /* Continue an operation */
#define HT_CLOSED               29992   /* The socket was closed */
#define HT_PERSISTENT           29993   /* Wait for persistent connection */
#define HT_IGNORE               29994   /* Ignore this in the Net manager */
#define HT_NO_DATA              29995   /* OK but no data was loaded */
#define HT_RELOAD               29996   /* If we must reload the document */
#define HT_PERM_REDIRECT        29997   /* Redo the retrieve with a new URL */
#define HT_TEMP_REDIRECT        29998   /* Redo the retrieve with a new URL */
#define HT_LOADED               29999   /* Instead of a socket */

#define HT_ERROR                -1      /* Generic failure */

#define HT_NO_ACCESS            -10     /* Access not available */
#define HT_FORBIDDEN            -11     /* Access forbidden */
#define HT_RETRY                -13     /* If service isn't available */
#define HT_NO_PROXY_ACCESS      -14     /* No proxy access */

#define HT_INTERNAL             -100    /* Weird -- should never happen. */

#define HT_WOULD_BLOCK          -29997  /* If we are in a select */
#define HT_INTERRUPTED          -29998  /* Note the negative value! */
#define HT_PAUSE                -29999  /* If we want to pause a stream */
/*

UPPER- AND LOWERCASE MACROS

   The problem here is that toupper(x) is not defined officially unless isupper(x) is.
   These macros are CERTAINLY needed on #if defined(pyr) || define(mips) or BDSI
   platforms. For safefy, we make them mandatory.
   
 */
/* #ifndef TOLOWER */
/* #define TOLOWER(c) tolower(c) */
/* #define TOUPPER(c) toupper(c) */
#define TOUPPER(x) ((((unsigned int) (x)) > 0x7f) ? x : toupper(x))
/* #endif */
/*

MAX AND MIN VALUES FOR INTEGERS AND FLOATING POINT

 */
#ifdef FLT_EPSILON                                  /* The ANSI C way define */
#define HT_EPSILON FLT_EPSILON
#else
#define HT_EPSILON 0.00000001
#endif
/*

WHITE CHARACTERS

   Is character _c_ white space?
   
 */
/* #define WHITE(c) isspace(c) */
#define WHITE(c) isspace(c)
/*

THE LOCAL EQUIVALENTS OF CR AND LF

   We can check for these after net ascii text has been converted to the local
   representation. Similarly, we include them in strings to be sent as net ascii after
   translation.
   
 */
/* #define LF   FROMASCII('\012') */  /* ASCII line feed LOCAL EQUIVALENT */
/* #define CR   FROMASCII('\015') */  /* Will be converted to ^M for transmission */
/*

LIBRARY DYNAMIC MEMORY MAGEMENT

   The Library has it's own dynamic memory API which is declared in memory management
   module.
   
 */
#include "htmemory.h"
/*

 */
#endif /* HT_UTILS.h */
/*

   
   ___________________________________
   
                           @(#) $Id: htutils.h,v 1.1 1999/03/18 22:32:50 neeti%netscape.com Exp $
                                                                                          
    */
