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
    if ( NULL == name)
    {
        return (name);
    }

	if ( *map == NULL ) {
		if ( (fp = NSLDAPI_FOPEN( filename, "r" )) == NULL )
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
