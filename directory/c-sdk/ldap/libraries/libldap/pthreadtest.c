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
#include <stdio.h>
#include <malloc.h>
#include <errno.h>
#include <pthread.h>
#include <ldap.h>

#define NAME		"cn=Directory Manager"
#define PASSWORD	"secret99"
#define BASE		"o=Airius.com"

static void *search_thread();
static void *modify_thread();
static void *add_thread();
static void *delete_thread();
static void *my_mutex_alloc();
static void my_mutex_free();
static void set_ld_error();
static int  get_ld_error();
static void set_errno();
static int  get_errno();
static void tsd_setup();

typedef struct ldapmsgwrapper {
    LDAPMessage			*lmw_messagep;
    struct ldapmsgwrapper	*lmw_next;
} ldapmsgwrapper;


LDAP		*ld;
pthread_key_t	key;

main( int argc, char **argv )
{
	pthread_attr_t	attr;
	pthread_t	search_tid, search_tid2, search_tid3, search_tid4;
	pthread_t	modify_tid, add_tid, delete_tid;
	void		*status;
	struct ldap_thread_fns	tfns;

	if ( argc != 3 ) {
		fprintf( stderr, "usage: %s host port\n", argv[0] );
		exit( 1 );
	}

	if ( pthread_key_create( &key, free ) != 0 ) {
		perror( "pthread_key_create" );
	}
	tsd_setup();

	if ( (ld = ldap_init( argv[1], atoi( argv[2] ) )) == NULL ) {
		perror( "ldap_open" );
		exit( 1 );
	}
	/* set mutex pointers */
	memset( &tfns, '\0', sizeof(struct ldap_thread_fns) );
	tfns.ltf_mutex_alloc = (void *(*)(void)) my_mutex_alloc;
	tfns.ltf_mutex_free = (void (*)(void *)) my_mutex_free;
	tfns.ltf_mutex_lock = (int (*)(void *)) pthread_mutex_lock;
	tfns.ltf_mutex_unlock = (int (*)(void *)) pthread_mutex_unlock;
	tfns.ltf_get_errno = get_errno;
	tfns.ltf_set_errno = set_errno;
	tfns.ltf_get_lderrno = get_ld_error;
	tfns.ltf_set_lderrno = set_ld_error;
	tfns.ltf_lderrno_arg = NULL;
	/* set ld_errno pointers */
	if ( ldap_set_option( ld, LDAP_OPT_THREAD_FN_PTRS, (void *) &tfns )
	    != 0 ) {
		ldap_perror( ld, "ldap_set_option: thread pointers" );
	}
	if ( ldap_simple_bind_s( ld, NAME, PASSWORD ) != LDAP_SUCCESS ) {
		ldap_perror( ld, "ldap_simple_bind_s" );
		exit( 1 );
	}

	if ( pthread_attr_init( &attr ) != 0 ) {
		perror( "pthread_attr_init" );
		exit( 1 );
	}
	pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_JOINABLE );
	if ( pthread_create( &search_tid, &attr, search_thread, "1" ) != 0 ) {
		perror( "pthread_create search_thread" );
		exit( 1 );
	}
	if ( pthread_create( &modify_tid, &attr, modify_thread, "2" ) != 0 ) {
		perror( "pthread_create modify_thread" );
		exit( 1 );
	}
	if ( pthread_create( &search_tid2, &attr, search_thread, "3" ) != 0 ) {
		perror( "pthread_create search_thread2" );
		exit( 1 );
	}
	if ( pthread_create( &add_tid, &attr, add_thread, "4" ) != 0 ) {
		perror( "pthread_create add_thread" );
		exit( 1 );
	}
	if ( pthread_create( &search_tid3, &attr, search_thread, "5" ) != 0 ) {
		perror( "pthread_create search_thread3" );
		exit( 1 );
	}
	if ( pthread_create( &delete_tid, &attr, delete_thread, "6" ) != 0 ) {
		perror( "pthread_create delete_thread" );
		exit( 1 );
	}
	if ( pthread_create( &search_tid4, &attr, search_thread, "7" ) != 0 ) {
		perror( "pthread_create search_thread4" );
		exit( 1 );
	}
	pthread_join( search_tid2, &status );
	pthread_join( search_tid3, &status );
	pthread_join( search_tid4, &status );
	pthread_join( modify_tid, &status );
	pthread_join( add_tid, &status );
	pthread_join( delete_tid, &status );
	pthread_join( search_tid, &status );
}

static void *
search_thread( char *id )
{
	LDAPMessage	*res;
	LDAPMessage	*e;
	char		*a;
	char		**v;
	char		*dn;
	BerElement	*ber;
	int		i, rc, msgid;
	void		*tsd;

	printf( "search_thread\n" );
	tsd_setup();
	for ( ;; ) {
		printf( "%sSearching...\n", id );
		if ( (msgid = ldap_search( ld, BASE, LDAP_SCOPE_SUBTREE,
		    "(objectclass=*)", NULL, 0 )) == -1 ) {
			ldap_perror( ld, "ldap_search_s" );
			continue;
		}
		while ( (rc = ldap_result( ld, msgid, 0, NULL, &res ))
		    == LDAP_RES_SEARCH_ENTRY ) {
			for ( e = ldap_first_entry( ld, res ); e != NULL;
			    e = ldap_next_entry( ld, e ) ) {
				dn = ldap_get_dn( ld, e );
				/* printf( "%sdn: %s\n", id, dn ); */
				free( dn );
				for ( a = ldap_first_attribute( ld, e, &ber );
				    a != NULL; a = ldap_next_attribute( ld, e,
				    ber ) ) {
					v = ldap_get_values( ld, e, a );
					for ( i = 0; v && v[i] != 0; i++ ) {
						/*
						printf( "%s%s: %s\n", id, a,
						    v[i] );
						*/
					}
					ldap_value_free( v );
					free( a );
				}
			}
			ldap_msgfree( res );
			/* printf( "%s\n", id ); */
		}
		if ( rc == -1 || ldap_result2error( ld, res, 0 ) !=
		    LDAP_SUCCESS ) {
			ldap_perror( ld, "ldap_search" );
		} else {
			printf( "%sDone with one round\n", id );
		}
	}
}

static void *
modify_thread( char *id )
{
	LDAPMessage	*res;
	LDAPMessage	*e;
	int		i, modentry, entries, msgid, rc;
	LDAPMod		mod;
	LDAPMod		*mods[2];
	char		*vals[2];
	char		*dn;
	ldapmsgwrapper	*list, *lmwp, *lastlmwp;

	printf( "modify_thread\n" );
	tsd_setup();
	if ( (msgid = ldap_search( ld, BASE, LDAP_SCOPE_SUBTREE,
	    "(objectclass=*)", NULL, 0 )) == -1 ) {
		ldap_perror( ld, "ldap_search_s" );
		exit( 1 );
	}
	entries = 0;
	list = lastlmwp = NULL;
	while ( (rc = ldap_result( ld, msgid, 0, NULL, &res ))
	    == LDAP_RES_SEARCH_ENTRY ) {
		entries++;
		if (( lmwp = (ldapmsgwrapper *)
		    malloc( sizeof( ldapmsgwrapper ))) == NULL ) {
			perror( "modify_thread: malloc" );
			exit( 1 );
		}
		lmwp->lmw_messagep = res;
		lmwp->lmw_next = NULL;
		if ( lastlmwp == NULL ) {
		    list = lastlmwp = lmwp;
		} else {
		    lastlmwp->lmw_next = lmwp;
		}
		lastlmwp = lmwp;
	}
	if ( rc == -1 || ldap_result2error( ld, res, 0 ) != LDAP_SUCCESS ) {
		ldap_perror( ld, "modify_thread: ldap_search" );
		exit( 1 );
	} else {
		entries++;
		printf( "%sModify got %d entries\n", id, entries );
	}

	mods[0] = &mod;
	mods[1] = NULL;
	vals[0] = "bar";
	vals[1] = NULL;
	for ( ;; ) {
		modentry = rand() % entries;
		for ( i = 0, lmwp = list; lmwp != NULL && i < modentry;
		    i++, lmwp = lmwp->lmw_next ) {
			/* NULL */
		}

		if ( lmwp == NULL ) {
			fprintf( stderr,
			    "%sModify could not find entry %d of %d\n",
			    id, modentry, entries );
			continue;
		}
		e = lmwp->lmw_messagep;
		printf( "%sPicked entry %d of %d\n", id, i, entries );
		dn = ldap_get_dn( ld, e );
		mod.mod_op = LDAP_MOD_REPLACE;
		mod.mod_type = "description";
		mod.mod_values = vals;
		printf( "%sModifying (%s)\n", id, dn );
		if ( ldap_modify_s( ld, dn, mods ) != LDAP_SUCCESS ) {
			ldap_perror( ld, "ldap_modify_s" );
		}
		free( dn );
	}
}

static void *
add_thread( char *id )
{
	LDAPMod	mod[5];
	LDAPMod	*mods[6];
	char	dn[BUFSIZ], name[40];
	char	*cnvals[2], *snvals[2], *ocvals[3];
	int	i;

	printf( "add_thread\n" );
	tsd_setup();
	for ( i = 0; i < 5; i++ ) {
		mods[i] = &mod[i];
	}
	mods[5] = NULL;
	mod[0].mod_op = 0;
	mod[0].mod_type = "cn";
	mod[0].mod_values = cnvals;
	cnvals[1] = NULL;
	mod[1].mod_op = 0;
	mod[1].mod_type = "sn";
	mod[1].mod_values = snvals;
	snvals[1] = NULL;
	mod[2].mod_op = 0;
	mod[2].mod_type = "objectclass";
	mod[2].mod_values = ocvals;
	ocvals[0] = "top";
	ocvals[1] = "person";
	ocvals[2] = NULL;
	mods[3] = NULL;

	for ( ;; ) {
		sprintf( name, "%d", rand() );
		sprintf( dn, "cn=%s, " BASE, name );
		cnvals[0] = name;
		snvals[0] = name;

		printf( "%sAdding entry (%s)\n", id, dn );
		if ( ldap_add_s( ld, dn, mods ) != LDAP_SUCCESS ) {
			ldap_perror( ld, "ldap_add_s" );
		}
	}
}

static void *
delete_thread( char *id )
{
	LDAPMessage	*res;
	char		dn[BUFSIZ], name[40];
	int		entries, msgid, rc;

	printf( "delete_thread\n" );
	tsd_setup();
	if ( (msgid = ldap_search( ld, BASE, LDAP_SCOPE_SUBTREE,
	    "(objectclass=*)", NULL, 0 )) == -1 ) {
		ldap_perror( ld, "delete_thread: ldap_search_s" );
		exit( 1 );
	}
	entries = 0;
	while ( (rc = ldap_result( ld, msgid, 0, NULL, &res ))
	    == LDAP_RES_SEARCH_ENTRY ) {
		entries++;
		ldap_msgfree( res );
	}
	entries++;
	if ( rc == -1 || ldap_result2error( ld, res, 0 ) != LDAP_SUCCESS ) {
		ldap_perror( ld, "delete_thread: ldap_search" );
	} else {
		printf( "%sDelete got %d entries\n", id, entries );
	}

	for ( ;; ) {
		sprintf( name, "%d", rand() );
		sprintf( dn, "cn=%s, " BASE, name );

		printf( "%sDeleting entry (%s)\n", id, dn );
		if ( ldap_delete_s( ld, dn ) != LDAP_SUCCESS ) {
			ldap_perror( ld, "ldap_delete_s" );
		}
	}
}

static void *
my_mutex_alloc( void )
{
	pthread_mutex_t	*mutexp;

	if ( (mutexp = malloc( sizeof(pthread_mutex_t) )) != NULL ) {
		pthread_mutex_init( mutexp, NULL );
	}

	return( mutexp );
}

static void
my_mutex_free( void *mutexp )
{
	pthread_mutex_destroy( (pthread_mutex_t *) mutexp );
}

struct ldap_error {
	int	le_errno;
	char	*le_matched;
	char	*le_errmsg;
};

static void
tsd_setup()
{
	void	*tsd;

	tsd = pthread_getspecific( key );
	if ( tsd != NULL ) {
		fprintf( stderr, "tsd non-null!\n" );
		pthread_exit( NULL );
	}
	tsd = (void *) calloc( 1, sizeof(struct ldap_error) );
	pthread_setspecific( key, tsd );
}

static void
set_ld_error( int err, char *matched, char *errmsg, void *dummy )
{
	struct ldap_error *le;

	le = pthread_getspecific( key );
	le->le_errno = err;
	if ( le->le_matched != NULL ) {
		ldap_memfree( le->le_matched );
	}
	le->le_matched = matched;
	if ( le->le_errmsg != NULL ) {
		ldap_memfree( le->le_errmsg );
	}
	le->le_errmsg = errmsg;
}

static int
get_ld_error( char **matched, char **errmsg, void *dummy )
{
	struct ldap_error *le;

	le = pthread_getspecific( key );
	if ( matched != NULL ) {
		*matched = le->le_matched;
	}
	if ( errmsg != NULL ) {
		*errmsg = le->le_errmsg;
	}
	return( le->le_errno );
}

static void
set_errno( int err )
{
	errno = err;
}

static int
get_errno( void )
{
	return( errno );
}
