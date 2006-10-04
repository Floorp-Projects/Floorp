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

/* encode.c - ber output encoding routines */

#include "lber-int.h"

static int
ber_calc_taglen( ber_tag_t tag )
{
	int			i;
	ber_int_t	mask;

	/* find the first non-all-zero byte in the tag */
	for ( i = sizeof(ber_int_t) - 1; i > 0; i-- ) {
		mask = (0xff << (i * 8));
		/* not all zero */
		if ( tag & mask )
			break;
	}

	return( i + 1 );
}

static int
ber_put_tag( BerElement	*ber, ber_tag_t tag, int nosos )
{
	int			taglen;
	ber_tag_t	ntag;

	taglen = ber_calc_taglen( tag );

	ntag = LBER_HTONL( tag );

	return( ber_write( ber, ((char *) &ntag) + sizeof(ber_int_t) - taglen,
	    taglen, nosos ) );
}

static int
ber_calc_lenlen( ber_len_t len )
{
	/*
	 * short len if it's less than 128 - one byte giving the len,
	 * with bit 8 0.
	 */

	if ( len <= 0x7F )
		return( 1 );

	/*
	 * long len otherwise - one byte with bit 8 set, giving the
	 * length of the length, followed by the length itself.
	 */

	if ( len <= 0xFF )
		return( 2 );
	if ( len <= 0xFFFF )
		return( 3 );
	if ( len <= 0xFFFFFF )
		return( 4 );

	return( 5 );
}

static int
ber_put_len( BerElement *ber, ber_len_t len, int nosos )
{
	int			i;
	char		lenlen;
	ber_int_t	mask;
	ber_len_t	netlen;

	/*
	 * short len if it's less than 128 - one byte giving the len,
	 * with bit 8 0.
	 */

	if ( len <= 127 ) {
		netlen = LBER_HTONL( len );
		return( ber_write( ber, (char *) &netlen + sizeof(ber_int_t) - 1,
		    1, nosos ) );
	}

	/*
	 * long len otherwise - one byte with bit 8 set, giving the
	 * length of the length, followed by the length itself.
	 */

	/* find the first non-all-zero byte */
	for ( i = sizeof(ber_int_t) - 1; i > 0; i-- ) {
		mask = (0xff << (i * 8));
		/* not all zero */
		if ( len & mask )
			break;
	}
	lenlen = ++i;
	if ( lenlen > 4 )
		return( -1 );
	lenlen |= 0x80;

	/* write the length of the length */
	if ( ber_write( ber, &lenlen, 1, nosos ) != 1 )
		return( -1 );

	/* write the length itself */
	netlen = LBER_HTONL( len );
	if ( ber_write( ber, (char *) &netlen + (sizeof(ber_int_t) - i), i, nosos )
	    != i )
		return( -1 );

	return( i + 1 );
}

static int
ber_put_int_or_enum( BerElement *ber, ber_int_t num, ber_tag_t tag )
{
	int			i, sign, taglen;
	int			len, lenlen;
	ber_int_t	netnum, mask;

	sign = (num < 0);

	/*
	 * high bit is set - look for first non-all-one byte
	 * high bit is clear - look for first non-all-zero byte
	 */
	for ( i = sizeof(ber_int_t) - 1; i > 0; i-- ) {
		mask = (0xff << (i * 8));

		if ( sign ) {
			/* not all ones */
			if ( (num & mask) != mask )
				break;
		} else {
			/* not all zero */
			if ( num & mask )
				break;
		}
	}

	/*
	 * we now have the "leading byte".  if the high bit on this
	 * byte matches the sign bit, we need to "back up" a byte.
	 */
	mask = (num & (0x80 << (i * 8)));
	if ( (mask && !sign) || (sign && !mask) )
		i++;

	len = i + 1;

	if ( (taglen = ber_put_tag( ber, tag, 0 )) == -1 )
		return( -1 );

	if ( (lenlen = ber_put_len( ber, len, 0 )) == -1 )
		return( -1 );
	i++;
	netnum = LBER_HTONL( num );
	if ( ber_write( ber, (char *) &netnum + (sizeof(ber_int_t) - i), i, 0 ) 
		== i) 
		/* length of tag + length + contents */
		return( taglen + lenlen + i );

	return( -1 );
}

int
LDAP_CALL
ber_put_enum( BerElement *ber, ber_int_t num, ber_tag_t tag )
{
	if ( tag == LBER_DEFAULT )
		tag = LBER_ENUMERATED;

	return( ber_put_int_or_enum( ber, num, tag ) );
}

int
LDAP_CALL
ber_put_int( BerElement *ber, ber_int_t num, ber_tag_t tag )
{
	if ( tag == LBER_DEFAULT )
		tag = LBER_INTEGER;

	return( ber_put_int_or_enum( ber, num, tag ) );
}

int
LDAP_CALL
ber_put_ostring( BerElement *ber, char *str, ber_len_t len,
	ber_tag_t tag )
{
	int	taglen, lenlen, rc;
#ifdef STR_TRANSLATION
	int	free_str;
#endif /* STR_TRANSLATION */

	if ( tag == LBER_DEFAULT )
		tag = LBER_OCTETSTRING;

	if ( (taglen = ber_put_tag( ber, tag, 0 )) == -1 )
		return( -1 );

#ifdef STR_TRANSLATION
	if ( len > 0 && ( ber->ber_options & LBER_OPT_TRANSLATE_STRINGS ) != 0
	    && ber->ber_encode_translate_proc != NULL ) {
		if ( (*(ber->ber_encode_translate_proc))( &str, &len, 0 )
		    != 0 ) {
			return( -1 );
		}
		free_str = 1;
	} else {
		free_str = 0;
	}
#endif /* STR_TRANSLATION */

    /*  
     *  Note:  below is a spot where we limit ber_write 
     *         to signed long (instead of unsigned long)
     */

	if ( (lenlen = ber_put_len( ber, len, 0 )) == -1 ||
		ber_write( ber, str, len, 0 ) != (ber_int_t) len ) {
		rc = -1;
	} else {
		/* return length of tag + length + contents */
		rc = taglen + lenlen + len;
	}

#ifdef STR_TRANSLATION
	if ( free_str ) {
		NSLBERI_FREE( str );
	}
#endif /* STR_TRANSLATION */

	return( rc );
}

int
LDAP_CALL
ber_put_string( BerElement *ber, char *str, ber_tag_t tag )
{
	return( ber_put_ostring( ber, str, (ber_len_t) strlen( str ), tag ));
}

int
LDAP_CALL
ber_put_bitstring( BerElement *ber, char *str,
	ber_len_t blen /* in bits */, ber_tag_t tag )
{
	int		taglen, lenlen, len;
	unsigned char	unusedbits;

	if ( tag == LBER_DEFAULT )
		tag = LBER_BITSTRING;

	if ( (taglen = ber_put_tag( ber, tag, 0 )) == -1 )
		return( -1 );

	len = ( blen + 7 ) / 8;
	unusedbits = (unsigned char) (len * 8 - blen);
	if ( (lenlen = ber_put_len( ber, len + 1, 0 )) == -1 )
		return( -1 );

	if ( ber_write( ber, (char *)&unusedbits, 1, 0 ) != 1 )
		return( -1 );

	if ( ber_write( ber, str, len, 0 ) != len )
		return( -1 );

	/* return length of tag + length + unused bit count + contents */
	return( taglen + 1 + lenlen + len );
}

int
LDAP_CALL
ber_put_null( BerElement *ber, ber_tag_t tag )
{
	int	taglen;

	if ( tag == LBER_DEFAULT )
		tag = LBER_NULL;

	if ( (taglen = ber_put_tag( ber, tag, 0 )) == -1 )
		return( -1 );

	if ( ber_put_len( ber, 0, 0 ) != 1 )
		return( -1 );

	return( taglen + 1 );
}

int
LDAP_CALL
ber_put_boolean( BerElement *ber, ber_int_t boolval, ber_tag_t tag )
{
	int		taglen;
	unsigned char	trueval = 0xff;
	unsigned char	falseval = 0x00;

	if ( tag == LBER_DEFAULT )
		tag = LBER_BOOLEAN;

	if ( (taglen = ber_put_tag( ber, tag, 0 )) == -1 )
		return( -1 );

	if ( ber_put_len( ber, 1, 0 ) != 1 )
		return( -1 );

	if ( ber_write( ber, (char *)(boolval ? &trueval : &falseval), 1, 0 )
	    != 1 )
		return( -1 );

	return( taglen + 2 );
}

#define FOUR_BYTE_LEN	5


/* the idea here is roughly this: we maintain a stack of these Seqorset
 * structures. This is pushed when we see the beginning of a new set or
 * sequence. It is popped when we see the end of a set or sequence.
 * Since we don't want to malloc and free these structures all the time,
 * we pre-allocate a small set of them within the ber element structure.
 * thus we need to spot when we've overflowed this stack and fall back to
 * malloc'ing instead.
 */
static int
ber_start_seqorset( BerElement *ber, ber_tag_t tag )
{
	Seqorset	*new_sos;

	/* can we fit into the local stack ? */
	if (ber->ber_sos_stack_posn < SOS_STACK_SIZE) {
		/* yes */
		new_sos = &ber->ber_sos_stack[ber->ber_sos_stack_posn];
	} else {
		/* no */
		if ( (new_sos = (Seqorset *)NSLBERI_MALLOC( sizeof(Seqorset)))
		    == NULLSEQORSET ) {
			return( -1 );
		}
	}
	ber->ber_sos_stack_posn++;

	if ( ber->ber_sos == NULLSEQORSET )
		new_sos->sos_first = ber->ber_ptr;
	else
		new_sos->sos_first = ber->ber_sos->sos_ptr;

	/* Set aside room for a 4 byte length field */
	new_sos->sos_ptr = new_sos->sos_first + ber_calc_taglen( tag ) + FOUR_BYTE_LEN;
	new_sos->sos_tag = tag;

	new_sos->sos_next = ber->ber_sos;
	new_sos->sos_clen = 0;

	ber->ber_sos = new_sos;
    if (ber->ber_sos->sos_ptr > ber->ber_end) {
        nslberi_ber_realloc(ber, ber->ber_sos->sos_ptr - ber->ber_end);
    }
	return( 0 );
}

int
LDAP_CALL
ber_start_seq( BerElement *ber, ber_tag_t tag )
{
	if ( tag == LBER_DEFAULT )
		tag = LBER_SEQUENCE;

	return( ber_start_seqorset( ber, tag ) );
}

int
LDAP_CALL
ber_start_set( BerElement *ber, ber_tag_t tag )
{
	if ( tag == LBER_DEFAULT )
		tag = LBER_SET;

	return( ber_start_seqorset( ber, tag ) );
}

static int
ber_put_seqorset( BerElement *ber )
{
	ber_len_t		len, netlen;
	int				taglen, lenlen;
	unsigned char	ltag = 0x80 + FOUR_BYTE_LEN - 1;
	Seqorset		*next;
	Seqorset		**sos = &ber->ber_sos;

	if ( *sos == NULL ) {
        /* No sequence or set to put... fatal error. */
        return( -1 );
	}

    /*
	 * If this is the toplevel sequence or set, we need to actually
	 * write the stuff out.  Otherwise, it's already been put in
	 * the appropriate buffer and will be written when the toplevel
	 * one is written.  In this case all we need to do is update the
	 * length and tag.
	 */

	len = (*sos)->sos_clen;
	netlen = LBER_HTONL( len );
	if ( sizeof(ber_int_t) > 4 && len > 0xFFFFFFFF )
		return( -1 );

	if ( ber->ber_options & LBER_OPT_USE_DER ) {
		lenlen = ber_calc_lenlen( len );
	} else {
		lenlen = FOUR_BYTE_LEN;
	}

	if ( (next = (*sos)->sos_next) == NULLSEQORSET ) {
		/* write the tag */
		if ( (taglen = ber_put_tag( ber, (*sos)->sos_tag, 1 )) == -1 )
			return( -1 );

		if ( ber->ber_options & LBER_OPT_USE_DER ) {
			/* Write the length in the minimum # of octets */
			if ( ber_put_len( ber, len, 1 ) == -1 )
				return( -1 );

			if (lenlen != FOUR_BYTE_LEN) {
				/*
				 * We set aside FOUR_BYTE_LEN bytes for
				 * the length field.  Move the data if
				 * we don't actually need that much
				 */
				SAFEMEMCPY( (*sos)->sos_first + taglen +
				    lenlen, (*sos)->sos_first + taglen +
				    FOUR_BYTE_LEN, len );
			}
		} else {
			/* Fill FOUR_BYTE_LEN bytes for length field */
			/* one byte of length length */
			if ( ber_write( ber, (char *)&ltag, 1, 1 ) != 1 )
				return( -1 );

			/* the length itself */
			if ( ber_write( ber, (char *) &netlen + sizeof(ber_int_t)
			    - (FOUR_BYTE_LEN - 1), FOUR_BYTE_LEN - 1, 1 )
			    != FOUR_BYTE_LEN - 1 )
				return( -1 );
		}
		/* The ber_ptr is at the set/seq start - move it to the end */
		ber->ber_ptr += len;
	} else {
		ber_tag_t	ntag;

		/* the tag */
		taglen = ber_calc_taglen( (*sos)->sos_tag );
		ntag = LBER_HTONL( (*sos)->sos_tag );
		SAFEMEMCPY( (*sos)->sos_first, (char *) &ntag +
		    sizeof(ber_int_t) - taglen, taglen );

		if ( ber->ber_options & LBER_OPT_USE_DER ) {
			ltag = (lenlen == 1) ? (unsigned char)len :  
                (unsigned char) (0x80 + (lenlen - 1));
		}

		/* one byte of length length */
		SAFEMEMCPY( (*sos)->sos_first + 1, &ltag, 1 );

		if ( ber->ber_options & LBER_OPT_USE_DER ) {
			if (lenlen > 1) {
				/* Write the length itself */
				SAFEMEMCPY( (*sos)->sos_first + 2,
				    (char *)&netlen + sizeof(ber_uint_t) -
				    (lenlen - 1),
				    lenlen - 1 );
			}
			if (lenlen != FOUR_BYTE_LEN) {
				/*
				 * We set aside FOUR_BYTE_LEN bytes for
				 * the length field.  Move the data if
				 * we don't actually need that much
				 */
				SAFEMEMCPY( (*sos)->sos_first + taglen +
				    lenlen, (*sos)->sos_first + taglen +
				    FOUR_BYTE_LEN, len );
			}
		} else {
			/* the length itself */
			SAFEMEMCPY( (*sos)->sos_first + taglen + 1,
			    (char *) &netlen + sizeof(ber_int_t) -
			    (FOUR_BYTE_LEN - 1), FOUR_BYTE_LEN - 1 );
		}

		next->sos_clen += (taglen + lenlen + len);
		next->sos_ptr += (taglen + lenlen + len);
	}

	/* we're done with this seqorset, so free it up */
	/* was this one from the local stack ? */
	if (ber->ber_sos_stack_posn <= SOS_STACK_SIZE) {
		/* yes */
	} else {
		/* no */
		NSLBERI_FREE( (char *) (*sos) );
	}
	ber->ber_sos_stack_posn--;
	*sos = next;

	return( taglen + lenlen + len );
}

int
LDAP_CALL
ber_put_seq( BerElement *ber )
{
	return( ber_put_seqorset( ber ) );
}

int
LDAP_CALL
ber_put_set( BerElement *ber )
{
	return( ber_put_seqorset( ber ) );
}

/* VARARGS */
int
LDAP_C
ber_printf( BerElement *ber, const char *fmt, ... )
{
	va_list			ap;
	char			*s, **ss;
	struct berval	*bval, **bv;
	int				rc, i;
	ber_len_t		len;

	va_start( ap, fmt );

#ifdef LDAP_DEBUG
	if ( lber_debug & 64 ) {
		char msg[80];
		sprintf( msg, "ber_printf fmt (%s)\n", fmt );
		ber_err_print( msg );
	}
#endif

	for ( rc = 0; *fmt && rc != -1; fmt++ ) {
		switch ( *fmt ) {
		case 'b':	/* boolean */
			i = va_arg( ap, int );
			rc = ber_put_boolean( ber, i, ber->ber_tag );
			break;

		case 'i':	/* int */
			i = va_arg( ap, int );
			rc = ber_put_int( ber, (ber_int_t)i, ber->ber_tag );
			break;

		case 'e':	/* enumeration */
			i = va_arg( ap, int );
			rc = ber_put_enum( ber, (ber_int_t)i, ber->ber_tag );
			break;

		case 'n':	/* null */
			rc = ber_put_null( ber, ber->ber_tag );
			break;

		case 'o':	/* octet string (non-null terminated) */
			s = va_arg( ap, char * );
			len = va_arg( ap, int );
			rc = ber_put_ostring( ber, s, len, ber->ber_tag );
			break;

		case 'O':	/* berval octet string */
			if( ( bval = va_arg( ap, struct berval * ) ) == NULL )
				break;
			if( bval->bv_len == 0 ) {
				rc = ber_put_ostring( ber, "", 0, ber->ber_tag );
			} else {
				rc = ber_put_ostring( ber, bval->bv_val, bval->bv_len,
					ber->ber_tag );
			}
 			break;

		case 's':	/* string */
			s = va_arg( ap, char * );
			rc = ber_put_string( ber, s, ber->ber_tag );
			break;

		case 'B':	/* bit string */
			s = va_arg( ap, char * );
			len = va_arg( ap, int );	/* in bits */
			rc = ber_put_bitstring( ber, s, len, ber->ber_tag );
			break;

		case 't':	/* tag for the next element */
			ber->ber_tag = va_arg( ap, ber_tag_t );
			ber->ber_usertag = 1;
			break;

		case 'v':	/* vector of strings */
			if ( (ss = va_arg( ap, char ** )) == NULL )
				break;
			for ( i = 0; ss[i] != NULL; i++ ) {
				if ( (rc = ber_put_string( ber, ss[i],
				    ber->ber_tag )) == -1 )
					break;
			}
			break;

		case 'V':	/* sequences of strings + lengths */
			if ( (bv = va_arg( ap, struct berval ** )) == NULL )
				break;
			for ( i = 0; bv[i] != NULL; i++ ) {
				if ( (rc = ber_put_ostring( ber, bv[i]->bv_val,
				    bv[i]->bv_len, ber->ber_tag )) == -1 )
					break;
			}
			break;

		case '{':	/* begin sequence */
			rc = ber_start_seq( ber, ber->ber_tag );
			break;

		case '}':	/* end sequence */
			rc = ber_put_seqorset( ber );
			break;

		case '[':	/* begin set */
			rc = ber_start_set( ber, ber->ber_tag );
			break;

		case ']':	/* end set */
			rc = ber_put_seqorset( ber );
			break;

		default: {
				char msg[80];
				sprintf( msg, "unknown fmt %c\n", *fmt );
				ber_err_print( msg );
				rc = -1;
				break;
			}
		}

		if ( ber->ber_usertag == 0 )
			ber->ber_tag = LBER_DEFAULT;
		else
			ber->ber_usertag = 0;
	}

	va_end( ap );

	return( rc );
}
