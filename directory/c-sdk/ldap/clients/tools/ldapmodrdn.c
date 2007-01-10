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

/* ldapmodrdn.c - generic program to modify an entry's RDN using LDAP */

#include "ldaptool.h"

static int domodrdn( LDAP *ld, char *dn, char *rdn, int remove,
    LDAPControl **serverctrls);
static void options_callback( int option, char *optarg );

static int	contoper, remove_oldrdn;
static LDAP	*ld;


static void
usage( void )
{
    fprintf( stderr, "usage: %s [options] [dn rdn]\n", ldaptool_progname );
    fprintf( stderr, "options:\n" );
    ldaptool_common_usage( 0 );
    fprintf( stderr, "    -c\t\tcontinuous mode (do not stop on errors)\n" );
    fprintf( stderr, "    -r\t\tremove old RDN\n" );
    fprintf( stderr, "    -f file\tread changes from file\n" );
    exit( LDAP_PARAM_ERROR );
}


int main( int argc, char **argv )
{
    char	*entrydn, *rdn, buf[ 4096 ];
    int		rc, havedn, deref, optind;
    LDAPControl	*ctrls[2], **serverctrls;

    contoper = remove_oldrdn = 0;

    optind = ldaptool_process_args( argc, argv, "cr", 0, options_callback );

    if ( optind == -1 ) {
	usage();
    }

    if ( ldaptool_fp == NULL ) {
	ldaptool_fp = stdin;
    }

    havedn = 0;
    if (argc - optind == 2) {
	if (( rdn = strdup( argv[argc - 1] )) == NULL ) {
	    perror( "strdup" );
	    exit( LDAP_NO_MEMORY );
	}
        if (( entrydn = strdup( argv[argc - 2] )) == NULL ) {
	    perror( "strdup" );
	    exit( LDAP_NO_MEMORY );
        }
	++havedn;
    } else if ( argc - optind != 0 ) {
	fprintf( stderr, "%s: invalid number of arguments, only two allowed\n",
	    ldaptool_progname );
	usage();
    }

    ld = ldaptool_ldap_init( 0 );

    if ( !ldaptool_not ) {
	deref = LDAP_DEREF_NEVER;	/* this seems prudent */
	ldap_set_option( ld, LDAP_OPT_DEREF, &deref );
    }

    ldaptool_bind( ld );

    if (( ctrls[0] = ldaptool_create_manage_dsait_control()) != NULL ) {
	ctrls[1] = NULL;
	serverctrls = ctrls;
    } else {
	serverctrls = NULL;
    }

    rc = 0;
    if (havedn) {
	rc = domodrdn(ld, entrydn, rdn, remove_oldrdn, serverctrls);
    } else while ((rc == 0 || contoper) &&
	    fgets(buf, sizeof(buf), ldaptool_fp) != NULL) {
	if ( *buf != '\0' && *buf != '\n' ) {	/* skip blank lines */
	    buf[ strlen( buf ) - 1 ] = '\0';	/* remove nl */

	    if ( havedn ) {	/* have DN, get RDN */
		if (( rdn = strdup( buf )) == NULL ) {
                    perror( "strdup" );
                    exit( LDAP_NO_MEMORY );
		}
		rc = domodrdn(ld, entrydn, rdn, remove_oldrdn, serverctrls);
		havedn = 0;
	    } else if ( !havedn ) {	/* don't have DN yet */
	        if (( entrydn = strdup( buf )) == NULL ) {
		    perror( "strdup" );
		    exit( LDAP_NO_MEMORY );
	        }
		++havedn;
	    }
	}
    }

    ldaptool_cleanup( ld );
    exit( rc );
}


static void
options_callback( int option, char *optarg )
{
    switch( option ) {
    case 'c':	/* continuous operation mode */
	++contoper;
	break;
    case 'r':	/* remove old RDN */
	++remove_oldrdn;
	break;
    default:
	usage();
    }
}


static int
domodrdn( LDAP *ld, char *dn, char *rdn, int remove_oldrdn,
	LDAPControl **serverctrls )
{
    int	i;

    if ( ldaptool_verbose ) {
	printf( "modrdn %s:\n\t%s\n", dn, rdn );
	if (remove_oldrdn)
	    printf("removing old RDN\n");
	else
	    printf("keeping old RDN\n");
    }

    if ( !ldaptool_not ) {
	if (( i = ldaptool_rename_s( ld, dn, rdn, NULL, remove_oldrdn,
		serverctrls, NULL, "ldap_rename" )) == LDAP_SUCCESS
		&& ldaptool_verbose ) {
	    printf( "modrdn complete\n" );
	}
    } else {
	i = LDAP_SUCCESS;
    }

    return( i );
}
