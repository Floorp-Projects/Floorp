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

/*
 * Copyright (c) 1990 Regents of the University of Michigan.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and that due credit is given
 * to the University of Michigan at Ann Arbor. The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission. This software
 * is provided ``as is'' without express or implied warranty.
 */
/* lbet-int.h - internal header file for liblber */

#ifndef _LBERINT_H
#define _LBERINT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>

#ifdef macintosh
# include "ldap-macos.h"
#else /* macintosh */
#if !defined(BSDI) && !defined(DARWIN) && !defined(FREEBSD)
# include <malloc.h>
#endif
# include <errno.h>
# include <sys/types.h>
#if defined(SUNOS4) || defined(SCOOS)
# include <sys/time.h>
#endif
#if defined( _WINDOWS )
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <basetsd.h>
#  define ssize_t SSIZE_T
#  include <time.h>
/* No stderr in a 16-bit Windows DLL */
#  if defined(_WINDLL) && !defined(_WIN32)
#    define USE_DBG_WIN
#  endif
# else
/* #  include <sys/varargs.h> */
#  include <sys/socket.h>
#  include <netinet/in.h>
#if !defined(XP_OS2) && !defined(DARWIN)
#  include <unistd.h>
#endif
# endif /* defined( _WINDOWS ) */
#endif /* macintosh */

#include <memory.h>
#include <string.h>
#include "portable.h"

#ifdef _WINDOWS
#  if defined(FD_SETSIZE)
#  else
#    define FD_SETSIZE 256 /* set it before winsock sets it to 64! */
#  endif
#include <winsock.h>
#include <io.h>
#endif /* _WINDOWS */

#ifdef XP_OS2
#include <io.h>
#endif /* XP_OS2 */

/* No stderr in a 16-bit Windows DLL */
#if defined(_WINDLL) && !defined(_WIN32)
#define stderr NULL
#endif

#include "lber.h"

#ifdef macintosh
#define NSLDAPI_LBER_SOCKET_IS_PTR
#endif

#define OLD_LBER_SEQUENCE	0x10	/* w/o constructed bit - broken */
#define OLD_LBER_SET		0x11	/* w/o constructed bit - broken */

#ifndef _IFP
#define _IFP
typedef int (LDAP_C LDAP_CALLBACK *IFP)();
#endif

typedef struct seqorset {
  ber_len_t	sos_clen;
  ber_tag_t	sos_tag;
  char		*sos_first;
  char		*sos_ptr;
  struct seqorset	*sos_next;
} Seqorset;
#define NULLSEQORSET	((Seqorset *) 0)

#define SOS_STACK_SIZE 8 /* depth of the pre-allocated sos structure stack */

#define MAX_TAG_SIZE (1 + sizeof(ber_int_t)) /* One byte for the length of the tag */
#define MAX_LEN_SIZE (1 + sizeof(ber_int_t)) /* One byte for the length of the length */
#define MAX_VALUE_PREFIX_SIZE (2 + sizeof(ber_int_t)) /* 1 byte for the tag and 1 for the len (msgid) */
#define BER_ARRAY_QUANTITY 7 /* 0:Tag   1:Length   2:Value-prefix   3:Value   4:Value-suffix  */
#define BER_STRUCT_TAG 0     /* 5:ControlA   6:ControlB */
#define BER_STRUCT_LEN 1
#define BER_STRUCT_PRE 2
#define BER_STRUCT_VAL 3
#define BER_STRUCT_SUF 4
#define BER_STRUCT_CO1 5
#define BER_STRUCT_CO2 6

struct berelement {
  ldap_x_iovec  ber_struct[BER_ARRAY_QUANTITY];   /* See above */

  char          ber_tag_contents[MAX_TAG_SIZE];
  char          ber_len_contents[MAX_LEN_SIZE];
  char          ber_pre_contents[MAX_VALUE_PREFIX_SIZE];
  char          ber_suf_contents[MAX_LEN_SIZE+1];

  char      *ber_buf; /* update the value value when writing in case realloc is called */
  char		*ber_ptr;
  char		*ber_end;
  struct seqorset	*ber_sos;
  ber_len_t ber_tag_len_read;
  ber_tag_t	ber_tag; /* Remove me someday */
  ber_len_t	ber_len; /* Remove me someday */
  int		ber_usertag;
  char		ber_options;
  char		*ber_rwptr;
  BERTranslateProc ber_encode_translate_proc;
  BERTranslateProc ber_decode_translate_proc;
  int		ber_flags;
#define LBER_FLAG_NO_FREE_BUFFER	1	/* don't free ber_buf */
  unsigned  int ber_buf_reallocs;		/* realloc counter */
  int		ber_sos_stack_posn;
  Seqorset	ber_sos_stack[SOS_STACK_SIZE];
};

#define BER_CONTENTS_STRUCT_SIZE (sizeof(ldap_x_iovec) * BER_ARRAY_QUANTITY)

#define NULLBER	((BerElement *)NULL)

#ifdef LDAP_DEBUG
void ber_dump( BerElement *ber, int inout );
#endif



/*
 * structure for read/write I/O callback functions.
 */
struct nslberi_io_fns {
    LDAP_IOF_READ_CALLBACK	*lbiof_read;
    LDAP_IOF_WRITE_CALLBACK	*lbiof_write;
};


/*
 * Old  structure for use with LBER_SOCKBUF_OPT_EXT_IO_FNS:
 */
struct lber_x_ext_io_fns_rev0 {
	    /* lbextiofn_size should always be set to LBER_X_EXTIO_FNS_SIZE */
	int				lbextiofn_size;
	LDAP_X_EXTIOF_READ_CALLBACK	*lbextiofn_read;
	LDAP_X_EXTIOF_WRITE_CALLBACK	*lbextiofn_write;
	struct lextiof_socket_private	*lbextiofn_socket_arg;
};
#define LBER_X_EXTIO_FNS_SIZE_REV0    sizeof(struct lber_x_ext_io_fns_rev0)



struct sockbuf {
	LBER_SOCKET	sb_sd;
	BerElement	sb_ber;
	int		sb_naddr;	/* > 0 implies using CLDAP (UDP) */
	void		*sb_useaddr;	/* pointer to sockaddr to use next */
	void		*sb_fromaddr;	/* pointer to message source sockaddr */
	void		**sb_addrs;	/* actually an array of pointers to
					   sockaddrs */

	int		sb_options;	/* to support copying ber elements */
	LBER_SOCKET	sb_copyfd;	/* for LBER_SOCKBUF_OPT_TO_FILE* opts */
	ber_len_t	sb_max_incoming;
	ber_tag_t   sb_valid_tag;	/* valid tag to accept */
	struct nslberi_io_fns
			sb_io_fns;	/* classic I/O callback functions */

	struct lber_x_ext_io_fns
			sb_ext_io_fns;	/* extended I/O callback functions */
};
#define NULLSOCKBUF	((Sockbuf *)NULL)

/* needed by libldap, even in non-DEBUG builds */
void ber_err_print( char *data );

#ifndef NSLBERI_LBER_INT_FRIEND
/*
 * Everything from this point on is excluded if NSLBERI_LBER_INT_FRIEND is
 * defined.  The code under ../libraries/libldap defines this.
 */

#define READBUFSIZ	8192

/*
 * macros used to check validity of data structures and parameters
 */
#define NSLBERI_VALID_BERELEMENT_POINTER( ber ) \
	( (ber) != NULLBER )

#define NSLBERI_VALID_SOCKBUF_POINTER( sb ) \
	( (sb) != NULLSOCKBUF )

#define LBER_HTONL( l )	htonl( l )
#define LBER_NTOHL( l )	ntohl( l )

/* function prototypes */
#ifdef LDAP_DEBUG
void lber_bprint( char *data, int len );
#endif
void ber_err_print( char *data );
void *nslberi_malloc( size_t size );
void *nslberi_calloc( size_t nelem, size_t elsize );
void *nslberi_realloc( void *ptr, size_t size );
void nslberi_free( void *ptr );
int nslberi_ber_realloc( BerElement *ber, ber_len_t len );

/* blame: dboreham 
 * slapd spends much of its time doing memcpy's for the ber code.
 * Most of these are single-byte, so we special-case those and speed
 * things up considerably. 
 */

#ifdef sunos4
#define THEMEMCPY( d, s, n )	bcopy( s, d, n )
#else /* sunos4 */
#define THEMEMCPY( d, s, n )	memmove( d, s, n )
#endif /* sunos4 */

#ifdef SAFEMEMCPY
#undef SAFEMEMCPY
#define SAFEMEMCPY(d,s,n) if (1 == n) *((char*)d) = *((char*)s); else THEMEMCPY(d,s,n);
#endif

/*
 * Memory allocation done in liblber should all go through one of the
 * following macros. This is so we can plug-in alternative memory
 * allocators, etc. as the need arises.
 */
#define NSLBERI_MALLOC( size )		nslberi_malloc( size )
#define NSLBERI_CALLOC( nelem, elsize )	nslberi_calloc( nelem, elsize )
#define NSLBERI_REALLOC( ptr, size )	nslberi_realloc( ptr, size )
#define NSLBERI_FREE( ptr )		nslberi_free( ptr )

/* allow the library to access the debug variable */

extern int lber_debug;

#endif /* !NSLBERI_LBER_INT_FRIEND */


#ifdef __cplusplus
}
#endif
#endif /* _LBERINT_H */
