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
/* charray.c - routines for dealing with char * arrays */


#include "ldap-int.h" 
 
/*
 * Add s at the end of the array of strings *a.
 * Return 0 for success, -1 for failure.
 */
int
LDAP_CALL
ldap_charray_add(
    char	***a,
    char	*s
)
{
	int	n;

	if ( *a == NULL ) {
		*a = (char **)NSLDAPI_MALLOC( 2 * sizeof(char *) );
		if ( *a == NULL ) {
			return -1;
		}
		n = 0;
	} else {
		for ( n = 0; *a != NULL && (*a)[n] != NULL; n++ ) {
			;	/* NULL */
		}

		*a = (char **)NSLDAPI_REALLOC( (char *) *a,
		    (n + 2) * sizeof(char *) );
		if ( *a == NULL ) {
			return -1;
		}
	}

	(*a)[n++] = s;
	(*a)[n] = NULL;
	return 0;
}

/*
 * Add array of strings s at the end of the array of strings *a.
 * Return 0 for success, -1 for failure.
 */
int
LDAP_CALL
ldap_charray_merge(
    char	***a,
    char	**s
)
{
	int	i, n, nn;

	if ( (s == NULL) || (s[0] == NULL) )
	    return 0;

	for ( n = 0; *a != NULL && (*a)[n] != NULL; n++ ) {
		;	/* NULL */
	}
	for ( nn = 0; s[nn] != NULL; nn++ ) {
		;	/* NULL */
	}

	*a = (char **)NSLDAPI_REALLOC( (char *) *a,
	    (n + nn + 1) * sizeof(char *) );
	if ( *a == NULL ) {
		return -1;
	}

	for ( i = 0; i < nn; i++ ) {
		(*a)[n + i] = s[i];
	}
	(*a)[n + nn] = NULL;
	return 0;
}

void
LDAP_CALL
ldap_charray_free( char **array )
{
	char	**a;

	if ( array == NULL ) {
		return;
	}

	for ( a = array; *a != NULL; a++ ) {
		if ( *a != NULL ) {
			NSLDAPI_FREE( *a );
		}
	}
	NSLDAPI_FREE( (char *) array );
}

int
LDAP_CALL
ldap_charray_inlist(
    char	**a,
    char	*s
)
{
	int	i;

	if ( a == NULL )
		return( 0 );

	for ( i = 0; a[i] != NULL; i++ ) {
		if ( strcasecmp( s, a[i] ) == 0 ) {
			return( 1 );
		}
	}

	return( 0 );
}

/*
 * Duplicate the array of strings a, return NULL upon any memory failure.
 */
char **
LDAP_CALL
ldap_charray_dup( char **a )
{
	int	i;
	char	**new;

	for ( i = 0; a[i] != NULL; i++ )
		;	/* NULL */

	new = (char **)NSLDAPI_MALLOC( (i + 1) * sizeof(char *) );
	if ( new == NULL ) {
		return NULL;
	}

	for ( i = 0; a[i] != NULL; i++ ) {
		new[i] = nsldapi_strdup( a[i] );
		if ( new[i] == NULL ) {
			int	j;

			for ( j = 0; j < i; j++ )
			    NSLDAPI_FREE( new[j] );
			NSLDAPI_FREE( new );
			return NULL;
		}
	}
	new[i] = NULL;

	return( new );
}

/*
 * Tokenize the string str, return NULL upon any memory failure.
 * XXX: on many platforms this function is not thread safe because it
 *	uses strtok().
 */
char **
LDAP_CALL
ldap_str2charray( char *str, char *brkstr )
     /* This implementation fails if brkstr contains multibyte characters.
        But it works OK if str is UTF-8 and brkstr is 7-bit ASCII.
      */
{
	char	**res;
	char	*s;
	int	i;

	i = 1;
	for ( s = str; *s; s++ ) {
		if ( strchr( brkstr, *s ) != NULL ) {
			i++;
		}
	}

	res = (char **)NSLDAPI_MALLOC( (i + 1) * sizeof(char *) );
	if ( res == NULL ) {
		return NULL;
	}
	i = 0;
	for ( s = strtok( str, brkstr ); s != NULL; s = strtok( NULL,
	    brkstr ) ) {
		res[i++] = nsldapi_strdup( s );
		if ( res[i - 1] == NULL ) {
			int	j;

			for ( j = 0; j < (i - 1); j++ )
			    NSLDAPI_FREE( res[j] );
			NSLDAPI_FREE( res );
			return NULL;
		}
	}
	res[i] = NULL;

	return( res );
}

int
LDAP_CALL
ldap_charray_position( char **a, char *s )
{
	int     i;

	for ( i = 0; a[i] != NULL; i++ ) {
		if ( strcasecmp( s, a[i] ) == 0 ) {
			return( i );
		}
	}

	return( -1 );
}
