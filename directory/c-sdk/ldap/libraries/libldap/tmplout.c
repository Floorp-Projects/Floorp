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
 * tmplout.c:  display template library output routines for LDAP clients
 * 
 */

#include "ldap-int.h"
#include "disptmpl.h"

#if defined(_WINDOWS) || defined(aix) || defined(SCOOS) || defined(OSF1) || defined(SOLARIS)
#include <time.h> /* for struct tm and ctime */
#endif


/* This is totally lame, since it should be coming from time.h, but isn't. */
#if defined(SOLARIS) 
char *ctime_r(const time_t *, char *, int);
#endif

static int do_entry2text( LDAP *ld, char *buf, char *base, LDAPMessage *entry,
	struct ldap_disptmpl *tmpl, char **defattrs, char ***defvals,
	writeptype writeproc, void *writeparm, char *eol, int rdncount,
	unsigned long opts, char *urlprefix );
static int do_entry2text_search( LDAP *ld, char *dn, char *base,
	LDAPMessage *entry, struct ldap_disptmpl *tmpllist, char **defattrs,
	char ***defvals, writeptype writeproc, void *writeparm, char *eol,
	int rdncount, unsigned long opts, char *urlprefix );
static int do_vals2text( LDAP *ld, char *buf, char **vals, char *label,
	int labelwidth, unsigned long syntaxid, writeptype writeproc,
	void *writeparm, char *eol, int rdncount, char *urlprefix );
static int max_label_len( struct ldap_disptmpl *tmpl );
static int output_label( char *buf, char *label, int width,
	writeptype writeproc, void *writeparm, char *eol, int html );
static int output_dn( char *buf, char *dn, int width, int rdncount,
	writeptype writeproc, void *writeparm, char *eol, char *urlprefix );
static void strcat_escaped( char *s1, char *s2 );
static char *time2text( char *ldtimestr, int dateonly );
static long gtime( struct tm *tm );
static int searchaction( LDAP *ld, char *buf, char *base, LDAPMessage *entry,
	char *dn, struct ldap_tmplitem *tip, int labelwidth, int rdncount,
	writeptype writeproc, void *writeparm, char *eol, char *urlprefix );

#define DEF_LABEL_WIDTH		15
#define SEARCH_TIMEOUT_SECS	120
#define OCATTRNAME		"objectClass"


#define NONFATAL_LDAP_ERR( err )	( err == LDAP_SUCCESS || \
	err == LDAP_TIMELIMIT_EXCEEDED || err == LDAP_SIZELIMIT_EXCEEDED )

#define DEF_LDAP_URL_PREFIX	"ldap:///"

 
int
LDAP_CALL
ldap_entry2text(
	LDAP			*ld,
	char			*buf,		/* NULL for "use internal" */
	LDAPMessage		*entry,
	struct ldap_disptmpl	*tmpl,
	char			**defattrs,
	char			***defvals,
	writeptype		writeproc,
	void			*writeparm,
	char			*eol,
	int			rdncount,
	unsigned long		opts
)
{
    LDAPDebug( LDAP_DEBUG_TRACE, "ldap_entry2text\n", 0, 0, 0 );

    return( do_entry2text( ld, buf, NULL, entry, tmpl, defattrs, defvals,
		writeproc, writeparm, eol, rdncount, opts, NULL ));

}



int
LDAP_CALL
ldap_entry2html(
	LDAP			*ld,
	char			*buf,		/* NULL for "use internal" */
	LDAPMessage		*entry,
	struct ldap_disptmpl	*tmpl,
	char			**defattrs,
	char			***defvals,
	writeptype		writeproc,
	void			*writeparm,
	char			*eol,
	int			rdncount,
	unsigned long		opts,
	char			*base,
	char			*urlprefix
)
{
    LDAPDebug( LDAP_DEBUG_TRACE, "ldap_entry2html\n", 0, 0, 0 );

    if ( urlprefix == NULL ) {
	urlprefix = DEF_LDAP_URL_PREFIX;
    }

    return( do_entry2text( ld, buf, base, entry, tmpl, defattrs, defvals,
		writeproc, writeparm, eol, rdncount, opts, urlprefix ));
}


static int
do_entry2text(
	LDAP			*ld,
	char			*buf,		/* NULL for use-internal */
	char			*base,		/* used for search actions */
	LDAPMessage		*entry,
	struct ldap_disptmpl	*tmpl,
	char			**defattrs,
	char			***defvals,
	writeptype		writeproc,
	void			*writeparm,
	char			*eol,
	int			rdncount,
	unsigned long		opts,
	char			*urlprefix	/* if non-NULL, do HTML */
)
{
    int				i, err, html, show, labelwidth;
    int				freebuf,  freevals;
    char			*dn, **vals;
    struct ldap_tmplitem	*rowp, *colp;

    if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
	return( LDAP_PARAM_ERROR );
    }

    if ( writeproc == NULL ||
	    !NSLDAPI_VALID_LDAPMESSAGE_ENTRY_POINTER( entry )) {
	err = LDAP_PARAM_ERROR;
	LDAP_SET_LDERRNO( ld, err, NULL, NULL );
	return( err );
    }

    if (( dn = ldap_get_dn( ld, entry )) == NULL ) {
	return( LDAP_GET_LDERRNO( ld, NULL, NULL ) );
    }

    if ( buf == NULL ) {
	if (( buf = NSLDAPI_MALLOC( LDAP_DTMPL_BUFSIZ )) == NULL ) {
	    err = LDAP_NO_MEMORY;
	    LDAP_SET_LDERRNO( ld, err, NULL, NULL );
	    NSLDAPI_FREE( dn );
	    return( err );
	}
	freebuf = 1;
    } else {
	freebuf = 0;
    }

    html = ( urlprefix != NULL );

    if ( html ) {
	/*
	 * add HTML intro. and title
	 */
	if (!(( opts & LDAP_DISP_OPT_HTMLBODYONLY ) != 0 )) {
	    sprintf( buf, "<HTML>%s<HEAD>%s<TITLE>%s%s - ", eol, eol, eol,
		    ( tmpl == NULL ) ? "Entry" : tmpl->dt_name );
	    (*writeproc)( writeparm, buf, strlen( buf ));
	    output_dn( buf, dn, 0, rdncount, writeproc, writeparm, "", NULL );
	    sprintf( buf, "%s</TITLE>%s</HEAD>%s<BODY>%s<H3>%s - ", eol, eol,
		    eol, eol, ( tmpl == NULL ) ? "Entry" : tmpl->dt_name );
	    (*writeproc)( writeparm, buf, strlen( buf ));
	    output_dn( buf, dn, 0, rdncount, writeproc, writeparm, "", NULL );
	    sprintf( buf, "</H3>%s", eol );
	    (*writeproc)( writeparm, buf, strlen( buf ));
	}

	if (( opts & LDAP_DISP_OPT_NONLEAF ) != 0 &&
		( vals = ldap_explode_dn( dn, 0 )) != NULL ) {
	    char	*untagged;

	    /*
	     * add "Move Up" link
	     */
	    sprintf( buf, "<A HREF=\"%s", urlprefix );
	    for ( i = 1; vals[ i ] != NULL; ++i ) {
		if ( i > 1 ) {
		     strcat_escaped( buf, ", " );
		}
		strcat_escaped( buf, vals[ i ] );
	    }
	    if ( vals[ 1 ] != NULL ) {
		untagged = strchr( vals[ 1 ], '=' );
	    } else {
		untagged = "=The World";
	    }
	    sprintf( buf + strlen( buf ),
		    "%s\">Move Up To <EM>%s</EM></A>%s<BR>",
		    ( vals[ 1 ] == NULL ) ? "??one" : "",
		    ( untagged != NULL ) ? untagged + 1 : vals[ 1 ], eol );
	    (*writeproc)( writeparm, buf, strlen( buf ));

	    /*
	     * add "Browse" link
	     */
	    untagged = strchr( vals[ 0 ], '=' );
	    sprintf( buf, "<A HREF=\"%s", urlprefix );
	    strcat_escaped( buf, dn );
	    sprintf( buf + strlen( buf ), "??one?(!(objectClass=dsa))\">Browse Below <EM>%s</EM></A>%s%s",
		    ( untagged != NULL ) ? untagged + 1 : vals[ 0 ], eol, eol );
	    (*writeproc)( writeparm, buf, strlen( buf ));

	    ldap_value_free( vals );
	}

	(*writeproc)( writeparm, "<HR>", 4 );	/* horizontal rule */
    } else {
	(*writeproc)( writeparm, "\"", 1 );
	output_dn( buf, dn, 0, rdncount, writeproc, writeparm, "", NULL );
	sprintf( buf, "\"%s", eol );
	(*writeproc)( writeparm, buf, strlen( buf ));
    }

    if ( tmpl != NULL && ( opts & LDAP_DISP_OPT_AUTOLABELWIDTH ) != 0 ) {
	labelwidth = max_label_len( tmpl ) + 3;
    } else {
	labelwidth = DEF_LABEL_WIDTH;;
    }

    err = LDAP_SUCCESS;

    if ( tmpl == NULL ) {
	BerElement	*ber;
	char		*attr;

	ber = NULL;
	for ( attr = ldap_first_attribute( ld, entry, &ber );
		NONFATAL_LDAP_ERR( err ) && attr != NULL;
		attr = ldap_next_attribute( ld, entry, ber )) {
	    if (( vals = ldap_get_values( ld, entry, attr )) == NULL ) {
		freevals = 0;
		if ( defattrs != NULL ) {
		    for ( i = 0; defattrs[ i ] != NULL; ++i ) {
			if ( strcasecmp( attr, defattrs[ i ] ) == 0 ) {
			    break;
			}
		    }
		    if ( defattrs[ i ] != NULL ) {
			vals = defvals[ i ];
		    }
		}
	    } else {
		freevals = 1;
	    }

	    if ( islower( *attr )) {	/* cosmetic -- upcase attr. name */
		*attr = toupper( *attr );
	    }

	    err = do_vals2text( ld, buf, vals, attr, labelwidth,
		    LDAP_SYN_CASEIGNORESTR, writeproc, writeparm, eol, 
		    rdncount, urlprefix );
	    if ( freevals ) {
		ldap_value_free( vals );
	    }
	}
	if ( ber == NULL ) {
	    ber_free( ber, 0 );
	}
	/*
	 * XXX check for errors in ldap_first_attribute/ldap_next_attribute
	 * here (but what should we do if there was one?)
	 */

    } else {
	for ( rowp = ldap_first_tmplrow( tmpl );
		NONFATAL_LDAP_ERR( err ) && rowp != NULLTMPLITEM;
		rowp = ldap_next_tmplrow( tmpl, rowp )) {
	    for ( colp = ldap_first_tmplcol( tmpl, rowp ); colp != NULLTMPLITEM;
		    colp = ldap_next_tmplcol( tmpl, rowp, colp )) {
		vals = NULL;
		if ( colp->ti_attrname == NULL || ( vals = ldap_get_values( ld,
			entry, colp->ti_attrname )) == NULL ) {
		    freevals = 0;
		    if ( !LDAP_IS_TMPLITEM_OPTION_SET( colp,
			    LDAP_DITEM_OPT_HIDEIFEMPTY ) && defattrs != NULL
			    && colp->ti_attrname != NULL ) {
			for ( i = 0; defattrs[ i ] != NULL; ++i ) {
			    if ( strcasecmp( colp->ti_attrname, defattrs[ i ] )
				    == 0 ) {
				break;
			    }
			}
			if ( defattrs[ i ] != NULL ) {
			    vals = defvals[ i ];
			}
		    }
		} else {
		    freevals = 1;
		    if ( LDAP_IS_TMPLITEM_OPTION_SET( colp,
			    LDAP_DITEM_OPT_SORTVALUES ) && vals[ 0 ] != NULL
			    && vals[ 1 ] != NULL ) {
			ldap_sort_values(ld, vals, ldap_sort_strcasecmp);
		    }
		}

		/*
		 * don't bother even calling do_vals2text() if no values
		 * or boolean with value false and "hide if false" option set
		 */
		show = ( vals != NULL && vals[ 0 ] != NULL );
		if ( show && LDAP_GET_SYN_TYPE( colp->ti_syntaxid )
			== LDAP_SYN_TYPE_BOOLEAN && LDAP_IS_TMPLITEM_OPTION_SET(
			colp, LDAP_DITEM_OPT_HIDEIFFALSE ) &&
			toupper( vals[ 0 ][ 0 ] ) != 'T' ) {
		    show = 0;
		}

		if ( colp->ti_syntaxid == LDAP_SYN_SEARCHACTION ) {
		    if (( opts & LDAP_DISP_OPT_DOSEARCHACTIONS ) != 0 ) {
			if ( colp->ti_attrname == NULL || ( show &&
				toupper( vals[ 0 ][ 0 ] ) == 'T' )) {
			    err = searchaction( ld, buf, base, entry, dn, colp,
				    labelwidth, rdncount, writeproc,
				    writeparm, eol, urlprefix );
			}
		    }
		    show = 0;
		}

		if ( show ) {
		    err = do_vals2text( ld, buf, vals, colp->ti_label,
			labelwidth, colp->ti_syntaxid, writeproc, writeparm,
			eol, rdncount, urlprefix );
		}

		if ( freevals ) {
		    ldap_value_free( vals );
		}
	    }
	}
    }

    if ( html  && !(( opts & LDAP_DISP_OPT_HTMLBODYONLY ) != 0 )) {
	sprintf( buf, "</BODY>%s</HTML>%s", eol, eol );
	(*writeproc)( writeparm, buf, strlen( buf ));
    }

    NSLDAPI_FREE( dn );
    if ( freebuf ) {
	NSLDAPI_FREE( buf );
    }

    return( err );
}

	
int
LDAP_CALL
ldap_entry2text_search(
	LDAP			*ld,
	char			*dn,		/* if NULL, use entry */
	char			*base,		/* if NULL, no search actions */
	LDAPMessage		*entry,		/* if NULL, use dn */
	struct ldap_disptmpl*	tmpllist,	/* if NULL, load default file */
	char			**defattrs,
	char			***defvals,
	writeptype		writeproc,
	void			*writeparm,
	char			*eol,
	int			rdncount,	/* if 0, display full DN */
	unsigned long		opts
)
{
    LDAPDebug( LDAP_DEBUG_TRACE, "ldap_entry2text_search\n", 0, 0, 0 );

    return( do_entry2text_search( ld, dn, base, entry, tmpllist, defattrs,
	    defvals, writeproc, writeparm, eol, rdncount, opts, NULL ));
}



int
LDAP_CALL
ldap_entry2html_search(
	LDAP			*ld,
	char			*dn,		/* if NULL, use entry */
	char			*base,		/* if NULL, no search actions */
	LDAPMessage		*entry,		/* if NULL, use dn */
	struct ldap_disptmpl*	tmpllist,	/* if NULL, load default file */
	char			**defattrs,
	char			***defvals,
	writeptype		writeproc,
	void			*writeparm,
	char			*eol,
	int			rdncount,	/* if 0, display full DN */
	unsigned long		opts,
	char			*urlprefix
)
{
    LDAPDebug( LDAP_DEBUG_TRACE, "ldap_entry2html_search\n", 0, 0, 0 );

    return( do_entry2text_search( ld, dn, base, entry, tmpllist, defattrs,
	    defvals, writeproc, writeparm, eol, rdncount, opts, urlprefix ));
}


static int
do_entry2text_search(
	LDAP			*ld,
	char			*dn,		/* if NULL, use entry */
	char			*base,		/* if NULL, no search actions */
	LDAPMessage		*entry,		/* if NULL, use dn */
	struct ldap_disptmpl*	tmpllist,	/* if NULL, no template used */
	char			**defattrs,
	char			***defvals,
	writeptype		writeproc,
	void			*writeparm,
	char			*eol,
	int			rdncount,	/* if 0, display full DN */
	unsigned long		opts,
	char			*urlprefix
)
{
    int				err, freedn, html;
    char			*buf, **fetchattrs, **vals;
    LDAPMessage			*ldmp;
    struct ldap_disptmpl	*tmpl;
    struct timeval		timeout;

    if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
	return( LDAP_PARAM_ERROR );
    }

    if ( dn == NULL && entry == NULLMSG ) {
	err = LDAP_PARAM_ERROR;
	LDAP_SET_LDERRNO( ld, err, NULL, NULL );
	return( err );
    }

    html = ( urlprefix != NULL );

    timeout.tv_sec = SEARCH_TIMEOUT_SECS;
    timeout.tv_usec = 0;

    if (( buf = NSLDAPI_MALLOC( LDAP_DTMPL_BUFSIZ )) == NULL ) {
	err = LDAP_NO_MEMORY;
	LDAP_SET_LDERRNO( ld, err, NULL, NULL );
	return( err );
    }

    freedn = 0;
    tmpl = NULL;

    if ( dn == NULL ) {
	if (( dn = ldap_get_dn( ld, entry )) == NULL ) {
	    NSLDAPI_FREE( buf );
	    return( LDAP_GET_LDERRNO( ld, NULL, NULL ) );
	}
	freedn = 1;
    }


    if ( tmpllist != NULL ) {
	ldmp = NULLMSG;

	if ( entry == NULL ) {
	    char	*ocattrs[2];

	    ocattrs[0] = OCATTRNAME;
	    ocattrs[1] = NULL;
#ifdef CLDAP
	    if ( LDAP_IS_CLDAP( ld ))
		    err = cldap_search_s( ld, dn, LDAP_SCOPE_BASE,
			"objectClass=*", ocattrs, 0, &ldmp, NULL );
	    else
#endif /* CLDAP */
		    err = ldap_search_st( ld, dn, LDAP_SCOPE_BASE,
			    "objectClass=*", ocattrs, 0, &timeout, &ldmp );

	    if ( err == LDAP_SUCCESS ) {
		entry = ldap_first_entry( ld, ldmp );
	    }
	}

	if ( entry != NULL ) {
	    vals = ldap_get_values( ld, entry, OCATTRNAME );
	    tmpl = ldap_oc2template( vals, tmpllist );
	    if ( vals != NULL ) {
		ldap_value_free( vals );
	    }
	}
	if ( ldmp != NULL ) {
	    ldap_msgfree( ldmp );
	}
    }

    entry = NULL;

    if ( tmpl == NULL ) {
	fetchattrs = NULL;
    } else {
	fetchattrs = ldap_tmplattrs( tmpl, NULL, 1, LDAP_SYN_OPT_DEFER );
    }

#ifdef CLDAP
    if ( LDAP_IS_CLDAP( ld ))
	err = cldap_search_s( ld, dn, LDAP_SCOPE_BASE, "objectClass=*",
		fetchattrs, 0, &ldmp, NULL );
    else
#endif /* CLDAP */
	err = ldap_search_st( ld, dn, LDAP_SCOPE_BASE, "objectClass=*",
		fetchattrs, 0, &timeout, &ldmp );

    if ( freedn ) {
	NSLDAPI_FREE( dn );
    }
    if ( fetchattrs != NULL ) {
	ldap_value_free( fetchattrs );
    }

    if ( err != LDAP_SUCCESS ||
	    ( entry = ldap_first_entry( ld, ldmp )) == NULL ) {
	NSLDAPI_FREE( buf );
	return( LDAP_GET_LDERRNO( ld, NULL, NULL ) );
    }

    err = do_entry2text( ld, buf, base, entry, tmpl, defattrs, defvals,
	    writeproc, writeparm, eol, rdncount, opts, urlprefix );

    NSLDAPI_FREE( buf );
    ldap_msgfree( ldmp );
    return( err );
}
	    

int
LDAP_CALL
ldap_vals2text(
	LDAP			*ld,
	char			*buf,		/* NULL for "use internal" */
	char			**vals,
	char			*label,
	int			labelwidth,	/* 0 means use default */
	unsigned long		syntaxid,
	writeptype		writeproc,
	void			*writeparm,
	char			*eol,
	int			rdncount
)
{
    LDAPDebug( LDAP_DEBUG_TRACE, "ldap_vals2text\n", 0, 0, 0 );

    return( do_vals2text( ld, buf, vals, label, labelwidth, syntaxid,
		writeproc, writeparm, eol, rdncount, NULL ));
}


int
LDAP_CALL
ldap_vals2html(
	LDAP			*ld,
	char			*buf,		/* NULL for "use internal" */
	char			**vals,
	char			*label,
	int			labelwidth,	/* 0 means use default */
	unsigned long		syntaxid,
	writeptype		writeproc,
	void			*writeparm,
	char			*eol,
	int			rdncount,
	char			*urlprefix
)
{
    LDAPDebug( LDAP_DEBUG_TRACE, "ldap_vals2html\n", 0, 0, 0 );

    if ( urlprefix == NULL ) {
	urlprefix = DEF_LDAP_URL_PREFIX;
    }

    return( do_vals2text( ld, buf, vals, label, labelwidth, syntaxid,
		writeproc, writeparm, eol, rdncount, urlprefix ));
}


static int
do_vals2text(
	LDAP			*ld,
	char			*buf,		/* NULL for "use internal" */
	char			**vals,
	char			*label,
	int			labelwidth,	/* 0 means use default */
	unsigned long		syntaxid,
	writeptype		writeproc,
	void			*writeparm,
	char			*eol,
	int			rdncount,
	char			*urlprefix
)
{
    int		err, i, html, writeoutval, freebuf, notascii;
    char	*p, *s, *outval;

    if ( !NSLDAPI_VALID_LDAP_POINTER( ld ) || writeproc == NULL ) {
	return( LDAP_PARAM_ERROR );
    }

    if ( vals == NULL ) {
	return( LDAP_SUCCESS );
    }

    html = ( urlprefix != NULL );

    switch( LDAP_GET_SYN_TYPE( syntaxid )) {
    case LDAP_SYN_TYPE_TEXT:
    case LDAP_SYN_TYPE_BOOLEAN:
	break;		/* we only bother with these two types... */
    default:
	return( LDAP_SUCCESS );
    }

    if ( labelwidth == 0 || labelwidth < 0 ) {
	labelwidth = DEF_LABEL_WIDTH;
    }

    if ( buf == NULL ) {
	if (( buf = NSLDAPI_MALLOC( LDAP_DTMPL_BUFSIZ )) == NULL ) {
	    err = LDAP_NO_MEMORY;
	    LDAP_SET_LDERRNO( ld, err, NULL, NULL );
	    return( err );
	}
	freebuf = 1;
    } else {
	freebuf = 0;
    }

    output_label( buf, label, labelwidth, writeproc, writeparm, eol, html );

    for ( i = 0; vals[ i ] != NULL; ++i ) {
	for ( p = vals[ i ]; *p != '\0'; ++p ) {
	    if ( !isascii( *p )) {
		break;
	    }
	}
	notascii = ( *p != '\0' );
	outval = notascii ? "(unable to display non-ASCII text value)"
		: vals[ i ];

	writeoutval = 0;	/* if non-zero, write outval after switch */

	switch( syntaxid ) {
	case LDAP_SYN_CASEIGNORESTR:
	    ++writeoutval;
	    break;

	case LDAP_SYN_RFC822ADDR:
	    if ( html ) {
		strcpy( buf, "<DD><A HREF=\"mailto:" );
		strcat_escaped( buf, outval );
		sprintf( buf + strlen( buf ), "\">%s</A><BR>%s", outval, eol );
		(*writeproc)( writeparm, buf, strlen( buf ));
	    } else {
		++writeoutval;
	    }
	    break;

	case LDAP_SYN_DN:	/* for now */
	    output_dn( buf, outval, labelwidth, rdncount, writeproc,
		    writeparm, eol, urlprefix );
	    break;

	case LDAP_SYN_MULTILINESTR:
	    if ( i > 0 && !html ) {
		output_label( buf, label, labelwidth, writeproc,
			writeparm, eol, html );
	    }

	    p = s = outval;
	    while (( s = strchr( s, '$' )) != NULL ) {
		*s++ = '\0';
		while ( ldap_utf8isspace( s )) {
		    ++s;
		}
		if ( html ) {
		    sprintf( buf, "<DD>%s<BR>%s", p, eol );
		} else {
		    sprintf( buf, "%-*s%s%s", labelwidth, " ", p, eol );
		}
		(*writeproc)( writeparm, buf, strlen( buf ));
		p = s;
	    }
	    outval = p;
	    ++writeoutval;
	    break;

	case LDAP_SYN_BOOLEAN:
	    outval = toupper( outval[ 0 ] ) == 'T' ? "TRUE" : "FALSE";
	    ++writeoutval;
	    break;

	case LDAP_SYN_TIME:
	case LDAP_SYN_DATE:
	    outval = time2text( outval, syntaxid == LDAP_SYN_DATE );
	    ++writeoutval;
	    break;

	case LDAP_SYN_LABELEDURL:
	    if ( !notascii && ( p = strchr( outval, '$' )) != NULL ) {
		*p++ = '\0';
		while ( ldap_utf8isspace( p )) {
		    ++p;
		}
		s = outval;
	    } else if ( !notascii && ( s = strchr( outval, ' ' )) != NULL ) {
		*s++ = '\0';
		while ( ldap_utf8isspace( s )) {
		    ++s;
		}
		p = outval;
	    } else {
		s = "URL";
		p = outval;
	    }

	    /*
	     * at this point `s' points to the label & `p' to the URL
	     */
	    if ( html ) {
		sprintf( buf, "<DD><A HREF=\"%s\">%s</A><BR>%s", p, s, eol );
	    } else {
		sprintf( buf, "%-*s%s%s%-*s%s%s", labelwidth, " ",
		    s, eol, labelwidth + 2, " ",p , eol );
	    }
	    (*writeproc)( writeparm, buf, strlen( buf ));
	    break;

	default:
	    sprintf( buf, " Can't display item type %ld%s",
		    syntaxid, eol );
	    (*writeproc)( writeparm, buf, strlen( buf ));
	}

	if ( writeoutval ) {
	    if ( html ) {
		sprintf( buf, "<DD>%s<BR>%s", outval, eol );
	    } else {
		sprintf( buf, "%-*s%s%s", labelwidth, " ", outval, eol );
	    }
	    (*writeproc)( writeparm, buf, strlen( buf ));
	}
    }

    if ( freebuf ) {
	NSLDAPI_FREE( buf );
    }

    return( LDAP_SUCCESS );
}


static int
max_label_len( struct ldap_disptmpl *tmpl )
{
    struct ldap_tmplitem	*rowp, *colp;
    int				len, maxlen;

    maxlen = 0;

    for ( rowp = ldap_first_tmplrow( tmpl ); rowp != NULLTMPLITEM;
	    rowp = ldap_next_tmplrow( tmpl, rowp )) {
	for ( colp = ldap_first_tmplcol( tmpl, rowp ); colp != NULLTMPLITEM;
		colp = ldap_next_tmplcol( tmpl, rowp, colp )) {
	    if (( len = strlen( colp->ti_label )) > maxlen ) {
		maxlen = len;
	    }
	}
    }

    return( maxlen );
}


static int
output_label( char *buf, char *label, int width, writeptype writeproc,
	void *writeparm, char *eol, int html )
{
    char	*p;

    if ( html ) {
	sprintf( buf, "<DT><B>%s</B>", label );
    } else {
	auto size_t w;
	sprintf( buf, " %s:", label );
	p = buf + strlen( buf );

	for (w = ldap_utf8characters(buf); w < (size_t)width; ++w) {
	    *p++ = ' ';
	}

	*p = '\0';
	strcat( buf, eol );
    }

    return ((*writeproc)( writeparm, buf, strlen( buf )));
}


static int
output_dn( char *buf, char *dn, int width, int rdncount,
	writeptype writeproc, void *writeparm, char *eol, char *urlprefix )
{
    char	**dnrdns;
    int		i;

    if (( dnrdns = ldap_explode_dn( dn, 1 )) == NULL ) {
	return( -1 );
    }

    if ( urlprefix != NULL ) {
	sprintf( buf, "<DD><A HREF=\"%s", urlprefix );
	strcat_escaped( buf, dn );
	strcat( buf, "\">" );
    } else if ( width > 0 ) {
	sprintf( buf, "%-*s", width, " " );
    } else {
	*buf = '\0';
    }

    for ( i = 0; dnrdns[ i ] != NULL && ( rdncount == 0 || i < rdncount );
	    ++i ) {
	if ( i > 0 ) {
	    strcat( buf, ", " );
	}
	strcat( buf, dnrdns[ i ] );
    }

    if ( urlprefix != NULL ) {
	strcat( buf, "</A><BR>" );
    }

    ldap_value_free( dnrdns );

    strcat( buf, eol );

    return ((*writeproc)( writeparm, buf, strlen( buf )));
}



#define HREF_CHAR_ACCEPTABLE( c )	(( c >= '-' && c <= '9' ) ||	\
					 ( c >= '@' && c <= 'Z' ) ||	\
					 ( c == '_' ) ||		\
					 ( c >= 'a' && c <= 'z' ))

static void
strcat_escaped( char *s1, char *s2 )
{
    char	*p, *q;
    char	*hexdig = "0123456789ABCDEF";

    p = s1 + strlen( s1 );
    for ( q = s2; *q != '\0'; ++q ) {
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


#define GET2BYTENUM( p )	(( *p - '0' ) * 10 + ( *(p+1) - '0' ))

static char *
time2text( char *ldtimestr, int dateonly )
{
    int			len;
    struct tm		t;
    char		*p, *timestr, zone, *fmterr = "badly formatted time";
    time_t		gmttime;
/* CTIME for this platform doesn't use this. */
#if !defined(SUNOS4) && !defined(BSDI) && !defined(LINUX1_2) && \
    !defined(SNI) && !defined(_WIN32) && !defined(macintosh) && !defined(LINUX)
    char		buf[26];
#endif

    memset( (char *)&t, 0, sizeof( struct tm ));
    if (( len = (int)strlen( ldtimestr )) < 13 ) {
	return( fmterr );
    }
    if ( len > 15 ) {   /* throw away excess from 4-digit year time string */
	len = 15;
    } else if ( len == 14 ) {
	len = 13;       /* assume we have a time w/2-digit year (len=13) */
    }

    for ( p = ldtimestr; p - ldtimestr + 1 < len; ++p ) {
	if ( !isdigit( *p )) {
	    return( fmterr );
	}
    }

    p = ldtimestr;
    t.tm_year = GET2BYTENUM( p ); p += 2;
    if ( len == 15 ) {
	t.tm_year = 100 * (t.tm_year - 19);
	t.tm_year += GET2BYTENUM( p ); p += 2;
    }
    else {
	/* 2 digit years...assumed to be in the range (19)70 through
	   (20)69 ...less than 70 (for now, 38) means 20xx */
	if(t.tm_year < 70) {
	    t.tm_year += 100;
	}
    }
    t.tm_mon = GET2BYTENUM( p ) - 1; p += 2;
    t.tm_mday = GET2BYTENUM( p ); p += 2;
    t.tm_hour = GET2BYTENUM( p ); p += 2;
    t.tm_min = GET2BYTENUM( p ); p += 2;
    t.tm_sec = GET2BYTENUM( p ); p += 2;

    if (( zone = *p ) == 'Z' ) {	/* GMT */
	zone = '\0';	/* no need to indicate on screen, so we make it null */
    }

    gmttime = gtime( &t );
    timestr = NSLDAPI_CTIME( &gmttime, buf, sizeof(buf) );

    timestr[ strlen( timestr ) - 1 ] = zone;	/* replace trailing newline */
    if ( dateonly ) {
	strcpy( timestr + 11, timestr + 20 );
    }

    return( timestr );
}



/* gtime.c - inverse gmtime */

#if !defined( macintosh ) && !defined( _WINDOWS ) && !defined( DOS ) && !defined(XP_OS2)
#include <sys/time.h>
#endif /* !macintosh */

/* gtime(): the inverse of localtime().
	This routine was supplied by Mike Accetta at CMU many years ago.
 */

static int	dmsize[] = {
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

#define	dysize(y)	\
	(((y) % 4) ? 365 : (((y) % 100) ? 366 : (((y) % 400) ? 365 : 366)))

/*
#define	YEAR(y)		((y) >= 100 ? (y) : (y) + 1900)
*/
#define YEAR(y)		(((y) < 1900) ? ((y) + 1900) : (y)) 

/*  */

static long	gtime ( struct tm *tm )
{
    register int    i,
                    sec,
                    mins,
                    hour,
                    mday,
                    mon,
                    year;
    register long   result;

    if ((sec = tm -> tm_sec) < 0 || sec > 59
	    || (mins = tm -> tm_min) < 0 || mins > 59
	    || (hour = tm -> tm_hour) < 0 || hour > 24
	    || (mday = tm -> tm_mday) < 1 || mday > 31
	    || (mon = tm -> tm_mon + 1) < 1 || mon > 12)
	return ((long) -1);
    if (hour == 24) {
	hour = 0;
	mday++;
    }
    year = YEAR (tm -> tm_year);

    result = 0L;
    for (i = 1970; i < year; i++)
	result += dysize (i);
    if (dysize (year) == 366 && mon >= 3)
	result++;
    while (--mon)
	result += dmsize[mon - 1];
    result += mday - 1;
    result = 24 * result + hour;
    result = 60 * result + mins;
    result = 60 * result + sec;

    return result;
}

static int
searchaction( LDAP *ld, char *buf, char *base, LDAPMessage *entry, char *dn,
	struct ldap_tmplitem *tip, int labelwidth, int rdncount,
	writeptype writeproc, void *writeparm, char *eol, char *urlprefix )
{
    int			err = LDAP_SUCCESS, lderr, i, count, html;
    char		**vals, **members;
    char		*value, *filtpattern, *attr, *selectname;
    char		*retattrs[2], filter[ 256 ];
    LDAPMessage		*ldmp;
    struct timeval	timeout;

    html = ( urlprefix != NULL );

    for ( i = 0; tip->ti_args != NULL && tip->ti_args[ i ] != NULL; ++i ) {
	;
    }
    if ( i < 3 ) {
	return( LDAP_PARAM_ERROR );
    }
    attr = tip->ti_args[ 0 ];
    filtpattern = tip->ti_args[ 1 ];
    retattrs[ 0 ] = tip->ti_args[ 2 ];
    retattrs[ 1 ] = NULL;
    selectname = tip->ti_args[ 3 ];

    vals = NULL;
    if ( attr == NULL ) {
	value = NULL;
    } else if ( strcasecmp( attr, "-dnb" ) == 0 ) {
	return( LDAP_PARAM_ERROR );
    } else if ( strcasecmp( attr, "-dnt" ) == 0 ) {
	value = dn;
    } else if (( vals = ldap_get_values( ld, entry, attr )) != NULL ) {
	value = vals[ 0 ];
    } else {
	value = NULL;
    }

    ldap_build_filter( filter, sizeof( filter ), filtpattern, NULL, NULL, NULL,
	    value, NULL );

    if ( html ) {
	/*
	 * if we are generating HTML, we add an HREF link that embodies this
	 * search action as an LDAP URL, instead of actually doing the search
	 * now.
	 */
	sprintf( buf, "<DT><A HREF=\"%s", urlprefix );
	if ( base != NULL ) {
	    strcat_escaped( buf, base );
	}
	strcat( buf, "??sub?" );
	strcat_escaped( buf, filter );
	sprintf( buf + strlen( buf ), "\"><B>%s</B></A><DD><BR>%s",
		tip->ti_label, eol );
	if ((*writeproc)( writeparm, buf, strlen( buf )) < 0 ) {
	    return( LDAP_LOCAL_ERROR );
	}
	return( LDAP_SUCCESS );
    }

    timeout.tv_sec = SEARCH_TIMEOUT_SECS;
    timeout.tv_usec = 0;

#ifdef CLDAP
    if ( LDAP_IS_CLDAP( ld ))
	lderr = cldap_search_s( ld, base, LDAP_SCOPE_SUBTREE, filter, retattrs,
		0, &ldmp, NULL );
    else
#endif /* CLDAP */
	lderr = ldap_search_st( ld, base, LDAP_SCOPE_SUBTREE, filter,
		retattrs, 0, &timeout, &ldmp );

    if ( lderr == LDAP_SUCCESS || NONFATAL_LDAP_ERR( lderr )) {
	if (( count = ldap_count_entries( ld, ldmp )) > 0 ) {
	    if (( members = (char **)NSLDAPI_MALLOC( (count + 1)
		    * sizeof(char *))) == NULL ) {
		err = LDAP_NO_MEMORY;
	    } else {
		for ( i = 0, entry = ldap_first_entry( ld, ldmp );
			entry != NULL;
			entry = ldap_next_entry( ld, entry ), ++i ) {
		    members[ i ] = ldap_get_dn( ld, entry );
		}
		members[ i ] = NULL;

		ldap_sort_values(ld,members, ldap_sort_strcasecmp);

		err = do_vals2text( ld, NULL, members, tip->ti_label,
			html ? -1 : 0, LDAP_SYN_DN, writeproc, writeparm,
			eol, rdncount, urlprefix );

		ldap_value_free( members );
	    }
	}
	ldap_msgfree( ldmp );
    }

    
    if ( vals != NULL ) {
	ldap_value_free( vals );
    }

    return(( err == LDAP_SUCCESS ) ? lderr : err );
}
