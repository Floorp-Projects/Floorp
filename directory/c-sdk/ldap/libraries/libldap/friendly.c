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
 *  friendly.c
 */

#if 0
#ifndef lint 
static char copyright[] = "@(#) Copyright (c) 1993 Regents of the University of Michigan.\nAll rights reserved.\n";
#endif
#endif

#include "ldap-int.h"

char *
LDAP_CALL
ldap_friendly_name( char *filename, char *name, FriendlyMap *map )
{
	int	i, entries;
	FILE	*fp;
	char	*s;
	char	buf[BUFSIZ];

	if ( map == NULL ) {
		return( name );
	}

	if ( *map == NULL ) {
		if ( (fp = fopen( filename, "r" )) == NULL )
			return( name );

		entries = 0;
		while ( fgets( buf, sizeof(buf), fp ) != NULL ) {
			if ( buf[0] != '#' )
				entries++;
		}
		rewind( fp );

		if ( (*map = (FriendlyMap)NSLDAPI_MALLOC( (entries + 1) *
		    sizeof(struct friendly) )) == NULL ) {
			fclose( fp );
			return( name );
		}

		i = 0;
		while ( fgets( buf, sizeof(buf), fp ) != NULL && i < entries ) {
			if ( buf[0] == '#' )
				continue;

			if ( (s = strchr( buf, '\n' )) != NULL )
				*s = '\0';

			if ( (s = strchr( buf, '\t' )) == NULL )
				continue;
			*s++ = '\0';

			if ( *s == '"' ) {
				int	esc = 0, found = 0;

				for ( ++s; *s && !found; s++ ) {
					switch ( *s ) {
					case '\\':
						esc = 1;
						break;
					case '"':
						if ( !esc )
							found = 1;
						/* FALL */
					default:
						esc = 0;
						break;
					}
				}
			}

			(*map)[i].f_unfriendly = nsldapi_strdup( buf );
			(*map)[i].f_friendly = nsldapi_strdup( s );
			i++;
		}

		fclose( fp );
		(*map)[i].f_unfriendly = NULL;
	}

	for ( i = 0; (*map)[i].f_unfriendly != NULL; i++ ) {
		if ( strcasecmp( name, (*map)[i].f_unfriendly ) == 0 )
			return( (*map)[i].f_friendly );
	}
	return( name );
}


void
LDAP_CALL
ldap_free_friendlymap( FriendlyMap *map )
{
	struct friendly* pF;

	if ( map == NULL || *map == NULL ) {
		return;
	}

	for ( pF = *map; pF->f_unfriendly; pF++ ) {
		NSLDAPI_FREE( pF->f_unfriendly );
		NSLDAPI_FREE( pF->f_friendly );
	}
	NSLDAPI_FREE( *map );
	*map = NULL;
}
