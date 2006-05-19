/*
 * Copyright 2005 Sun Microsystems, Inc. All Rights Reserved
 * Use of this product is subject to license terms.
 */

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
 * Demonstrate use of the create filter function.
 */

#include "examples.h"

#define FILT_BUFSIZ	4096

struct filt_words {
    char	**f_words;
    char	*f_internal;
};

static int get_string_input( const char *prompt, const char *defaultval,
    char *resultbuf );
static struct filt_words *val2words( const char *value, const char *delims );
static void freewords( struct filt_words *words );


int
main( int argc, char **argv )
{
    int			rc;
    char		patbuf[ FILT_BUFSIZ ];
    char		attrbuf[ FILT_BUFSIZ ], valuebuf[ FILT_BUFSIZ ];
    char		filtbuf[ FILT_BUFSIZ ];
    struct filt_words	*fwords;

    patbuf[0] = attrbuf[0] = valuebuf[0] = '\0';

    while ( 1 ) {
	if ( get_string_input( "Enter a filter pattern: [%s] ", patbuf,
		patbuf ) != 0 ) {
	    break;
	}

	if ( get_string_input( "Enter an attribute type: [%s] ", attrbuf,
		attrbuf ) != 0 ) {
	    break;
	}

	if ( get_string_input( "Enter a value: [%s] ", valuebuf,
		valuebuf ) != 0 ) {
	    break;
	}

	fwords = val2words( valuebuf, " " );
	rc = ldap_create_filter( filtbuf, sizeof( filtbuf ), patbuf,
		NULL, NULL, attrbuf, valuebuf, fwords->f_words );
	freewords( fwords );
	
	if ( rc != LDAP_SUCCESS ) {
	    fprintf( stderr, "ldap_create_filter: failed (%d - %s)\n", rc,
		    ldap_err2string( rc ));
	} else {
	    printf( "Resulting filter: %s\n", filtbuf );
	}
	putchar( '\n' );
    }

    return( 0 );
}


/*
 * Prompt the user for a string.  The entered string is placed in resultbuf.
 * If a zero-length string is entered, i.e., if they just hit return, the
 * contents of defaultval are copied to resultbuf.
 * Returns 0 if all goes well and -1 if error or end of file.
 */
static int
get_string_input( const char *prompt, const char *defaultval, char *resultbuf )
{
    char inbuf[ FILT_BUFSIZ ];

    inbuf[0] = '\0';
    printf( prompt, defaultval );
    if ( fgets( inbuf, sizeof( inbuf ), stdin ) == NULL ) {
	return( -1 );
    }
    inbuf[ strlen( inbuf ) - 1 ] = '\0';	/* strip trailing newline */
    if ( inbuf[ 0 ] == '\0' ) {			/* use default value */
	if ( defaultval != resultbuf ) {
	    strcpy( resultbuf, defaultval );
	}
    } else {					/* use newly entered value */
	strcpy( resultbuf, inbuf );
    }

    return( 0 );
}


static struct filt_words *
val2words( const char *value, const char *delims )
{
    struct filt_words	*fw;
    char		*word;
    int			i;

    if (( fw = calloc( 1, sizeof( struct filt_words ))) == NULL ||
	    ( fw->f_internal = strdup( value )) == NULL ) {
	perror( "calloc OR strdup" );
	exit( 1 );
    }

    word = strtok( fw->f_internal, delims );
    i = 0;

    while ( word != NULL ) {
	if ( fw->f_words == NULL ) {
	    fw->f_words = (char **)malloc( (i + 2 ) * sizeof( char * ));
	} else {
	    fw->f_words = (char **)realloc( fw->f_words,
		    (i + 2 ) * sizeof( char * ));
	}

	if ( fw->f_words == NULL ) {
	    perror( "malloc OR realloc" );
	    exit( 1 );
	}

	fw->f_words[ i ] = word;
	fw->f_words[ ++i ] = NULL;
	word = strtok( NULL, delims );
    }

    if ( i > 0 ) {
	fw->f_words[ i ] = NULL;
    }

    return( fw );
}


static void
freewords( struct filt_words *words )
{
    if ( words != NULL ) {
	if ( words->f_words != NULL ) {
	    free( words->f_words );
	}
	if ( words->f_internal != NULL ) {
	    free( words->f_internal );
	}
	free( words );
    }
}
