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

/* tmpltest.c - implements a test/config templates. */
#include <stdio.h>
#include <sys/types.h>

#ifdef _WINDOWS
#include <windows.h>
#endif

#include "ldap-int.h"
#include "disptmpl.h"
#include "srchpref.h"

#ifdef MACOS
#include <stdlib.h>
#include <console.h>
#endif /* MACOS */

#ifdef NEEDPROTOS
void dump_tmpl( struct ldap_disptmpl *tmpl );
void dump_srchpref( struct ldap_searchobj *sp );
#else /* NEEDPROTOS */
void dump_tmpl();
void dump_srchpref();
#endif /* NEEDPROTOS */


#define NULLSTRINGIFNULL( s )	( s == NULL ? "(null)" : s )


int
main( int argc, char **argv )
{
    struct ldap_disptmpl	*templates, *dtp;
    struct ldap_searchobj	*so, *sop;
    int				err;

#ifdef MACOS
	ccommand( &argv );
	for ( argc = 0; argv[ argc ] != NULL; ++argc ) {
	    ;
	}
	cshow( stdout );
#endif /* MACOS */

    if (( err = ldap_init_templates( "ldaptemplates.conf", &templates ))
	    != 0 ) {
	fprintf( stderr, "ldap_init_templates failed (%d)\n", err );
	exit( 1 );
    }

    if (( err = ldap_init_searchprefs( "ldapsearchprefs.conf", &so ))
	    != 0 ) {
	fprintf( stderr, "ldap_init_searchprefs failed (%d)\n", err );
	exit( 1 );
    }

    if ( argc == 1 ) {
	printf( "*** Display Templates:\n" );
	for ( dtp = ldap_first_disptmpl( templates ); dtp != NULLDISPTMPL;
		dtp = ldap_next_disptmpl( templates, dtp )) {
	    dump_tmpl( dtp );
	    printf( "\n\n" );
	}

	printf( "\n\n*** Search Objects:\n" );
	for ( sop = ldap_first_searchobj( so ); sop != NULLSEARCHOBJ;
		    sop = ldap_next_searchobj( so, sop )) {
	    dump_srchpref( sop );
	    printf( "\n\n" );
	}

    } else {
	if (( dtp = ldap_oc2template( ++argv, templates )) == NULL ) {
	    fprintf( stderr, "no matching template found\n" );
	} else {
	    dump_tmpl( dtp );
	}
    }


    ldap_free_templates( templates );
    ldap_free_searchprefs( so );

    exit( 0 );
}


static char *syn_name[] = {
    "?", "CIS", "MLS", "DN", "BOOL", "JPEG", "JPEGBTN", "FAX", "FAXBTN",
    "AUDIOBTN", "TIME", "DATE", "URL", "SEARCHACT", "LINKACT", "ADDDNACT",
    "VERIFYACT",
};

static char *syn_type[] = {
    "?", "txt", "img", "?", "bool", "?", "?", "?", "btn",
    "?", "?", "?", "?", "?", "?", "?",
    "action", "?"
};

static char *includeattrs[] = { "objectClass", "sn", NULL };

static char *item_opts[] = {
    "ro", "sort", "1val", "hide", "required", "hideiffalse", NULL
};

static unsigned long item_opt_vals[] = {
    LDAP_DITEM_OPT_READONLY,		LDAP_DITEM_OPT_SORTVALUES,
    LDAP_DITEM_OPT_SINGLEVALUED,	LDAP_DITEM_OPT_HIDEIFEMPTY,
    LDAP_DITEM_OPT_VALUEREQUIRED,	LDAP_DITEM_OPT_HIDEIFFALSE,
};


void
dump_tmpl( struct ldap_disptmpl *tmpl )
{
    struct ldap_tmplitem	*rowp, *colp;
    int				i, rowcnt, colcnt;
    char			**fetchattrs;
    struct ldap_oclist		*ocp;
    struct ldap_adddeflist	*adp;

    printf( "** Template \"%s\" (plural \"%s\", icon \"%s\")\n",
	    NULLSTRINGIFNULL( tmpl->dt_name ),
	    NULLSTRINGIFNULL( tmpl->dt_pluralname ),
	    NULLSTRINGIFNULL( tmpl->dt_iconname ));

    printf( "object class list:\n" );
    for ( ocp = tmpl->dt_oclist; ocp != NULL; ocp = ocp->oc_next ) {
	for ( i = 0; ocp->oc_objclasses[ i ] != NULL; ++i ) {
	    printf( "%s%s", i == 0 ? "  " : " & ",
		    NULLSTRINGIFNULL( ocp->oc_objclasses[ i ] ));
	}
	putchar( '\n' );
    }
    putchar( '\n' );

    printf( "template options:          " );
    if ( tmpl->dt_options == 0L ) {
	printf( "NONE\n" );
    } else {
	printf( "%s %s %s\n", LDAP_IS_DISPTMPL_OPTION_SET( tmpl,
		LDAP_DTMPL_OPT_ADDABLE ) ? "addable" : "",
		LDAP_IS_DISPTMPL_OPTION_SET( tmpl, LDAP_DTMPL_OPT_ALLOWMODRDN )
		? "modrdn" : "",
		LDAP_IS_DISPTMPL_OPTION_SET( tmpl, LDAP_DTMPL_OPT_ALTVIEW )
		? "altview" : "" );
    }

    printf( "authenticate as attribute: %s\n", tmpl->dt_authattrname != NULL ?
	    tmpl->dt_authattrname : "<default>" );

    printf( "default RDN attribute:     %s\n", tmpl->dt_defrdnattrname != NULL ?
	    tmpl->dt_defrdnattrname : "NONE" );

    printf( "default add location:      %s\n", tmpl->dt_defaddlocation != NULL ?
	    tmpl->dt_defaddlocation : "NONE" );

    printf( "\nnew entry value default rules:\n" );
    for ( adp = tmpl->dt_adddeflist; adp != NULL; adp = adp->ad_next ) {
	if ( adp->ad_source == LDAP_ADSRC_CONSTANTVALUE ) {
	    printf( "  attribute %s <-- constant value \"%s\"\n",
		NULLSTRINGIFNULL( adp->ad_attrname),
		NULLSTRINGIFNULL( adp->ad_value ));
	} else {
	    printf( "  attribute %s <-- adder's DN\n",
		    NULLSTRINGIFNULL( adp->ad_attrname ));
	}
    }
    putchar( '\n' );

    printf( "\nfetch attributes & values:\n" );
    if (( fetchattrs = ldap_tmplattrs( tmpl, includeattrs, 1,
		LDAP_SYN_OPT_DEFER )) == NULL ) {
	printf( "  <none>\n" );
    } else {
	for ( i = 0; fetchattrs[ i ] != NULL; ++i ) {
	    printf( "  %s\n", fetchattrs[ i ] );
	    free( fetchattrs[ i ] );
	}
	free( (char *)fetchattrs );
    }

    printf( "\nfetch attributes only:\n" );
    if (( fetchattrs = ldap_tmplattrs( tmpl, NULL, 0,
		LDAP_SYN_OPT_DEFER )) == NULL ) {
	printf( "  <none>\n" );
    } else {
	for ( i = 0; fetchattrs[ i ] != NULL; ++i ) {
	    printf( "  %s\n", fetchattrs[ i ] );
	    free( fetchattrs[ i ] );
	}
	free( (char *)fetchattrs );
    }

    printf( "\ntemplate items:\n" );
    rowcnt = 0;
    for ( rowp = ldap_first_tmplrow( tmpl ); rowp != NULLTMPLITEM;
	    rowp = ldap_next_tmplrow( tmpl, rowp )) {
	++rowcnt;
	colcnt = 0;
	for ( colp = ldap_first_tmplcol( tmpl, rowp ); colp != NULLTMPLITEM;
		colp = ldap_next_tmplcol( tmpl, rowp, colp )) {
	    ++colcnt;
	    printf( "  %2d-%d: %s (%s%s", rowcnt, colcnt,
		syn_name[ colp->ti_syntaxid & 0x0000FFFF ],
		syn_type[ LDAP_GET_SYN_TYPE( colp->ti_syntaxid ) >> 24 ],
		(( LDAP_GET_SYN_OPTIONS( colp->ti_syntaxid ) &
		LDAP_SYN_OPT_DEFER ) != 0 ) ? ",defer" : "" );

	    for ( i = 0; item_opts[ i ] != NULL; ++i ) {
		if ( LDAP_IS_TMPLITEM_OPTION_SET( colp, item_opt_vals[ i ] )) {
		    printf( ",%s", NULLSTRINGIFNULL( item_opts[ i ] ));
		}
	    }

	    printf( "), %s, %s", NULLSTRINGIFNULL( colp->ti_attrname ),
		    NULLSTRINGIFNULL( colp->ti_label ));
	    if ( colp->ti_args != NULL ) {
		printf( ",args=" );
		for ( i = 0; colp->ti_args[ i ] != NULL; ++i ) {
		    printf( "<%s>", NULLSTRINGIFNULL( colp->ti_args[ i ] ));
		}
	    }

	    putchar( '\n' );
	}
    }
}


void
dump_srchpref( struct ldap_searchobj *so )
{
    int i;
    struct ldap_searchattr *sa;
    struct ldap_searchmatch *sm;

    printf( "Object type prompt:  %s\n",
	    NULLSTRINGIFNULL( so->so_objtypeprompt ));
    printf( "Options:             %s\n",
	    LDAP_IS_SEARCHOBJ_OPTION_SET( so, LDAP_SEARCHOBJ_OPT_INTERNAL ) ?
	    "internal" : "NONE" );
    printf( "Prompt:              %s\n", NULLSTRINGIFNULL( so->so_prompt ));
    printf( "Scope:               " );
    switch ( so->so_defaultscope ) {
    case LDAP_SCOPE_BASE:
	printf( "LDAP_SCOPE_BASE" );
	break;
    case LDAP_SCOPE_ONELEVEL:
	printf( "LDAP_SCOPE_ONELEVEL" );
	break;
    case LDAP_SCOPE_SUBTREE:
	printf( "LDAP_SCOPE_SUBTREE" );
	break;
    default:
	printf("*** unknown!" );
    }
    puts( "\n" );
    printf( "Filter prefix:       %s\n",
	    NULLSTRINGIFNULL( so->so_filterprefix ));
    printf( "Filter tag:          %s\n",
	    NULLSTRINGIFNULL( so->so_filtertag ));
    printf( "Default select attr: %s\n",
	    NULLSTRINGIFNULL( so->so_defaultselectattr ));
    printf( "Default select text: %s\n",
	    NULLSTRINGIFNULL( so->so_defaultselecttext ));
    printf( "Searchable attributes ---- \n" );
    for ( sa = so->so_salist; sa != NULL; sa = sa->sa_next ) {
	printf( "  Label: %s\n", NULLSTRINGIFNULL( sa->sa_attrlabel ));
	printf( "  Attribute: %s\n", NULLSTRINGIFNULL( sa->sa_attr ));
	printf( "  Select attr: %s\n", NULLSTRINGIFNULL( sa->sa_selectattr ));
	printf( "  Select text: %s\n", NULLSTRINGIFNULL( sa->sa_selecttext ));
	printf( "  Match types ---- \n" );
	for ( i = 0, sm = so->so_smlist; sm != NULL; i++, sm = sm->sm_next ) {
	    if (( sa->sa_matchtypebitmap >> i ) & 1 ) {
		printf( "    %s (%s)\n",
			NULLSTRINGIFNULL( sm->sm_matchprompt ),
			NULLSTRINGIFNULL( sm->sm_filter ));
	    }
	}
    }
}
