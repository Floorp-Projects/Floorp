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
/* lber.h - header file for ber_* functions */
#ifndef _LBER_H
#define _LBER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>	/* to pick up size_t typedef */

/*
 * Note that LBER_ERROR and LBER_DEFAULT are values that can never appear
 * as valid BER tags, and so it is safe to use them to report errors.  In
 * fact, any tag for which the following is true is invalid:
 *     (( tag & 0x00000080 ) != 0 ) && (( tag & 0xFFFFFF00 ) != 0 )
 */
#define LBER_ERROR              0xffffffffUL
#define LBER_DEFAULT            0xffffffffUL	
#define LBER_END_OF_SEQORSET	0xfffffffeUL

/* BER classes and mask */
#define LBER_CLASS_UNIVERSAL    0x00
#define LBER_CLASS_APPLICATION  0x40
#define LBER_CLASS_CONTEXT      0x80
#define LBER_CLASS_PRIVATE      0xc0
#define LBER_CLASS_MASK         0xc0

/* BER encoding type and mask */
#define LBER_PRIMITIVE          0x00
#define LBER_CONSTRUCTED        0x20
#define LBER_ENCODING_MASK      0x20

#define LBER_BIG_TAG_MASK       0x1f
#define LBER_MORE_TAG_MASK      0x80

/* general BER types we know about */
#define LBER_BOOLEAN		0x01L
#define LBER_INTEGER		0x02L
#define LBER_BITSTRING		0x03L
#define LBER_OCTETSTRING	0x04L
#define LBER_NULL		0x05L
#define LBER_ENUMERATED		0x0aL
#define LBER_SEQUENCE		0x30L
#define LBER_SET		0x31L

/* BerElement set/get options */
#define LBER_OPT_REMAINING_BYTES	0x01
#define LBER_OPT_TOTAL_BYTES		0x02
#define LBER_OPT_USE_DER		0x04
#define LBER_OPT_TRANSLATE_STRINGS	0x08
#define LBER_OPT_BYTES_TO_WRITE		0x10
#define LBER_OPT_MEMALLOC_FN_PTRS	0x20
#define LBER_OPT_DEBUG_LEVEL		0x40
/*
 * LBER_USE_DER is defined for compatibility with the C LDAP API RFC.
 * In our implementation, we recognize it (instead of the numerically
 * identical LBER_OPT_REMAINING_BYTES) in calls to ber_alloc_t() and 
 * ber_init_w_nullchar() only.  Callers of ber_set_option() or
 * ber_get_option() must use LBER_OPT_USE_DER instead.  Sorry!
 */
#define LBER_USE_DER			0x01


/* Sockbuf set/get options */
#define LBER_SOCKBUF_OPT_TO_FILE		0x001
#define LBER_SOCKBUF_OPT_TO_FILE_ONLY		0x002
#define LBER_SOCKBUF_OPT_MAX_INCOMING_SIZE	0x004
#define LBER_SOCKBUF_OPT_NO_READ_AHEAD		0x008
#define LBER_SOCKBUF_OPT_DESC			0x010
#define LBER_SOCKBUF_OPT_COPYDESC		0x020
#define LBER_SOCKBUF_OPT_READ_FN		0x040
#define LBER_SOCKBUF_OPT_WRITE_FN		0x080
#define LBER_SOCKBUF_OPT_EXT_IO_FNS		0x100
#define	LBER_SOCKBUF_OPT_VALID_TAG		0x200
#define LBER_SOCKBUF_OPT_SOCK_ARG		0x400

#define LBER_OPT_ON	((void *) 1)
#define LBER_OPT_OFF	((void *) 0)


typedef struct berval {
	unsigned long	bv_len;
	char		*bv_val;
} BerValue;

typedef struct berelement BerElement;
typedef struct sockbuf Sockbuf;
typedef int (*BERTranslateProc)( char **bufp, unsigned long *buflenp,
	int free_input );
#if defined( _WINDOWS ) || defined( _WIN32) || defined( _CONSOLE )
#include <winsock.h> /* for SOCKET */
typedef SOCKET LBER_SOCKET;
#else
typedef int LBER_SOCKET;
#endif /* _WINDOWS */

/* calling conventions used by library */
#ifndef LDAP_CALL
#if defined( _WINDOWS ) || defined( _WIN32 )
#define LDAP_C __cdecl
#ifndef _WIN32 
#define __stdcall _far _pascal
#define LDAP_CALLBACK _loadds
#else
#define LDAP_CALLBACK
#endif /* _WIN32 */
#define LDAP_PASCAL __stdcall
#define LDAP_CALL LDAP_PASCAL
#else /* _WINDOWS */
#define LDAP_C
#define LDAP_CALLBACK
#define LDAP_PASCAL
#define LDAP_CALL
#endif /* _WINDOWS */
#endif /* LDAP_CALL */

/*
 * function prototypes for lber library
 */

#ifndef LDAP_API
#if defined( _WINDOWS ) || defined( _WIN32 )
#define LDAP_API(rt) rt
#else /* _WINDOWS */
#define LDAP_API(rt) rt
#endif /* _WINDOWS */
#endif /* LDAP_API */

struct lextiof_socket_private;          /* Defined by the extended I/O */
                                        /* callback functions */
struct lextiof_session_private;         /* Defined by the extended I/O */
                                        /* callback functions */

/* This is modeled after the PRIOVec that is passed to the NSPR
   writev function! The void* is a char* in that struct */
typedef struct ldap_x_iovec {
        char    *ldapiov_base;
        int     ldapiov_len;
} ldap_x_iovec;

/*
 * libldap read and write I/O function callbacks.  The rest of the I/O callback
 * types are defined in ldap.h
 */
typedef int (LDAP_C LDAP_CALLBACK LDAP_IOF_READ_CALLBACK)( LBER_SOCKET s,
	void *buf, int bufsize );
typedef int (LDAP_C LDAP_CALLBACK LDAP_IOF_WRITE_CALLBACK)( LBER_SOCKET s,
	const void *buf, int len );
typedef int (LDAP_C LDAP_CALLBACK LDAP_X_EXTIOF_READ_CALLBACK)( int s,
	void *buf, int bufsize, struct lextiof_socket_private *socketarg );
typedef int (LDAP_C LDAP_CALLBACK LDAP_X_EXTIOF_WRITE_CALLBACK)( int s,
	const void *buf, int len, struct lextiof_socket_private *socketarg );
typedef int (LDAP_C LDAP_CALLBACK LDAP_X_EXTIOF_WRITEV_CALLBACK)(int s,
	const ldap_x_iovec iov[], int iovcnt, struct lextiof_socket_private *socketarg);
						     

/*
 * Structure for use with LBER_SOCKBUF_OPT_EXT_IO_FNS:
 */
struct lber_x_ext_io_fns {
	    /* lbextiofn_size should always be set to LBER_X_EXTIO_FNS_SIZE */
	int				lbextiofn_size;
	LDAP_X_EXTIOF_READ_CALLBACK	*lbextiofn_read;
	LDAP_X_EXTIOF_WRITE_CALLBACK	*lbextiofn_write;
	struct lextiof_socket_private	*lbextiofn_socket_arg;
        LDAP_X_EXTIOF_WRITEV_CALLBACK   *lbextiofn_writev;
};
#define LBER_X_EXTIO_FNS_SIZE sizeof(struct lber_x_ext_io_fns)

/*
 * liblber memory allocation callback functions.  These are global to all
 *  Sockbufs and BerElements.  Install your own functions by using a call
 *  like this: ber_set_option( NULL, LBER_OPT_MEMALLOC_FN_PTRS, &memalloc_fns );
 */
typedef void * (LDAP_C LDAP_CALLBACK LDAP_MALLOC_CALLBACK)( size_t size );
typedef void * (LDAP_C LDAP_CALLBACK LDAP_CALLOC_CALLBACK)( size_t nelem,
	size_t elsize );
typedef void * (LDAP_C LDAP_CALLBACK LDAP_REALLOC_CALLBACK)( void *ptr,
	size_t size );
typedef void (LDAP_C LDAP_CALLBACK LDAP_FREE_CALLBACK)( void *ptr );

struct lber_memalloc_fns {
	LDAP_MALLOC_CALLBACK	*lbermem_malloc;
	LDAP_CALLOC_CALLBACK	*lbermem_calloc;
	LDAP_REALLOC_CALLBACK	*lbermem_realloc;
	LDAP_FREE_CALLBACK	*lbermem_free;
};

/*
 * decode routines
 */
LDAP_API(unsigned long) LDAP_CALL ber_get_tag( BerElement *ber );
LDAP_API(unsigned long) LDAP_CALL ber_skip_tag( BerElement *ber, 
	unsigned long *len );
LDAP_API(unsigned long) LDAP_CALL ber_peek_tag( BerElement *ber, 
	unsigned long *len );
LDAP_API(unsigned long) LDAP_CALL ber_get_int( BerElement *ber, long *num );
LDAP_API(unsigned long) LDAP_CALL ber_get_stringb( BerElement *ber, char *buf,
	unsigned long *len );
LDAP_API(unsigned long) LDAP_CALL ber_get_stringa( BerElement *ber, 
	char **buf );
LDAP_API(unsigned long) LDAP_CALL ber_get_stringal( BerElement *ber,
	struct berval **bv );
LDAP_API(unsigned long) LDAP_CALL ber_get_bitstringa( BerElement *ber, 
	char **buf, unsigned long *len );
LDAP_API(unsigned long) LDAP_CALL ber_get_null( BerElement *ber );
LDAP_API(unsigned long) LDAP_CALL ber_get_boolean( BerElement *ber, 
	int *boolval );
LDAP_API(unsigned long) LDAP_CALL ber_first_element( BerElement *ber,
	unsigned long *len, char **last );
LDAP_API(unsigned long) LDAP_CALL ber_next_element( BerElement *ber,
	unsigned long *len, char *last );
LDAP_API(unsigned long) LDAP_C ber_scanf( BerElement *ber, const char *fmt,
	... );
LDAP_API(void) LDAP_CALL ber_bvfree( struct berval *bv );
LDAP_API(void) LDAP_CALL ber_bvecfree( struct berval **bv );
LDAP_API(void) LDAP_CALL ber_svecfree( char **vals );
LDAP_API(struct berval *) LDAP_CALL ber_bvdup( const struct berval *bv );
LDAP_API(void) LDAP_CALL ber_set_string_translators( BerElement *ber,
	BERTranslateProc encode_proc, BERTranslateProc decode_proc );
LDAP_API(BerElement *) LDAP_CALL ber_init( const struct berval *bv );

/*
 * encoding routines
 */
LDAP_API(int) LDAP_CALL ber_put_enum( BerElement *ber, long num, 
	unsigned long tag );
LDAP_API(int) LDAP_CALL ber_put_int( BerElement *ber, long num, 
	unsigned long tag );
LDAP_API(int) LDAP_CALL ber_put_ostring( BerElement *ber, char *str, 
	unsigned long len, unsigned long tag );
LDAP_API(int) LDAP_CALL ber_put_string( BerElement *ber, char *str,
	unsigned long tag );
LDAP_API(int) LDAP_CALL ber_put_bitstring( BerElement *ber, char *str,
	unsigned long bitlen, unsigned long tag );
LDAP_API(int) LDAP_CALL ber_put_null( BerElement *ber, unsigned long tag );
LDAP_API(int) LDAP_CALL ber_put_boolean( BerElement *ber, int boolval,
	unsigned long tag );
LDAP_API(int) LDAP_CALL ber_start_seq( BerElement *ber, unsigned long tag );
LDAP_API(int) LDAP_CALL ber_start_set( BerElement *ber, unsigned long tag );
LDAP_API(int) LDAP_CALL ber_put_seq( BerElement *ber );
LDAP_API(int) LDAP_CALL ber_put_set( BerElement *ber );
LDAP_API(int) LDAP_C ber_printf( BerElement *ber, const char *fmt, ... );
LDAP_API(int) LDAP_CALL ber_flatten( BerElement *ber,
	struct berval **bvPtr );

/*
 * miscellaneous routines
 */
LDAP_API(void) LDAP_CALL ber_free( BerElement *ber, int freebuf );
LDAP_API(void) LDAP_CALL ber_special_free(void* buf, BerElement *ber);
LDAP_API(int) LDAP_CALL ber_flush( Sockbuf *sb, BerElement *ber, int freeit );
LDAP_API(BerElement*) LDAP_CALL ber_alloc( void );
LDAP_API(BerElement*) LDAP_CALL der_alloc( void );
LDAP_API(BerElement*) LDAP_CALL ber_alloc_t( int options );
LDAP_API(void*) LDAP_CALL ber_special_alloc(size_t size, BerElement **ppBer);
LDAP_API(BerElement*) LDAP_CALL ber_dup( BerElement *ber );
LDAP_API(unsigned long) LDAP_CALL ber_get_next( Sockbuf *sb, unsigned long *len,
	BerElement *ber );
LDAP_API(unsigned long) LDAP_CALL ber_get_next_buffer( void *buffer,
	size_t buffer_size, unsigned long *len, BerElement *ber,
	unsigned long *Bytes_Scanned );
LDAP_API(unsigned long) LDAP_CALL ber_get_next_buffer_ext( void *buffer,
	size_t buffer_size, unsigned long *len, BerElement *ber,
	unsigned long *Bytes_Scanned, Sockbuf *sb );
LDAP_API(long) LDAP_CALL ber_read( BerElement *ber, char *buf, 
	unsigned long len );
LDAP_API(long) LDAP_CALL ber_write( BerElement *ber, char *buf, 
	unsigned long len, int nosos );
LDAP_API(void) LDAP_CALL ber_init_w_nullchar( BerElement *ber, int options );
LDAP_API(void) LDAP_CALL ber_reset( BerElement *ber, int was_writing );
LDAP_API(int) LDAP_CALL ber_set_option( BerElement *ber, int option, 
	void *value );
LDAP_API(int) LDAP_CALL ber_get_option( BerElement *ber, int option, 
	void *value );
LDAP_API(Sockbuf*) LDAP_CALL ber_sockbuf_alloc( void );
LDAP_API(void) LDAP_CALL ber_sockbuf_free( Sockbuf* p );
LDAP_API(int) LDAP_CALL ber_sockbuf_set_option( Sockbuf *sb, int option, 
	void *value );
LDAP_API(int) LDAP_CALL ber_sockbuf_get_option( Sockbuf *sb, int option, 
	void *value );

#ifdef __cplusplus
}
#endif
#endif /* _LBER_H */
