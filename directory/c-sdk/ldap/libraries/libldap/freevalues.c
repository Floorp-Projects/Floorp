/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/*
 *  Copyright (c) 1990 Regents of the University of Michigan.
 *  All rights reserved.
 */
/*
 *  freevalues.c
 */

#include "ldap-int.h"

void
LDAP_CALL
ldap_value_free( char **vals )
{
	int	i;

	if ( vals == NULL )
		return;
	for ( i = 0; vals[i] != NULL; i++ )
		NSLDAPI_FREE( vals[i] );
	NSLDAPI_FREE( (char *) vals );
}

void
LDAP_CALL
ldap_value_free_len( struct berval **vals )
{
	int	i;

	if ( vals == NULL )
		return;
	for ( i = 0; vals[i] != NULL; i++ ) {
		NSLDAPI_FREE( vals[i]->bv_val );
		NSLDAPI_FREE( vals[i] );
	}
	NSLDAPI_FREE( (char *) vals );
}
