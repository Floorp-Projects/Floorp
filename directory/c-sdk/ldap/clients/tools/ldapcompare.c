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

/* ldapdelete.c - simple program to delete an entry using LDAP */

#include "ldaptool.h"
#include "fileurl.h"

static int	contoper = 0;
static LDAP	*ld;
static int	ldapcompare_quiet = 0;

static int docompare( LDAP *ld, const char *dn, const char *attrtype,
		const struct berval *bvalue, LDAPControl **serverctrls );
static void options_callback( int option, char *optarg );
static int typeval2berval( char *typeval, char **typep, struct berval *bvp );


static void
usage( int rc )
{
    fprintf( stderr, "usage: %s [options] attributetype:value [dn...]\n",
		ldaptool_progname );
    fprintf( stderr, "       %s [options] attributetype::base64value [dn...]\n",
		ldaptool_progname );
    fprintf( stderr, "       %s [options] attributetype:<fileurl [dn...]\n",
		ldaptool_progname );
    fprintf( stderr, "options:\n" );
    ldaptool_common_usage( 0 );
    fprintf( stderr, "    -c\t\tcontinuous mode (do not stop on errors)\n" );
    fprintf( stderr, "    -f file\tread DNs to compare against from file\n" );
    fprintf( stderr, "    -q\t\tbe quiet when comparing entries\n" );
    exit( rc );
}


int
main( int argc, char **argv )
{
    char		buf[ 4096 ], *typeval = NULL, *type = NULL;
    struct berval	bv;
    int			rc, optind;
    LDAPControl		*ldctrl;

#ifdef notdef
#ifdef HPUX11
#ifndef __LP64__
	_main( argc, argv);
#endif /* __LP64_ */
#endif /* HPUX11 */
#endif

    optind = ldaptool_process_args( argc, argv, "cq", 0, options_callback );

    if ( ldaptool_fp == NULL && optind >= argc ) {
	ldaptool_fp = stdin;
    }

    ld = ldaptool_ldap_init( 0 );

    ldaptool_bind( ld );

    if (( ldctrl = ldaptool_create_manage_dsait_control()) != NULL ) {
	ldaptool_add_control_to_array( ldctrl, ldaptool_request_ctrls);
    } 

    if ((ldctrl = ldaptool_create_proxyauth_control(ld)) !=NULL) {
	ldaptool_add_control_to_array( ldctrl, ldaptool_request_ctrls);
    }

    if ( optind >= argc ) {
	usage( LDAP_PARAM_ERROR );
    }

    typeval = ldaptool_local2UTF8( argv[optind], "type and value" );
    if (( rc = typeval2berval( typeval, &type, &bv )) != LDAP_SUCCESS ) {
	fprintf( stderr, "%s: unable to parse \"%s\"\n",
		    ldaptool_progname, argv[optind] );
	usage( rc );
	free( typeval );
    }
    ++optind;

    rc = 0;
    if ( ldaptool_fp == NULL ) {
	for ( ; optind < argc &&
		( contoper || !LDAPTOOL_RESULT_IS_AN_ERROR( rc ) );
		++optind ) {
            char *conv;

            conv = ldaptool_local2UTF8( argv[ optind ], "DN" );
	    rc = docompare( ld, conv, type, &bv, ldaptool_request_ctrls );
            if ( conv != NULL ) {
                free( conv );
	    }
	}
    } else {
	while (( contoper || !LDAPTOOL_RESULT_IS_AN_ERROR( rc )) &&
		fgets(buf, sizeof(buf), ldaptool_fp) != NULL) {
	    buf[ strlen( buf ) - 1 ] = '\0';	/* remove trailing newline */
	    if ( *buf != '\0' ) {
		rc = docompare( ld, buf, type, &bv, ldaptool_request_ctrls );
	    }
	}
    }

    ldaptool_reset_control_array( ldaptool_request_ctrls );
    ldaptool_cleanup( ld );
    if ( typeval != NULL ) free( typeval );
    if ( bv.bv_val != NULL ) free( bv.bv_val );

    return( rc );
}

static void
options_callback( int option, char *optarg )
{
    switch( option ) {
    case 'c':	/* continuous operation mode */
	++contoper;
	break;
    case 'q':	/* continuous operation mode */
	++ldapcompare_quiet;
	break;
    default:
	usage( LDAP_PARAM_ERROR );
    }
}


static int
docompare( LDAP *ld, const char *dn, const char *attrtype,
		const struct berval *bvalue, LDAPControl **serverctrls )
{
    int		rc;

    if ( !ldapcompare_quiet ) {
	char	*valuestr, tmpbuf[256];

	if ( ldaptool_berval_is_ascii( bvalue )) {
		valuestr = bvalue->bv_val;
	} else {
#ifdef HAVE_SNPRINTF
	    snprintf( tmpbuf, sizeof(tmpbuf), "NOT ASCII (%ld bytes)",
			bvalue->bv_len );
#else
	    sprintf( tmpbuf, "NOT ASCII (%ld bytes)",
			bvalue->bv_len );
#endif
	    valuestr = tmpbuf;
	}
	printf( "%scomparing type: \"%s\" value: \"%s\" in entry \"%s\"\n",
		ldaptool_not ? "!" : "", attrtype, valuestr, dn );
    }
    if ( ldaptool_not ) {
	rc = LDAP_COMPARE_TRUE;
    } else {
	rc = ldaptool_compare_ext_s( ld, dn, attrtype, bvalue,
	    serverctrls, NULL, "ldap_compare" );
	if ( !ldapcompare_quiet ) {
	    if ( rc == LDAP_COMPARE_TRUE ) {
		puts( "compare TRUE" );
	    } else if ( rc == LDAP_COMPARE_FALSE ) {
		puts( "compare FALSE" );
	    }
	}
    }

    return( rc );
}


/*
 * Parse an ldapcompare type:value or type::value argument.
 *
 * The *typep is set to point into the typeval string.
 * bvp->bv_val is created from malloc'd memory.
 *
 * This function returns an LDAP error code (LDAP_SUCCESS if all goes well).
 */
static int
typeval2berval( char *typeval, char **typep, struct berval *bvp )
{
    char	*value;
    int		vlen, rc;

    if ( ldif_parse_line( typeval, typep, &value, &vlen ) != 0 ) {
	return( LDAP_PARAM_ERROR );
    }

    rc = ldaptool_berval_from_ldif_value( value, vlen, bvp,
	    1 /* recognize file URLs */, 0 /* always try file */,
	    1 /* report errors */ );

    return( ldaptool_fileurlerr2ldaperr( rc ));
}
