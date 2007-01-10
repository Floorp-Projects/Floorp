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
 * Copyright (c) 1993, 1994 Regents of the University of Michigan.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and that due credit is given
 * to the University of Michigan at Ann Arbor. The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission. This software
 * is provided ``as is'' without express or implied warranty.
 *
 */
/*
 * searchpref.c:  search preferences library routines for LDAP clients
 */

#include "ldap-int.h"
#include "srchpref.h"

static void free_searchobj( struct ldap_searchobj *so );
static int read_next_searchobj( char **bufp, long *blenp,
	struct ldap_searchobj **sop, int soversion );


static char		*sobjoptions[] = {
    "internal",
    NULL
};


static unsigned long	sobjoptvals[] = {
    LDAP_SEARCHOBJ_OPT_INTERNAL,
};


int
LDAP_CALL
ldap_init_searchprefs( char *file, struct ldap_searchobj **solistp )
{
    FILE	*fp;
    char	*buf;
    long	rlen, len;
    int		rc, eof;

    if (( fp = NSLDAPI_FOPEN( file, "r" )) == NULL ) {
	return( LDAP_SEARCHPREF_ERR_FILE );
    }

    if ( fseek( fp, 0L, SEEK_END ) != 0 ) {	/* move to end to get len */
	fclose( fp );
	return( LDAP_SEARCHPREF_ERR_FILE );
    }

    len = ftell( fp );

    if ( fseek( fp, 0L, SEEK_SET ) != 0 ) {	/* back to start of file */
	fclose( fp );
	return( LDAP_SEARCHPREF_ERR_FILE );
    }

    if (( buf = NSLDAPI_MALLOC( (size_t)len )) == NULL ) {
	fclose( fp );
	return( LDAP_SEARCHPREF_ERR_MEM );
    }

    rlen = fread( buf, 1, (size_t)len, fp );
    eof = feof( fp );
    fclose( fp );

    if ( rlen != len && !eof ) {	/* error:  didn't get the whole file */
	NSLDAPI_FREE( buf );
	return( LDAP_SEARCHPREF_ERR_FILE );
    }

    rc = ldap_init_searchprefs_buf( buf, rlen, solistp );
    NSLDAPI_FREE( buf );

    return( rc );
}


int
LDAP_CALL
ldap_init_searchprefs_buf( char *buf, long buflen,
	struct ldap_searchobj **solistp )
{
    int				rc = 0, version;
    char			**toks;
    struct ldap_searchobj	*prevso, *so;

    *solistp = prevso = NULLSEARCHOBJ;

    if ( nsldapi_next_line_tokens( &buf, &buflen, &toks ) != 2 ||
	    strcasecmp( toks[ 0 ], "version" ) != 0 ) {
	nsldapi_free_strarray( toks );
	return( LDAP_SEARCHPREF_ERR_SYNTAX );
    }
    version = atoi( toks[ 1 ] );
    nsldapi_free_strarray( toks );
    if ( version != LDAP_SEARCHPREF_VERSION &&
	    version != LDAP_SEARCHPREF_VERSION_ZERO ) {
	return( LDAP_SEARCHPREF_ERR_VERSION );
    }

    while ( buflen > 0 && ( rc = read_next_searchobj( &buf, &buflen, &so,
	    version )) == 0 && so != NULLSEARCHOBJ ) {
	if ( prevso == NULLSEARCHOBJ ) {
	    *solistp = so;
	} else {
	    prevso->so_next = so;
	}
	prevso = so;
    }

    if ( rc != 0 ) {
	ldap_free_searchprefs( *solistp );
    }

    return( rc );
}
	    


void
LDAP_CALL
ldap_free_searchprefs( struct ldap_searchobj *solist )
{
    struct ldap_searchobj	*so, *nextso;

    if ( solist != NULL ) {
	for ( so = solist; so != NULL; so = nextso ) {
	    nextso = so->so_next;
	    free_searchobj( so );
	}
    }
    /* XXX XXX need to do some work here */
}


static void
free_searchobj( struct ldap_searchobj *so )
{
    if ( so != NULL ) {
	if ( so->so_objtypeprompt != NULL ) {
	    NSLDAPI_FREE(  so->so_objtypeprompt );
	}
	if ( so->so_prompt != NULL ) {
	    NSLDAPI_FREE(  so->so_prompt );
	}
	if ( so->so_filterprefix != NULL ) {
	    NSLDAPI_FREE(  so->so_filterprefix );
	}
	if ( so->so_filtertag != NULL ) {
	    NSLDAPI_FREE(  so->so_filtertag );
	}
	if ( so->so_defaultselectattr != NULL ) {
	    NSLDAPI_FREE(  so->so_defaultselectattr );
	}
	if ( so->so_defaultselecttext != NULL ) {
	    NSLDAPI_FREE(  so->so_defaultselecttext );
	}
	if ( so->so_salist != NULL ) {
	    struct ldap_searchattr *sa, *nextsa;
	    for ( sa = so->so_salist; sa != NULL; sa = nextsa ) {
		nextsa = sa->sa_next;
		if ( sa->sa_attrlabel != NULL ) {
		    NSLDAPI_FREE( sa->sa_attrlabel );
		}
		if ( sa->sa_attr != NULL ) {
		    NSLDAPI_FREE( sa->sa_attr );
		}
		if ( sa->sa_selectattr != NULL ) {
		    NSLDAPI_FREE( sa->sa_selectattr );
		}
		if ( sa->sa_selecttext != NULL ) {
		    NSLDAPI_FREE( sa->sa_selecttext );
		}
		NSLDAPI_FREE( sa );
	    }
	}
	if ( so->so_smlist != NULL ) {
	    struct ldap_searchmatch *sm, *nextsm;
	    for ( sm = so->so_smlist; sm != NULL; sm = nextsm ) {
		nextsm = sm->sm_next;
		if ( sm->sm_matchprompt != NULL ) {
		    NSLDAPI_FREE( sm->sm_matchprompt );
		}
		if ( sm->sm_filter != NULL ) {
		    NSLDAPI_FREE( sm->sm_filter );
		}
		NSLDAPI_FREE( sm );
	    }
	}
	NSLDAPI_FREE( so );
    }
}



struct ldap_searchobj *
LDAP_CALL
ldap_first_searchobj( struct ldap_searchobj *solist )
{
    return( solist );
}


struct ldap_searchobj *
LDAP_CALL
ldap_next_searchobj( struct ldap_searchobj *solist, struct ldap_searchobj *so )
{
    return( so == NULLSEARCHOBJ ? so : so->so_next );
}



static int
read_next_searchobj( char **bufp, long *blenp, struct ldap_searchobj **sop,
	int soversion )
{
    int				i, j, tokcnt;
    char			**toks;
    struct ldap_searchobj	*so;
    struct ldap_searchattr	**sa;
    struct ldap_searchmatch	**sm;

    *sop = NULL;

    /*
     * Object type prompt comes first
     */
    if (( tokcnt = nsldapi_next_line_tokens( bufp, blenp, &toks )) != 1 ) {
	nsldapi_free_strarray( toks );
	return( tokcnt == 0 ? 0 : LDAP_SEARCHPREF_ERR_SYNTAX );
    }

    if (( so = (struct ldap_searchobj *)NSLDAPI_CALLOC( 1,
	    sizeof( struct ldap_searchobj ))) == NULL ) {
	nsldapi_free_strarray( toks );
	return(  LDAP_SEARCHPREF_ERR_MEM );
    }
    so->so_objtypeprompt = toks[ 0 ];
    NSLDAPI_FREE( (char *)toks );

    /*
     * if this is post-version zero, options come next
     */
    if ( soversion > LDAP_SEARCHPREF_VERSION_ZERO ) {
	if (( tokcnt = nsldapi_next_line_tokens( bufp, blenp, &toks )) < 1 ) {
	    nsldapi_free_strarray( toks );
	    ldap_free_searchprefs( so );
	    return( LDAP_SEARCHPREF_ERR_SYNTAX );
	}
	for ( i = 0; toks[ i ] != NULL; ++i ) {
	    for ( j = 0; sobjoptions[ j ] != NULL; ++j ) {
		if ( strcasecmp( toks[ i ], sobjoptions[ j ] ) == 0 ) {
		    so->so_options |= sobjoptvals[ j ];
		}
	    }
	}
	nsldapi_free_strarray( toks );
    }

    /*
     * "Fewer choices" prompt is next
     */
    if (( tokcnt = nsldapi_next_line_tokens( bufp, blenp, &toks )) != 1 ) {
	nsldapi_free_strarray( toks );
	ldap_free_searchprefs( so );
	return( LDAP_SEARCHPREF_ERR_SYNTAX );
    }
    so->so_prompt = toks[ 0 ];
    NSLDAPI_FREE( (char *)toks );

    /*
     * Filter prefix for "More Choices" searching is next
     */
    if (( tokcnt = nsldapi_next_line_tokens( bufp, blenp, &toks )) != 1 ) {
	nsldapi_free_strarray( toks );
	ldap_free_searchprefs( so );
	return( LDAP_SEARCHPREF_ERR_SYNTAX );
    }
    so->so_filterprefix = toks[ 0 ];
    NSLDAPI_FREE( (char *)toks );

    /*
     * "Fewer Choices" filter tag comes next
     */
    if (( tokcnt = nsldapi_next_line_tokens( bufp, blenp, &toks )) != 1 ) {
	nsldapi_free_strarray( toks );
	ldap_free_searchprefs( so );
	return( LDAP_SEARCHPREF_ERR_SYNTAX );
    }
    so->so_filtertag = toks[ 0 ];
    NSLDAPI_FREE( (char *)toks );

    /*
     * Selection (disambiguation) attribute comes next
     */
    if (( tokcnt = nsldapi_next_line_tokens( bufp, blenp, &toks )) != 1 ) {
	nsldapi_free_strarray( toks );
	ldap_free_searchprefs( so );
	return( LDAP_SEARCHPREF_ERR_SYNTAX );
    }
    so->so_defaultselectattr = toks[ 0 ];
    NSLDAPI_FREE( (char *)toks );

    /*
     * Label for selection (disambiguation) attribute
     */
    if (( tokcnt = nsldapi_next_line_tokens( bufp, blenp, &toks )) != 1 ) {
	nsldapi_free_strarray( toks );
	ldap_free_searchprefs( so );
	return( LDAP_SEARCHPREF_ERR_SYNTAX );
    }
    so->so_defaultselecttext = toks[ 0 ];
    NSLDAPI_FREE( (char *)toks );

    /*
     * Search scope is next
     */
    if (( tokcnt = nsldapi_next_line_tokens( bufp, blenp, &toks )) != 1 ) {
	nsldapi_free_strarray( toks );
	ldap_free_searchprefs( so );
	return( LDAP_SEARCHPREF_ERR_SYNTAX );
    }
    if ( !strcasecmp(toks[ 0 ], "subtree" )) {
	so->so_defaultscope = LDAP_SCOPE_SUBTREE;
    } else if ( !strcasecmp(toks[ 0 ], "onelevel" )) {
	so->so_defaultscope = LDAP_SCOPE_ONELEVEL;
    } else if ( !strcasecmp(toks[ 0 ], "base" )) {
	so->so_defaultscope = LDAP_SCOPE_BASE;
    } else {
	ldap_free_searchprefs( so );
	return( LDAP_SEARCHPREF_ERR_SYNTAX );
    }
    nsldapi_free_strarray( toks );


    /*
     * "More Choices" search option list comes next
     */
    sa = &( so->so_salist );
    while (( tokcnt = nsldapi_next_line_tokens( bufp, blenp, &toks )) > 0 ) {
	if ( tokcnt < 5 ) {
	    nsldapi_free_strarray( toks );
	    ldap_free_searchprefs( so );
	    return( LDAP_SEARCHPREF_ERR_SYNTAX );
	}
	if (( *sa = ( struct ldap_searchattr * )NSLDAPI_CALLOC( 1,
		sizeof( struct ldap_searchattr ))) == NULL ) {
	    nsldapi_free_strarray( toks );
	    ldap_free_searchprefs( so );
	    return(  LDAP_SEARCHPREF_ERR_MEM );
	}
	( *sa )->sa_attrlabel = toks[ 0 ];
	( *sa )->sa_attr = toks[ 1 ];
	( *sa )->sa_selectattr = toks[ 3 ];
	( *sa )->sa_selecttext = toks[ 4 ];
	/* Deal with bitmap */
	( *sa )->sa_matchtypebitmap = 0;
	for ( i = strlen( toks[ 2 ] ) - 1, j = 0; i >= 0; i--, j++ ) {
	    if ( toks[ 2 ][ i ] == '1' ) {
		( *sa )->sa_matchtypebitmap |= (1 << j);
	    }
	}
	NSLDAPI_FREE( toks[ 2 ] );
	NSLDAPI_FREE( ( char * ) toks );
	sa = &(( *sa )->sa_next);
    }
    *sa = NULL;

    /*
     * Match types are last
     */
    sm = &( so->so_smlist );
    while (( tokcnt = nsldapi_next_line_tokens( bufp, blenp, &toks )) > 0 ) {
	if ( tokcnt < 2 ) {
	    nsldapi_free_strarray( toks );
	    ldap_free_searchprefs( so );
	    return( LDAP_SEARCHPREF_ERR_SYNTAX );
	}
	if (( *sm = ( struct ldap_searchmatch * )NSLDAPI_CALLOC( 1,
		sizeof( struct ldap_searchmatch ))) == NULL ) {
	    nsldapi_free_strarray( toks );
	    ldap_free_searchprefs( so );
	    return(  LDAP_SEARCHPREF_ERR_MEM );
	}
	( *sm )->sm_matchprompt = toks[ 0 ];
	( *sm )->sm_filter = toks[ 1 ];
	NSLDAPI_FREE( ( char * ) toks );
	sm = &(( *sm )->sm_next );
    }
    *sm = NULL;

    *sop = so;
    return( 0 );
}
