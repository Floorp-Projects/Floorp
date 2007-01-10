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
 *  Copyright (c) 1993 Regents of the University of Michigan.
 *  All rights reserved.
 */
/*
 *  getfilter.c -- optional add-on to libldap
 */

#if 0
#ifndef lint 
static char copyright[] = "@(#) Copyright (c) 1993 Regents of the University of Michigan.\nAll rights reserved.\n";
#endif
#endif

#include "ldap-int.h"
#include "regex.h"

static int break_into_words( char *str, char *delims, char ***wordsp );

#if !defined( macintosh ) && !defined( DOS )
extern char	* LDAP_CALL re_comp();
#endif

#define FILT_MAX_LINE_LEN	1024

LDAPFiltDesc *
LDAP_CALL
ldap_init_getfilter( char *fname )
{
    FILE		*fp;
    char		*buf;
    long		rlen, len;
    int 		eof;
    LDAPFiltDesc	*lfdp;

    if (( fp = NSLDAPI_FOPEN( fname, "r" )) == NULL ) {
	return( NULL );
    }

    if ( fseek( fp, 0L, SEEK_END ) != 0 ) {	/* move to end to get len */
	fclose( fp );
	return( NULL );
    }

    len = ftell( fp );

    if ( fseek( fp, 0L, SEEK_SET ) != 0 ) {	/* back to start of file */
	fclose( fp );
	return( NULL );
    }

    if (( buf = NSLDAPI_MALLOC( (size_t)len )) == NULL ) {
	fclose( fp );
	return( NULL );
    }

    rlen = fread( buf, 1, (size_t)len, fp );
    eof = feof( fp );
    fclose( fp );

    if ( rlen != len && !eof ) {	/* error:  didn't get the whole file */
	NSLDAPI_FREE( buf );
	return( NULL );
    }


    lfdp = ldap_init_getfilter_buf( buf, rlen );
    NSLDAPI_FREE( buf );

    return( lfdp );
}


LDAPFiltDesc *
LDAP_CALL
ldap_init_getfilter_buf( char *buf, long buflen )
{
    LDAPFiltDesc	*lfdp;
    LDAPFiltList	*flp, *nextflp;
    LDAPFiltInfo	*fip, *nextfip;
    char		*errmsg, *tag, **tok;
    int			tokcnt, i;

    if ( (buf == NULL) || (buflen < 0) ||
	 ( lfdp = (LDAPFiltDesc *)NSLDAPI_CALLOC(1, sizeof( LDAPFiltDesc)))
	 == NULL ) {
	return( NULL );
    }

    flp = nextflp = NULL;
    fip = NULL;
    tag = NULL;

    while ( buflen > 0 && ( tokcnt = nsldapi_next_line_tokens( &buf, &buflen,
	    &tok )) > 0 ) {
	switch( tokcnt ) {
	case 1:		/* tag line */
	    if ( tag != NULL ) {
		NSLDAPI_FREE( tag );
	    }
	    tag = tok[ 0 ];
	    NSLDAPI_FREE( tok );
	    break;
	case 4:
	case 5:		/* start of filter info. list */
	    if (( nextflp = (LDAPFiltList *)NSLDAPI_CALLOC( 1,
		    sizeof( LDAPFiltList ))) == NULL ) {
		ldap_getfilter_free( lfdp );
		return( NULL );
	    }
	    nextflp->lfl_tag = nsldapi_strdup( tag );
	    nextflp->lfl_pattern = tok[ 0 ];
	    if (( errmsg = re_comp( nextflp->lfl_pattern )) != NULL ) {
		char    msg[512];
		ldap_getfilter_free( lfdp );
#ifdef HAVE_SNPRINTF
		snprintf( msg, sizeof(msg),
#else
		sprintf( msg,
#endif
			"bad regular expression \"%s\" - %s\n",
			nextflp->lfl_pattern, errmsg );
		ber_err_print( msg );
		nsldapi_free_strarray( tok );
		return( NULL );
	    }
		
	    nextflp->lfl_delims = tok[ 1 ];
	    nextflp->lfl_ilist = NULL;
	    nextflp->lfl_next = NULL;
	    if ( flp == NULL ) {	/* first one */
		lfdp->lfd_filtlist = nextflp;
	    } else {
		flp->lfl_next = nextflp;
	    }
	    flp = nextflp;
	    fip = NULL;
	    for ( i = 2; i < 5; ++i ) {
		tok[ i - 2 ] = tok[ i ];
	    }
	    /* fall through */

	case 2:
	case 3:		/* filter, desc, and optional search scope */
	    if ( nextflp != NULL ) { /* add to info list */
		if (( nextfip = (LDAPFiltInfo *)NSLDAPI_CALLOC( 1,
			sizeof( LDAPFiltInfo ))) == NULL ) {
		    ldap_getfilter_free( lfdp );
		    nsldapi_free_strarray( tok );
		    return( NULL );
		}
		if ( fip == NULL ) {	/* first one */
		    nextflp->lfl_ilist = nextfip;
		} else {
		    fip->lfi_next = nextfip;
		}
		fip = nextfip;
		nextfip->lfi_next = NULL;
		nextfip->lfi_filter = tok[ 0 ];
		nextfip->lfi_desc = tok[ 1 ];
		if ( tok[ 2 ] != NULL ) {
		    if ( strcasecmp( tok[ 2 ], "subtree" ) == 0 ) {
			nextfip->lfi_scope = LDAP_SCOPE_SUBTREE;
		    } else if ( strcasecmp( tok[ 2 ], "onelevel" ) == 0 ) {
			nextfip->lfi_scope = LDAP_SCOPE_ONELEVEL;
		    } else if ( strcasecmp( tok[ 2 ], "base" ) == 0 ) {
			nextfip->lfi_scope = LDAP_SCOPE_BASE;
		    } else {
			nsldapi_free_strarray( tok );
			ldap_getfilter_free( lfdp );
			return( NULL );
		    }
		    NSLDAPI_FREE( tok[ 2 ] );
		    tok[ 2 ] = NULL;
		} else {
		    nextfip->lfi_scope = LDAP_SCOPE_SUBTREE;	/* default */
		}
		nextfip->lfi_isexact = ( strchr( tok[ 0 ], '*' ) == NULL &&
			strchr( tok[ 0 ], '~' ) == NULL );
		NSLDAPI_FREE( tok );
	    }
	    break;

	default:
	    nsldapi_free_strarray( tok );
	    ldap_getfilter_free( lfdp );
	    return( NULL );
	}
    }

    if ( tag != NULL ) {
	NSLDAPI_FREE( tag );
    }

    return( lfdp );
}


int
LDAP_CALL
ldap_set_filter_additions( LDAPFiltDesc *lfdp, char *prefix, char *suffix )
{
    if ( lfdp == NULL ) {
	return( LDAP_PARAM_ERROR );
    }

    if ( lfdp->lfd_filtprefix != NULL ) {
	NSLDAPI_FREE( lfdp->lfd_filtprefix );
    }
    lfdp->lfd_filtprefix = ( prefix == NULL ) ? NULL : nsldapi_strdup( prefix );

    if ( lfdp->lfd_filtsuffix != NULL ) {
	NSLDAPI_FREE( lfdp->lfd_filtsuffix );
    }
    lfdp->lfd_filtsuffix = ( suffix == NULL ) ? NULL : nsldapi_strdup( suffix );

    return( LDAP_SUCCESS );
}


/*
 * ldap_setfilteraffixes() is deprecated -- use ldap_set_filter_additions()
 */
void
LDAP_CALL
ldap_setfilteraffixes( LDAPFiltDesc *lfdp, char *prefix, char *suffix )
{
    (void)ldap_set_filter_additions( lfdp, prefix, suffix );
}


LDAPFiltInfo *
LDAP_CALL
ldap_getfirstfilter( LDAPFiltDesc *lfdp, char *tagpat, char *value )
{
    LDAPFiltList	*flp;

    if ( lfdp == NULL || tagpat == NULL || value == NULL ) {
	return( NULL );	/* punt */
    }

    if ( lfdp->lfd_curvalcopy != NULL ) {
	NSLDAPI_FREE( lfdp->lfd_curvalcopy );
	NSLDAPI_FREE( lfdp->lfd_curvalwords );
    }

    NSLDAPI_FREE(lfdp->lfd_curval);
    if ((lfdp->lfd_curval = nsldapi_strdup(value)) == NULL) {
	return( NULL );
    }

    lfdp->lfd_curfip = NULL;

    for ( flp = lfdp->lfd_filtlist; flp != NULL; flp = flp->lfl_next ) {
	if ( re_comp( tagpat ) == NULL && re_exec( flp->lfl_tag ) == 1
		&& re_comp( flp->lfl_pattern ) == NULL
		&& re_exec( lfdp->lfd_curval ) == 1 ) {
	    lfdp->lfd_curfip = flp->lfl_ilist;
	    break;
	}
    }

    if ( lfdp->lfd_curfip == NULL ) {
	return( NULL );
    }

    if (( lfdp->lfd_curvalcopy = nsldapi_strdup( value )) == NULL ) {
	return( NULL );
    }

    if ( break_into_words( lfdp->lfd_curvalcopy, flp->lfl_delims,
		&lfdp->lfd_curvalwords ) < 0 ) {
	NSLDAPI_FREE( lfdp->lfd_curvalcopy );
	lfdp->lfd_curvalcopy = NULL;
	return( NULL );
    }

    return( ldap_getnextfilter( lfdp ));
}


LDAPFiltInfo *
LDAP_CALL
ldap_getnextfilter( LDAPFiltDesc *lfdp )
{
    LDAPFiltInfo	*fip;

    if ( lfdp == NULL || ( fip = lfdp->lfd_curfip ) == NULL ) {
	return( NULL );
    }

    lfdp->lfd_curfip = fip->lfi_next;

    ldap_build_filter( lfdp->lfd_filter, LDAP_FILT_MAXSIZ, fip->lfi_filter,
	    lfdp->lfd_filtprefix, lfdp->lfd_filtsuffix, NULL,
	    lfdp->lfd_curval, lfdp->lfd_curvalwords );
    lfdp->lfd_retfi.lfi_filter = lfdp->lfd_filter;
    lfdp->lfd_retfi.lfi_desc = fip->lfi_desc;
    lfdp->lfd_retfi.lfi_scope = fip->lfi_scope;
    lfdp->lfd_retfi.lfi_isexact = fip->lfi_isexact;

    return( &lfdp->lfd_retfi );
}


static char*
filter_add_strn( char *f, char *flimit, char *v, size_t vlen )
     /* Copy v into f.  If flimit is too small, return NULL;
      * otherwise return (f + vlen).
      */
{
    auto size_t flen = flimit - f;
    if ( vlen > flen ) { /* flimit is too small */
	if ( flen > 0 ) SAFEMEMCPY( f, v, flen );
	return NULL;
    }
    if ( vlen > 0 ) SAFEMEMCPY( f, v, vlen );
    return f + vlen;
}

static char*
filter_add_value( char *f, char *flimit, char *v, int escape_all )
     /* Copy v into f, but with parentheses escaped.  But only escape * and \
      * if escape_all is non-zero so that either "*" or "\2a" can be used in
      * v, with different meanings.
      * If flimit is too small, return NULL; otherwise
      * return (f + the number of bytes copied).
      */
{
    auto char x[4];
    auto size_t slen;
    while ( f && *v ) {
	switch ( *v ) {
	case '*':
	    if ( escape_all ) {
		f = filter_add_strn( f, flimit, "\\2a", 3 );
		v++;
	    } else {
		if ( f < flimit ) {
		    *f++ = *v++;
		} else {
		    f = NULL; /* overflow */
		}
	    }
	    break;

	case '(':
	case ')':
	    sprintf( x, "\\%02x", (unsigned)*v );
	    f = filter_add_strn( f, flimit, x, 3 );
	    v++;
	    break;

	case '\\':
	    if ( escape_all ) {
		f = filter_add_strn( f, flimit, "\\5c", 3 );
		v++;
	    } else {
		slen = (ldap_utf8isxdigit( v+1 ) &&
			ldap_utf8isxdigit( v+2 )) ? 3 : (v[1] ? 2 : 1);
		f = filter_add_strn( f, flimit, v, slen );
		v += slen;
	    }
	    break;
	    
	default:
	    if ( f < flimit ) {
		*f++ = *v++;
	    } else {
		f = NULL; /* overflow */
	    }
	    break;
	}
    }
    return f;
}

int
LDAP_CALL
ldap_create_filter( char *filtbuf, unsigned long buflen, char *pattern,
	char *prefix, char *suffix, char *attr, char *value, char **valwords )
{
	char	*p, *f, *flimit;
	int	i, wordcount, wordnum, endwordnum, escape_all;

    /* 
     * there is some confusion on what to create for a filter if 
     * attr or value are null pointers.  For now we just leave them
     * as TO BE DEALT with
     */

	if ( filtbuf == NULL || buflen == 0 || pattern == NULL ){
		return( LDAP_PARAM_ERROR );
	}
	
	if ( valwords == NULL ) {
	    wordcount = 0;
	} else {
	    for ( wordcount = 0; valwords[ wordcount ] != NULL; ++wordcount ) {
		;
	    }
	}

	f = filtbuf;
	flimit = filtbuf + buflen - 1;

	if ( prefix != NULL ) {
	    f = filter_add_strn( f, flimit, prefix, strlen( prefix ));
	}

	for ( p = pattern; f != NULL && *p != '\0'; ++p ) {
	    if ( *p == '%' ) {
		++p;
		if ( *p == 'v' || *p == 'e' ) {
		    escape_all = ( *p == 'e' );
		    if ( ldap_utf8isdigit( p+1 )) {
			++p;
			wordnum = *p - '1';
			if ( *(p+1) == '-' ) {
			    ++p;
			    if ( ldap_utf8isdigit( p+1 )) {
				++p;
				endwordnum = *p - '1';	/* e.g., "%v2-4" */
				if ( endwordnum > wordcount - 1 ) {
				    endwordnum = wordcount - 1;
				}
			    } else {
				endwordnum = wordcount - 1;  /* e.g., "%v2-" */
			    }
			} else {
			    endwordnum = wordnum;	/* e.g., "%v2" */
			}

			if ( wordcount > 0 ) {
			    for ( i = wordnum; i <= endwordnum; ++i ) {
				if ( i > wordnum ) {  /* add blank btw words */
				    f = filter_add_strn( f, flimit, " ", 1 );
				    if ( f == NULL ) break;
				}
				f = filter_add_value( f, flimit, valwords[ i ],
					escape_all );
				if ( f == NULL ) break;
			    }
			}
		    } else if ( *(p+1) == '$' ) {
			++p;
			if ( wordcount > 0 ) {
			    wordnum = wordcount - 1;
			    f = filter_add_value( f, flimit,
				    valwords[ wordnum ], escape_all );
			}
		    } else if ( value != NULL ) {
			f = filter_add_value( f, flimit, value, escape_all );
		    }
		} else if ( *p == 'a' && attr != NULL ) {
		    f = filter_add_strn( f, flimit, attr, strlen( attr ));
		} else {
		    *f++ = *p;
		}
	    } else {
		*f++ = *p;
	    }
	    if ( f > flimit ) { /* overflow */
		f = NULL;
	    }
	}

	if ( suffix != NULL && f != NULL) {
	    f = filter_add_strn( f, flimit, suffix, strlen( suffix ));
	}

	if ( f == NULL ) {
	    *flimit = '\0';
	    return( LDAP_SIZELIMIT_EXCEEDED );
	}
	*f = '\0';
	return( LDAP_SUCCESS );
}


/*
 * ldap_build_filter() is deprecated -- use ldap_create_filter() instead
 */
void
LDAP_CALL
ldap_build_filter( char *filtbuf, unsigned long buflen, char *pattern,
	char *prefix, char *suffix, char *attr, char *value, char **valwords )
{
    (void)ldap_create_filter( filtbuf, buflen, pattern, prefix, suffix, attr,
	    value, valwords );
}


static int
break_into_words( char *str, char *delims, char ***wordsp )
{
    char	*word, **words;
    int		count;
    char	*lasts;
	
    if (( words = (char **)NSLDAPI_CALLOC( 1, sizeof( char * ))) == NULL ) {
	return( -1 );
    }
    count = 0;
    words[ count ] = NULL;

    word = ldap_utf8strtok_r( str, delims, &lasts );
    while ( word != NULL ) {
	if (( words = (char **)NSLDAPI_REALLOC( words,
		( count + 2 ) * sizeof( char * ))) == NULL ) {
	    return( -1 );
	}

	words[ count ] = word;
	words[ ++count ] = NULL;
	word = ldap_utf8strtok_r( NULL, delims, &lasts );
    }
	
    *wordsp = words;
    return( count );
}
