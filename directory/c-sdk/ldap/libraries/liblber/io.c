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
 * internal global structure for memory allocation callback functions
 */
struct lber_memalloc_fns nslberi_memalloc_fns;


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
		if ( sb->sb_read_fn != NULL ) {
			rc = sb->sb_read_fn( sb->sb_sd, sb->sb_ber.ber_buf,
			    ((sb->sb_options & LBER_SOCKBUF_OPT_NO_READ_AHEAD)
			    && (len < READBUFSIZ)) ? len : READBUFSIZ );
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
	long	nwritten, towrite, rc;

	if ( ber->ber_rwptr == NULL ) {
		ber->ber_rwptr = ber->ber_buf;
	}
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
		rc = write( sb->sb_fd, ber->ber_buf, towrite );
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
			if ( sb->sb_write_fn != NULL ) {
				if ( (rc = sb->sb_write_fn( sb->sb_sd,
				    ber->ber_rwptr, (size_t) towrite )) <= 0 ) {
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
	    sizeof(struct berelement) + EXBUFSIZ )) == NULL ) {
		return( NULL );
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

/* return the tag - LBER_DEFAULT returned means trouble */
static unsigned long
get_tag( Sockbuf *sb )
{
	unsigned char	xbyte;
	unsigned long	tag;
	char		*tagp;
	int		i;

	if ( (i = BerRead( sb, (char *) &xbyte, 1 )) != 1 ) {
		return( LBER_DEFAULT );
	}

	if ( (xbyte & LBER_BIG_TAG_MASK) != LBER_BIG_TAG_MASK ) {
		return( (unsigned long) xbyte );
	}

	tagp = (char *) &tag;
	tagp[0] = xbyte;
	for ( i = 1; i < sizeof(long); i++ ) {
		if ( BerRead( sb, (char *) &xbyte, 1 ) != 1 )
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

unsigned long
LDAP_CALL
ber_get_next( Sockbuf *sb, unsigned long *len, BerElement *ber )
{
	unsigned long	tag = 0, netlen, toread;
	unsigned char	lc;
	long		rc;
	int		noctets, diff;

#ifdef LDAP_DEBUG
	if ( lber_debug )
		ber_err_print( "ber_get_next\n" );
#endif

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

	if ( ber->ber_rwptr == NULL ) {
		/*
		 * First, we read the tag.
		 */

		if ( (tag = get_tag( sb )) == LBER_DEFAULT ) {
			return( LBER_DEFAULT );
		}
		ber->ber_tag = tag;

		/*
		 * Next, read the length.  The first byte contains the length
		 * of the length.  If bit 8 is set, the length is the long
		 * form, otherwise it's the short form.  We don't allow a
		 * length that's greater than what we can hold in an unsigned
		 * long.
		 */

		*len = netlen = 0;
		if ( BerRead( sb, (char *) &lc, 1 ) != 1 ) {
			return( LBER_DEFAULT );
		}
		if ( lc & 0x80 ) {
			noctets = (lc & 0x7f);
			if ( noctets > sizeof(unsigned long) )
				return( LBER_DEFAULT );
			diff = sizeof(unsigned long) - noctets;
			if ( BerRead( sb, (char *) &netlen + diff, noctets ) !=
			    noctets ) {
				return( LBER_DEFAULT );
			}
			*len = LBER_NTOHL( netlen );
		} else {
			*len = lc;
		}
		ber->ber_len = *len;

		/*
		 * Finally, malloc a buffer for the contents and read it in.
		 * It's this buffer that's passed to all the other ber decoding
		 * routines.
		 */

#if defined( DOS ) && !( defined( _WIN32 ) || defined(XP_OS2) )
		if ( *len > 65535 ) {	/* DOS can't allocate > 64K */
		    return( LBER_DEFAULT );
		}
#endif /* DOS && !_WIN32 */

		if ( ( sb->sb_options & LBER_SOCKBUF_OPT_MAX_INCOMING_SIZE )
		    && *len > sb->sb_max_incoming ) {
			return( LBER_DEFAULT );
		}

		if ( (ber->ber_buf = (char *)NSLBERI_CALLOC( 1,(size_t)*len ))
		    == NULL ) {
			return( LBER_DEFAULT );
		}
		ber->ber_flags &= ~LBER_FLAG_NO_FREE_BUFFER;
		ber->ber_ptr = ber->ber_buf;
		ber->ber_end = ber->ber_buf + *len;
		ber->ber_rwptr = ber->ber_buf;
	}

	toread = (unsigned long)ber->ber_end - (unsigned long)ber->ber_rwptr;
	do {
		if ( (rc = BerRead( sb, ber->ber_rwptr, (long)toread )) <= 0 ) {
			return( LBER_DEFAULT );
		}

		toread -= rc;
		ber->ber_rwptr += rc;
	} while ( toread > 0 );

#ifdef LDAP_DEBUG
	if ( lber_debug ) {
		char msg[80];
		sprintf( msg, "ber_get_next: tag 0x%lx len %ld contents:\n",
		    tag, ber->ber_len );
		ber_err_print( msg );
		if ( lber_debug > 1 )
			ber_dump( ber, 1 );
	}
#endif

	*len = ber->ber_len;
	ber->ber_rwptr = NULL;
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
	if ( !NSLBERI_VALID_SOCKBUF_POINTER( sb )) {
		return( -1 );
	}

	switch ( option ) {
	case LBER_SOCKBUF_OPT_MAX_INCOMING_SIZE:
		sb->sb_max_incoming = *((int *) value);
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
		sb->sb_fd = *((LBER_SOCKET *) value);
		break;
	case LBER_SOCKBUF_OPT_READ_FN:
		sb->sb_read_fn = (LDAP_IOF_READ_CALLBACK *) value;
		break;
	case LBER_SOCKBUF_OPT_WRITE_FN:
		sb->sb_write_fn = (LDAP_IOF_WRITE_CALLBACK *) value;
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
	if ( !NSLBERI_VALID_SOCKBUF_POINTER( sb )) {
		return( -1 );
	}

	switch ( option ) {
	case LBER_SOCKBUF_OPT_MAX_INCOMING_SIZE:
		*((int *) value) = sb->sb_max_incoming;
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
		*((LBER_SOCKET *) value) = sb->sb_fd;
		break;
	case LBER_SOCKBUF_OPT_READ_FN:
		*((LDAP_IOF_READ_CALLBACK **) value) = sb->sb_read_fn;
		break;
	case LBER_SOCKBUF_OPT_WRITE_FN:
		*((LDAP_IOF_WRITE_CALLBACK **) value) = sb->sb_write_fn;
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
	if (0 != size & 0x03) {
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

unsigned long
LDAP_CALL
ber_get_next_buffer( void *buffer, size_t buffer_size, unsigned long *len,
    BerElement *ber, unsigned long *Bytes_Scanned )
{
	unsigned long	tag = 0, netlen, toread;
	unsigned char	lc;
	long		rc;
	int		noctets, diff;
	int offset = 0;
	byte_buffer sb = {0};


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

		if ( (tag = get_buffer_tag( &sb )) == LBER_DEFAULT ) {
			goto premature_exit;
		}
		ber->ber_tag = tag;

		/*
		 * Next, read the length.  The first byte contains the length
		 * of the length.  If bit 8 is set, the length is the long
		 * form, otherwise it's the short form.  We don't allow a
		 * length that's greater than what we can hold in an unsigned
		 * long.
		 */

		*len = netlen = 0;
		if ( read_bytes( &sb, &lc, 1 ) != 1 ) {
			goto premature_exit;
		}
		if ( lc & 0x80 ) {
			noctets = (lc & 0x7f);
			if ( noctets > sizeof(unsigned long) )
				goto premature_exit;
			diff = sizeof(unsigned long) - noctets;
			if ( read_bytes( &sb, (unsigned char *)&netlen + diff,
			    noctets ) != noctets ) {
				goto premature_exit;
			}
			*len = LBER_NTOHL( netlen );
		} else {
			*len = lc;
		}
		ber->ber_len = *len;

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

		if ( ber->ber_buf + *len > ber->ber_end ) {
			if ( nslberi_ber_realloc( ber, *len ) != 0 )
				goto premature_exit;
		}
		ber->ber_ptr = ber->ber_buf;
		ber->ber_end = ber->ber_buf + *len;
		ber->ber_rwptr = ber->ber_buf;
	}

	toread = (unsigned long)ber->ber_end - (unsigned long)ber->ber_rwptr;
	do {
		if ( (rc = read_bytes( &sb, (unsigned char *)ber->ber_rwptr,
		    (long)toread )) <= 0 ) {
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
ber_init( struct berval *bv )
{
	BerElement *ber;

	/* construct BerElement */
	if (( ber = ber_alloc_t( 0 )) != NULLBER ) {
		/* copy data from the bv argument into BerElement */
		if ( (ber_write ( ber, bv->bv_val, bv->bv_len, 0 ))
		    != bv->bv_len ) {
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
