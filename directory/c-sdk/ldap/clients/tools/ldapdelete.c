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

static int	contoper;
static LDAP	*ld;

static int dodelete( LDAP *ld, char *dn, LDAPControl **serverctrls );
static void options_callback( int option, char *optarg );


static void
usage( void )
{
    fprintf( stderr, "usage: %s [options] [dn...]\n", ldaptool_progname );
    fprintf( stderr, "options:\n" );
    ldaptool_common_usage( 0 );
    fprintf( stderr, "    -c\t\tcontinuous mode (do not stop on errors)\n" );
    fprintf( stderr, "    -f file\tread DNs to delete from file (default: standard input)\n" );
    exit( LDAP_PARAM_ERROR );
}


int
main( int argc, char **argv )
{
    char	buf[ 4096 ];
    int		rc, deref, optind;
    LDAPControl	*ldctrl;

#ifdef notdef
#ifdef HPUX11
#ifndef __LP64__
	_main( argc, argv);
#endif /* __LP64_ */
#endif /* HPUX11 */
#endif

    contoper = 0;

    optind = ldaptool_process_args( argc, argv, "c", 0, options_callback );
	
    if ( optind == -1 ) {
		usage();
    }

    if ( ldaptool_fp == NULL && optind >= argc ) {
	ldaptool_fp = stdin;
    }

    ld = ldaptool_ldap_init( 0 );

    deref = LDAP_DEREF_NEVER;	/* prudent, but probably unnecessary */
    ldap_set_option( ld, LDAP_OPT_DEREF, &deref );

    ldaptool_bind( ld );

    if (( ldctrl = ldaptool_create_manage_dsait_control()) != NULL ) {
	ldaptool_add_control_to_array( ldctrl, ldaptool_request_ctrls);
    } 

    if ((ldctrl = ldaptool_create_proxyauth_control(ld)) !=NULL) {
	ldaptool_add_control_to_array( ldctrl, ldaptool_request_ctrls);
    }

    if ( ldaptool_fp == NULL ) {
	for ( ; optind < argc; ++optind ) {
            char *conv;

            conv = ldaptool_local2UTF8( argv[ optind ] );
	    rc = dodelete( ld, conv, ldaptool_request_ctrls );
            if( conv != NULL )
                free( conv );
	}
    } else {
	rc = 0;
	while ((rc == 0 || contoper) &&
		fgets(buf, sizeof(buf), ldaptool_fp) != NULL) {
	    buf[ strlen( buf ) - 1 ] = '\0';	/* remove trailing newline */
	    if ( *buf != '\0' ) {
		rc = dodelete( ld, buf, ldaptool_request_ctrls );
	    }
	}
    }

    ldaptool_reset_control_array( ldaptool_request_ctrls );
    ldaptool_cleanup( ld );
    return( rc );
}

static void
options_callback( int option, char *optarg )
{
    switch( option ) {
    case 'c':	/* continuous operation mode */
	++contoper;
	break;
    default:
	usage();
    }
}


static int
dodelete( LDAP *ld, char *dn, LDAPControl **serverctrls )
{
    int		rc;

    if ( ldaptool_verbose ) {
	printf( "%sdeleting entry %s\n", ldaptool_not ? "!" : "", dn );
    }
    if ( ldaptool_not ) {
	rc = LDAP_SUCCESS;
    } else if (( rc = ldaptool_delete_ext_s( ld, dn, serverctrls, NULL,
	    "ldap_delete" )) == LDAP_SUCCESS && ldaptool_verbose ) {
	printf( "entry removed\n" );
    }

    return( rc );
}
