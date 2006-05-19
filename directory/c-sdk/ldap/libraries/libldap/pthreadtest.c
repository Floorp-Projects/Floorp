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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <values.h>
#include <errno.h>
#include <pthread.h>
#include <synch.h>

#include <ldap.h>

/* Authentication and search information. */
#define NAME		"cn=Directory Manager"
#define PASSWORD	"rtfm11111"
#define BASE		"dc=example,dc=com"
#define SCOPE		LDAP_SCOPE_SUBTREE

static void *modify_thread();
static void *add_thread();
static void *delete_thread();
static void *bind_thread();
static void *compare_thread();
static void *search_thread();
static void *my_mutex_alloc();
static void my_mutex_free();
void *my_sema_alloc( void );
void my_sema_free( void * );
int my_sema_wait( void * );
int my_sema_post( void * );
static void set_ld_error();
static int  get_ld_error();
static void set_errno();
static int  get_errno();
static void tsd_setup();
static void tsd_cleanup();
static int get_random_id( void );
static char *get_id_str( int id );

/* Linked list of LDAPMessage structs for search results. */
typedef struct ldapmsgwrapper {
    LDAPMessage			*lmw_messagep;
    struct ldapmsgwrapper	*lmw_next;
} ldapmsgwrapper;

LDAP		*ld;
pthread_key_t	key;
int		maxid = MAXINT;
int		maxops = 0;		/* zero means no limit */
int		range_filters = 0;	/* if non-zero use >= and >= filters */

main( int argc, char **argv )

{
	pthread_attr_t			attr;
	pthread_t			*threadids;
	void				*status;
	struct ldap_thread_fns		tfns;
	struct ldap_extra_thread_fns	extrafns;
	int		rc, c, errflg, i, inited_attr;
	int		doadd, dodelete, domodify, docompare, dosearch, dobind;
	int		option_extthreads, option_restart;
	int		each_thread_count, thread_count;
	extern int	optind;
	extern char	*optarg;

	doadd = dodelete = domodify = docompare = dobind = dosearch = 0;
	option_extthreads = option_restart = 0;
	inited_attr = 0;
	errflg = 0;
	each_thread_count = 1;	/* how many of each type of thread? */
	rc = LDAP_SUCCESS;	/* optimistic */

	while (( c = getopt( argc, argv, "abcdmrsERi:n:o:S:" )) != EOF ) {
		switch( c ) {
		case 'a':		/* perform add operations */
			++doadd;
			break;
		case 'b':		/* perform bind operations */
			++dobind;
			break;
		case 'c':		/* perform compare operations */
			++docompare;
			break;
		case 'd':		/* perform delete operations */
			++dodelete;
			break;
		case 'm':		/* perform modify operations */
			++domodify;
			break;
		case 'r':		/* use range filters in searches */
			++range_filters;
			break;
		case 's':		/* perform search operations */
			++dosearch;
			break;
		case 'E':		/* use extended thread functions */
			++option_extthreads;
			break;
		case 'R':		/* turn on LDAP_OPT_RESTART */
			++option_restart;
			break;
		case 'i':		/* highest # used for entry names */
			maxid = atoi( optarg );
			break;
		case 'n':		/* # threads for each operation */
			if (( each_thread_count = atoi( optarg )) < 1 ) {
				fprintf( stderr, "thread count must be > 0\n" );
				++errflg;
			}
			break;
		case 'o':		/* operations to perform per thread */
			if (( maxops = atoi( optarg )) < 0 ) {
				fprintf( stderr,
				    "operation limit must be >= 0\n" );
				++errflg;
			}
			break;
		case 'S':		/* random number seed */
			if ( *optarg == 'r' ) {
				int	seed = (int)time( (time_t *)0 );
				srandom( seed );
				printf( "Random seed: %d\n", seed );
			} else {
				srandom( atoi( optarg ));
			}
			break;
		default:
			++errflg;
			break;
		}
	}
	
	/* Check command-line syntax. */
	thread_count = each_thread_count * ( doadd + dodelete + domodify
	    + dobind + docompare + dosearch );
	if ( thread_count < 1 ) {
		fprintf( stderr,
		    "Specify at least one of -a, -b, -c, -d, -m, or -s\n" );
		++errflg;
	}

	if ( errflg || argc - optind != 2 ) {
		fprintf( stderr, "usage: %s [-abcdmrsER] [-i id] [-n thr]"
		    " [-o oplim] [-S sd] host port\n", argv[0] );
		fputs( "\nWhere:\n"
		    "\t\"id\" is the highest entry id (name) to use"
			" (default is MAXINT)\n"
		    "\t\"thr\" is the number of threads for each operation"
			" (default is 1)\n"
		    "\t\"oplim\" is the number of ops done by each thread"
			" (default is infinite)\n"
		    "\t\"sd\" is a random() number seed (default is 1).  Use"
			" the letter r for\n"
			"\t\tsd to seed random() using time of day.\n"
		    "\t\"host\" is the hostname of an LDAP directory server\n"
		    "\t\"port\" is the TCP port of the LDAP directory server\n"
		    , stderr );
		fputs( "\nAnd the single character options are:\n"
		    "\t-a\tstart thread(s) to perform add operations\n"
		    "\t-b\tstart thread(s) to perform bind operations\n"
		    "\t-c\tstart thread(s) to perform compare operations\n"
		    "\t-d\tstart thread(s) to perform delete operations\n"
		    "\t-m\tstart thread(s) to perform modify operations\n"
		    "\t-s\tstart thread(s) to perform search operations\n"
		    "\t-r\tuse range filters in searches\n"
		    "\t-E\tinstall LDAP_OPT_EXTRA_THREAD_FN_PTRS\n"
		    "\t-R\tturn on LDAP_OPT_RESTART\n", stderr );

		return( LDAP_PARAM_ERROR );
	}

	/* Create a key. */
	if ( pthread_key_create( &key, free ) != 0 ) {
		perror( "pthread_key_create" );
	}
	tsd_setup();

	/* Allocate space for thread ids */
	if (( threadids = (pthread_t *)calloc( thread_count,
	    sizeof( pthread_t ))) == NULL ) {
		rc = LDAP_LOCAL_ERROR;
		goto clean_up_and_return;
	}


	/* Initialize the LDAP session. */
	if (( ld = ldap_init( argv[optind], atoi( argv[optind+1] ))) == NULL ) {
		perror( "ldap_init" );
		rc = LDAP_LOCAL_ERROR;
		goto clean_up_and_return;
	}

	/* Set the function pointers for dealing with mutexes
	   and error information. */
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

	/* Set up this session to use those function pointers. */

	rc = ldap_set_option( ld, LDAP_OPT_THREAD_FN_PTRS, (void *) &tfns );
	if ( rc < 0 ) {
		rc = ldap_get_lderrno( ld, NULL, NULL );
		fprintf( stderr,
		    "ldap_set_option (LDAP_OPT_THREAD_FN_PTRS): %s\n",
		    ldap_err2string( rc ) );
		goto clean_up_and_return;
	}

	if ( option_extthreads ) {
		/* Set the function pointers for working with semaphores. */

		memset( &extrafns, '\0', sizeof(struct ldap_extra_thread_fns) );
		extrafns.ltf_mutex_trylock =
		    (int (*)(void *)) pthread_mutex_trylock;
		extrafns.ltf_sema_alloc = (void *(*)(void)) my_sema_alloc;
		extrafns.ltf_sema_free = (void (*)(void *)) my_sema_free;
		extrafns.ltf_sema_wait = (int (*)(void *)) my_sema_wait;
		extrafns.ltf_sema_post = (int (*)(void *)) my_sema_post;

		/* Set up this session to use those function pointers. */

		if ( ldap_set_option( ld, LDAP_OPT_EXTRA_THREAD_FN_PTRS,
		    (void *) &extrafns ) != 0 ) {
			rc = ldap_get_lderrno( ld, NULL, NULL );
			ldap_perror( ld, "ldap_set_option"
			    " (LDAP_OPT_EXTRA_THREAD_FN_PTRS)" );
			goto clean_up_and_return;
		}
	}


	if ( option_restart && ldap_set_option( ld, LDAP_OPT_RESTART,
	    LDAP_OPT_ON ) != 0 ) {
		rc = ldap_get_lderrno( ld, NULL, NULL );
		ldap_perror( ld, "ldap_set_option(LDAP_OPT_RESTART)" );
		goto clean_up_and_return;
	}

	/* Attempt to bind to the server. */
	rc = ldap_simple_bind_s( ld, NAME, PASSWORD );
	if ( rc != LDAP_SUCCESS ) {
		fprintf( stderr, "ldap_simple_bind_s: %s\n",
		    ldap_err2string( rc ) );
		goto clean_up_and_return;
	}

	/* Initialize the attribute. */
	if ( pthread_attr_init( &attr ) != 0 ) {
		perror( "pthread_attr_init" );
		rc = LDAP_LOCAL_ERROR;
		goto clean_up_and_return;
	}
	++inited_attr;

	/* Specify that the threads are joinable. */
	pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_JOINABLE );

	/* Create all the requested threads */
	thread_count = 0;
	if ( domodify ) {
		for ( i = 0; i < each_thread_count; ++i ) {
			if ( pthread_create( &threadids[thread_count], &attr,
			    modify_thread, get_id_str(thread_count) ) != 0 ) {
				perror( "pthread_create modify_thread" );
			    rc = LDAP_LOCAL_ERROR;
			    goto clean_up_and_return;
			}
			++thread_count;
		}
	}

	if ( doadd ) {
		for ( i = 0; i < each_thread_count; ++i ) {
			if ( pthread_create( &threadids[thread_count], &attr,
			    add_thread, get_id_str(thread_count) ) != 0 ) {
				perror( "pthread_create add_thread" );
			    rc = LDAP_LOCAL_ERROR;
			    goto clean_up_and_return;
			}
			++thread_count;
		}
	}

	if ( dodelete ) {
		for ( i = 0; i < each_thread_count; ++i ) {
			if ( pthread_create( &threadids[thread_count], &attr,
			    delete_thread, get_id_str(thread_count) ) != 0 ) {
				perror( "pthread_create delete_thread" );
			    rc = LDAP_LOCAL_ERROR;
			    goto clean_up_and_return;
			}
			++thread_count;
		}
	}

	if ( dobind ) {
		for ( i = 0; i < each_thread_count; ++i ) {
			if ( pthread_create( &threadids[thread_count], &attr,
			    bind_thread, get_id_str(thread_count) ) != 0 ) {
				perror( "pthread_create bind_thread" );
			    rc = LDAP_LOCAL_ERROR;
			    goto clean_up_and_return;
			}
			++thread_count;
		}
	}

	if ( docompare ) {
		for ( i = 0; i < each_thread_count; ++i ) {
			if ( pthread_create( &threadids[thread_count], &attr,
			    compare_thread, get_id_str(thread_count) ) != 0 ) {
				perror( "pthread_create compare_thread" );
			    rc = LDAP_LOCAL_ERROR;
			    goto clean_up_and_return;
			}
			++thread_count;
		}
	}

	if ( dosearch ) {
		for ( i = 0; i < each_thread_count; ++i ) {
			if ( pthread_create( &threadids[thread_count], &attr,
			    search_thread, get_id_str(thread_count) ) != 0 ) {
				perror( "pthread_create search_thread" );
			    rc = LDAP_LOCAL_ERROR;
			    goto clean_up_and_return;
			}
			++thread_count;
		}
	}

	/* Wait until these threads exit. */
	for ( i = 0; i < thread_count; ++i ) {
		pthread_join( threadids[i], &status );
	}

clean_up_and_return:
	if ( ld != NULL ) {
		set_ld_error( 0, NULL, NULL, NULL );  /* disposes of memory */
		ldap_unbind( ld );
	}
	if ( threadids != NULL ) {
		free( threadids );
	}
	if ( inited_attr ) {
		pthread_attr_destroy( &attr );
	}
	tsd_cleanup();

	return( rc );
}


static void *
modify_thread( char *id )
{
	LDAPMessage	*res;
	LDAPMessage	*e;
	int		i, modentry, num_entries, msgid, parse_rc, finished;
	int		rc, opcount;
	LDAPMod		mod;
	LDAPMod		*mods[2];
	char		*vals[2];
	char		*dn;
	ldapmsgwrapper	*list, *lmwp, *lastlmwp;
	struct timeval	zerotime;
	void		*voidrc = (void *)0;

	zerotime.tv_sec = zerotime.tv_usec = 0L;

	printf( "Starting modify_thread %s.\n", id );
	opcount = 0;
	tsd_setup();

	rc = ldap_search_ext( ld, BASE, SCOPE, "(objectclass=*)",
		NULL, 0, NULL, NULL, NULL, LDAP_NO_LIMIT, &msgid );
	if ( rc != LDAP_SUCCESS ) {
		fprintf( stderr, "Thread %s error: Modify thread: "
		"ldap_search_ext: %s\n", id, ldap_err2string( rc ) );
		exit( 1 );
	}
	list = lastlmwp = NULL;
	finished = 0;
	num_entries = 0;
	while ( !finished ) {
		rc = ldap_result( ld, msgid, LDAP_MSG_ONE, &zerotime, &res );
		switch ( rc ) {
		case -1:
			rc = ldap_get_lderrno( ld, NULL, NULL );
			fprintf( stderr, "ldap_result: %s\n", ldap_err2string( rc ) );
			exit( 1 );
			break;
		case 0:
			break;
		/* Keep track of the number of entries found. */
		case LDAP_RES_SEARCH_ENTRY:
			num_entries++;
			if (( lmwp = (ldapmsgwrapper *)
				malloc( sizeof( ldapmsgwrapper ))) == NULL ) {
				fprintf( stderr, "Thread %s: Modify thread: Cannot malloc\n", id );
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
			break;
		case LDAP_RES_SEARCH_REFERENCE:
			break;
		case LDAP_RES_SEARCH_RESULT:
			finished = 1;
			parse_rc = ldap_parse_result( ld, res, &rc, NULL, NULL, NULL, NULL, 1 );
			if ( parse_rc != LDAP_SUCCESS ) {
				fprintf( stderr, "Thread %s error: can't parse result code.\n", id );
				exit( 1 );
			} else {
				if ( rc != LDAP_SUCCESS ) {
					fprintf( stderr, "Thread %s error: ldap_search: %s\n", id, ldap_err2string( rc ) );
				} else {
					printf( "Thread %s: Got %d results.\n", id, num_entries );
				}
			}
			break;
		default:
			break;
		}
	}

	mods[0] = &mod;
	mods[1] = NULL;
	vals[0] = "bar";
	vals[1] = NULL;

	for ( ;; ) {
		modentry = random() % num_entries;
		for ( i = 0, lmwp = list; lmwp != NULL && i < modentry;
		    i++, lmwp = lmwp->lmw_next ) {
			/* NULL */
		}

		if ( lmwp == NULL ) {
			fprintf( stderr,
			    "Thread %s: Modify thread could not find entry %d of %d\n",
			    id, modentry, num_entries );
			continue;
		}

		e = lmwp->lmw_messagep;
		printf( "Thread %s: Modify thread picked entry %d of %d\n", id, i, num_entries );
		dn = ldap_get_dn( ld, e );

		mod.mod_op = LDAP_MOD_REPLACE;
		mod.mod_type = "description";
		mod.mod_values = vals;
		printf( "Thread %s: Modifying (%s)\n", id, dn );

		rc = ldap_modify_ext_s( ld, dn, mods, NULL, NULL );
		if ( rc != LDAP_SUCCESS ) {
			fprintf( stderr, "ldap_modify_ext_s: %s\n",
			    ldap_err2string( rc ) );
			if ( rc == LDAP_SERVER_DOWN ) {
				perror( "ldap_modify_ext_s" );
				voidrc = (void *)1;
				goto modify_cleanup_and_return;
			}
		}
		free( dn );

		++opcount;
		if ( maxops != 0 && opcount >= maxops ) {
			break;
		}
	}

modify_cleanup_and_return:
	printf( "Thread %s: attempted %d modify operations\n", id, opcount );
	set_ld_error( 0, NULL, NULL, NULL );	/* disposes of memory */
	tsd_cleanup();
	free( id );
	return voidrc;
}


static void *
add_thread( char *id )
{
	LDAPMod	mod[4];
	LDAPMod	*mods[5];
	char	dn[BUFSIZ], name[40];
	char	*cnvals[2], *snvals[2], *pwdvals[2], *ocvals[3];
	int	i, rc, opcount;
	void	*voidrc = (void *)0;

	printf( "Starting add_thread %s.\n", id );
	opcount = 0;
	tsd_setup();

	for ( i = 0; i < 4; i++ ) {
		mods[i] = &mod[i];
	}

	mods[4] = NULL;

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
	mod[3].mod_op = 0;
	mod[3].mod_type = "userPassword";
	mod[3].mod_values = pwdvals;
	pwdvals[1] = NULL;
	mods[4] = NULL;

	for ( ;; ) {
		sprintf( name, "%d", get_random_id() );
		sprintf( dn, "cn=%s, " BASE, name );
		cnvals[0] = name;
		snvals[0] = name;
		pwdvals[0] = name;

		printf( "Thread %s: Adding entry (%s)\n", id, dn );
		rc = ldap_add_ext_s( ld, dn, mods, NULL, NULL ); 
		if ( rc != LDAP_SUCCESS ) {
			fprintf( stderr, "ldap_add_ext_s: %s\n",
			    ldap_err2string( rc ) );
			if ( rc == LDAP_SERVER_DOWN ) {
				perror( "ldap_add_ext_s" );
				voidrc = (void *)1;
				goto add_cleanup_and_return;
			}
		}

		++opcount;
		if ( maxops != 0 && opcount >= maxops ) {
			break;
		}
	}

add_cleanup_and_return:
	printf( "Thread %s: attempted %d add operations\n", id, opcount );
	set_ld_error( 0, NULL, NULL, NULL );	/* disposes of memory */
	tsd_cleanup();
	free( id );
	return voidrc;
}


static void *
delete_thread( char *id )
{
	LDAPMessage	*res;
	char		dn[BUFSIZ], name[40];
	int		num_entries, msgid, rc, parse_rc, finished, opcount;
	struct timeval	zerotime;
	void		*voidrc = (void *)0;

	zerotime.tv_sec = zerotime.tv_usec = 0L;

	printf( "Starting delete_thread %s.\n", id );
	opcount = 0;
	tsd_setup();

	rc = ldap_search_ext( ld, BASE, SCOPE, "(objectclass=*)",
		NULL, 0, NULL, NULL, NULL, LDAP_NO_LIMIT, &msgid );
	if ( rc != LDAP_SUCCESS ) {
		fprintf( stderr, "Thread %s error: Delete thread: "
		"ldap_search_ext: %s\n", id, ldap_err2string( rc ) );
		exit( 1 );
	}

	finished = 0;
	num_entries = 0;
	while ( !finished ) {
		rc = ldap_result( ld, msgid, LDAP_MSG_ONE, &zerotime, &res );
		switch ( rc ) {
		case -1:
			rc = ldap_get_lderrno( ld, NULL, NULL );
			fprintf( stderr, "ldap_result: %s\n", ldap_err2string( rc ) );
			exit( 1 );
			break;
		case 0:
			break;
		/* Keep track of the number of entries found. */
		case LDAP_RES_SEARCH_ENTRY:
			num_entries++;
			break;
		case LDAP_RES_SEARCH_REFERENCE:
			break;
		case LDAP_RES_SEARCH_RESULT:
			finished = 1;
			parse_rc = ldap_parse_result( ld, res, &rc, NULL, NULL, NULL, NULL, 1 );
			if ( parse_rc != LDAP_SUCCESS ) {
				fprintf( stderr, "Thread %s error: can't parse result code.\n", id );
				exit( 1 );
			} else {
				if ( rc != LDAP_SUCCESS ) {
					fprintf( stderr, "Thread %s error: ldap_search: %s\n", id, ldap_err2string( rc ) );
				} else {
					printf( "Thread %s: Got %d results.\n", id, num_entries );
				}
			}
			break;
		default:
			break;
		}
	}

	for ( ;; ) {
		sprintf( name, "%d", get_random_id() );
		sprintf( dn, "cn=%s, " BASE, name );
		printf( "Thread %s: Deleting entry (%s)\n", id, dn );

		if (( rc = ldap_delete_ext_s( ld, dn, NULL, NULL ))
		    != LDAP_SUCCESS ) {
			ldap_perror( ld, "ldap_delete_ext_s" );
			if ( rc == LDAP_SERVER_DOWN ) {
				perror( "ldap_delete_ext_s" );
				voidrc = (void *)1;
				goto delete_cleanup_and_return;
			}
		}

		++opcount;
		if ( maxops != 0 && opcount >= maxops ) {
			break;
		}
	}

delete_cleanup_and_return:
	printf( "Thread %s: attempted %d delete operations\n", id, opcount );
	set_ld_error( 0, NULL, NULL, NULL );	/* disposes of memory */
	tsd_cleanup();
	free( id );
	return voidrc;
}


static void *
bind_thread( char *id )
{
	char		dn[BUFSIZ], name[40];
	int		rc, opcount;
	void		*voidrc = (void *)0;

	printf( "Starting bind_thread %s.\n", id );
	opcount = 0;
	tsd_setup();

	for ( ;; ) {
		sprintf( name, "%d", get_random_id() );
		sprintf( dn, "cn=%s, " BASE, name );
		printf( "Thread %s: Binding as entry (%s)\n", id, dn );

		if (( rc = ldap_simple_bind_s( ld, dn, name ))
		    != LDAP_SUCCESS ) {
			ldap_perror( ld, "ldap_simple_bind_s" );
			if ( rc == LDAP_SERVER_DOWN ) {
				perror( "ldap_simple_bind_s" );
				voidrc = (void *)1;
				goto bind_cleanup_and_return;
			}
		} else {
			printf( "Thread %s: bound as entry (%s)\n", id, dn );
		}

		++opcount;
		if ( maxops != 0 && opcount >= maxops ) {
			break;
		}
	}

bind_cleanup_and_return:
	printf( "Thread %s: attempted %d bind operations\n", id, opcount );
	set_ld_error( 0, NULL, NULL, NULL );	/* disposes of memory */
	tsd_cleanup();
	free( id );
	return voidrc;
}


static void *
compare_thread( char *id )
{
	char		dn[BUFSIZ], name[40], cmpval[40];
	int		rc, randval, opcount;
	struct berval	bv;
	void		*voidrc = (void *)0;

	printf( "Starting compare_thread %s.\n", id );
	opcount = 0;
	tsd_setup();

	for ( ;; ) {
		randval = get_random_id();
		sprintf( name, "%d", randval );
		sprintf( dn, "cn=%s, " BASE, name );
		sprintf( cmpval, "%d", randval + random() % 3 );
		bv.bv_val = cmpval;
		bv.bv_len = strlen( cmpval );

		printf( "Thread %s: Comparing cn in entry (%s) with %s\n",
		    id, dn, cmpval );

		rc = ldap_compare_ext_s( ld, dn, "cn", &bv, NULL, NULL );
		switch ( rc ) {
		case LDAP_COMPARE_TRUE:
			printf( "Thread %s: entry (%s) contains cn %s\n",
			    id, dn, cmpval );
			break;
		case LDAP_COMPARE_FALSE:
			printf( "Thread %s: entry (%s) doesn't contain cn %s\n",
			    id, dn, cmpval );
			break;
		default:
			ldap_perror( ld, "ldap_compare_ext_s" );
			if ( rc == LDAP_SERVER_DOWN ) {
				perror( "ldap_compare_ext_s" );
				voidrc = (void *)1;
				goto compare_cleanup_and_return;
			}
		}

		++opcount;
		if ( maxops != 0 && opcount >= maxops ) {
			break;
		}
	}

compare_cleanup_and_return:
	printf( "Thread %s: attempted %d compare operations\n", id, opcount );
	set_ld_error( 0, NULL, NULL, NULL );	/* disposes of memory */
	tsd_cleanup();
	free( id );
	return voidrc;
}


static void *
search_thread( char *id )
{
	LDAPMessage	*res, *entry;
	char		*dn, filter[40];
	int		rc, opcount;
	void		*voidrc = (void *)0;

	printf( "Starting search_thread %s.\n", id );
	opcount = 0;
	tsd_setup();

	for ( ;; ) {
		if ( range_filters ) {
			switch( get_random_id() % 3 ) {
			case 0:
				sprintf( filter, "(cn>=%d)", get_random_id());
				break;
			case 1:
				sprintf( filter, "(cn<=%d)", get_random_id());
				break;
			case 2:
				sprintf( filter, "(&(cn>=%d)(cn<=%d))",
				    get_random_id(), get_random_id() );
				break;
			}
		} else {
			sprintf( filter, "cn=%d", get_random_id() );
		}

		printf( "Thread %s: Searching for entry (%s)\n", id, filter );

		res = NULL;
		if (( rc = ldap_search_ext_s( ld, BASE, SCOPE, filter, NULL, 0,
		    NULL, NULL, NULL, 0, &res )) != LDAP_SUCCESS ) {
			ldap_perror( ld, "ldap_search_ext_s" );
			if ( rc == LDAP_SERVER_DOWN ) {
				perror( "ldap_search_ext_s" );
				voidrc = (void *)1;
				goto search_cleanup_and_return;
			}
		}
		if ( res != NULL ) {
			entry = ldap_first_entry( ld, res );
			if ( entry == NULL ) {
				printf( "Thread %s: found no entries\n", id );
			} else {
				dn = ldap_get_dn( ld, entry );
				printf(
				    "Thread %s: found entry (%s); %d total\n",
				    id, dn == NULL ? "(Null)" : dn,
				    ldap_count_entries( ld, res ));
				ldap_memfree( dn );
			}
			ldap_msgfree( res );
		}

		++opcount;
		if ( maxops != 0 && opcount >= maxops ) {
			break;
		}
	}

search_cleanup_and_return:
	printf( "Thread %s: attempted %d search operations\n", id, opcount );
	set_ld_error( 0, NULL, NULL, NULL );	/* disposes of memory */
	tsd_cleanup();
	free( id );
	return voidrc;
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


void *
my_sema_alloc( void )
{
	sema_t *semptr;

	if( (semptr = malloc( sizeof(sema_t) ) ) != NULL ) {
		sema_init( semptr, 0, USYNC_THREAD, NULL );
	}
	return ( semptr );
}


static void
my_mutex_free( void *mutexp )
{
	pthread_mutex_destroy( (pthread_mutex_t *) mutexp );
	free( mutexp );
}


void
my_sema_free( void *semptr )
{
	sema_destroy( (sema_t *) semptr );
	free( semptr );
}


int
my_sema_wait( void *semptr )
{
	if( semptr != NULL )
		return( sema_wait( (sema_t *) semptr ) );
	else
		return( -1 );
}


int
my_sema_post( void *semptr )
{
	if( semptr != NULL )
		return( sema_post( (sema_t *) semptr ) );
	else
		return( -1 );
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
tsd_cleanup()
{
	void	*tsd;

	if (( tsd = pthread_getspecific( key )) != NULL ) {
		pthread_setspecific( key, NULL );
		free( tsd );
	}
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
get_ld_error( char **matchedp, char **errmsgp, void *dummy )
{
	struct ldap_error *le;

	le = pthread_getspecific( key );
	if ( matchedp != NULL ) {
		*matchedp = le->le_matched;
	}
	if ( errmsgp != NULL ) {
		*errmsgp = le->le_errmsg;
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


static int
get_random_id()
{
	return( random() % maxid );
}


static char *
get_id_str( int id )
{
	char	idstr[ 10 ];

	sprintf( idstr, "%d", id );
	return( strdup( idstr ));
}
