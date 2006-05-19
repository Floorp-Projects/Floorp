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

#define EXBUFSIZ			1024
size_t  lber_bufsize = EXBUFSIZ;

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
static ber_tag_t get_tag( Sockbuf *sb, BerElement *ber);
static ber_len_t get_ber_len( BerElement *ber);
static ber_len_t read_len_in_ber( Sockbuf *sb, BerElement *ber);

/*
 * internal global structure for memory allocation callback functions
 */
static struct lber_memalloc_fns nslberi_memalloc_fns;


/*
 * buffered read from "sb".
 * returns value of first character read on success and -1 on error.
 */
static int
ber_filbuf( Sockbuf *sb, ber_slen_t len )
{
	ssize_t	rc;
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
			rc = read( sb->sb_sd, sb->sb_ber.ber_buf,
			    ((sb->sb_options & LBER_SOCKBUF_OPT_NO_READ_AHEAD)
			    && (len < READBUFSIZ)) ? len : READBUFSIZ );
		}
	}

	if ( rc > 0 ) {
		sb->sb_ber.ber_ptr = sb->sb_ber.ber_buf + 1;
		sb->sb_ber.ber_end = sb->sb_ber.ber_buf + rc;
		return( (unsigned char)*sb->sb_ber.ber_buf );
	}

	return( -1 );
}


static ber_int_t
BerRead( Sockbuf *sb, char *buf, ber_slen_t len )
{
  int c;
  ber_int_t nread = 0;
  
  while (len > 0)
    {
      ber_int_t inberbuf = sb->sb_ber.ber_end - sb->sb_ber.ber_ptr;
      if (inberbuf > 0)
	{
	  size_t tocopy = len > inberbuf ? inberbuf : len;
	  SAFEMEMCPY(buf, sb->sb_ber.ber_ptr, tocopy);
	  buf += tocopy;
	  sb->sb_ber.ber_ptr += tocopy;
	  nread += tocopy;
	  len -= tocopy;
	}
      else
	{
	  c = ber_filbuf(sb, len);
	  if (c < 0)
	    {
	      if (nread > 0)
		break;
               else
                 return c;
             }
           *buf++ = c;
           nread++;
           len--;
         }
     }
   return (nread);
}


/*
 * Note: ber_read() only uses the ber_end and ber_ptr elements of ber.
 * Functions like ber_get_tag(), ber_skip_tag, and ber_peek_tag() rely on
 * that fact, so if this code is changed to use any additional elements of
 * the ber structure, those functions will need to be changed as well.
 */
ber_int_t
LDAP_CALL
ber_read( BerElement *ber, char *buf, ber_len_t len )
{
	ber_len_t	actuallen;
	ber_uint_t  nleft;

	nleft = ber->ber_end - ber->ber_ptr;
	actuallen = nleft < len ? nleft : len;

	SAFEMEMCPY( buf, ber->ber_ptr, (size_t)actuallen );

	ber->ber_ptr += actuallen;

	return( (ber_int_t)actuallen );
}

/*
 * enlarge the ber buffer.
 * return 0 on success, -1 on error.
 */
int
nslberi_ber_realloc( BerElement *ber, ber_len_t len )
{
	ber_uint_t	need, have, total;
	size_t		have_bytes;
	Seqorset	*s;
	ber_int_t	off;
	char		*oldbuf;
	int			freeoldbuf = 0;

	ber->ber_buf_reallocs++;

	have_bytes = ber->ber_end - ber->ber_buf;
	have = have_bytes / lber_bufsize;
	need = (len < lber_bufsize ? 1 : (len + (lber_bufsize - 1)) / lber_bufsize);
	total = have * lber_bufsize + need * lber_bufsize * ber->ber_buf_reallocs;
	
	oldbuf = ber->ber_buf;

	if (ber->ber_buf == NULL) {
		if ( (ber->ber_buf = (char *)NSLBERI_MALLOC( (size_t)total ))
			 == NULL ) {
			return( -1 );
		}
		ber->ber_flags &= ~LBER_FLAG_NO_FREE_BUFFER;
	} else {
		if ( !(ber->ber_flags & LBER_FLAG_NO_FREE_BUFFER) ) {
			freeoldbuf = 1;
		}
		/* transition to malloc'd buffer */
		if ( (ber->ber_buf = (char *)NSLBERI_MALLOC(
			(size_t)total )) == NULL ) {
			return( -1 );
		}
		ber->ber_flags &= ~LBER_FLAG_NO_FREE_BUFFER;
		/* copy existing data into new malloc'd buffer */
		SAFEMEMCPY( ber->ber_buf, oldbuf, have_bytes );
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
		
		if ( freeoldbuf && oldbuf ) {
			NSLBERI_FREE( oldbuf );
		}
	}

	return( 0 );
}

/*
 * returns "len" on success and -1 on failure.
 */
ber_int_t
LDAP_CALL
ber_write( BerElement *ber, char *buf, ber_len_t len, int nosos )
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
	ssize_t	nwritten = 0, towrite, rc;
	int     i = 0;
	
	if (ber->ber_rwptr == NULL) {
	  ber->ber_rwptr = ber->ber_buf;
	} else if (ber->ber_rwptr >= ber->ber_end) {
	  /* we will use the ber_rwptr to continue an exited flush,
	     so if rwptr is not within the buffer we return an error. */
	  return( -1 );
	}
	
	/* writev section - for iDAR only!!! */
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
	  
	  if ( freeit )
		ber_free( ber, 1 );

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
				if ( (rc = BerWrite( sb, ber->ber_rwptr,
				    (size_t) towrite )) <= 0 ) {
					return( -1 );
				}
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
	    sizeof(struct berelement) + lber_bufsize )) == NULL ) {
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
	ber->ber_end = ber->ber_buf + lber_bufsize;
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
	ber->ber_tag_len_read = 0;

	memset(ber->ber_struct, 0, BER_CONTENTS_STRUCT_SIZE);
}

/* Returns the length of the ber buffer so far,
   taking into account sequences/sets also.
   CAUTION: Returns 0 on null buffers as well 
   as 0 on empty buffers!
*/
size_t
LDAP_CALL
ber_get_buf_datalen( BerElement *ber )
{
  size_t datalen;

  if ( ( ber == NULL) || ( ber->ber_buf == NULL) || ( ber->ber_ptr == NULL ) ) {
    datalen = 0;
  } else if (ber->ber_sos == NULLSEQORSET) {
    /* there are no sequences or sets yet, 
       so just subtract ptr from the beginning of the ber buffer */
    datalen = ber->ber_ptr - ber->ber_buf;
  } else {
    /* sequences exist, so just take the ptr of the sequence
       on top of the stack and subtract the beginning of the
       buffer from it */
    datalen = ber->ber_sos->sos_ptr - ber->ber_buf;
  }
  
  return datalen;
}

/*
  if buf is 0 then malloc a buffer of length size
  returns > 0 on success, 0 otherwise
*/
int
LDAP_CALL
ber_stack_init(BerElement *ber, int options, char * buf,
        size_t size)
{
  if (NULL == ber)
    return 0;
  
  memset(ber, 0, sizeof(*ber));

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
  
  if ( ber->ber_buf && !(ber->ber_flags & LBER_FLAG_NO_FREE_BUFFER)) {
	  NSLBERI_FREE(ber->ber_buf);
  }
  
  if (buf) {
    ber->ber_buf = ber->ber_ptr = buf;
    ber->ber_flags = LBER_FLAG_NO_FREE_BUFFER;
  } else {
    ber->ber_buf = ber->ber_ptr = (char *) NSLBERI_MALLOC(size);
  }

  ber->ber_end = ber->ber_buf + size;

  return ber->ber_buf != 0;
}

/*
 * This call allows to release only the data part of
 * the target Sockbuf.
 * Other info of this Sockbuf are kept unchanged.
 */
void
LDAP_CALL
ber_sockbuf_free_data(Sockbuf *p)
{
    if ( p != NULL ) {
	if ( p->sb_ber.ber_buf != NULL &&
	     !(p->sb_ber.ber_flags & LBER_FLAG_NO_FREE_BUFFER) ) {
	    NSLBERI_FREE( p->sb_ber.ber_buf );
	    p->sb_ber.ber_buf = NULL;
	}
    }
}

/* simply returns ber_buf in the ber ...
   explicitly for DS MMR only...
*/
char *
LDAP_CALL
ber_get_buf_databegin (BerElement * ber)
{
  if (NULL != ber) {
    return ber->ber_buf;
  } else {
    return NULL;
  }
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
static ber_tag_t
get_tag( Sockbuf *sb, BerElement *ber)
{
        unsigned char   xbyte;

        if ( (BerRead( sb, (char *) &xbyte, 1 )) != 1 ) {
                return( LBER_DEFAULT );
        }

        if ( (xbyte & LBER_BIG_TAG_MASK) == LBER_BIG_TAG_MASK ) {
                return( LBER_DEFAULT );
        }

	ber->ber_tag_contents[0] = xbyte;
	ber->ber_struct[BER_STRUCT_TAG].ldapiov_len = 1;
	return((ber_tag_t)xbyte);
}


/* Error checking? */
/* Takes a ber and returns the actual length */
static ber_len_t
get_ber_len( BerElement *ber)
{
  int noctets;
  ber_len_t len = 0;
  char xbyte;

  xbyte = ber->ber_len_contents[0];
  
  /* long form */
  if (xbyte & 0x80) {
    noctets = (int) (xbyte & 0x7f);
    if (noctets >= MAX_LEN_SIZE) {
      return(LBER_DEFAULT);
    }
    memcpy((char*) &len + sizeof(ber_len_t) - noctets, &ber->ber_len_contents[1], noctets);
    len = LBER_NTOHL(len);
    return(len);
  } else {
    return((ber_len_t)(xbyte));
  }
}

/* LBER_DEFAULT means trouble
   reads in the length, stores it in ber->ber_struct, and returns get_ber_len */
static ber_len_t
read_len_in_ber( Sockbuf *sb, BerElement *ber)
{
  unsigned char xbyte;
  int           noctets;
  int           rc = 0, read_result = 0;

  /*
   * Next, read the length.  The first byte contains the length
   * of the length.  If bit 8 is set, the length is the long
   * form, otherwise it's the short form.  We don't allow a
   * length that's greater than what we can hold in a ber_int_t
   */
  if ( ber->ber_tag_len_read == 1) {
      /* the length of the length hasn't been read yet */
      if ( BerRead( sb, (char *) &xbyte, 1 ) != 1 ) {
	  return( LBER_DEFAULT );
      }
      ber->ber_tag_len_read = 2;
      ber->ber_len_contents[0] = xbyte;
  } else {
      rc = ber->ber_tag_len_read - 2;
      xbyte = ber->ber_len_contents[0];
  }
  
  /* long form of the length value */
  if ( xbyte & 0x80 ) {
    noctets = (xbyte & 0x7f);
    if ( noctets >= MAX_LEN_SIZE )
	return( LBER_DEFAULT );
    while (rc < noctets) {
	read_result = BerRead( sb, &(ber->ber_len_contents[1]) + rc, noctets - rc );
	if (read_result <= 0) {
	    ber->ber_tag_len_read = rc + 2; /* so we can continue later - include tag and lenlen */
	    return( LBER_DEFAULT );
	}
	rc += read_result;
    }
    ber->ber_tag_len_read = rc + 2; /* adds tag (1 byte) and lenlen (1 byte) */
    ber->ber_struct[BER_STRUCT_LEN].ldapiov_len = 1 + noctets;
  } else { /* short form of the length value */ 
    ber->ber_struct[BER_STRUCT_LEN].ldapiov_len = 1;
  }
  return(get_ber_len(ber));
}


ber_tag_t
LDAP_CALL
ber_get_next( Sockbuf *sb, ber_len_t *len, BerElement *ber )
{
	ber_len_t	newlen;
	ber_uint_t  toread;
	ber_int_t	rc;
	ber_len_t   orig_taglen_read = 0;
	char * orig_rwptr = ber->ber_rwptr ? ber->ber_rwptr : ber->ber_buf;

#ifdef LDAP_DEBUG
	if ( lber_debug )
		ber_err_print( "ber_get_next\n" );
#endif

	/*
	 * When rwptr is NULL this signifies that the tag and length have not been
	 * read in their entirety yet. (if at all)
	 */
	if ( ber->ber_rwptr == NULL ) {

            /* first save the amount we previously read, so we know what to return in len. */
	    orig_taglen_read = ber->ber_tag_len_read;

	    /* read the tag - if tag_len_read is greater than 0, then it has already been read. */
	    if (ber->ber_tag_len_read == 0) {
		if ((ber->ber_tag = get_tag(sb, ber)) == LBER_DEFAULT ) {
		    *len = 0;
		    return( LBER_DEFAULT );
		}

		ber->ber_tag_contents[0] = (char)ber->ber_tag; /* we only handle 1 byte tags */
		ber->ber_tag_len_read = 1;  

                /* check for validity */
		if((sb->sb_options & LBER_SOCKBUF_OPT_VALID_TAG) &&
		   (ber->ber_tag != sb->sb_valid_tag)) {
		    *len = 1; /* we just read the tag so far */
		    return( LBER_DEFAULT);
		}              
	    }	  
	  
	    /* read the length */
	    if ((newlen = read_len_in_ber(sb, ber)) == LBER_DEFAULT ) {
		*len = ber->ber_tag_len_read - orig_taglen_read;
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
	    if ( ((ber_len_t) ber->ber_end - (ber_len_t) ber->ber_buf) < newlen) {
			if ( ber->ber_buf && !(ber->ber_flags & LBER_FLAG_NO_FREE_BUFFER)) {
				NSLBERI_FREE(ber->ber_buf);
			}
			if ( (ber->ber_buf = (char *)NSLBERI_CALLOC( 1,(size_t)newlen ))
				 == NULL ) {
				return( LBER_DEFAULT );
			}
			ber->ber_flags &= ~LBER_FLAG_NO_FREE_BUFFER;
			orig_rwptr = ber->ber_buf;
	    }
	    
	    
	    ber->ber_len = newlen;
	    ber->ber_ptr = ber->ber_buf;
	    ber->ber_end = ber->ber_buf + newlen;
	    ber->ber_rwptr = ber->ber_buf;
	    ber->ber_tag_len_read = 0; /* now that rwptr is set, this doesn't matter */
	}
	
	/* OK, we've malloc-ed the buffer; now read the rest of the expected length */
	toread = (ber_uint_t)ber->ber_end - (ber_uint_t)ber->ber_rwptr;
	do {
	    if ( (rc = BerRead( sb, ber->ber_rwptr, (ber_int_t)toread )) <= 0 ) {
		*len = (ber_len_t) ber->ber_rwptr - (ber_len_t) orig_rwptr;
		return( LBER_DEFAULT );
	    }
	    
	    toread -= rc;
	    ber->ber_rwptr += rc;
	} while ( toread > 0 );
	
#ifdef LDAP_DEBUG
	if ( lber_debug ) {
	    char msg[80];
	    sprintf( msg, "ber_get_next: tag 0x%lx len %ld contents:\n",
		     ber->ber_tag, ber->ber_len );
	    ber_err_print( msg );
	    if ( lber_debug > 1 )
		ber_dump( ber, 1 );
	}
#endif
	
	*len = (ber_len_t) ber->ber_rwptr - (ber_len_t) orig_rwptr;
	ber->ber_rwptr = NULL;
	ber->ber_struct[BER_STRUCT_VAL].ldapiov_len = ber->ber_len;
	return( ber->ber_tag );
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
  * lber_bufsize is global, so it is OK to pass
  * NULL for ber. Handle this as a special case.
  */	
  if ( option == LBER_OPT_BUFSIZE ) {
	if ( *(size_t *)value > EXBUFSIZ ) {
		lber_bufsize = *(size_t *)value;
	}
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
		ber->ber_end = ber->ber_ptr + *((ber_uint_t *)value);
		break;
	case LBER_OPT_TOTAL_BYTES:
		ber->ber_end = ber->ber_buf + *((ber_uint_t *)value);
		break;
	case LBER_OPT_BYTES_TO_WRITE:
		ber->ber_ptr = ber->ber_buf + *((ber_uint_t *)value);
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
	 * lber_bufsize is global, so it is OK to pass
	 * NULL for ber. Handle this as a special case.
	 */
	if ( option == LBER_OPT_BUFSIZE ) {
	  *(size_t *)value = lber_bufsize;
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
		*((ber_uint_t *) value) = ber->ber_end - ber->ber_ptr;
		break;
	case LBER_OPT_TOTAL_BYTES:
		*((ber_uint_t *) value) = ber->ber_end - ber->ber_buf;
		break;
	case LBER_OPT_BYTES_TO_WRITE:
		*((ber_uint_t *) value) = ber->ber_ptr - ber->ber_buf;
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

        /* check for a NULL value for certain options. */
	if (NULL == value) {
	    switch ( option ) {
	    case LBER_SOCKBUF_OPT_TO_FILE:
	    case LBER_SOCKBUF_OPT_TO_FILE_ONLY:
	    case LBER_SOCKBUF_OPT_NO_READ_AHEAD:
	    case LBER_SOCKBUF_OPT_READ_FN:
	    case LBER_SOCKBUF_OPT_WRITE_FN:
	    case LBER_SOCKBUF_OPT_EXT_IO_FNS:
	    case LBER_SOCKBUF_OPT_MAX_INCOMING_SIZE:
		/* do nothing - it's OK to have a NULL value for these options */
		break;
	    default:
		return( -1 );
	    }
	}

	switch ( option ) {
	case LBER_SOCKBUF_OPT_VALID_TAG:
		sb->sb_valid_tag= *((ber_tag_t *) value);
		/* I'm not sure why this is possible - doesn't value have to be non-NULL here? */
		if ( value != NULL ) {
			sb->sb_options |= option;
		} else {
			sb->sb_options &= ~option;
		}
		break;
	case LBER_SOCKBUF_OPT_MAX_INCOMING_SIZE:
		if ( value != NULL ) {
		    sb->sb_max_incoming = *((ber_uint_t *) value);
		    sb->sb_options |= option;
		} else {
		    /* setting the max incoming to 0 seems to be the only
		       way to tell the callers of ber_sockbuf_get_option
		       that this option isn't set. */
		    sb->sb_max_incoming = 0;
		    sb->sb_options &= ~option;
		}
		break;
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

	if ( !NSLBERI_VALID_SOCKBUF_POINTER( sb ) || (NULL == value)) {
		return( -1 );
	}

	switch ( option ) {
	case LBER_SOCKBUF_OPT_VALID_TAG:
		*((ber_tag_t *) value) = sb->sb_valid_tag;
		break;
	case LBER_SOCKBUF_OPT_MAX_INCOMING_SIZE:
		*((ber_uint_t *) value) = sb->sb_max_incoming;
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
		if ( extiofns->lbextiofn_size
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
		size += (sizeof(ber_int_t) - (size & 0x03));
	}

	mem = NSLBERI_MALLOC(sizeof(struct berelement) + lber_bufsize + size );
	if (NULL == mem) {
		return NULL;
	} 
	*ppBer = (BerElement*) (mem + size);
	memset(*ppBer,0,sizeof(struct berelement));
	(*ppBer)->ber_tag = LBER_DEFAULT;
	(*ppBer)->ber_buf = mem + size + sizeof(struct berelement);
	(*ppBer)->ber_ptr = (*ppBer)->ber_buf;
	(*ppBer)->ber_end = (*ppBer)->ber_buf + lber_bufsize;
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
	} else 
	if (0 == bytes_to_copy) {
		;
	} else
	{
		memcpy(return_buffer,b->p+b->offset,bytes_to_copy);
		b->offset += bytes_to_copy;
	}
	return bytes_to_copy;
}

/* return the tag - LBER_DEFAULT returned means trouble */
static ber_tag_t
get_buffer_tag(byte_buffer *sb )
{
	unsigned char	xbyte;
	ber_tag_t		tag;
	char			*tagp;
	int				i;

	if ( (i = read_bytes( sb, &xbyte, 1 )) != 1 ) {
		return( LBER_DEFAULT );
	}

	if ( (xbyte & LBER_BIG_TAG_MASK) != LBER_BIG_TAG_MASK ) {
		return( (ber_uint_t) xbyte );
	}

	tagp = (char *) &tag;
	tagp[0] = xbyte;
	for ( i = 1; i < sizeof(ber_int_t); i++ ) {
		if ( read_bytes( sb, &xbyte, 1 ) != 1 )
			return( LBER_DEFAULT );

		tagp[i] = xbyte;

		if ( ! (xbyte & LBER_MORE_TAG_MASK) )
			break;
	}

	/* tag too big! */
	if ( i == sizeof(ber_int_t) )
		return( LBER_DEFAULT );

	/* want leading, not trailing 0's */
	return( tag >> (sizeof(ber_int_t) - i - 1) );
}

/* Like ber_get_next, but from a byte buffer the caller already has. */
/* Bytes_Scanned returns the number of bytes we actually looked at in the buffer. */
/* ber_get_next_buffer is now implemented in terms of ber_get_next_buffer_ext */
/* and is here for backward compatibility.  This new function allows us to pass */
/* the Sockbuf structure along */

ber_uint_t
LDAP_CALL
ber_get_next_buffer( void *buffer, size_t buffer_size, ber_len_t *len,
    BerElement *ber, ber_uint_t *Bytes_Scanned )
{
	return (ber_get_next_buffer_ext( buffer, buffer_size, len, ber,
		Bytes_Scanned, NULL));
}

ber_uint_t
LDAP_CALL
ber_get_next_buffer_ext( void *buffer, size_t buffer_size, ber_len_t *len,
    BerElement *ber, ber_uint_t *Bytes_Scanned, Sockbuf *sock )
{
	ber_tag_t		tag = 0; 
	ber_len_t		netlen;
	ber_uint_t		toread;
	unsigned char	lc;
	ssize_t			rc;
	int				noctets, diff;
	byte_buffer		sb = {0};


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

	if ( ber->ber_rwptr == NULL ) {

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
		   (ber->ber_tag != sock->sb_valid_tag)) {
			*Bytes_Scanned=0;
			return( LBER_DEFAULT);
		}

		/* If we have been called before with an incomplete length,
		 * the fragment of the length read is in ber->ber_len_contents
		 * ber->ber_tag_len_read is the # of bytes of the length available
		 * from a previous fragment
		 */

		if (ber->ber_tag_len_read) {
			int nbytes;
			
			noctets = ((ber->ber_len_contents[0]) & 0x7f);
			diff = noctets +1 /* tag */ - ber->ber_tag_len_read;
			
			if ( (nbytes = read_bytes( &sb, (unsigned char *) &ber->ber_len_contents[0] +
						   ber->ber_tag_len_read, diff )) != diff ) {
				
				if (nbytes > 0)
					ber->ber_tag_len_read+=nbytes;
				
				goto premature_exit;
			}
			*len = get_ber_len(ber); /* cast ber->ber_len_contents to unsigned long */
			
		} else {
			/*
			 * Next, read the length.  The first byte contains the length
			 * of the length.  If bit 8 is set, the length is the long
			 * form, otherwise it's the short form.  We don't allow a
			 * length that's greater than what we can hold in an unsigned
			 * long.
			 */

			/* if the length is in long form and we don't get it in one
			 * fragment, we should handle this (TBD).
			 */

			*len = netlen = 0;
			if ( read_bytes( &sb, &lc, 1 ) != 1 ) {
				goto premature_exit;
			}
			if ( lc & 0x80 ) {
				int nbytes;
				
				noctets = (lc & 0x7f);
				if ( noctets > sizeof(ber_uint_t) )
					goto premature_exit;
                                diff = sizeof(ber_uint_t) - noctets;
				if ( (nbytes = read_bytes( &sb, (unsigned char *)&netlen + diff,
							   noctets )) != noctets ) {				       
					/*
					 * The length is in long form and we don't get it in one
					 * fragment, so stash partial length in the ber element
					 * for later use
					 */

					ber->ber_tag_len_read = nbytes + 1;
					ber->ber_len_contents[0]=lc;					
					memset(&(ber->ber_len_contents[1]), 0, sizeof(ber_uint_t));
					memcpy(&(ber->ber_len_contents[1]), (unsigned char *)&netlen + diff, nbytes);

					goto premature_exit;
                                }
                                *len = LBER_NTOHL( netlen );
                        } else {
                                *len = lc;
                        }
		}
		
                ber->ber_len = *len;
		/* length fully decoded */
		ber->ber_tag_len_read=0;
		/*
		 * Finally, malloc a buffer for the contents and read it in.
		 * It's this buffer that's passed to all the other ber decoding
		 * routines.
		 */

#if defined( DOS ) && !defined( _WIN32 )
		if ( *len > 65535 ) {	/* DOS can't allocate > 64K */
			goto premature_exit;
		}
#endif /* DOS && !_WIN32 */

                if ( (sock != NULL)  &&
		    ( sock->sb_options & LBER_SOCKBUF_OPT_MAX_INCOMING_SIZE )
                    && (*len > sock->sb_max_incoming) ) {
                        return( LBER_OVERFLOW );
                }

		if ( ber->ber_buf + *len > ber->ber_end ) {
			if ( nslberi_ber_realloc( ber, *len ) != 0 )
				goto premature_exit;
		}
		ber->ber_ptr = ber->ber_buf;
		ber->ber_end = ber->ber_buf + *len;
		ber->ber_rwptr = ber->ber_buf;
	}

	toread = (ber_uint_t)ber->ber_end - (ber_uint_t)ber->ber_rwptr;
	do {
		if ( (rc = read_bytes( &sb, (unsigned char *)ber->ber_rwptr,
		    (ber_int_t)toread )) <= 0 ) {
			goto premature_exit;
		}

		toread -= rc;
		ber->ber_rwptr += rc;
	} while ( toread > 0 );

	*len = ber->ber_len;
	*Bytes_Scanned = sb.offset;
	return( ber->ber_tag );

premature_exit:
	/*
	 * we're here because we hit the end of the buffer before seeing
	 * all of the PDU
	 */
	*Bytes_Scanned = sb.offset;
	return(LBER_DEFAULT);
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
	ber_len_t len;

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
		    != (ber_slen_t)bv->bv_len ) {
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
 *
 * abobrov> make sure LBER_SOCKET stays of type 'long' 
 * in lber.h to deal with *PRFileDesc trickery <abobrov
 *
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
