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
 * Add a new entry to the directory.
 *
 * Instead of calling the synchronous ldap_add_s() routine, we call
 * the asynchronous routine ldap_add() and poll for results using
 * ldap_result().
 *
 * Since it is an error to attempt to add an entry which already exists,
 * you cannot run this example program twice in a row.  You can use the
 * adel.c example program to delete the entry which this example adds.
 *
 */

#include "examples.h"

static void do_other_work();
unsigned long	global_counter = 0;
static void free_mods( LDAPMod **mods );

#define	NMODS	5

int
main( int argc, char **argv )
{
    LDAP	    	*ld;
    LDAPMessage	    	*result;
    char	    	*dn;
    int		    	i;
    int		   	rc;
    int			msgid;
    int			finished;
    struct timeval	zerotime;
    LDAPMod		**mods;

    char *objectclass_values[] = { "top", "person", "organizationalPerson",
				   "inetOrgPerson", NULL };
    char *cn_values[] = { "William B Jensen", "William Jensen", "Bill Jensen",
		 	  NULL };
    char *sn_values[] = { "Jensen", NULL };
    char *givenname_values[] = { "William", "Bill", NULL };
    char *telephonenumber_values[] = { "+1 415 555 1212", NULL };

    zerotime.tv_sec = zerotime.tv_usec = 0L;

    /* Specify the DN we're adding */
    dn = "cn=William B Jensen, " PEOPLE_BASE;	/* see examples.h */

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

    /* Construct the array of values to add */
    mods = ( LDAPMod ** ) malloc(( NMODS + 1 ) * sizeof( LDAPMod * ));
    if ( mods == NULL ) {
	fprintf( stderr, "Cannot allocate memory for mods array\n" );
    }
    for ( i = 0; i < NMODS; i++ ) {
	if (( mods[ i ] = ( LDAPMod * ) malloc( sizeof( LDAPMod ))) == NULL ) {
	    fprintf( stderr, "Cannot allocate memory for mods element\n" );
	    exit( 1 );
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
    mods[ 4 ]->mod_op = 0;
    mods[ 4 ]->mod_type = "telephonenumber";
    mods[ 4 ]->mod_values = telephonenumber_values;
    mods[ 5 ] = NULL;


    /* Initiate the add operation */
    if (( msgid = ldap_add( ld, dn, mods )) < 0 ) {
	ldap_perror( ld, "ldap_add" );
	free_mods( mods );
	return( 1 );
    }

    /* Poll for the result */
    finished = 0;
    while ( !finished ) {
	rc = ldap_result( ld, msgid, LDAP_MSG_ONE, &zerotime, &result );
	switch ( rc ) {
	case -1:
	    /* some error occurred */
	    ldap_perror( ld, "ldap_result" );
	    free_mods( mods );
	    return( 1 );
	case 0:
	    /* Timeout was exceeded.  No entries are ready for retrieval */
	    break;
	default:
	    /* Should be finished here */
	    finished = 1;
	    if (( rc = ldap_result2error( ld, result, 0 )) == LDAP_SUCCESS ) {
		printf( "Entry added successfully.  I counted to %ld "
			"while waiting.\n", global_counter );
	    } else {
		printf( "Error while adding entry: %s\n",
			ldap_err2string( rc ));
	    }
	    ldap_msgfree( result );
	}
	do_other_work();
    }
    ldap_unbind( ld );
    free_mods( mods );
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
	

/*
 * Perform other work while polling for results.  This doesn't do anything
 * useful, but it could.
 */
static void
do_other_work()
{
    global_counter++;
}
