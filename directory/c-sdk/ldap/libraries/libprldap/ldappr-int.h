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
 * Internal header for libprldap -- glue NSPR (Netscape Portable Runtime)
 * to libldap.
 *
 */

#include "ldap.h"
#include "nspr.h"
#include "ldappr.h"

#include <errno.h>
#include <string.h>
#include <stdarg.h>

/*
 * Macros:
 */

/* #define PRLDAP_DEBUG	1	*/ 	/* uncomment to enable debugging printfs */

/*
 * All of the sockets we use are IPv6 capable.
 * Change the following #define to PR_AF_INET to support IPv4 only.
 */
#define PRLDAP_DEFAULT_ADDRESS_FAMILY	PR_AF_INET6


/*
 * Data structures:
 */

/* data structure that populates the I/O callback session arg. */
typedef struct lextiof_session_private {
	PRPollDesc	*prsess_pollds;		/* for poll callback */
	int		prsess_pollds_count;	/* # of elements in pollds */
	int		prsess_io_max_timeout;	/* in milliseconds */
	void		*prsess_appdata;	/* application specific data */
} PRLDAPIOSessionArg;

/* data structure that populates the I/O callback socket-specific arg. */
typedef struct lextiof_socket_private {
	PRFileDesc	*prsock_prfd;		/* associated NSPR file desc. */
	int		prsock_io_max_timeout;	/* in milliseconds */
	void		*prsock_appdata;	/* application specific data */
} PRLDAPIOSocketArg;


/*
 * Function prototypes:
 */

/*
 * From ldapprio.c:
 */
int prldap_install_io_functions( LDAP *ld, int shared );
int prldap_session_arg_from_ld( LDAP *ld, PRLDAPIOSessionArg **sessargpp );
int prldap_set_io_max_timeout( PRLDAPIOSessionArg *prsessp,
	int io_max_timeout );
int prldap_get_io_max_timeout( PRLDAPIOSessionArg *prsessp,
	int *io_max_timeoutp );
PRLDAPIOSocketArg *prldap_socket_arg_alloc( PRLDAPIOSessionArg *sessionarg );


/*
 * From ldapprthreads.c:
 */
int prldap_install_thread_functions( LDAP *ld, int shared );
int prldap_thread_new_handle( LDAP *ld, void *sessionarg );
void prldap_thread_dispose_handle( LDAP *ld, void *sessionarg );


/*
 * From ldapprdns.c:
 */
int prldap_install_dns_functions( LDAP *ld );


/*
 * From ldapprerror.c:
 */
void prldap_set_system_errno( int e );
int prldap_get_system_errno( void );
int prldap_prerr2errno( void );
