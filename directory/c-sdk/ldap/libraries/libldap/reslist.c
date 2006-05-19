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
 *  reslist.c
 */

#if 0
#ifndef lint 
static char copyright[] = "@(#) Copyright (c) 1990 Regents of the University of Michigan.\nAll rights reserved.\n";
#endif
#endif

#include "ldap-int.h"

LDAPMessage *
ldap_delete_result_entry( LDAPMessage **list, LDAPMessage *e )
{
	LDAPMessage	*tmp, *prev = NULL;

	for ( tmp = *list; tmp != NULL && tmp != e; tmp = tmp->lm_chain )
		prev = tmp;

	if ( tmp == NULL )
		return( NULL );

	if ( prev == NULL )
		*list = tmp->lm_chain;
	else
		prev->lm_chain = tmp->lm_chain;
	tmp->lm_chain = NULL;

	return( tmp );
}

void
ldap_add_result_entry( LDAPMessage **list, LDAPMessage *e )
{
	e->lm_chain = *list;
	*list = e;
}
