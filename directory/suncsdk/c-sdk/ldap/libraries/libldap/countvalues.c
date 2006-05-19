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
 *  Copyright (c) 1990 Regents of the University of Michigan.
 *  All rights reserved.
 */
/*
 *  countvalues.c
 */

#include "ldap-int.h"

int
LDAP_CALL
ldap_count_values( char **vals )
{
	int	i;

	if ( vals == NULL )
		return( 0 );

	for ( i = 0; vals[i] != NULL; i++ )
		;	/* NULL */

	return( i );
}

int
LDAP_CALL
ldap_count_values_len( struct berval **vals )
{
	return( ldap_count_values( (char **) vals ) );
}
