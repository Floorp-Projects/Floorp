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
/* io.c - ber general i/o routines */

#include "lber-int.h"

#define bergetc( sb, len )    ( sb->sb_ber.ber_end > sb->sb_ber.ber_ptr ? \
			  (unsigned char)*sb->sb_ber.ber_ptr++ : \
			  ber_filbuf( sb, len ))

# ifdef macintosh
/*
 * MacTCP/OpenTransport
 */
#  define read( s, b, l ) tcpread( s, 0, (unsigned char *)b, l, NULL )
#  define MAX_WRITE	65535
#  define BerWrite( sb, b, l )   tcpwrite( sb->sb_sd, (unsigned char *)(b), (l<MAX_WRITE)? l : MAX_WRITE )
# else /* macintosh */
#  if defined(_WIN32) || defined(_WINDOWS) || defined(XP_OS2)
/*
 * 32-bit Windows Socket API (under Windows NT or Windows 95)
 */
#   define read( s, b, l )		recv( s, b, l, 0 )
#   define BerWrite( s, b, l )	send( s->sb_sd, b, l, 0 )
#  else /* _WIN32 */
/*
 * everything else (Unix/BSD 4.3 socket API)
 */
#   define BerWrite( sb, b, l )	write( sb->sb_sd, b, l )
#   define udp_read( sb, b, l, al ) recvfrom(sb->sb_sd, (char *)b, l, 0, \
		(struct sockaddr *)sb->sb_fromaddr, \
		(al = sizeof(struct sockaddr), &al))
#   define udp_write( sb, b, l ) sendto(sb->sb_sd, (char *)(b), l, 0, \
		(struct sockaddr *)sb->sb_useaddr, sizeof(struct sockaddr))
#  endif /* _WIN32 */
# endif /* macintosh */

#ifndef udp_read
#define udp_read( sb, b, l, al )	CLDAP NOT SUPPORTED
#define udp_write( sb, b, l )		CLDAP NOT SUPPORTED
#endif /* udp_read */

#define EXBUFSIZ	1024

#ifdef LDAP_DEBUG
int	lber_debug;
#endif

/*
 * function prototypes
 */
static void nslberi_install_compat_io_fns( Sockbuf *sb );
static int nslberi_extread_compat( int s, void *buf, int len,
		struct lextiof_socket_private *arg );
static int nslberi_extwrite_compat( int s, const void *buf, int len,
		struct lextiof_socket_private *arg );
static unsigned long get_tag( Sockbuf *sb, BerElement *ber);
static unsigned long get_ber_len( BerElement *ber);
static unsigned long read_len_in_ber( Sockbuf *sb, BerElement *ber);
static int get_and_check_length( BerElement *ber,
		unsigned long length, Sockbuf *sock );

/*
 * internal global structure for memory allocation callback functions
 */
static struct lber_memalloc_fns nslberi_memalloc_fns;


/*
 * buffered read from "sb".
 * returns value of first character read on success and -1 on error.
 */
static int
ber_filbuf( Sockbuf *sb, long len )
{
	short	rc;
#ifdef CLDAP
	int	addrlen;
#endif /* CLDAP */

	if ( sb->sb_ber.ber_buf == NULL ) {
		if ( (sb->sb_ber.ber_buf = (char *)NSLBERI_MALLOC(
		    READBUFSIZ )) == NULL ) {
			return( -1 );
		}
		sb->sb_ber.ber_flags &= ~LBER_FLAG_NO_FREE_BUFFER;
		sb->sb_ber.ber_ptr = sb->sb_ber.ber_buf;
		sb->sb_ber.ber_end = sb->sb_ber.ber_buf;
	}

	if ( sb->sb_naddr > 0 ) {
#ifdef CLDAP
		rc = udp_read(sb, sb->sb_ber.ber_buf, READBUFSIZ, addrlen );
#ifdef LDAP_DEBUG
		if ( lber_debug ) {
			char msg[80];
			sprintf( msg, "ber_filbuf udp_read %d bytes\n",
				rc );
			ber_err_print( msg );
			if ( lber_debug > 1 && rc > 0 )
				lber_bprint( sb->sb_ber.ber_buf, rc );
		}
#endif /* LDAP_DEBUG */
#else /* CLDAP */
		rc = -1;
#endif /* CLDAP */
	} else {
		if ( sb->sb_ext_io_fns.lbextiofn_read != NULL ) {
			rc = sb->sb_ext_io_fns.lbextiofn_read(
			    sb->sb_sd, sb->sb_ber.ber_buf,
			    ((sb->sb_options & LBER_SOCKBUF_OPT_NO_READ_AHEAD)
			    && (len < READBUFSIZ)) ? len : READBUFSIZ,
			    sb->sb_ext_io_fns.lbextiofn_socket_arg );
		} else {
#ifdef NSLDAPI_AVOID_OS_SOCKETS
			return( -1 );
#else
			rc = read( sb->sb_sd, sb->sb_ber.ber_buf,
			    ((sb->sb_options & LBER_SOCKBUF_OPT_NO_READ_AHEAD)
			    && (len < READBUFSIZ)) ? len : READBUFSIZ );
#endif
		}
	}

	if ( rc > 0 ) {
		sb->sb_ber.ber_ptr = sb->sb_ber.ber_buf + 1;
		sb->sb_ber.ber_end = sb->sb_ber.ber_buf + rc;
		return( (unsigned char)*sb->sb_ber.ber_buf );
	}

	return( -1 );
}


static long
BerRead( Sockbuf *sb, char *buf, long len )
{
	int	c;
	long	nread = 0;

	while ( len > 0 ) {
		if ( (c = bergetc( sb, len )) < 0 ) {
			if ( nread > 0 )
				break;
			return( c );
		}
		*buf++ = c;
		nread++;
		len--;
	}

	return( nread );
}


/*
 * Note: ber_read() only uses the ber_end and ber_ptr elements of ber.
 * Functions like ber_get_tag(), ber_skip_tag, and ber_peek_tag() rely on
 * that fact, so if this code is changed to use any additional elements of
 * the ber structure, those functions will need to be changed as well.
 */
long
LDAP_CALL
ber_read( BerElement *ber, char *buf, unsigned long len )
{
	unsigned long	actuallen, nleft;

	nleft = ber->ber_end - ber->ber_ptr;
	actuallen = nleft < len ? nleft : len;

	SAFEMEMCPY( buf, ber->ber_ptr, (size_t)actuallen );

	ber->ber_ptr += actuallen;

	return( (long)actuallen );
}

/*
 * enlarge the ber buffer.
 * return 0 on success, -1 on error.
 */
int
nslberi_ber_realloc( BerElement *ber, unsigned long len )
{
	unsigned long	need, have, total;
	size_t		have_bytes;
	Seqorset	*s;
	long		off;
	char		*oldbuf;

	have_bytes = ber->ber_end - ber->ber_buf;
	have = have_bytes / EXBUFSIZ;
	need = (len < EXBUFSIZ ? 1 : (len + (EXBUFSIZ - 1)) / EXBUFSIZ);
	total = have * EXBUFSIZ + need * EXBUFSIZ;

	oldbuf = ber->ber_buf;

	if (ber->ber_buf == NULL) {
		if ( (ber->ber_buf = (char *)NSLBERI_MALLOC( (size_t)total ))
		    == NULL ) {
			return( -1 );
		}
		ber->ber_flags &= ~LBER_FLAG_NO_FREE_BUFFER;
	} else {
		if ( ber->ber_flags & LBER_FLAG_NO_FREE_BUFFER ) {
			/* transition to malloc'd buffer */
			if ( (ber->ber_buf = (char *)NSLBERI_MALLOC(
			    (size_t)total )) == NULL ) {
				return( -1 );
			}
			ber->ber_flags &= ~LBER_FLAG_NO_FREE_BUFFER;
			/* copy existing data into new malloc'd buffer */
			SAFEMEMCPY( ber->ber_buf, oldbuf, have_bytes );
		} else {
			if ( (ber->ber_buf = (char *)NSLBERI_REALLOC(
			    ber->ber_buf,(size_t)total )) == NULL ) {
				return( -1 );
			}
		}
	}

	ber->ber_end = ber->ber_buf + total;

	/*
	 * If the stinking thing was moved, we need to go through and
	 * reset all the sos and ber pointers.  Offsets would've been
	 * a better idea... oh well.
	 */

	if ( ber->ber_buf != oldbuf ) {	
		ber->ber_ptr = ber->ber_buf + (ber->ber_ptr - oldbuf);

		for ( s = ber->ber_sos; s != NULLSEQORSET; s = s->sos_next ) {
			off = s->sos_first - oldbuf;
			s->sos_first = ber->ber_buf + off;

			off = s->sos_ptr - oldbuf;
			s->sos_ptr = ber->ber_buf + off;
		}
	}

	return( 0 );
}

/*
 * returns "len" on success and -1 on failure.
 */
long
LDAP_CALL
ber_write( BerElement *ber, char *buf, unsigned long len, int nosos )
{
	if ( nosos || ber->ber_sos == NULL ) {
		if ( ber->ber_ptr + len > ber->ber_end ) {
			if ( nslberi_ber_realloc( ber, len ) != 0 )
				return( -1 );
		}
		SAFEMEMCPY( ber->ber_ptr, buf, (size_t)len );
		ber->ber_ptr += len;
		return( len );
	} else {
		if ( ber->ber_sos->sos_ptr + len > ber->ber_end ) {
			if ( nslberi_ber_realloc( ber, len ) != 0 )
				return( -1 );
		}
		SAFEMEMCPY( ber->ber_sos->sos_ptr, buf, (size_t)len );
		ber->ber_sos->sos_ptr += len;
		ber->ber_sos->sos_clen += len;
		return( len );
	}
}

void
LDAP_CALL
ber_free( BerElement *ber, int freebuf )
{
	if ( ber != NULL ) {
		    if ( freebuf &&
			!(ber->ber_flags & LBER_FLAG_NO_FREE_BUFFER)) {
			    NSLBERI_FREE(ber->ber_buf);
		    }
		    NSLBERI_FREE( (char *) ber );
	}
}

/*
 * return >= 0 on success, -1 on failure.
 */
int
LDAP_CALL
ber_flush( Sockbuf *sb, BerElement *ber, int freeit )
{
	long	nwritten = 0, towrite, rc;
	int     i = 0;
	
	if (ber->ber_rwptr == NULL) {
	  ber->ber_rwptr = ber->ber_buf;
	} else if (ber->ber_rwptr >= ber->ber_end) {
	  /* we will use the ber_rwptr to continue an exited flush,
	     so if rwptr is not within the buffer we return an error. */
	  return( -1 );
	}
	
	/* writev section - for iDAR only!!! This section ignores freeit!!! */
	if (sb->sb_ext_io_fns.lbextiofn_writev != NULL) {

	  /* add the sizes of the different buffers to write with writev */
	  for (towrite = 0, i = 0; i < BER_ARRAY_QUANTITY; ++i) {
	    /* don't add lengths of null buffers - writev will ignore them */
	    if (ber->ber_struct[i].ldapiov_base) {
	      towrite += ber->ber_struct[i].ldapiov_len;
	    }
	  }
	  
	  rc = sb->sb_ext_io_fns.lbextiofn_writev(sb->sb_sd, ber->ber_struct, BER_ARRAY_QUANTITY, 
						  sb->sb_ext_io_fns.lbextiofn_socket_arg);
	  
	  if (rc >= 0) {
	    /* return the number of bytes TO BE written */
	    return (towrite - rc);
	  } else {
	    /* otherwise it's an error */
	    return (rc);
	  }
	} /* end writev section */

	towrite = ber->ber_ptr - ber->ber_rwptr;

#ifdef LDAP_DEBUG
	if ( lber_debug ) {
		char msg[80];
		sprintf( msg, "ber_flush: %ld bytes to sd %ld%s\n", towrite,
		    sb->sb_sd, ber->ber_rwptr != ber->ber_buf ? " (re-flush)"
		    : "" );
		ber_err_print( msg );
		if ( lber_debug > 1 )
			lber_bprint( ber->ber_rwptr, towrite );
	}
#endif
#if !defined(macintosh) && !defined(DOS)
	if ( sb->sb_options & (LBER_SOCKBUF_OPT_TO_FILE | LBER_SOCKBUF_OPT_TO_FILE_ONLY) ) {
		rc = write( sb->sb_copyfd, ber->ber_buf, towrite );
		if ( sb->sb_options & LBER_SOCKBUF_OPT_TO_FILE_ONLY ) {
			return( (int)rc );
		}
	}
#endif

	nwritten = 0;
	do {
		if (sb->sb_naddr > 0) {
#ifdef CLDAP
			rc = udp_write( sb, ber->ber_buf + nwritten,
			    (size_t)towrite );
#else /* CLDAP */
			rc = -1;
#endif /* CLDAP */
			if ( rc <= 0 )
				return( -1 );
			/* fake error if write was not atomic */
			if (rc < towrite) {
#if !defined( macintosh ) && !defined( DOS )
			    errno = EMSGSIZE;  /* For Win32, see portable.h */
#endif
			    return( -1 );
			}
		} else {
			if ( sb->sb_ext_io_fns.lbextiofn_write != NULL ) {
				if ( (rc = sb->sb_ext_io_fns.lbextiofn_write(
				    sb->sb_sd, ber->ber_rwptr, (size_t)towrite,
				    sb->sb_ext_io_fns.lbextiofn_socket_arg ))
				    <= 0 ) {
					return( -1 );
				}
			} else {
#ifdef NSLDAPI_AVOID_OS_SOCKETS
				return( -1 );
#else
				if ( (rc = BerWrite( sb, ber->ber_rwptr,
				    (size_t) towrite )) <= 0 ) {
					return( -1 );
				}
#endif
			}
		}
		towrite -= rc;
		nwritten += rc;
		ber->ber_rwptr += rc;
	} while ( towrite > 0 );

	if ( freeit )
		ber_free( ber, 1 );

	return( 0 );
}


/* we pre-allocate a buffer to save the extra malloc later */
BerElement *
LDAP_CALL
ber_alloc_t( int options )
{
	BerElement	*ber;

	if ( (ber = (BerElement*)NSLBERI_CALLOC( 1,
	    sizeof(struct berelement) + EXBUFSIZ )) == NULL ) {
		return( NULL );
	}

	/*
	 * for compatibility with the C LDAP API standard, we recognize
	 * LBER_USE_DER as LBER_OPT_USE_DER.  See lber.h for a bit more info.
	 */
	if ( options & LBER_USE_DER ) {
		options &= ~LBER_USE_DER;
		options |= LBER_OPT_USE_DER;
	}

	ber->ber_tag = LBER_DEFAULT;
	ber->ber_options = options;
	ber->ber_buf = (char*)ber + sizeof(struct berelement);
	ber->ber_ptr = ber->ber_buf;
	ber->ber_end = ber->ber_buf + EXBUFSIZ;
	ber->ber_flags = LBER_FLAG_NO_FREE_BUFFER;

	return( ber );
}


BerElement *
LDAP_CALL
ber_alloc()
{
	return( ber_alloc_t( 0 ) );
}

BerElement *
LDAP_CALL
der_alloc()
{
	return( ber_alloc_t( LBER_OPT_USE_DER ) );
}

BerElement *
LDAP_CALL
ber_dup( BerElement *ber )
{
	BerElement	*new;

	if ( (new = ber_alloc()) == NULL )
		return( NULL );

	*new = *ber;

	return( new );
}


void
LDAP_CALL
ber_init_w_nullchar( BerElement *ber, int options )
{
	(void) memset( (char *)ber, '\0', sizeof(struct berelement) );
	ber->ber_tag = LBER_DEFAULT;

	/*
	 * For compatibility with the C LDAP API standard, we recognize
	 * LBER_USE_DER as LBER_OPT_USE_DER.  See lber.h for a bit more info.
	 */
	if ( options & LBER_USE_DER ) {
		options &= ~LBER_USE_DER;
		options |= LBER_OPT_USE_DER;
	}

	ber->ber_options = options;
}

void
LDAP_CALL
ber_reset( BerElement *ber, int was_writing )
{
	if ( was_writing ) {
		ber->ber_end = ber->ber_ptr;
		ber->ber_ptr = ber->ber_buf;
	} else {
		ber->ber_ptr = ber->ber_end;
	}

	ber->ber_rwptr = NULL;

	memset(ber->ber_struct, 0, BER_CONTENTS_STRUCT_SIZE);
}


#ifdef LDAP_DEBUG

void
ber_dump( BerElement *ber, int inout )
{
	char msg[128];
	sprintf( msg, "ber_dump: buf 0x%lx, ptr 0x%lx, rwptr 0x%lx, end 0x%lx\n",
	    ber->ber_buf, ber->ber_ptr, ber->ber_rwptr, ber->ber_end );
	ber_err_print( msg );
	if ( inout == 1 ) {
		sprintf( msg, "          current len %ld, contents:\n",
		    ber->ber_end - ber->ber_ptr );
		ber_err_print( msg );
		lber_bprint( ber->ber_ptr, ber->ber_end - ber->ber_ptr );
	} else {
		sprintf( msg, "          current len %ld, contents:\n",
		    ber->ber_ptr - ber->ber_buf );
		ber_err_print( msg );
		lber_bprint( ber->ber_buf, ber->ber_ptr - ber->ber_buf );
	}
}

void
ber_sos_dump( Seqorset *sos )
{
	char msg[80];
	ber_err_print ( "*** sos dump ***\n" );
	while ( sos != NULLSEQORSET ) {
		sprintf( msg, "ber_sos_dump: clen %ld first 0x%lx ptr 0x%lx\n",
		    sos->sos_clen, sos->sos_first, sos->sos_ptr );
		ber_err_print( msg );
		sprintf( msg, "              current len %ld contents:\n",
		    sos->sos_ptr - sos->sos_first );
		ber_err_print( msg );
		lber_bprint( sos->sos_first, sos->sos_ptr - sos->sos_first );

		sos = sos->sos_next;
	}
	ber_err_print( "*** end dump ***\n" );
}

#endif

/* return the tag - LBER_DEFAULT returned means trouble
 * assumes the tag is only one byte! */
static unsigned long
get_tag( Sockbuf *sb, BerElement *ber)
{
        unsigned char   xbyte;

        if ( (BerRead( sb, (char *) &xbyte, 1 )) != 1 ) {
                return( LBER_DEFAULT );
        }

	/* we only handle small (one byte) tags */
        if ( (xbyte & LBER_BIG_TAG_MASK) == LBER_BIG_TAG_MASK ) {
                return( LBER_DEFAULT );
        }

	ber->ber_tag_contents[0] = xbyte;
	ber->ber_struct[BER_STRUCT_TAG].ldapiov_len = 1;
	return((unsigned long)xbyte);
}


/* Error checking? */
/* Takes a ber and returns the actual length in a long */
static unsigned long
get_ber_len( BerElement *ber)
{
  int noctets;
  unsigned long len = 0;
  char xbyte;

  xbyte = ber->ber_len_contents[0];
  
  /* long form */
  if (xbyte & 0x80) {
    noctets = (int) (xbyte & 0x7f);
    if (noctets >= MAX_LEN_SIZE) {
      return(LBER_DEFAULT);
    }
    memcpy((char*) &len + sizeof(unsigned long) - noctets, &ber->ber_len_contents[1], noctets);
    len = LBER_NTOHL(len);
    return(len);
  } else {
    return((unsigned long)(xbyte));
  }
}

/* LBER_DEFAULT means trouble
   reads in the length, stores it in ber->ber_struct, and returns get_ber_len */
static unsigned long
read_len_in_ber( Sockbuf *sb, BerElement *ber)
{
  unsigned char xbyte;
  int           noctets;
  int           rc = 0;

  /*
   * Next, read the length.  The first byte contains the length
   * of the length.  If bit 8 is set, the length is the long
   * form, otherwise it's the short form.  We don't allow a
   * length that's greater than what we can hold in a long (2GB)
   */
 
  if ( BerRead( sb, (char *) &xbyte, 1 ) != 1 ) {
    return( LBER_DEFAULT );
  }

  ber->ber_len_contents[0] = xbyte;

  /* long form of the length value */
  if ( xbyte & 0x80 ) {
    noctets = (xbyte & 0x7f);
    if ( noctets >= MAX_LEN_SIZE )
      return( LBER_DEFAULT );
    while (rc < noctets) {
      if ( (rc += BerRead( sb, &(ber->ber_len_contents[1]) + rc, noctets - rc )) <= 0) {
	return( LBER_DEFAULT );
      }
    }
    ber->ber_struct[BER_STRUCT_LEN].ldapiov_len = 1 + noctets;
  } else { /* short form of the length value */ 
    ber->ber_struct[BER_STRUCT_LEN].ldapiov_len = 1;
  }
  return(get_ber_len(ber));
}


/*
 * Returns the tag of the message or LBER_DEFAULT if an error occurs.
 * To check for EWOULDBLOCK or other temporary errors, the system error
 * number (errno) should be consulted.
 */
unsigned long
LDAP_CALL
ber_get_next( Sockbuf *sb, unsigned long *len, BerElement *ber )
{
	unsigned long	toread;
	long		rc;

#ifdef LDAP_DEBUG
	if ( lber_debug )
		ber_err_print( "ber_get_next\n" );
#endif

	/*
	 * first time through - malloc the buffer, set up ptrs, and
	 * read the tag and the length and as much of the rest as we can
	 */

	if ( ber->ber_rwptr == NULL ) {
	  /* read the tag */
	  unsigned long tag = get_tag(sb, ber);
	  unsigned long newlen;

	  if (tag == LBER_DEFAULT ) {
	    return( LBER_DEFAULT );
	  }

	  if((sb->sb_options & LBER_SOCKBUF_OPT_VALID_TAG) &&
	     (tag != sb->sb_valid_tag)) {
	    return( LBER_DEFAULT);
          }

	  /* read the length */
	  if ((newlen = read_len_in_ber(sb, ber)) == LBER_DEFAULT ) {
	    return( LBER_DEFAULT );
	  }

	  /*
	   * Finally, malloc a buffer for the contents and read it in.
	   * It's this buffer that's passed to all the other ber decoding
	   * routines.
	   */
	  
#if defined( DOS ) && !( defined( _WIN32 ) || defined(XP_OS2) )
	  if ( newlen > 65535 ) {	/* DOS can't allocate > 64K */
	    return( LBER_DEFAULT );
	  }
#endif /* DOS && !_WIN32 */
	  
	  if ( ( sb->sb_options & LBER_SOCKBUF_OPT_MAX_INCOMING_SIZE )
	       && newlen > sb->sb_max_incoming ) {
	    return( LBER_DEFAULT );
	  }
	  
	  /* check to see if we already have enough memory allocated */
	  if ( (ber->ber_end - ber->ber_buf) < (size_t)newlen) {	  
	    if ( (ber->ber_buf = (char *)NSLBERI_CALLOC( 1,(size_t)newlen ))
		 == NULL ) {
	      return( LBER_DEFAULT );
	    }
	    ber->ber_flags &= ~LBER_FLAG_NO_FREE_BUFFER;
	  }
	  

	  ber->ber_len = newlen;
	  ber->ber_ptr = ber->ber_buf;
	  ber->ber_end = ber->ber_buf + newlen;
	  ber->ber_rwptr = ber->ber_buf;
	}

	/* OK, we've malloc-ed the buffer; now read the rest of the expected length */
	toread = (unsigned long)ber->ber_end - (unsigned long)ber->ber_rwptr;
	while ( toread > 0 ) {
	  if ( (rc = BerRead( sb, ber->ber_rwptr, (long)toread )) <= 0 ) {
	    return( LBER_DEFAULT );
	  }
	  
	  toread -= rc;
	  ber->ber_rwptr += rc;
	}
	
#ifdef LDAP_DEBUG
	if ( lber_debug ) {
	  char msg[80];
	  sprintf( msg, "ber_get_next: tag 0x%lx len %ld contents:\n",
		   ber->ber_tag_contents[0], ber->ber_len );
	  ber_err_print( msg );
	  if ( lber_debug > 1 )
	    ber_dump( ber, 1 );
	}
#endif
		
	ber->ber_rwptr = NULL;
	*len = ber->ber_struct[BER_STRUCT_VAL].ldapiov_len = ber->ber_len;
	return(ber->ber_tag_contents[0]);
}

Sockbuf *
LDAP_CALL
ber_sockbuf_alloc()
{
	return( (Sockbuf *)NSLBERI_CALLOC( 1, sizeof(struct sockbuf) ) );
}

void
LDAP_CALL
ber_sockbuf_free(Sockbuf *p)
{
	if ( p != NULL ) {
		if ( p->sb_ber.ber_buf != NULL &&
		    !(p->sb_ber.ber_flags & LBER_FLAG_NO_FREE_BUFFER) ) {
			NSLBERI_FREE( p->sb_ber.ber_buf );
		}
		NSLBERI_FREE(p);
	}
}

/*
 * return 0 on success and -1 on error
 */
int
LDAP_CALL
ber_set_option( struct berelement *ber, int option, void *value )
{
  
  /*
   * memory allocation callbacks are global, so it is OK to pass
   * NULL for ber.  Handle this as a special case.
   */
  if ( option == LBER_OPT_MEMALLOC_FN_PTRS ) {
    /* struct copy */
    nslberi_memalloc_fns = *((struct lber_memalloc_fns *)value);
    return( 0 );
  }
  
  /*
   * lber_debug is global, so it is OK to pass
   * NULL for ber.  Handle this as a special case.
   */
  if ( option == LBER_OPT_DEBUG_LEVEL ) {
#ifdef LDAP_DEBUG
    lber_debug = *(int *)value;
#endif
    return( 0 );
  }
  
  /*
   * all the rest require a non-NULL ber
   */
  if ( !NSLBERI_VALID_BERELEMENT_POINTER( ber )) {
    return( -1 );
  }
  
  switch ( option ) {
	case LBER_OPT_USE_DER:
	case LBER_OPT_TRANSLATE_STRINGS:
		if ( value != NULL ) {
			ber->ber_options |= option;
		} else {
			ber->ber_options &= ~option;
		}
		break;
	case LBER_OPT_REMAINING_BYTES:
		ber->ber_end = ber->ber_ptr + *((unsigned long *)value);
		break;
	case LBER_OPT_TOTAL_BYTES:
		ber->ber_end = ber->ber_buf + *((unsigned long *)value);
		break;
	case LBER_OPT_BYTES_TO_WRITE:
		ber->ber_ptr = ber->ber_buf + *((unsigned long *)value);
		break;
	default:
		return( -1 );
  }
  
  return( 0 );
}

/*
 * return 0 on success and -1 on error
 */
int
LDAP_CALL
ber_get_option( struct berelement *ber, int option, void *value )
{
	/*
	 * memory callocation callbacks are global, so it is OK to pass
	 * NULL for ber.  Handle this as a special case
	 */
	if ( option == LBER_OPT_MEMALLOC_FN_PTRS ) {
		/* struct copy */
		*((struct lber_memalloc_fns *)value) = nslberi_memalloc_fns;
		return( 0 );
	}
	
	/*
	 * lber_debug is global, so it is OK to pass
	 * NULL for ber.  Handle this as a special case.
	 */
	if ( option == LBER_OPT_DEBUG_LEVEL ) {
#ifdef LDAP_DEBUG
	 *(int *)value =  lber_debug;
#endif
	  return( 0 );
	}
	/*
	 * all the rest require a non-NULL ber
	 */
	if ( !NSLBERI_VALID_BERELEMENT_POINTER( ber )) {
		return( -1 );
	}

	switch ( option ) {
	case LBER_OPT_USE_DER:
	case LBER_OPT_TRANSLATE_STRINGS:
		*((int *) value) = (ber->ber_options & option);
		break;
	case LBER_OPT_REMAINING_BYTES:
		*((unsigned long *) value) = ber->ber_end - ber->ber_ptr;
		break;
	case LBER_OPT_TOTAL_BYTES:
		*((unsigned long *) value) = ber->ber_end - ber->ber_buf;
		break;
	case LBER_OPT_BYTES_TO_WRITE:
		*((unsigned long *) value) = ber->ber_ptr - ber->ber_buf;
		break;
	default:
		return( -1 );
	}

	return( 0 );
}

/*
 * return 0 on success and -1 on error
 */
int
LDAP_CALL
ber_sockbuf_set_option( Sockbuf *sb, int option, void *value )
{
	struct lber_x_ext_io_fns	*extiofns;

	if ( !NSLBERI_VALID_SOCKBUF_POINTER( sb )) {
		return( -1 );
	}

	switch ( option ) {
	case LBER_SOCKBUF_OPT_VALID_TAG:
		sb->sb_valid_tag= *((unsigned long *) value);
		if ( value != NULL ) {
			sb->sb_options |= option;
		} else {
			sb->sb_options &= ~option;
		}
		break;
	case LBER_SOCKBUF_OPT_MAX_INCOMING_SIZE:
		sb->sb_max_incoming = *((unsigned long *) value);
		/* FALL */
	case LBER_SOCKBUF_OPT_TO_FILE:
	case LBER_SOCKBUF_OPT_TO_FILE_ONLY:
	case LBER_SOCKBUF_OPT_NO_READ_AHEAD:
		if ( value != NULL ) {
			sb->sb_options |= option;
		} else {
			sb->sb_options &= ~option;
		}
		break;
	case LBER_SOCKBUF_OPT_DESC:
		sb->sb_sd = *((LBER_SOCKET *) value);
		break;
	case LBER_SOCKBUF_OPT_COPYDESC:
		sb->sb_copyfd = *((LBER_SOCKET *) value);
		break;
	case LBER_SOCKBUF_OPT_READ_FN:
		sb->sb_io_fns.lbiof_read = (LDAP_IOF_READ_CALLBACK *) value;
		nslberi_install_compat_io_fns( sb );
		break;
	case LBER_SOCKBUF_OPT_WRITE_FN:
		sb->sb_io_fns.lbiof_write = (LDAP_IOF_WRITE_CALLBACK *) value;
		nslberi_install_compat_io_fns( sb );
		break;
	case LBER_SOCKBUF_OPT_EXT_IO_FNS:
		extiofns = (struct lber_x_ext_io_fns *) value;
		if ( extiofns == NULL ) {	/* remove */
			(void)memset( (char *)&sb->sb_ext_io_fns, '\0',
				sizeof(sb->sb_ext_io_fns ));
		} else if ( extiofns->lbextiofn_size
		    == LBER_X_EXTIO_FNS_SIZE ) {
			/* struct copy */
			sb->sb_ext_io_fns = *extiofns;
		} else if ( extiofns->lbextiofn_size
			    == LBER_X_EXTIO_FNS_SIZE_REV0 ) {
			/* backwards compatiblity for older struct */
			sb->sb_ext_io_fns.lbextiofn_size =
				LBER_X_EXTIO_FNS_SIZE;
			sb->sb_ext_io_fns.lbextiofn_read =
				extiofns->lbextiofn_read;
			sb->sb_ext_io_fns.lbextiofn_write =
				    extiofns->lbextiofn_write;
			sb->sb_ext_io_fns.lbextiofn_writev = NULL;
			sb->sb_ext_io_fns.lbextiofn_socket_arg =
				    extiofns->lbextiofn_socket_arg;
		} else {
			return( -1 );
		}
		break;
	case LBER_SOCKBUF_OPT_SOCK_ARG:
		sb->sb_ext_io_fns.lbextiofn_socket_arg = 
		(struct lextiof_socket_private *) value;
		break;
		
	default:
		return( -1 );
	}

	return( 0 );
}

/*
 * return 0 on success and -1 on error
 */
int
LDAP_CALL
ber_sockbuf_get_option( Sockbuf *sb, int option, void *value )
{
	struct lber_x_ext_io_fns	*extiofns;

	if ( !NSLBERI_VALID_SOCKBUF_POINTER( sb )) {
		return( -1 );
	}

	switch ( option ) {
	case LBER_SOCKBUF_OPT_VALID_TAG:
		*((unsigned long *) value) = sb->sb_valid_tag;
		break;
	case LBER_SOCKBUF_OPT_MAX_INCOMING_SIZE:
		*((unsigned long *) value) = sb->sb_max_incoming;
		break;
	case LBER_SOCKBUF_OPT_TO_FILE:
	case LBER_SOCKBUF_OPT_TO_FILE_ONLY:
	case LBER_SOCKBUF_OPT_NO_READ_AHEAD:
		*((int *) value) = (sb->sb_options & option);
		break;
	case LBER_SOCKBUF_OPT_DESC:
		*((LBER_SOCKET *) value) = sb->sb_sd;
		break;
	case LBER_SOCKBUF_OPT_COPYDESC:
		*((LBER_SOCKET *) value) = sb->sb_copyfd;
		break;
	case LBER_SOCKBUF_OPT_READ_FN:
		*((LDAP_IOF_READ_CALLBACK **) value)
		    = sb->sb_io_fns.lbiof_read;
		break;
	case LBER_SOCKBUF_OPT_WRITE_FN:
		*((LDAP_IOF_WRITE_CALLBACK **) value)
		    = sb->sb_io_fns.lbiof_write;
		break;
	case LBER_SOCKBUF_OPT_EXT_IO_FNS:
		extiofns = (struct lber_x_ext_io_fns *) value;
		if ( extiofns == NULL ) {
			return( -1 );
		} else if ( extiofns->lbextiofn_size
			    == LBER_X_EXTIO_FNS_SIZE ) {
			/* struct copy */
			*extiofns = sb->sb_ext_io_fns;
		} else if ( extiofns->lbextiofn_size
			    == LBER_X_EXTIO_FNS_SIZE_REV0 ) {
			/* backwards compatiblity for older struct */
			extiofns->lbextiofn_read = sb->sb_ext_io_fns.lbextiofn_read;
			extiofns->lbextiofn_write = sb->sb_ext_io_fns.lbextiofn_write;
			extiofns->lbextiofn_socket_arg = sb->sb_ext_io_fns.lbextiofn_socket_arg;
		} else {
			return( -1 );
		}
		break;
	case LBER_SOCKBUF_OPT_SOCK_ARG:
		*((struct lextiof_socket_private **)value) = sb->sb_ext_io_fns.lbextiofn_socket_arg;
		break;
		
	default:
		return( -1 );
	}

	return( 0 );
}


/* new dboreham code below: */

struct byte_buffer  {
	unsigned char *p;
	int offset;
	int length;
};
typedef struct byte_buffer byte_buffer;


/* This call allocates us a BerElement structure plus some extra memory. 
 * It returns a pointer to the BerElement, plus a pointer to the extra memory.
 * This routine also allocates a ber data buffer within the same block, thus
 * saving a call to calloc later when we read data.
 */
void*
LDAP_CALL
ber_special_alloc(size_t size, BerElement **ppBer)
{
	char *mem = NULL;

	/* Make sure mem size requested is aligned */
	if (0 != ( size & 0x03 )) {
		size += (sizeof(long) - (size & 0x03));
	}

	mem = NSLBERI_MALLOC(sizeof(struct berelement) + EXBUFSIZ + size );
	if (NULL == mem) {
		return NULL;
	} 
	*ppBer = (BerElement*) (mem + size);
	memset(*ppBer,0,sizeof(struct berelement));
	(*ppBer)->ber_tag = LBER_DEFAULT;
	(*ppBer)->ber_buf = mem + size + sizeof(struct berelement);
	(*ppBer)->ber_ptr = (*ppBer)->ber_buf;
	(*ppBer)->ber_end = (*ppBer)->ber_buf + EXBUFSIZ;
	(*ppBer)->ber_flags = LBER_FLAG_NO_FREE_BUFFER;
	return (void*)mem;
}

void
LDAP_CALL
ber_special_free(void* buf, BerElement *ber)
{
	if (!(ber->ber_flags & LBER_FLAG_NO_FREE_BUFFER)) {
		NSLBERI_FREE(ber->ber_buf);
	}
	NSLBERI_FREE( buf );
}

/* Copy up to bytes_to_read bytes from b into return_buffer.
 * Returns a count of bytes copied (always >= 0).
 */
static int
read_bytes(byte_buffer *b, unsigned char *return_buffer, int bytes_to_read)
{
	/* copy up to bytes_to_read bytes into the caller's buffer, return the number of bytes copied */
	int bytes_to_copy = 0;

	if (bytes_to_read <= (b->length - b->offset) ) {
		bytes_to_copy = bytes_to_read;
	} else {
		bytes_to_copy = (b->length - b->offset);
	}
	if (1 == bytes_to_copy) {
		*return_buffer = *(b->p+b->offset++);
	} else if (bytes_to_copy <= 0) {
		bytes_to_copy = 0;	/* never return a negative result */
	} else {
		memcpy(return_buffer,b->p+b->offset,bytes_to_copy);
		b->offset += bytes_to_copy;
	}
	return bytes_to_copy;
}

/* return the tag - LBER_DEFAULT returned means trouble */
static unsigned long
get_buffer_tag(byte_buffer *sb )
{
	unsigned char	xbyte;
	unsigned long	tag;
	char		*tagp;
	int		i;

	if ( (i = read_bytes( sb, &xbyte, 1 )) != 1 ) {
		return( LBER_DEFAULT );
	}

	if ( (xbyte & LBER_BIG_TAG_MASK) != LBER_BIG_TAG_MASK ) {
		return( (unsigned long) xbyte );
	}

	tagp = (char *) &tag;
	tagp[0] = xbyte;
	for ( i = 1; i < sizeof(long); i++ ) {
		if ( read_bytes( sb, &xbyte, 1 ) != 1 )
			return( LBER_DEFAULT );

		tagp[i] = xbyte;

		if ( ! (xbyte & LBER_MORE_TAG_MASK) )
			break;
	}

	/* tag too big! */
	if ( i == sizeof(long) )
		return( LBER_DEFAULT );

	/* want leading, not trailing 0's */
	return( tag >> (sizeof(long) - i - 1) );
}

/* Like ber_get_next, but from a byte buffer the caller already has. */
/* Bytes_Scanned returns the number of bytes we actually looked at in the buffer. */
/* ber_get_next_buffer is now implemented in terms of ber_get_next_buffer_ext */
/* and is here for backward compatibility.  This new function allows us to pass */
/* the Sockbuf structure along */

unsigned long
LDAP_CALL
ber_get_next_buffer( void *buffer, size_t buffer_size, unsigned long *len,
    BerElement *ber, unsigned long *Bytes_Scanned )
{
	return (ber_get_next_buffer_ext( buffer, buffer_size, len, ber,
		Bytes_Scanned, NULL));
}

/*
 * Returns the tag of the message or LBER_DEFAULT if an error occurs. There
 * are two cases where LBER_DEFAULT is returned:
 *
 * 1) There was not enough data in the buffer to complete the message; this
 *    is a "soft" error. In this case, *Bytes_Scanned is set to a positive
 *    number.
 *
 * 2) A "fatal" error occurs. In this case, *Bytes_Scanned is set to zero.
 *    To check for specific errors, the system error number (errno) must
 *    be consulted.  These errno values are explicitly set by this
 *    function; other errno values may be set by underlying OS functions:
 *
 *    EINVAL   - LBER_SOCKBUF_OPT_VALID_TAG option set but tag does not match.
 *    EMSGSIZE - length was not represented as <= sizeof(long) bytes or the
 *                  LBER_SOCKBUF_OPT_MAX_INCOMING_SIZE option was set and the
 *                  message is longer than the maximum. *len will be set in
 *                  the latter situation.
 */
unsigned long
LDAP_CALL
ber_get_next_buffer_ext( void *buffer, size_t buffer_size, unsigned long *len,
    BerElement *ber, unsigned long *Bytes_Scanned, Sockbuf *sock )
{
	unsigned long	tag = 0, netlen, toread;
	unsigned char	lc;
	long		rc;
	int		noctets, diff;
	byte_buffer sb = {0};
	int rcnt = 0;


	/*
	 * Any ber element looks like this: tag length contents.
	 * Assuming everything's ok, we return the tag byte (we
	 * can assume a single byte), return the length in len,
	 * and the rest of the undecoded element in buf.
	 *
	 * Assumptions:
	 *	1) small tags (less than 128)
	 *	2) definite lengths
	 *	3) primitive encodings used whenever possible
	 */

	/*
	 * first time through - malloc the buffer, set up ptrs, and
	 * read the tag and the length and as much of the rest as we can
	 */

	sb.p = buffer;
	sb.length = buffer_size;
	*len = netlen = 0;

	if ( ber->ber_rwptr == NULL || ber->ber_tag == LBER_DEFAULT ) {
		/*
		 * First, we read the tag.
		 */

		/* if we have been called before with a fragment not
		 * containing a complete length, we have no rwptr but
	 	 * a tag already
		*/
		if ( ber->ber_tag == LBER_DEFAULT ) {
			if ( (tag = get_buffer_tag( &sb )) == LBER_DEFAULT ) {
				goto premature_exit;
			}
			ber->ber_tag = tag;
		}

		if((sock->sb_options & LBER_SOCKBUF_OPT_VALID_TAG) &&
		  (tag != sock->sb_valid_tag)) {
#if !defined( macintosh ) && !defined( DOS )
			errno = EINVAL;
#endif
			goto error_exit;
		}

		/*
		 * Next, read the length.  The first byte contains the length
		 * of the length.  If bit 8 is set, the length is the long
		 * form, otherwise it's the short form.  We don't allow a
		 * length that's greater than what we can hold in an unsigned
		 * long.
		 */

		if ( read_bytes( &sb, &lc, 1 ) != 1 ) {
			goto premature_exit;
		}
		if ( lc & 0x80 ) {
			noctets = (lc & 0x7f);
			if ( noctets > sizeof(unsigned long) ) {
#if !defined( macintosh ) && !defined( DOS )
				errno = EMSGSIZE;
#endif
				goto error_exit;
			}
			diff = sizeof(unsigned long) - noctets;
			rcnt = read_bytes( &sb, (unsigned char *)&netlen + diff, noctets );
			if ( rcnt != noctets ) {
				/* keep the read bytes in buffer for the next round */
				if ( ber->ber_end - ber->ber_buf < rcnt + 1 ) {
					if ( nslberi_ber_realloc( ber, rcnt + 1 ) != 0 )
						/* errno set by realloc() */
						goto error_exit;
					ber->ber_end = ber->ber_buf + rcnt + 1;
				}
				ber->ber_ptr = ber->ber_buf;
				ber->ber_buf[0] = lc;
				if ( rcnt > 0 ) {
					SAFEMEMCPY( &ber->ber_buf[1],
								(const char *)&netlen + diff, rcnt );
				}

				/* The way how ber_rwptr is used here is exceptional.
				 * Here, ber_rwptr is set after the buffer contents,
				 * not to the next byte to be read.
				 * This is b/c we need the number of bytes that were read (rcnt)
				 * in the next process [ in "else if (0 == ber->ber_len)" ]
				 */
				ber->ber_rwptr = ber->ber_buf + rcnt + 1;
				goto premature_exit;
			}
			*len = LBER_NTOHL( netlen );
		} else {
			*len = lc;
		}

		if ( 0 != get_and_check_length(ber, *len, sock)) {
			/* errno set by get_and_check_length() */
			goto error_exit;
		}
	} else if (0 == ber->ber_len) {
		/* Still waiting to receive the length */
		lc = ber->ber_buf[0];
		if ( lc & 0x80 ) {
			int prevcnt = ber->ber_rwptr - ber->ber_buf - 1/* lc */;

			if ( prevcnt < 0 ) { /* This is unexpected */
				ber_reset( ber, 0 );
				ber->ber_tag = LBER_DEFAULT;
#if !defined( macintosh ) && !defined( DOS )
				errno = EMSGSIZE;	/* kind of a guess */
#endif
				goto error_exit;
			}

			noctets = (lc & 0x7f);
			diff = sizeof(unsigned long) - noctets;
			SAFEMEMCPY( (char *)&netlen + diff, &ber->ber_buf[1], prevcnt );
			rcnt = read_bytes( &sb, (unsigned char *)&netlen + diff + prevcnt,
				noctets - prevcnt );
			if ( rcnt != noctets - prevcnt) {
				/* keep the read bytes in buffer for the next round */
				if ( ber->ber_end - ber->ber_rwptr < rcnt ) {
					if ( nslberi_ber_realloc( ber, prevcnt + rcnt + 1 ) != 0 )
						/* errno set by realloc() */
						goto error_exit;
					ber->ber_end = ber->ber_rwptr + rcnt;
				}
				SAFEMEMCPY( ber->ber_rwptr,
					(const char *)&netlen + diff + prevcnt, rcnt );
				/* ditto */
				ber->ber_rwptr += rcnt;
				goto premature_exit;
			}
			*len = LBER_NTOHL( netlen );
		} else {
			*len = lc;	/* should not come here */
		}

		if ( 0 != get_and_check_length(ber, *len, sock)) {
			/* errno set by get_and_check_length() */
			goto error_exit;
		}
	}

	toread = (unsigned long)ber->ber_end - (unsigned long)ber->ber_rwptr;
	while ( toread > 0 ) {
		if ( (rc = read_bytes( &sb, (unsigned char *)ber->ber_rwptr,
		    (long)toread )) <= 0 ) {
			goto premature_exit;
		}

		toread -= rc;
		ber->ber_rwptr += rc;
	}

	*len = ber->ber_len;
	*Bytes_Scanned = sb.offset;
	return( ber->ber_tag );

error_exit:
	*Bytes_Scanned = 0;
	return(LBER_DEFAULT);

premature_exit:
	/*
	 * we're here because we hit the end of the buffer before seeing
	 * all of the PDU (but no error occurred; we just need more data).
	 */
	*Bytes_Scanned = sb.offset;
	return(LBER_DEFAULT);
}


/*
 * Returns 0 if successful and -1 if not.
 * Sets errno if not successful; see the comment above ber_get_next_buffer_ext()
 * Always sets ber->ber_len = length.
 */
static int
get_and_check_length( BerElement *ber, unsigned long length, Sockbuf *sock )
{
	/*
	 * malloc a buffer for the contents and read it in.  This buffer
	 * is passed to all the other ber decoding routines.
	 */
	ber->ber_len = length;

#if defined( DOS ) && !defined( _WIN32 )
	if ( length > 65535 ) {	/* DOS can't allocate > 64K */
		/* no errno on Win16 */
		return( -1 );
	}
#endif /* DOS && !_WIN32 */

	if ( (sock != NULL)  &&
		( sock->sb_options & LBER_SOCKBUF_OPT_MAX_INCOMING_SIZE )
		&& (length > sock->sb_max_incoming) ) {
#if !defined( macintosh ) && !defined( DOS )
		errno = EMSGSIZE;
#endif
		return( -1 );
	}

	if ( ber->ber_buf + length > ber->ber_end ) {
		if ( nslberi_ber_realloc( ber, length ) != 0 ) {
			/* errno is set by realloc() */
			return( -1 );
		}
	}
	ber->ber_ptr = ber->ber_buf;
	ber->ber_end = ber->ber_buf + length;
	ber->ber_rwptr = ber->ber_buf;

	return 0;
}


/* The ber_flatten routine allocates a struct berval whose contents
 * are a BER encoding taken from the ber argument. The bvPtr pointer
 * points to the returned berval, which must be freed using
 * ber_bvfree().  This routine returns 0 on success and -1 on error.
 * The use of ber_flatten on a BerElement in which all '{' and '}'
 * format modifiers have not been properly matched can result in a
 * berval whose contents are not a valid BER encoding. 
 * Note that the ber_ptr is not modified.
 */
int
LDAP_CALL
ber_flatten( BerElement *ber, struct berval **bvPtr )
{
	struct berval *new;
	unsigned long len;

	/* allocate a struct berval */
	if ( (new = (struct berval *)NSLBERI_MALLOC( sizeof(struct berval) ))
	    == NULL ) {
		return( -1 );
	}

	/*
	 * Copy everything from the BerElement's ber_buf to ber_ptr
	 * into the berval structure.
	 */
	if ( ber == NULL ) {
	    new->bv_val = NULL;
	    new->bv_len = 0;
	} else {
 	    len = ber->ber_ptr - ber->ber_buf; 
	    if ( ( new->bv_val = (char *)NSLBERI_MALLOC( len + 1 )) == NULL ) {
		    ber_bvfree( new );
		    return( -1 );
	    }
 	    SAFEMEMCPY( new->bv_val, ber->ber_buf, (size_t)len ); 
	    new->bv_val[len] = '\0';
	    new->bv_len = len;
	}

	/* set bvPtr pointer to point to the returned berval */
  	*bvPtr = new; 

        return( 0 );
}


/*
 * The ber_init function constructs and returns a new BerElement
 * containing a copy of the data in the bv argument.  ber_init
 * returns the null pointer on error.
 */
BerElement *
LDAP_CALL
ber_init( const struct berval *bv )
{
	BerElement *ber;

	/* construct BerElement */
	if (( ber = ber_alloc_t( 0 )) != NULLBER ) {
		/* copy data from the bv argument into BerElement */
		/* XXXmcs: had to cast unsigned long bv_len to long */
		if ( (ber_write ( ber, bv->bv_val, bv->bv_len, 0 ))
		    != (long)bv->bv_len ) {
			ber_free( ber, 1 );
			return( NULL );
		}
	}
	
	/*
	 * reset ber_ptr back to the beginning of buffer so that this new
	 * and initialized ber element can be READ
	 */
	ber_reset( ber, 1);

	/*
	 * return a ptr to a new BerElement containing a copy of the data
	 * in the bv argument or a null pointer on error
	 */
	return( ber );
}


/*
 * memory allocation functions.
 */
void *
nslberi_malloc( size_t size )
{
	return( nslberi_memalloc_fns.lbermem_malloc == NULL ?
	    malloc( size ) :
	    nslberi_memalloc_fns.lbermem_malloc( size ));
}


void *
nslberi_calloc( size_t nelem, size_t elsize )
{
	return( nslberi_memalloc_fns.lbermem_calloc == NULL ?
	    calloc(  nelem, elsize ) :
	    nslberi_memalloc_fns.lbermem_calloc( nelem, elsize ));
}


void *
nslberi_realloc( void *ptr, size_t size )
{
	return( nslberi_memalloc_fns.lbermem_realloc == NULL ?
	    realloc( ptr, size ) :
	    nslberi_memalloc_fns.lbermem_realloc( ptr, size ));
}


void
nslberi_free( void *ptr )
{
	if ( nslberi_memalloc_fns.lbermem_free == NULL ) {
		free( ptr );
	} else {
		nslberi_memalloc_fns.lbermem_free( ptr );
	}
}


/*
 ******************************************************************************
 * functions to bridge the gap between new extended I/O functions that are
 *    installed using ber_sockbuf_set_option( ..., LBER_SOCKBUF_OPT_EXT_IO_FNS,
 *    ... ).
 *
 * the basic strategy is to use the new extended arg to hold a pointer to the
 *    Sockbuf itself so we can find the old functions and call them.
 * note that the integer socket s passed in is not used.  we use the sb_sd
 *    from the Sockbuf itself because it is the correct type.
 */
static int
nslberi_extread_compat( int s, void *buf, int len,
	struct lextiof_socket_private *arg )
{
	Sockbuf	*sb = (Sockbuf *)arg;

	return( sb->sb_io_fns.lbiof_read( sb->sb_sd, buf, len ));
}


static int
nslberi_extwrite_compat( int s, const void *buf, int len,
	struct lextiof_socket_private *arg )
{
	Sockbuf	*sb = (Sockbuf *)arg;

	return( sb->sb_io_fns.lbiof_write( sb->sb_sd, buf, len ));
}


/*
 * Install I/O compatiblity functions.  This can't fail.
 */
static void
nslberi_install_compat_io_fns( Sockbuf *sb )
{
	sb->sb_ext_io_fns.lbextiofn_size = LBER_X_EXTIO_FNS_SIZE;
	sb->sb_ext_io_fns.lbextiofn_read = nslberi_extread_compat;
	sb->sb_ext_io_fns.lbextiofn_write = nslberi_extwrite_compat;
	sb->sb_ext_io_fns.lbextiofn_writev = NULL;
	sb->sb_ext_io_fns.lbextiofn_socket_arg = (void *)sb;
}
/*
 * end of compat I/O functions
 ******************************************************************************
 */
