#include <stdio.h>
#include <malloc.h>
#include <errno.h>
#include <pthread.h>
#include <ldap.h>
#include <synch.h>

/* Authentication and search information. */
#define NAME		"cn=Directory Manager"
#define PASSWORD	"rtfm11111"
#define BASE		"o=Airius.com"
#define SCOPE	LDAP_SCOPE_SUBTREE

static void *modify_thread();
static void *add_thread();
static void *delete_thread();
static void *my_mutex_alloc();
void *my_sema_alloc( void );
static void my_mutex_free();
void my_sema_free( void * );
int my_sema_wait( void * );
int my_sema_post( void * );
static void set_ld_error();
static int  get_ld_error();
static void set_errno();
static int  get_errno();
static void tsd_setup();

/* Linked list of LDAPMessage structs for search results. */
typedef struct ldapmsgwrapper {
    LDAPMessage			*lmw_messagep;
    struct ldapmsgwrapper	*lmw_next;
} ldapmsgwrapper;

LDAP		*ld;
pthread_key_t	key;

main( int argc, char **argv )

{
	pthread_attr_t	attr;
	pthread_t	modify_tid, add_tid, delete_tid;
	void		*status;
	struct ldap_thread_fns	tfns;
	struct ldap_extra_thread_fns extrafns;
	int rc;

	/* Check command-line syntax. */
	if ( argc != 3 ) {
		fprintf( stderr, "usage: %s <host> <port>\n", argv[0] );
		exit( 1 );
	}

	/* Create a key. */
	if ( pthread_key_create( &key, free ) != 0 ) {
		perror( "pthread_key_create" );
	}
	tsd_setup();


	/* Initialize the LDAP session. */
	if ( (ld = ldap_init( argv[1], atoi( argv[2] ) )) == NULL ) {
		perror( "ldap_init" );
		exit( 1 );
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
		fprintf( stderr, "ldap_set_option (LDAP_OPT_THREAD_FN_PTRS): %s\n", ldap_err2string( rc ) );
		exit( 1 );
	}

	/* Set the function pointers for working with semaphores. */

	memset( &extrafns, '\0', sizeof(struct ldap_extra_thread_fns) );
	extrafns.ltf_mutex_trylock = (int (*)(void *)) pthread_mutex_trylock;
	extrafns.ltf_sema_alloc = (void *(*)(void)) my_sema_alloc;
	extrafns.ltf_sema_free = (void (*)(void *)) my_sema_free;
	extrafns.ltf_sema_wait = (int (*)(void *)) my_sema_wait;
	extrafns.ltf_sema_post = (int (*)(void *)) my_sema_post;

	/* Set up this session to use those function pointers. */

	rc = ldap_set_option( ld, LDAP_OPT_EXTRA_THREAD_FN_PTRS, (void *) &extrafns );
	if ( rc < 0 ) {
		fprintf( stderr, "ldap_set_option (LDAP_OPT_EXTRA_THREAD_FN_PTRS): %s\n", ldap_err2string( rc ) );
		exit( 1 );
	}

	/* Attempt to bind to the server. */
	rc = ldap_simple_bind_s( ld, NAME, PASSWORD );
	if ( rc != LDAP_SUCCESS ) {
		fprintf( stderr, "ldap_simple_bind_s: %s\n", ldap_err2string( rc ) );
		exit( 1 );
	}

	/* Initialize the attribute. */
	if ( pthread_attr_init( &attr ) != 0 ) {
		perror( "pthread_attr_init" );
		exit( 1 );
	}

	/* Specify that the threads are joinable. */
	pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_JOINABLE );

	/* Create three threads: one for adding, one for modifying, and
	   one for deleting. */
	if ( pthread_create( &modify_tid, &attr, modify_thread, "1" ) != 0 ) {
		perror( "pthread_create modify_thread" );
		exit( 1 );
	}

	if ( pthread_create( &add_tid, &attr, add_thread, "2" ) != 0 ) {
		perror( "pthread_create add_thread" );
		exit( 1 );
	}

	if ( pthread_create( &delete_tid, &attr, delete_thread, "3" ) != 0 ) {
		perror( "pthread_create delete_thread" );
		exit( 1 );
	}

	/* Wait until these threads exit. */
	pthread_join( modify_tid, &status );
	pthread_join( add_tid, &status );
	pthread_join( delete_tid, &status );
}

static void *
modify_thread( char *id )
{
	LDAPMessage	*res;
	LDAPMessage	*e;
	int		i, modentry, num_entries, msgid, rc, parse_rc, finished;
	LDAPMod		mod;
	LDAPMod		*mods[2];
	char		*vals[2];
	char		*dn;
	ldapmsgwrapper	*list, *lmwp, *lastlmwp;
	struct timeval	zerotime;
	zerotime.tv_sec = zerotime.tv_usec = 0L;

	printf( "Starting modify_thread %s.\n", id );
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
		modentry = rand() % num_entries;
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
			rc = ldap_get_lderrno( ld, NULL, NULL );
			fprintf( stderr, "ldap_modify_ext_s: %s\n", ldap_err2string( rc ) );
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
	int	i, rc;

	printf( "Starting add_thread %s.\n", id );
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

		printf( "Thread %s: Adding entry (%s)\n", id, dn );
		rc = ldap_add_ext_s( ld, dn, mods, NULL, NULL ); 
            if ( rc != LDAP_SUCCESS ) {
			rc = ldap_get_lderrno( ld, NULL, NULL );
			fprintf( stderr, "ldap_add_ext_s: %s\n", ldap_err2string( rc ) );
		}
	}

}



static void *
delete_thread( char *id )
{
	LDAPMessage	*res;
	char		dn[BUFSIZ], name[40];
	int		num_entries, msgid, rc, parse_rc, finished;
	struct timeval	zerotime;
	zerotime.tv_sec = zerotime.tv_usec = 0L;

	printf( "Starting delete_thread %s.\n", id );

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
		sprintf( name, "%d", rand() );
		sprintf( dn, "cn=%s, " BASE, name );
		printf( "Thread %s: Deleting entry (%s)\n", id, dn );

		if ( ldap_delete_ext_s( ld, dn, NULL, NULL ) != LDAP_SUCCESS ) {
			ldap_perror( ld, "ldap_delete_ext_s" );
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
