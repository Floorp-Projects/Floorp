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

/* line64.c - routines for dealing with the slapd line format */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#ifndef macintosh
#include <sys/types.h>
#endif
#ifdef _WIN32
#include <windows.h>
#elif !defined( macintosh )
#include <sys/socket.h>
#endif
#include "ldaplog.h"
#include "ldif.h"

#ifndef isascii
#define isascii( c )	(!((c) & ~0177))
#endif

#define RIGHT2			0x03
#define RIGHT4			0x0f
#define CONTINUED_LINE_MARKER	'\001'

#define ISBLANK(c) (c == ' ' || c == '\t' || c == '\n') /* not "\r\v\f" */

#define LDIF_OPT_ISSET( value, opt )	(((value) & (opt)) != 0 )

static char nib2b64[0x40f] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static unsigned char b642nib[0x80] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0x3e, 0xff, 0xff, 0xff, 0x3f,
	0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b,
	0x3c, 0x3d, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
	0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
	0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
	0x17, 0x18, 0x19, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
	0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
	0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30,
	0x31, 0x32, 0x33, 0xff, 0xff, 0xff, 0xff, 0xff
};

static int ldif_base64_encode_internal( unsigned char *src, char *dst, int srclen,
	int lenused, int wraplen );

/*
 * ldif_parse_line - takes a line of the form "type:[:] value" and splits it
 * into components "type" and "value".  if a double colon separates type from
 * value, then value is encoded in base 64, and parse_line un-decodes it
 * (in place) before returning.
 */

int
ldif_parse_line(
    char	*line,
    char	**type,
    char	**value,
    int		*vlen
)
{
	char	*p, *s, *d;
	int	b64;

	/* skip any leading space */
	while ( ISBLANK( *line ) ) {
		line++;
	}
	*type = line;

	for ( s = line; *s && *s != ':'; s++ )
		;	/* NULL */
	if ( *s == '\0' ) {

		/* Comment-out while we address calling libldif from ns-back-ldbm
			on NT. 1 of 3 */
#if defined( _WIN32 )
		/*
#endif
		 LDAPDebug( LDAP_DEBUG_PARSE, "ldif_parse_line: missing ':' "
			"on line \"%s\"\n", line, 0, 0 ); 
#if defined( _WIN32 )
		*/
#endif
		return( -1 );
	}

	/* trim any space between type and : */
	for ( p = s - 1; p > line && ISBLANK( *p ); p-- ) {
		*p = '\0';
	}
	*s++ = '\0';

	/* check for double : - indicates base 64 encoded value */
	if ( *s == ':' ) {
		s++;
		b64 = 1;

	/* single : - normally encoded value */
	} else {
		b64 = 0;
	}

	/* skip space between : and value */
	while ( ISBLANK( *s ) ) {
		s++;
	}

	/* 
	 * If no value is present, return a zero-length string for
	 * *value, with *vlen set to zero.
	 */
	if ( *s == '\0' ) {
		*value = s;
		*vlen = 0;
		return( 0 );
	}

	/* check for continued line markers that should be deleted */
	for ( p = s, d = s; *p; p++ ) {
		if ( *p != CONTINUED_LINE_MARKER )
			*d++ = *p;
	}
	*d = '\0';

	*value = s;
	if ( b64 ) {
		if (( *vlen = ldif_base64_decode( s, (unsigned char *)s ))
		    < 0 ) {
			/* Comment-out while we address calling libldif from ns-back-ldbm
				on NT. 3 of 3 */
#if defined( _WIN32 )
		/*
#endif
			 LDAPDebug( LDAP_DEBUG_ANY,
			    "ldif_parse_line: invalid base 64 char on line \"%s\"\n",
			    line, 0, 0 ); 
#if defined( _WIN32 )
		*/
#endif
			return( -1 );
		}
		s[ *vlen ] = '\0';
	} else {
		*vlen = (int) (d - s);
	}

	return( 0 );
}


/*
 * ldif_base64_decode - take the BASE64-encoded characters in "src"
 * (a zero-terminated string) and decode them into the the buffer "dst".
 * "src" and "dst" can be the same if in-place decoding is desired.
 * "dst" must be large enough to hold the decoded octets.  No more than
 *	3 * strlen( src ) / 4 bytes will be produced.
 * "dst" may contain zero octets anywhere within it, but it is not
 *	zero-terminated by this function.
 *
 * The number of bytes copied to "dst" is returned if all goes well.
 * -1 is returned if the BASE64 encoding in "src" is invalid.
 */

int
ldif_base64_decode( char *src, unsigned char *dst )
{
	char		*p, *stop;
	unsigned char	nib, *byte;
	int		i, len;

	stop = strchr( src, '\0' );
	byte = dst;
	for ( p = src, len = 0; p < stop; p += 4, len += 3 ) {
		for ( i = 0; i < 4; i++ ) {
			if ( p[i] != '=' && (p[i] & 0x80 ||
			    b642nib[ p[i] & 0x7f ] > 0x3f) ) {
				return( -1 );
			}
		}

		/* first digit */
		nib = b642nib[ p[0] & 0x7f ];
		byte[0] = nib << 2;

		/* second digit */
		nib = b642nib[ p[1] & 0x7f ];
		byte[0] |= nib >> 4;

		/* third digit */
		if ( p[2] == '=' ) {
			len += 1;
			break;
		}
		byte[1] = (nib & RIGHT4) << 4;
		nib = b642nib[ p[2] & 0x7f ];
		byte[1] |= nib >> 2;

		/* fourth digit */
		if ( p[3] == '=' ) {
			len += 2;
			break;
		}
		byte[2] = (nib & RIGHT2) << 6;
		nib = b642nib[ p[3] & 0x7f ];
		byte[2] |= nib;

		byte += 3;
	}

	return( len );
}

/*
 * ldif_getline - return the next "line" (minus newline) of input from a
 * string buffer of lines separated by newlines, terminated by \n\n
 * or \0.  this routine handles continued lines, bundling them into
 * a single big line before returning.  if a line begins with a white
 * space character, it is a continuation of the previous line. the white
 * space character (nb: only one char), and preceeding newline are changed
 * into CONTINUED_LINE_MARKER chars, to be deleted later by the
 * ldif_parse_line() routine above.
 *
 * it takes a pointer to a pointer to the buffer on the first call,
 * which it updates and must be supplied on subsequent calls.
 *
 * XXX need to update this function to also support <CR><LF> as EOL.
 * XXX supports <CR><LF> as of 07/29/1998 (richm)
 */

char *
ldif_getline( char **next )
{
	char	*l;
	char	c;
	char	*p;

	if ( *next == NULL || **next == '\n' || **next == '\0' ) {
		return( NULL );
	}

	while ( **next == '#' ) {	/* skip comment lines */
		if (( *next = strchr( *next, '\n' )) == NULL ) {
			return( NULL );
		}
		(*next)++;
	}

	l = *next;
	while ( (*next = strchr( *next, '\n' )) != NULL ) {
		p = *next - 1; /* pointer to character previous to the newline */
		c = *(*next + 1); /* character after the newline */
		if ( ISBLANK( c ) && c != '\n' ) {
			/* DOS EOL is \r\n, so if the character before */
			/* the \n is \r, continue it too */
			if (*p == '\r')
				*p = CONTINUED_LINE_MARKER;
			**next = CONTINUED_LINE_MARKER;
			*(*next+1) = CONTINUED_LINE_MARKER;
		} else {
			/* DOS EOL is \r\n, so if the character before */
			/* the \n is \r, null it too */
			if (*p == '\r')
				*p = '\0';
			*(*next)++ = '\0';
			break;
		}
		(*next)++;
	}

	return( l );
}


#define LDIF_SAFE_CHAR( c )		( (c) != '\r' && (c) != '\n' )
#define LDIF_CONSERVATIVE_CHAR( c )	( LDIF_SAFE_CHAR(c) && isascii((c)) \
					 && ( isprint((c)) || (c) == '\t' ))
#define LDIF_SAFE_INITCHAR( c )		( LDIF_SAFE_CHAR(c) && (c) != ':' \
					 && (c) != ' ' && (c) != '<' )
#define LDIF_CONSERVATIVE_INITCHAR( c ) ( LDIF_SAFE_INITCHAR( c ) && \
					 ! ( isascii((c)) && isspace((c))))
#define LDIF_CONSERVATIVE_FINALCHAR( c ) ( (c) != ' ' )


void
ldif_put_type_and_value_with_options( char **out, char *t, char *val,
	int vlen, unsigned long options )
{
	unsigned char	*p, *byte, *stop;
	char		*save;
	int		b64, len, savelen, wraplen;
	len = 0;

	if ( LDIF_OPT_ISSET( options, LDIF_OPT_NOWRAP )) {
		wraplen = -1;
	} else {
		wraplen = LDIF_MAX_LINE_WIDTH;
	}

	/* put the type + ": " */
	for ( p = (unsigned char *) t; *p; p++, len++ ) {
		*(*out)++ = *p;
	}
	*(*out)++ = ':';
	len++;
	if ( LDIF_OPT_ISSET( options, LDIF_OPT_VALUE_IS_URL )) {
		*(*out)++ = '<';	/* add '<' for URLs */
		len++;
	}
	save = *out;
	savelen = len;
	b64 = 0;

	stop = (unsigned char *)val;
	if ( val && vlen > 0 ) {
		*(*out)++ = ' ';
		stop = (unsigned char *) (val + vlen);
		if ( LDIF_OPT_ISSET( options, LDIF_OPT_MINIMAL_ENCODING )) {
			if ( !LDIF_SAFE_INITCHAR( val[0] )) {
				b64 = 1;
			}
		} else {
			if ( !LDIF_CONSERVATIVE_INITCHAR( val[0] ) ||
				 !LDIF_CONSERVATIVE_FINALCHAR( val[vlen-1] )) {
				b64 = 1;
			}
		}
	}

	if ( !b64 ) {
		for ( byte = (unsigned char *) val; byte < stop;
		    byte++, len++ ) {
			if ( LDIF_OPT_ISSET( options,
			    LDIF_OPT_MINIMAL_ENCODING )) {
				if ( !LDIF_SAFE_CHAR( *byte )) {
					b64 = 1;
					break;
				}
			} else if ( !LDIF_CONSERVATIVE_CHAR( *byte )) {
				b64 = 1;
				break;
			}
			
			if ( wraplen != -1 && len > wraplen ) {
				*(*out)++ = '\n';
				*(*out)++ = ' ';
				len = 1;
			}
			*(*out)++ = *byte;
		}
	}

	if ( b64 ) {
		*out = save;
		*(*out)++ = ':';
		*(*out)++ = ' ';
		len = ldif_base64_encode_internal( (unsigned char *)val, *out, vlen,
		    savelen + 2, wraplen );
		*out += len;
	}

	*(*out)++ = '\n';
}

void 
ldif_put_type_and_value( char **out, char *t, char *val, int vlen )
{
    ldif_put_type_and_value_with_options( out, t, val, vlen, 0 );
}

void 
ldif_put_type_and_value_nowrap( char **out, char *t, char *val, int vlen )
{
    ldif_put_type_and_value_with_options( out, t, val, vlen, LDIF_OPT_NOWRAP );
}

/*
 * ldif_base64_encode_internal - encode "srclen" bytes in "src", place BASE64
 * encoded bytes in "dst" and return the length of the BASE64
 * encoded string.  "dst" is also zero-terminated by this function.
 *
 * If "lenused" >= 0, newlines will be included in "dst" and "lenused" if
 * appropriate.  "lenused" should be a count of characters already used
 * on the current line.  The LDIF lines we create will contain at most
 * "wraplen" characters on each line, unless "wraplen" is -1, in which
 * case output line length is unlimited.
 *
 * If "lenused" < 0, no newlines will be included, and the LDIF_BASE64_LEN()
 * macro can be used to determine how many bytes will be placed in "dst."
 */

static int
ldif_base64_encode_internal( unsigned char *src, char *dst, int srclen, int lenused, int wraplen )
{
	unsigned char	*byte, *stop;
	unsigned char	buf[3];
	char		*out;
	unsigned long	bits;
	int		i, pad, len;

	len = 0;
	out = dst;
	stop = src + srclen;

	/* convert to base 64 (3 bytes => 4 base 64 digits) */
	for ( byte = src; byte < stop - 2; byte += 3 ) {
		bits = (byte[0] & 0xff) << 16;
		bits |= (byte[1] & 0xff) << 8;
		bits |= (byte[2] & 0xff);

		for ( i = 0; i < 4; i++, bits <<= 6 ) {
			if ( wraplen != -1 &&  lenused >= 0 && lenused++ > wraplen ) {
				*out++ = '\n';
				*out++ = ' ';
				lenused = 2;
			}

			/* get b64 digit from high order 6 bits */
			*out++ = nib2b64[ (bits & 0xfc0000L) >> 18 ];
		}
	}

	/* add padding if necessary */
	if ( byte < stop ) {
		for ( i = 0; byte + i < stop; i++ ) {
			buf[i] = byte[i];
		}
		for ( pad = 0; i < 3; i++, pad++ ) {
			buf[i] = '\0';
		}
		byte = buf;
		bits = (byte[0] & 0xff) << 16;
		bits |= (byte[1] & 0xff) << 8;
		bits |= (byte[2] & 0xff);

		for ( i = 0; i < 4; i++, bits <<= 6 ) {
			if ( wraplen != -1 && lenused >= 0 && lenused++ > wraplen ) {
				*out++ = '\n';
				*out++ = ' ';
				lenused = 2;
			}

			if (( i == 3 && pad > 0 ) || ( i == 2 && pad == 2 )) {
				/* Pad as appropriate */
				*out++ = '=';
			} else {
				/* get b64 digit from low order 6 bits */
				*out++ = nib2b64[ (bits & 0xfc0000L) >> 18 ];
			}
		}
	}

	*out = '\0';

	return( out - dst );
}

int
ldif_base64_encode( unsigned char *src, char *dst, int srclen, int lenused )
{
    return ldif_base64_encode_internal( src, dst, srclen, lenused, LDIF_MAX_LINE_WIDTH );
}

int
ldif_base64_encode_nowrap( unsigned char *src, char *dst, int srclen, int lenused )
{
    return ldif_base64_encode_internal( src, dst, srclen, lenused, -1 );
}


/*
 * return malloc'd, zero-terminated LDIF line
 */
char *
ldif_type_and_value_with_options( char *type, char *val, int vlen,
	unsigned long options )
{
    char	*buf, *p;
    int		tlen;

    tlen = strlen( type );
    if (( buf = (char *)malloc( LDIF_SIZE_NEEDED( tlen, vlen ) + 1 )) !=
	    NULL ) {
	p = buf;
	ldif_put_type_and_value_with_options( &p, type, val, vlen, options );
	*p = '\0';
    }

    return( buf );
}

char *
ldif_type_and_value( char *type, char *val, int vlen )
{
    return ldif_type_and_value_with_options( type, val, vlen, 0 );
}

char *
ldif_type_and_value_nowrap( char *type, char *val, int vlen )
{
    return ldif_type_and_value_with_options( type, val, vlen, LDIF_OPT_NOWRAP );
}

/*
 * ldif_get_entry - read the next ldif entry from the FILE referenced
 * by fp. return a pointer to a malloc'd, null-terminated buffer. also
 * returned is the last line number read, in *lineno.
 */
char *
ldif_get_entry( FILE *fp, int *lineno )
{
	char	line[BUFSIZ];
	char	*buf;
	int	max, cur, len, gotsome;

	buf = NULL;
	max = cur = gotsome = 0;
	while ( fgets( line, sizeof(line), fp ) != NULL ) {
		if ( lineno != NULL ) {
			(*lineno)++;
		}
		/* ldif entries are terminated by a \n on a line by itself */
		if ( line[0] == '\0' || line[0] == '\n'
#if !defined( XP_WIN32 )
		     || ( line[0] == '\r' && line[1] == '\n' ) /* DOS format */
#endif
		   ) {
			if ( gotsome ) {
				break;
			} else {
				continue;
			}
		} else if ( line[0] == '#' ) {
			continue;
		}
		gotsome = 1;
		len = strlen( line );
#if !defined( XP_WIN32 )
		/* DOS format */
		if ( len > 0 && line[len-1] == '\r' ) {
			--len;
			line[len] = '\0';
		} else if ( len > 1 && line[len-2] == '\r' && line[len-1] == '\n' ) {
			--len;
			line[len-1] = line[len];
			line[len] = '\0';
		}
#endif
		while ( cur + (len + 1) > max ) {
			if ( buf == NULL ) {
				max += BUFSIZ;
				buf = (char *) malloc( max );
			} else {
				max *= 2;
				buf = (char *) realloc( buf, max );
			}
			if ( buf == NULL ) {
				return( NULL );
			}
		}

		memcpy( buf + cur, line, len + 1 );
		cur += len;
	}

	return( buf );
}
