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
 *  LDAP tools fileurl.c -- functions for handling file URLs.
 *  Used by ldapmodify.
 */

#include "ldaptool.h"
#include "fileurl.h"
#include <ctype.h>	/* for isalpha() */

static int str_starts_with( const char *s, char *prefix );
static void hex_unescape( char *s );
static int unhex( char c );
static void strcpy_escaped_and_convert( char *s1, char *s2 );
static int berval_from_file( const char *path, struct berval *bvp,
	int reporterrs );

/*
 * Convert a file URL to a local path.
 *
 * If successful, LDAPTOOL_FILEURL_SUCCESS is returned and *localpathp is
 * set point to an allocated string.  If not, an different LDAPTOOL_FILEURL_
 * error code is returned.
 *
 * See RFCs 1738 and 2396 for a specification for file URLs... but
 * Netscape Navigator seems to be a bit more lenient in what it will
 * accept, especially on Windows).
 *
 * This function parses file URLs of these three forms:
 *
 *    file:///path
 *    file:/path
 *    file://localhost/path
 *    file://host/path		(rejected with a ...NONLOCAL error)
 *
 * On Windows, we convert leading drive letters of the form C| to C:
 * and if a drive letter is present we strip off the slash that precedes
 * path.  Otherwise, the leading slash is returned.
 *
 */
int
ldaptool_fileurl2path( const char *fileurl, char **localpathp )
{
    const char	*path;
    char	*pathcopy;

    /*
     * Make sure this is a file URL we can handle.
     */
    if ( !str_starts_with( fileurl, "file:" )) {
	return( LDAPTOOL_FILEURL_NOTAFILEURL );
    }

    path = fileurl + 5;		/* skip past "file:" scheme prefix */

    if ( *path != '/' ) {
	return( LDAPTOOL_FILEURL_MISSINGPATH );
    }

    ++path;			/* skip past '/' at end of "file:/" */

    if ( *path == '/' ) {
	++path;			/* remainder is now host/path or /path */
	if ( *path != '/' ) {
	    /*
	     * Make sure it is for the local host.
	     */
	    if ( str_starts_with( path, "localhost/" )) {
		path += 9;
	    } else {
		return( LDAPTOOL_FILEURL_NONLOCAL );
	    }
	}
    } else {		/* URL is of the form file:/path */
	--path;
    }

    /*
     * The remainder is now of the form /path.  On Windows, skip past the
     * leading slash if a drive letter is present.
     */
#ifdef _WINDOWS
    if ( isalpha( path[1] ) && ( path[2] == '|' || path[2] == ':' )) {
	++path;
    }
#endif /* _WINDOWS */

    /*
     * Duplicate the path so we can safely alter it.
     * Unescape any %HH sequences.
     */
    if (( pathcopy = strdup( path )) == NULL ) {
	return( LDAPTOOL_FILEURL_NOMEMORY );
    }
    hex_unescape( pathcopy );

#ifdef _WINDOWS
    /*
     * Convert forward slashes to backslashes for Windows.  Also,
     * if we see a drive letter / vertical bar combination (e.g., c|)
     * at the beginning of the path, replace the '|' with a ':'.
     */
    {
	char	*p;

	for ( p = pathcopy; *p != '\0'; ++p ) {
	    if ( *p == '/' ) {
		*p = '\\';
	    }
	}
    }

    if ( isalpha( pathcopy[0] ) && pathcopy[1] == '|' ) {
	pathcopy[1] = ':';
    }
#endif /* _WINDOWS */

    *localpathp = pathcopy;
    return( LDAPTOOL_FILEURL_SUCCESS );
}


/*
 * Convert a local path to a file URL.
 *
 * If successful, LDAPTOOL_FILEURL_SUCCESS is returned and *urlp is
 * set point to an allocated string.  If not, an different LDAPTOOL_FILEURL_
 * error code is returned.  At present, the only possible error is
 * LDAPTOOL_FILEURL_NOMEMORY.
 *
 * This function produces file URLs of the form file:path.
 *
 * On Windows, we convert leading drive letters to C|.
 *
 */
int
ldaptool_path2fileurl( char *path, char **urlp )
{
    char	*p, *url, *prefix ="file:";

    if ( NULL == path ) {
	path = "/";
    }

    /*
     * Allocate space for the URL, taking into account that path may
     * expand during the hex escaping process.
     */
    if (( url = malloc( strlen( prefix ) + 3 * strlen( path ) + 1 )) == NULL ) {
	return( LDAPTOOL_FILEURL_NOMEMORY );
    }

    strcpy( url, prefix );
    p = url + strlen( prefix );

#ifdef _WINDOWS
    /*
     * On Windows, convert leading drive letters (e.g., C:) to the correct URL
     * syntax (e.g., C|).
     */
    if ( isalpha( path[0] ) && path[1] == ':' ) {
	*p++ = path[0];
	*p++ = '|';
	path += 2;
	*p = '\0';
    }
#endif /* _WINDOWS */

    /*
     * Append the path, encoding any URL-special characters using the %HH
     * convention.
     * On Windows, convert backwards slashes in the path to forward ones.
     */
    strcpy_escaped_and_convert( p, path );

    *urlp = url;
    return( LDAPTOOL_FILEURL_SUCCESS );
}


/*
 * Populate *bvp from "value" of length "vlen."
 *
 * If recognize_url_syntax is non-zero, :<fileurl is recognized.
 * If always_try_file is recognized and no file URL was found, an
 * attempt is made to stat and read the value as if it were the name
 * of a file.
 *
 * If reporterrs is non-zero, specific error messages are printed to
 * stderr.
 *
 * If successful, LDAPTOOL_FILEURL_SUCCESS is returned and bvp->bv_len
 * and bvp->bv_val are set (the latter is set to malloc'd memory).
 * Upon failure, a different LDAPTOOL_FILEURL_ error code is returned.
 */
int
ldaptool_berval_from_ldif_value( const char *value, int vlen,
	struct berval *bvp, int recognize_url_syntax, int always_try_file,
	int reporterrs )
{
    int		rc = LDAPTOOL_FILEURL_SUCCESS;	/* optimistic */
    const char	*url = NULL;
    struct stat	fstats;

    /* recognize "attr :< url" syntax if LDIF version is >= 1 */

    if ( recognize_url_syntax && *value == '<' ) {

	for ( url = value + 1; isspace( *url ); ++url ) {
	    ;	/* NULL */
	}

	if (strlen(url) < 6 || strncasecmp(url, "file:/", 6) != 0) {
	    /*
	     * We only support file:// like URLs for now.
	     */
	    url = NULL;
	}
    }

    if ( NULL != url ) {
	char	*path;

	rc = ldaptool_fileurl2path( url, &path );
	switch( rc ) {
	case LDAPTOOL_FILEURL_NOTAFILEURL:
	    if ( reporterrs ) fprintf( stderr, "%s: unsupported URL \"%s\";"
		    " use a file:// URL instead.\n", ldaptool_progname, url );
	    break;

	case LDAPTOOL_FILEURL_MISSINGPATH:
	    if ( reporterrs ) fprintf( stderr,
		    "%s: unable to process URL \"%s\" --"
		    " missing path.\n", ldaptool_progname, url );
	    break;

	    case LDAPTOOL_FILEURL_NONLOCAL:
		if ( reporterrs ) fprintf( stderr,
			"%s: unable to process URL \"%s\" -- only"
			" local file:// URLs are supported.\n",
			ldaptool_progname, url );
		break;

	    case LDAPTOOL_FILEURL_NOMEMORY:
		if ( reporterrs ) perror( "ldaptool_fileurl2path" );
		break;

	    case LDAPTOOL_FILEURL_SUCCESS:
		if ( stat( path, &fstats ) != 0 ) {
		    if ( reporterrs ) perror( path );
		} else if ( fstats.st_mode & S_IFDIR ) {	
		    if ( reporterrs ) fprintf( stderr,
			    "%s: %s is a directory, not a file\n",
			    ldaptool_progname, path );
		    rc = LDAPTOOL_FILEURL_FILEIOERROR;
		} else {
		    rc = berval_from_file( path, bvp, reporterrs );
		}
		free( path );
		break;

	default:
	    if ( reporterrs ) fprintf( stderr,
		    "%s: unable to process URL \"%s\""
		    " -- unknown error\n", ldaptool_progname, url );
	}
    } else if ( always_try_file && (stat( value, &fstats ) == 0) &&
	    !(fstats.st_mode & S_IFDIR)) {	/* get value from file */
	rc = berval_from_file( value, bvp, reporterrs );
    } else {					/* use value as-is */
	bvp->bv_len = vlen;
	if (( bvp->bv_val = (char *)malloc( vlen + 1 )) == NULL ) {
	    if ( reporterrs ) perror( "malloc" );
	    rc = LDAPTOOL_FILEURL_NOMEMORY;
	} else {
	    SAFEMEMCPY( bvp->bv_val, value, vlen );
	    bvp->bv_val[ vlen ] = '\0';
	}
    }

    return( rc );
}


/*
 * Map an LDAPTOOL_FILEURL_ error code to an LDAP error code (crude).
 */
int
ldaptool_fileurlerr2ldaperr( int lderr )
{
    int		rc;

    switch( lderr ) {
    case LDAPTOOL_FILEURL_SUCCESS:
	rc = LDAP_SUCCESS;
	break;
    case LDAPTOOL_FILEURL_NOMEMORY:
	rc = LDAP_NO_MEMORY;
	break;
    default:
	rc = LDAP_PARAM_ERROR;
    }

    return( rc );
} 


/*
 * Populate *bvp with the contents of the file named by "path".
 *
 * If reporterrs is non-zero, specific error messages are printed to
 * stderr.
 *
 * If successful, LDAPTOOL_FILEURL_SUCCESS is returned and bvp->bv_len
 * and bvp->bv_val are set (the latter is set to malloc'd memory).
 * Upon failure, a different LDAPTOOL_FILEURL_ error code is returned.
 */

static int
berval_from_file( const char *path, struct berval *bvp, int reporterrs )
{
    FILE	*fp;
    long	rlen;
#if defined( XP_WIN32 )
    char	mode[20] = "r+b";
#else
    char	mode[20] = "r";
#endif

    if (( fp = ldaptool_open_file( path, mode )) == NULL ) {
	if ( reporterrs ) perror( path );
	return( LDAPTOOL_FILEURL_FILEIOERROR );
    }

    if ( fseek( fp, 0L, SEEK_END ) != 0 ) {
	if ( reporterrs ) perror( path );
	fclose( fp );
	return( LDAPTOOL_FILEURL_FILEIOERROR );
    }

    bvp->bv_len = ftell( fp );

    if (( bvp->bv_val = (char *)malloc( bvp->bv_len + 1 )) == NULL ) {
	if ( reporterrs ) perror( "malloc" );
	fclose( fp );
	return( LDAPTOOL_FILEURL_NOMEMORY );
    }

    if ( fseek( fp, 0L, SEEK_SET ) != 0 ) {
	if ( reporterrs ) perror( path );
	fclose( fp );
	return( LDAPTOOL_FILEURL_FILEIOERROR );
    }

    rlen = fread( bvp->bv_val, 1, bvp->bv_len, fp );
    fclose( fp );

    if ( rlen != (long)bvp->bv_len ) {
	if ( reporterrs ) perror( path );
	free( bvp->bv_val );
	return( LDAPTOOL_FILEURL_FILEIOERROR );
    }

    bvp->bv_val[ bvp->bv_len ] = '\0';
    return( LDAPTOOL_FILEURL_SUCCESS );
}


/*
 * Return a non-zero value if the string s begins with prefix and zero if not.
 */
static int
str_starts_with( const char *s, char *prefix )
{
    size_t	prefix_len;

    if ( s == NULL || prefix == NULL ) {
	return( 0 );
    }

    prefix_len = strlen( prefix );
    if ( strlen( s ) < prefix_len ) {
	return( 0 );
    }

    return( strncmp( s, prefix, prefix_len ) == 0 );
}


/*
 * Remove URL hex escapes from s... done in place.  The basic concept for
 * this routine is borrowed from the WWW library HTUnEscape() routine.
 *
 * A similar function called nsldapi_hex_unescape can be found in
 * ../../libraries/libldap/unescape.c
 */
static void
hex_unescape( char *s )
{
	char	*p;

	for ( p = s; *s != '\0'; ++s ) {
		if ( *s == '%' ) {
			if ( *++s != '\0' ) {
				*p = unhex( *s ) << 4;
			}
			if ( *++s != '\0' ) {
				*p++ += unhex( *s );
			}
		} else {
			*p++ = *s;
		}
	}

	*p = '\0';
}


/*
 * Return the integer equivalent of one hex digit (in c).
 *
 * A similar function can be found in ../../libraries/libldap/unescape.c
 */
static int
unhex( char c )
{
	return( c >= '0' && c <= '9' ? c - '0'
	    : c >= 'A' && c <= 'F' ? c - 'A' + 10
	    : c - 'a' + 10 );
}


#define HREF_CHAR_ACCEPTABLE( c )	(( c >= '-' && c <= '9' ) ||	\
					 ( c >= '@' && c <= 'Z' ) ||	\
					 ( c == '_' ) ||		\
					 ( c >= 'a' && c <= 'z' ))

/*
 * Like strcat(), except if any URL-special characters are found in s2
 * they are escaped using the %HH convention and backslash characters are
 * converted to forward slashes on Windows.
 *
 * Maximum space needed in s1 is 3 * strlen( s2 ) + 1.
 *
 * A similar function that does not convert the slashes called
 * strcat_escaped() can be found in ../../libraries/libldap/tmplout.c
 */
static void
strcpy_escaped_and_convert( char *s1, char *s2 )
{
    char	*p, *q;
    char	*hexdig = "0123456789ABCDEF";

    p = s1 + strlen( s1 );
    for ( q = s2; *q != '\0'; ++q ) {
#ifdef _WINDOWS
	if ( *q == '\\' ) {
                *p++ = '/';
	} else
#endif /* _WINDOWS */

	if ( HREF_CHAR_ACCEPTABLE( *q )) {
	    *p++ = *q;
	} else {
	    *p++ = '%';
	    *p++ = hexdig[ 0x0F & ((*(unsigned char*)q) >> 4) ];
	    *p++ = hexdig[ 0x0F & *q ];
	}
    }

    *p = '\0';
}
