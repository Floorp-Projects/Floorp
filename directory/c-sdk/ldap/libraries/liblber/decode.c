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

/* decode.c - ber input decoding routines */

#include "lber-int.h"

/*
 * Note: ber_get_tag() only uses the ber_end and ber_ptr elements of ber.
 * If that changes, the ber_peek_tag() and/or ber_skip_tag() implementations
 * will need to be changed.
 */
/* return the tag - LBER_DEFAULT returned means trouble */
unsigned long
LDAP_CALL
ber_get_tag( BerElement *ber )
{
	unsigned char	xbyte;
	unsigned long	tag;
	char		*tagp;
	int		i;

	if ( ber_read( ber, (char *) &xbyte, 1 ) != 1 )
		return( LBER_DEFAULT );

	if ( (xbyte & LBER_BIG_TAG_MASK) != LBER_BIG_TAG_MASK )
		return( (unsigned long) xbyte );

	tagp = (char *) &tag;
	tagp[0] = xbyte;
	for ( i = 1; i < sizeof(long); i++ ) {
		if ( ber_read( ber, (char *) &xbyte, 1 ) != 1 )
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

/*
 * Note: ber_skip_tag() only uses the ber_end and ber_ptr elements of ber.
 * If that changes, the implementation of ber_peek_tag() will need to
 * be changed.
 */
unsigned long
LDAP_CALL
ber_skip_tag( BerElement *ber, unsigned long *len )
{
	unsigned long	tag;
	unsigned char	lc;
	int		noctets, diff;
	unsigned long	netlen;

	/*
	 * Any ber element looks like this: tag length contents.
	 * Assuming everything's ok, we return the tag byte (we
	 * can assume a single byte), and return the length in len.
	 *
	 * Assumptions:
	 *	1) definite lengths
	 *	2) primitive encodings used whenever possible
	 */

	/*
	 * First, we read the tag.
	 */

	if ( (tag = ber_get_tag( ber )) == LBER_DEFAULT )
		return( LBER_DEFAULT );

	/*
	 * Next, read the length.  The first byte contains the length of
	 * the length.  If bit 8 is set, the length is the long form,
	 * otherwise it's the short form.  We don't allow a length that's
	 * greater than what we can hold in an unsigned long.
	 */

	*len = netlen = 0;
	if ( ber_read( ber, (char *) &lc, 1 ) != 1 )
		return( LBER_DEFAULT );
	if ( lc & 0x80 ) {
		noctets = (lc & 0x7f);
		if ( noctets > sizeof(unsigned long) )
			return( LBER_DEFAULT );
		diff = sizeof(unsigned long) - noctets;
		if ( ber_read( ber, (char *) &netlen + diff, noctets )
		    != noctets )
			return( LBER_DEFAULT );
		*len = LBER_NTOHL( netlen );
	} else {
		*len = lc;
	}

	return( tag );
}


/*
 * Note: Previously, we passed the "ber" parameter directly to ber_skip_tag(),
 * saving and restoring the ber_ptr element only.  We now take advantage
 * of the fact that the only ber structure elements touched by ber_skip_tag()
 * are ber_end and ber_ptr.  If that changes, this code must change too.
 */
unsigned long
LDAP_CALL
ber_peek_tag( BerElement *ber, unsigned long *len )
{
	BerElement	bercopy;

	bercopy.ber_end = ber->ber_end;
	bercopy.ber_ptr = ber->ber_ptr;
	return( ber_skip_tag( &bercopy, len ));
}

static int
ber_getnint( BerElement *ber, long *num, int len )
{
	int i;
	long value;
	unsigned char buffer[sizeof(long)];
	/*
	 * The tag and length have already been stripped off.  We should
	 * be sitting right before len bytes of 2's complement integer,
	 * ready to be read straight into an int.  We may have to sign
	 * extend after we read it in.
	 */

	if ( len > sizeof(long) )
		return( -1 );

	/* read into the low-order bytes of netnum */
	if ( ber_read( ber, (char *) buffer, len ) != len )
		return( -1 );

	/* This sets the required sign extension */
	if ( len != 0) {
		value = 0x80 & buffer[0] ? (-1L) : 0;
	} else {
		value = 0;
	}
		
	for ( i = 0; i < len; i++ )
	  value = (value << 8) | buffer[i];
	
	*num = value;
	
	return( len );
}

unsigned long
LDAP_CALL
ber_get_int( BerElement *ber, long *num )
{
	unsigned long	tag, len;

	if ( (tag = ber_skip_tag( ber, &len )) == LBER_DEFAULT )
		return( LBER_DEFAULT );

	/*
     * len is being demoted to a long here --  possible conversion error
     */
  
	if ( ber_getnint( ber, num, (int)len ) != (long)len )
		return( LBER_DEFAULT );
	else
		return( tag );
}

unsigned long
LDAP_CALL
ber_get_stringb( BerElement *ber, char *buf, unsigned long *len )
{
	unsigned long	datalen, tag;
#ifdef STR_TRANSLATION
	char		*transbuf;
#endif /* STR_TRANSLATION */

	if ( (tag = ber_skip_tag( ber, &datalen )) == LBER_DEFAULT )
		return( LBER_DEFAULT );
	if ( datalen > (*len - 1) )
		return( LBER_DEFAULT );

	/*
     * datalen is being demoted to a long here --  possible conversion error
     */

	if ( ber_read( ber, buf, datalen ) != (long) datalen )
		return( LBER_DEFAULT );

	buf[datalen] = '\0';

#ifdef STR_TRANSLATION
	if ( datalen > 0 && ( ber->ber_options & LBER_OPT_TRANSLATE_STRINGS )
	    != 0 && ber->ber_decode_translate_proc != NULL ) {
		transbuf = buf;
		++datalen;
		if ( (*(ber->ber_decode_translate_proc))( &transbuf, &datalen,
		    0 ) != 0 ) {
			return( LBER_DEFAULT );
		}
		if ( datalen > *len ) {
			NSLBERI_FREE( transbuf );
			return( LBER_DEFAULT );
		}
		SAFEMEMCPY( buf, transbuf, datalen );
		NSLBERI_FREE( transbuf );
		--datalen;
	}
#endif /* STR_TRANSLATION */

	*len = datalen;
	return( tag );
}

unsigned long
LDAP_CALL
ber_get_stringa( BerElement *ber, char **buf )
{
	unsigned long	datalen, ndatalen, tag;

	if ( (tag = ber_skip_tag( ber, &datalen )) == LBER_DEFAULT )
		return( LBER_DEFAULT );

	if ( ((ndatalen = (size_t)datalen + 1) < (size_t) datalen) ||
	   ( (*buf = (char *)NSLBERI_MALLOC( (size_t)ndatalen )) == NULL ))
		return( LBER_DEFAULT );

	/*
     * datalen is being demoted to a long here --  possible conversion error
     */
	if ( ber_read( ber, *buf, datalen ) != (long) datalen )
		return( LBER_DEFAULT );
	(*buf)[datalen] = '\0';

#ifdef STR_TRANSLATION
	if ( datalen > 0 && ( ber->ber_options & LBER_OPT_TRANSLATE_STRINGS )
	    != 0 && ber->ber_decode_translate_proc != NULL ) {
		++datalen;
		if ( (*(ber->ber_decode_translate_proc))( buf, &datalen, 1 )
		    != 0 ) {
			NSLBERI_FREE( *buf );
			return( LBER_DEFAULT );
		}
	}
#endif /* STR_TRANSLATION */

	return( tag );
}

unsigned long
LDAP_CALL
ber_get_stringal( BerElement *ber, struct berval **bv )
{
	unsigned long	len, nlen, tag;

	if ( (*bv = (struct berval *)NSLBERI_MALLOC( sizeof(struct berval) ))
	    == NULL ) {
		return( LBER_DEFAULT );
	}

	if ( (tag = ber_skip_tag( ber, &len )) == LBER_DEFAULT ) {
		return( LBER_DEFAULT );
	}

	if ( ((nlen = (size_t) len + 1) < (size_t)len) ||
	     (((*bv)->bv_val = (char *)NSLBERI_MALLOC( (size_t)nlen ))
	    == NULL )) {
		return( LBER_DEFAULT );
	}

	/*
     * len is being demoted to a long here --  possible conversion error
     */
	if ( ber_read( ber, (*bv)->bv_val, len ) != (int) len )
		return( LBER_DEFAULT );
	((*bv)->bv_val)[len] = '\0';
	(*bv)->bv_len = len;

#ifdef STR_TRANSLATION
	if ( len > 0 && ( ber->ber_options & LBER_OPT_TRANSLATE_STRINGS ) != 0
	    && ber->ber_decode_translate_proc != NULL ) {
		++len;
		if ( (*(ber->ber_decode_translate_proc))( &((*bv)->bv_val),
		    &len, 1 ) != 0 ) {
			NSLBERI_FREE( (*bv)->bv_val );
			return( LBER_DEFAULT );
		}
		(*bv)->bv_len = len - 1;
	}
#endif /* STR_TRANSLATION */

	return( tag );
}

unsigned long
LDAP_CALL
ber_get_bitstringa( BerElement *ber, char **buf, unsigned long *blen )
{
	unsigned long	datalen, tag;
	unsigned char	unusedbits;

	if ( (tag = ber_skip_tag( ber, &datalen )) == LBER_DEFAULT )
		return( LBER_DEFAULT );
	--datalen;

	if ( (*buf = (char *)NSLBERI_MALLOC( (size_t)datalen )) == NULL )
		return( LBER_DEFAULT );

	if ( ber_read( ber, (char *)&unusedbits, 1 ) != 1 )
		return( LBER_DEFAULT );

	/*
     * datalen is being demoted to a long here --  possible conversion error
     */
	if ( ber_read( ber, *buf, datalen ) != (long) datalen )
		return( LBER_DEFAULT );

	*blen = datalen * 8 - unusedbits;
	return( tag );
}

unsigned long
LDAP_CALL
ber_get_null( BerElement *ber )
{
	unsigned long	len, tag;

	if ( (tag = ber_skip_tag( ber, &len )) == LBER_DEFAULT )
		return( LBER_DEFAULT );

	if ( len != 0 )
		return( LBER_DEFAULT );

	return( tag );
}

unsigned long
LDAP_CALL
ber_get_boolean( BerElement *ber, int *boolval )
{
	long	longbool;
	int	rc;

	rc = ber_get_int( ber, &longbool );
	*boolval = longbool;

	return( rc );
}

unsigned long
LDAP_CALL
ber_first_element( BerElement *ber, unsigned long *len, char **last )
{
	/* skip the sequence header, use the len to mark where to stop */
	if ( ber_skip_tag( ber, len ) == LBER_DEFAULT ) {
		return( LBER_ERROR );
	}

	*last = ber->ber_ptr + *len;

	if ( *last == ber->ber_ptr ) {
		return( LBER_END_OF_SEQORSET );
	}

	return( ber_peek_tag( ber, len ) );
}

unsigned long
LDAP_CALL
ber_next_element( BerElement *ber, unsigned long *len, char *last )
{
	if ( ber->ber_ptr == last ) {
		return( LBER_END_OF_SEQORSET );
	}

	return( ber_peek_tag( ber, len ) );
}

/* VARARGS */
unsigned long
LDAP_C
ber_scanf( BerElement *ber, const char *fmt, ... )
{
	va_list		ap;
	char		*last, *p;
	char		*s, **ss, ***sss;
	struct berval 	***bv, **bvp, *bval;
	int		*i, j;
	long		*l, rc, tag;
	unsigned long	*t;
	unsigned long	len;
	size_t		array_size;
	
	va_start( ap, fmt );

#ifdef LDAP_DEBUG
	if ( lber_debug & 64 ) {
		char msg[80];
		sprintf( msg, "ber_scanf fmt (%s) ber:\n", fmt );
		ber_err_print( msg );
		ber_dump( ber, 1 );
	}
#endif
	for ( rc = 0, p = (char *) fmt; *p && rc != LBER_DEFAULT; p++ ) {
		switch ( *p ) {
		case 'a':	/* octet string - allocate storage as needed */
			ss = va_arg( ap, char ** );
			rc = ber_get_stringa( ber, ss );
			break;

		case 'b':	/* boolean */
			i = va_arg( ap, int * );
			rc = ber_get_boolean( ber, i );
			break;

		case 'e':	/* enumerated */
		case 'i':	/* int */
			l = va_arg( ap, long * );
			rc = ber_get_int( ber, l );
			break;

		case 'l':	/* length of next item */
			l = va_arg( ap, long * );
			rc = ber_peek_tag( ber, (unsigned long *)l );
			break;

		case 'n':	/* null */
			rc = ber_get_null( ber );
			break;

		case 's':	/* octet string - in a buffer */
			s = va_arg( ap, char * );
			l = va_arg( ap, long * );
			rc = ber_get_stringb( ber, s, (unsigned long *)l );
			break;

		case 'o':	/* octet string in a supplied berval */
			bval = va_arg( ap, struct berval * );
			ber_peek_tag( ber, &bval->bv_len );
			rc = ber_get_stringa( ber, &bval->bv_val );
			break;

		case 'O':	/* octet string - allocate & include length */
			bvp = va_arg( ap, struct berval ** );
			rc = ber_get_stringal( ber, bvp );
			break;

		case 'B':	/* bit string - allocate storage as needed */
			ss = va_arg( ap, char ** );
			l = va_arg( ap, long * ); /* for length, in bits */
			rc = ber_get_bitstringa( ber, ss, (unsigned long *)l );
			break;

		case 't':	/* tag of next item */
			t = va_arg( ap, unsigned long * );
			*t = rc = ber_peek_tag( ber, &len );
			break;

		case 'T':	/* skip tag of next item */
			t = va_arg( ap, unsigned long * );
			*t = rc = ber_skip_tag( ber, &len );
			break;

		case 'v':	/* sequence of strings */
			sss = va_arg( ap, char *** );
			*sss = NULL;
			j = 0;
			array_size = 0;
			for ( tag = ber_first_element( ber, &len, &last );
			    tag != LBER_DEFAULT && tag != LBER_END_OF_SEQORSET
			    && rc != LBER_DEFAULT;
			    tag = ber_next_element( ber, &len, last ) ) {
				if ( *sss == NULL ) {
				    /* Make room for at least 15 strings */
				    *sss = (char **)NSLBERI_MALLOC(16 * sizeof(char *) );
				    array_size = 16;
				} else {
				    if ( (size_t)(j+2) > array_size) {
					/* We'v overflowed our buffer */
					*sss = (char **)NSLBERI_REALLOC( *sss, (array_size * 2) * sizeof(char *) );
					array_size = array_size * 2;
				    }
				}
				rc = ber_get_stringa( ber, &((*sss)[j]) );
				j++;
			}
			if ( rc != LBER_DEFAULT &&
			    tag != LBER_END_OF_SEQORSET ) {
				rc = LBER_DEFAULT;
			}
			if ( j > 0 )
				(*sss)[j] = NULL;
			break;

		case 'V':	/* sequence of strings + lengths */
			bv = va_arg( ap, struct berval *** );
			*bv = NULL;
			j = 0;
			for ( tag = ber_first_element( ber, &len, &last );
			    tag != LBER_DEFAULT && tag != LBER_END_OF_SEQORSET
			    && rc != LBER_DEFAULT;
			    tag = ber_next_element( ber, &len, last ) ) {
				if ( *bv == NULL ) {
					*bv = (struct berval **)NSLBERI_MALLOC(
					    2 * sizeof(struct berval *) );
				} else {
					*bv = (struct berval **)NSLBERI_REALLOC(
					    *bv,
					    (j + 2) * sizeof(struct berval *) );
				}
				rc = ber_get_stringal( ber, &((*bv)[j]) );
				j++;
			}
			if ( rc != LBER_DEFAULT &&
			    tag != LBER_END_OF_SEQORSET ) {
				rc = LBER_DEFAULT;
			}
			if ( j > 0 )
				(*bv)[j] = NULL;
			break;

		case 'x':	/* skip the next element - whatever it is */
			if ( (rc = ber_skip_tag( ber, &len )) == LBER_DEFAULT )
				break;
			ber->ber_ptr += len;
			break;

		case '{':	/* begin sequence */
		case '[':	/* begin set */
			if ( *(p + 1) != 'v' && *(p + 1) != 'V' )
				rc = ber_skip_tag( ber, &len );
			break;

		case '}':	/* end sequence */
		case ']':	/* end set */
			break;

		default:
			{
				char msg[80];
				sprintf( msg, "unknown fmt %c\n", *p );
				ber_err_print( msg );
			}
			rc = LBER_DEFAULT;
			break;
		}
	}

	va_end( ap );

	if (rc == LBER_DEFAULT) {
	  va_start( ap, fmt );
	  for ( p--; fmt < p && *fmt; fmt++ ) {
		switch ( *fmt ) {
		case 'a':	/* octet string - allocate storage as needed */
			ss = va_arg( ap, char ** );
			NSLBERI_FREE(*ss);
			*ss = NULL;
			break;

		case 'b':	/* boolean */
			i = va_arg( ap, int * );
			break;

		case 'e':	/* enumerated */
		case 'i':	/* int */
			l = va_arg( ap, long * );
			break;

		case 'l':	/* length of next item */
			l = va_arg( ap, long * );
			break;

		case 'n':	/* null */
			break;

		case 's':	/* octet string - in a buffer */
			s = va_arg( ap, char * );
			l = va_arg( ap, long * );
			break;

		case 'o':	/* octet string in a supplied berval */
			bval = va_arg( ap, struct berval * );
			if (bval->bv_val) NSLBERI_FREE(bval->bv_val);
			memset(bval, 0, sizeof(struct berval));
			break;

		case 'O':	/* octet string - allocate & include length */
			bvp = va_arg( ap, struct berval ** );
			ber_bvfree(*bvp);
			bvp = NULL;
			break;

		case 'B':	/* bit string - allocate storage as needed */
			ss = va_arg( ap, char ** );
			l = va_arg( ap, long * ); /* for length, in bits */
			if (*ss) NSLBERI_FREE(*ss);
			*ss = NULL;
			break;

		case 't':	/* tag of next item */
			t = va_arg( ap, unsigned long * );
			break;

		case 'T':	/* skip tag of next item */
			t = va_arg( ap, unsigned long * );
			break;

		case 'v':	/* sequence of strings */
			sss = va_arg( ap, char *** );
			ber_svecfree(*sss);
			*sss = NULL;
			break;

		case 'V':	/* sequence of strings + lengths */
			bv = va_arg( ap, struct berval *** );
			ber_bvecfree(*bv);
			*bv = NULL;
			break;

		case 'x':	/* skip the next element - whatever it is */
			break;

		case '{':	/* begin sequence */
		case '[':	/* begin set */
			break;

		case '}':	/* end sequence */
		case ']':	/* end set */
			break;

		default:
			break;
		}
	  } /* for */
	  va_end( ap );
	} /* if */

	return( rc );
}

void
LDAP_CALL
ber_bvfree( struct berval *bv )
{
	if ( bv != NULL ) {
		if ( bv->bv_val != NULL ) {
			NSLBERI_FREE( bv->bv_val );
		}
		NSLBERI_FREE( (char *) bv );
	}
}

void
LDAP_CALL
ber_bvecfree( struct berval **bv )
{
	int	i;

	if ( bv != NULL ) {
		for ( i = 0; bv[i] != NULL; i++ ) {
			ber_bvfree( bv[i] );
		}
		NSLBERI_FREE( (char *) bv );
	}
}

struct berval *
LDAP_CALL
ber_bvdup( const struct berval *bv )
{
	struct berval	*new;

	if ( (new = (struct berval *)NSLBERI_MALLOC( sizeof(struct berval) ))
	    == NULL ) {
		return( NULL );
	}
	if ( bv->bv_val == NULL ) {
	    new->bv_val = NULL;
	    new->bv_len = 0;
	} else {
	    if ( (new->bv_val = (char *)NSLBERI_MALLOC( bv->bv_len + 1 ))
		== NULL ) {
		    return( NULL );
	    }
	    SAFEMEMCPY( new->bv_val, bv->bv_val, (size_t) bv->bv_len );
	    new->bv_val[bv->bv_len] = '\0';
	    new->bv_len = bv->bv_len;
	}

	return( new );
}

void
LDAP_CALL
ber_svecfree( char **vals )
{
        int     i;

        if ( vals == NULL )
                return;
        for ( i = 0; vals[i] != NULL; i++ )
                NSLBERI_FREE( vals[i] );
        NSLBERI_FREE( (char *) vals );
}

#ifdef STR_TRANSLATION
void
LDAP_CALL
ber_set_string_translators(
    BerElement		*ber,
    BERTranslateProc	encode_proc,
    BERTranslateProc	decode_proc
)
{
    ber->ber_encode_translate_proc = encode_proc;
    ber->ber_decode_translate_proc = decode_proc;
}
#endif /* STR_TRANSLATION */
