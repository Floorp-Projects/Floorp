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


#ifdef _WIN32
#define  VC_EXTRALEAN
#include <stdarg.h>
#include <windef.h>
#include <winbase.h>
#include <tchar.h>
#include <winnls.h>
static char *win_char_converter(const char *instr, int bFromUTF8);
#else
#include <locale.h>
#endif

#include "ldaptool.h"

#ifndef _WIN32
#include <iconv.h>
#include <langinfo.h>	/* for nl_langinfo() */
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Alternative names for the UTF-8 character set. Both of these (_A and _B)
 * are accepted as meaning UTF-8 on all platforms.
 */
#define LDAPTOOL_CHARSET_UTF8_A		"utf8"
#define LDAPTOOL_CHARSET_UTF8_B		"UTF-8"

/*
 * OS name for UTF-8.
 */
#if defined(_HPUX_SOURCE)
#define LDAPTOOL_CHARSET_UTF8_OSNAME	LDAPTOOL_CHARSET_UTF8_A	/* HP/UX */
#else
#define LDAPTOOL_CHARSET_UTF8_OSNAME	LDAPTOOL_CHARSET_UTF8_B	/* all others */
#endif

/* OS name for the default character set */
#if defined(_HPUX_SOURCE)
#define LDAPTOOL_CHARSET_DEFAULT	"roma8"		/* HP/UX */
#elif defined(__GLIBC__)
#define LDAPTOOL_CHARSET_DEFAULT	"US-ASCII"	/* glibc (Linux) */
#elif defined(_WIN32)
#define LDAPTOOL_CHARSET_DEFAULT	"windows-1252"	/* Windows */
#define LDAPTOOL_CHARSET_WINANSI	"ANSI"		/* synonym */
#else
#define LDAPTOOL_CHARSET_DEFAULT	"646"		/* all others */
#endif

/* Type used for the src parameter to iconv() (the 2nd parameter) */
#if defined(_HPUX_SOURCE) || defined(__GLIBC__)
#define LDAPTOOL_ICONV_SRC_TYPE	char **		/* HP/UX and glibc (Linux) */
#else
#define LDAPTOOL_ICONV_SRC_TYPE const char **	/* all others */
#endif

#if defined(SOLARIS)
/*
 * On some versions of Solaris, the inbytesleft parameter can't be NULL
 * even in calls to iconv() where inbuf itself is NULL
 */
#define LDAPTOOL_ICONV_NO_NULL_INBYTESLEFT	1
#endif

static char *convert_to_utf8( const char *src_charset, const char *src );
static const char *GetCurrentCharset(void);


/* Version that uses OS functions */
char *
ldaptool_local2UTF8( const char *src, const char *desc )
{
    static const char	*src_charset = NULL;
    char		*utf8;

    if ( src == NULL ) {		/* trivial case # 1 */
	utf8 = NULL;
    } else if ( *src == '\0' ) {	/* trivial case # 2 */
	utf8 = strdup( "" );
    } else {
	/* Determine the source charset if not already done */
	if ( NULL == src_charset ) {
	    if ( NULL != ldaptool_charset
			    && 0 != strcmp( ldaptool_charset, "" )) {
		src_charset = ldaptool_charset;
	    } else {
		src_charset = GetCurrentCharset();
	    }
	}

	if ( NULL != src_charset &&
		( 0 == strcasecmp( LDAPTOOL_CHARSET_UTF8_A, src_charset ) ||
		  0 == strcasecmp( LDAPTOOL_CHARSET_UTF8_B, src_charset ))) {
	    /* no conversion needs to be done */
	    return strdup( src );
	}

	utf8 = convert_to_utf8( src_charset, src );	/* the real deal */

	if ( NULL == utf8 ) {
	    utf8 = strdup( src );	/* fallback: no conversion */
	    fprintf( stderr, "%s: warning: no conversion of %s to "
		    LDAPTOOL_CHARSET_UTF8_OSNAME "\n",
		    ldaptool_progname, desc );
	}
    }

    return utf8;
}

#ifdef _WIN32
/*
 * Try to convert src to a UTF-8.
 * Returns a malloc'd string or NULL upon error (with messages logged).
 * src should not be NULL.
 */
static char *
convert_to_utf8( const char *src_charset, const char *src )
{
    if (NULL != src_charset
	    && 0 != strcasecmp( LDAPTOOL_CHARSET_DEFAULT, src_charset )
	    && 0 != strcasecmp( LDAPTOOL_CHARSET_WINANSI, src_charset )) {
	fprintf( stderr, "%s: conversion from %s to %s is not supported\n",
		    ldaptool_progname, src_charset,
		    LDAPTOOL_CHARSET_UTF8_OSNAME );
	return NULL;
    }

    return win_char_converter( src, FALSE );
}


/* returns a malloc'd string */
static const char *
GetCurrentCharset(void)
{
    /* Our concept of "locale" is very simple on Windows.... */
    return strdup( LDAPTOOL_CHARSET_DEFAULT );
}
#else /* _WIN32 */

/*
 * Try to convert src to a UTF-8.
 * Returns a malloc'd string or NULL upon error (with messages logged).
 * src should not be NULL.
 */
static char *
convert_to_utf8( const char *src_charset, const char *src )
{
    iconv_t		convdesc;
    char		*outbuf, *curoutbuf;
    size_t		inbytesleft, outbytesleft;

#ifdef LDAPTOOL_ICONV_NO_NULL_INBYTESLEFT
#define LDAPTOOL_ICONV_UNUSED_INBYTESLEFT	&inbytesleft
#else
#define LDAPTOOL_ICONV_UNUSED_INBYTESLEFT	NULL
#endif

    /* Get a converter */
    convdesc = iconv_open( LDAPTOOL_CHARSET_UTF8_OSNAME, src_charset );
    if ( (iconv_t)-1 == convdesc ) {
	if ( errno == EINVAL ) {
	    fprintf( stderr, "%s: conversion from %s to %s is not supported\n",
			ldaptool_progname, src_charset,
			LDAPTOOL_CHARSET_UTF8_OSNAME );
	} else {
	    perror( src_charset );
	}
	return NULL;
    }

    /* Allocate room for the UTF-8 equivalent (maximum expansion = 6 times) */
/* XXX is that correct? */
    inbytesleft = strlen( src );
    outbytesleft = 6 * inbytesleft + 1;
    if ( NULL == ( outbuf = (char *)malloc( outbytesleft ))) {
	perror( "convert_to_utf8 - malloc" );
	iconv_close( convdesc );
	return NULL;
    }

    curoutbuf = outbuf;
    /*
     * Three steps for a good conversion:
     * 1) Insert the initial shift sequence if any.
     * 2) Convert our characters.
     * 3) Insert the closing shift sequence, if any.
     */
    if ( (size_t)-1 == iconv( convdesc,
		( LDAPTOOL_ICONV_SRC_TYPE )0, LDAPTOOL_ICONV_UNUSED_INBYTESLEFT,
		&curoutbuf, &outbytesleft )	/* initial shift seq. */
	    || (size_t)-1 == iconv( convdesc,
		( LDAPTOOL_ICONV_SRC_TYPE ) &src, &inbytesleft,
		&curoutbuf, &outbytesleft ) 	/* convert our chars. */
	    || (size_t)-1 == iconv( convdesc,
		( LDAPTOOL_ICONV_SRC_TYPE )0, LDAPTOOL_ICONV_UNUSED_INBYTESLEFT,
		&curoutbuf, &outbytesleft )) {	/* closing shift seq. */
	perror( "convert_to_utf8 - iconv" );
	iconv_close( convdesc );
	free( outbuf );
	return NULL;
    }

    iconv_close( convdesc );
    *curoutbuf = '\0';	/* zero-terminate the resulting string */

    return outbuf;
}


/* returns a malloc'd string */
static const char *
GetCurrentCharset(void)
{
    static char	*locale = NULL;
    const char	*charset;

    if ( NULL == locale ) {
	locale = setlocale(LC_CTYPE, "");	/* need to call this once */
    }

    charset = nl_langinfo( CODESET );
    if ( NULL == charset || '\0' == *charset ) {
	charset = LDAPTOOL_CHARSET_DEFAULT;
    }
    return strdup( charset );
}
#endif /* else _WIN32 */

#ifdef __cplusplus
}
#endif	/* __cplusplus */

#ifdef _WIN32
/* returns a malloc'd string */
static char *
win_char_converter(const char *instr, int bFromUTF8)
{
    char  *outstr = NULL;
    int    inlen, wclen, outlen;
    LPWSTR wcstr;
         
    if (instr == NULL)
            return NULL;

    if ((inlen = strlen(instr)) <= 0)
            return NULL;
 
    /* output never becomes longer than input,   XXXmcs: really true?
    ** thus we don't have to ask for the length
    */
    wcstr = (LPWSTR) malloc( sizeof( WCHAR ) * (inlen+1) );
    if (!wcstr)
        return NULL;
 
    wclen = MultiByteToWideChar(bFromUTF8 ? CP_UTF8 : CP_ACP, 0, instr,
                                 inlen, wcstr, inlen);
    outlen = WideCharToMultiByte(bFromUTF8 ? CP_ACP : CP_UTF8, 0, wcstr,
                                  wclen, NULL, 0, NULL, NULL);
 
    if (outlen > 0) {
        outstr = (char *) malloc(outlen + 2);
        outlen = WideCharToMultiByte(bFromUTF8 ? CP_ACP : CP_UTF8, 0, wcstr,
                                      wclen, outstr, outlen, NULL, NULL);
        if (outlen > 0)
            *(outstr+outlen) = _T('\0');
        else
            return NULL;
    }
    free( wcstr );
    return outstr;
}
#endif /* _WIN32 */

