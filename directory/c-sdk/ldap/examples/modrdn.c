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

/*
 * Modify the RDN (relative distinguished name) of an entry.  In this
 * example, we change the dn "cn=Jacques Smith,ou=People,dc=example,dc=com
 * to "cn=Jacques M Smith,ou=People,dc=example,dc=com.
 *
 * Since it is an error to either (1) attempt to modrdn an entry which
 * does not exist, or (2) modrdn an entry where the destination name
 * already exists, we take some steps, for this example, to make sure
 * we'll succeed.  We (1) add "cn=Jacques Smith" (if the entry exists,
 * we just ignore the error, and (2) delete "cn=Jacques M Smith" (if the
 * entry doesn't exist, we ignore the error).
 *
 * We pass 0 for the "deleteoldrdn" argument to ldap_modrdn2_s().  This
 * means that after we change the RDN, the server will put the value
 * "Jacques Smith" into the cn attribute of the new entry, in addition to
 * "Jacques M Smith".
 */

#include "examples.h"

#define	NMODS	4

unsigned long	global_counter = 0;

static void free_mods( LDAPMod **mods );

int
main( int argc, char **argv )
{
    LDAP	    	*ld;
    char	    	*dn, *ndn, *nrdn;
    int		    	i;
    int		   	rc;
    LDAPMod		**mods;

    /* Values we'll use in creating the entry */
    char *objectclass_values[] = { "top", "person", "organizationalPerson",
				   "inetOrgPerson", NULL };
    char *cn_values[] = { "Jacques Smith", NULL };
    char *sn_values[] = { "Smith", NULL };
    char *givenname_values[] = { "Jacques", NULL };

    /* Specify the DN we're adding */
    dn = "cn=Jacques Smith, " PEOPLE_BASE;	/* see examples.h */
    /* the destination DN */
    ndn = "cn=Jacques M Smith, " PEOPLE_BASE;	/* see examples.h */
    /* the new RDN */
    nrdn = "cn=Jacques M Smith";

    /* get a handle to an LDAP connection */
    if ( (ld = ldap_init( MY_HOST, MY_PORT )) == NULL ) {
	perror( "ldap_init" );
	return( 1 );
    }
    /* authenticate to the directory as the Directory Manager */
    if ( ldap_simple_bind_s( ld, MGR_DN, MGR_PW ) != LDAP_SUCCESS ) {
	ldap_perror( ld, "ldap_simple_bind_s" );
	return( 1 );
    }

    if (( mods = ( LDAPMod ** ) malloc(( NMODS + 1 ) * sizeof( LDAPMod *)))
	    == NULL ) {
	fprintf( stderr, "Cannot allocate memory for mods array\n" );
	return( 1 );
    }
    /* Construct the array of values to add */
    for ( i = 0; i < NMODS; i++ ) {
	if (( mods[ i ] = ( LDAPMod * ) malloc( sizeof( LDAPMod ))) == NULL ) {
	    fprintf( stderr, "Cannot allocate memory for mods element\n" );
	    return( 1 );
	}
    }
    mods[ 0 ]->mod_op = 0;
    mods[ 0 ]->mod_type = "objectclass";
    mods[ 0 ]->mod_values = objectclass_values;
    mods[ 1 ]->mod_op = 0;
    mods[ 1 ]->mod_type = "cn";
    mods[ 1 ]->mod_values = cn_values;
    mods[ 2 ]->mod_op = 0;
    mods[ 2 ]->mod_type = "sn";
    mods[ 2 ]->mod_values = sn_values;
    mods[ 3 ]->mod_op = 0;
    mods[ 3 ]->mod_type = "givenname";
    mods[ 3 ]->mod_values = givenname_values;
    mods[ 4 ] = NULL;


    /* Add the entry */
    if (( rc = ldap_add_s( ld, dn, mods )) != LDAP_SUCCESS ) {
	/* If entry exists already, fine.  Ignore this error. */
	if ( rc == LDAP_ALREADY_EXISTS ) {
	    printf( "Entry \"%s is already in the directory.\n", dn );
	} else {
	    ldap_perror( ld, "ldap_add_s" );
	    free_mods( mods );
	    return( 1 );
	}
    } else {
	printf( "Added entry \"%s\".\n", dn );
    }
    free_mods( mods );

    /* Delete the destination entry, for this example */
    if (( rc = ldap_delete_s( ld, ndn )) != LDAP_SUCCESS ) {
	/* If entry does not exist, fine.  Ignore this error. */
	if ( rc == LDAP_NO_SUCH_OBJECT ) {
	    printf( "Entry \"%s\" is not in the directory.  "
		    "No need to delete.\n", ndn );
	} else {
	    ldap_perror( ld, "ldap_delete_s" );
	    return( 1 );
	}
    } else {
	printf( "Deleted entry \"%s\".\n", ndn );
    }

    /* Do the modrdn operation */
    if ( ldap_modrdn2_s( ld, dn, nrdn, 0 ) != LDAP_SUCCESS ) {
	ldap_perror( ld, "ldap_modrdn2_s" );
	return( 1 );
    }

    printf( "The modrdn operation was successful.  Entry\n"
	    "\"%s\" has been changed to\n"
	    "\"%s\".\n", dn, ndn );

    ldap_unbind( ld );
    return 0;
}



/*
 * Free a mods array.
 */
static void
free_mods( LDAPMod **mods )
{
    int i;
 
    for ( i = 0; i < NMODS; i++ ) {
        free( mods[ i ] );
    }
    free( mods );
}
