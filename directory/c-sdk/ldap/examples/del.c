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
 * Delete an entry from the directory.
 *
 * Instead of calling the synchronous ldap_delete_s() routine, we call
 * the asynchronous routine ldap_delete() and poll for results using
 * ldap_result().
 *
 * Since it is an error to attempt to delete an entry which does not
 * exist, you cannot run this example until you have added the entry
 * with the aadd.c example program.
 *
 */

#include "examples.h"

static void do_other_work();
unsigned long	global_counter = 0;

int
main( int argc, char **argv )
{
    LDAP	    	*ld;
    LDAPMessage	    	*result;
    char	    	*dn;
    int		   	rc;
    int			msgid;
    int			finished;
    struct timeval	zerotime;

    zerotime.tv_sec = zerotime.tv_usec = 0L;

    /* Specify the DN we're deleting */
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
    /* Initiate the delete operation */
    if (( msgid = ldap_delete( ld, dn )) < 0 ) {
	ldap_perror( ld, "ldap_delete" );
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
	    return( 1 );
	case 0:
	    /* Timeout was exceeded.  No entries are ready for retrieval */
	    break;
	default:
	    /* Should be finished here */
	    finished = 1;
	    if (( rc = ldap_result2error( ld, result, 0 )) == LDAP_SUCCESS ) {
		printf( "Entry deleted successfully.  I counted to %ld "
			"while waiting.\n", global_counter );
	    } else {
		printf( "Error while deleting entry: %s\n",
			ldap_err2string( rc ));
	    }
	    ldap_msgfree( result );
	}
	do_other_work();
    }
    ldap_unbind( ld );
    return 0;
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
