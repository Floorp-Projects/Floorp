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
#include <nspr.h>
#include <stdio.h>
#include <ldap.h>

#define NAME		"cn=Directory Manager"
#define PASSWORD	"secret99"
#define BASE		"dc=example,dc=com"

static int simplebind( LDAP *ld, char *msg, int tries );
static void search_thread( void * );
static void modify_thread( void * );
static void add_thread( void * );
static void delete_thread( void * );
static void set_ld_error();
static int  get_ld_error();
static void set_errno();
static int  get_errno();
static void tsd_setup();
static void *my_mutex_alloc( void );
static void my_mutex_free( void * );
static int my_mutex_lock( void * );
static int my_mutex_unlock( void * );
static LDAPHostEnt *my_gethostbyname( const char *name, LDAPHostEnt *result,
	char *buffer, int buflen, int *statusp, void *extradata );
static LDAPHostEnt *my_gethostbyaddr( const char *addr, int length,
	int type, LDAPHostEnt *result, char *buffer, int buflen,
	int *statusp, void *extradata );
static LDAPHostEnt *copyPRHostEnt2LDAPHostEnt( LDAPHostEnt *ldhp,
	PRHostEnt *prhp );

typedef struct ldapmsgwrapper {
    LDAPMessage			*lmw_messagep;
    struct ldapmsgwrapper	*lmw_next;
} ldapmsgwrapper;


#define CONNECTION_ERROR( lderr )	( (lderr) == LDAP_SERVER_DOWN || \
					(lderr) == LDAP_CONNECT_ERROR )


LDAP		*ld;
PRUintn		tsdindex;
#ifdef LDAP_MEMCACHE
LDAPMemCache	*memcache = NULL;
#define MEMCACHE_SIZE		(256*1024)	/* 256K bytes */
#define MEMCACHE_TTL		(15*60)		/* 15 minutes */
#endif


main( int argc, char **argv )
{
	PRThread		*search_tid, *search_tid2, *search_tid3;
	PRThread		*search_tid4, *modify_tid, *add_tid;
	PRThread		*delete_tid;
	struct ldap_thread_fns	tfns;
	struct ldap_dns_fns	dnsfns;
	int			rc;

	if ( argc != 3 ) {
		fprintf( stderr, "usage: %s host port\n", argv[0] );
		exit( 1 );
	}

	PR_Init( PR_USER_THREAD, PR_PRIORITY_NORMAL, 0 );
	if ( PR_NewThreadPrivateIndex( &tsdindex, NULL ) != PR_SUCCESS ) {
		perror( "PR_NewThreadPrivateIndex" );
		exit( 1 );
	}
	tsd_setup();	/* for main thread */

	if ( (ld = ldap_init( argv[1], atoi( argv[2] ) )) == NULL ) {
		perror( "ldap_open" );
		exit( 1 );
	}

	/* set thread function pointers */
	memset( &tfns, '\0', sizeof(struct ldap_thread_fns) );
	tfns.ltf_mutex_alloc = my_mutex_alloc;
	tfns.ltf_mutex_free = my_mutex_free;
	tfns.ltf_mutex_lock = my_mutex_lock;
	tfns.ltf_mutex_unlock = my_mutex_unlock;
	tfns.ltf_get_errno = get_errno;
	tfns.ltf_set_errno = set_errno;
	tfns.ltf_get_lderrno = get_ld_error;
	tfns.ltf_set_lderrno = set_ld_error;
	tfns.ltf_lderrno_arg = NULL;
	if ( ldap_set_option( ld, LDAP_OPT_THREAD_FN_PTRS, (void *) &tfns )
	    != 0 ) {
		ldap_perror( ld, "ldap_set_option: thread functions" );
		exit( 1 );
	}

	/* set DNS function pointers */
	memset( &dnsfns, '\0', sizeof(struct ldap_dns_fns) );
	dnsfns.lddnsfn_bufsize = PR_NETDB_BUF_SIZE;
	dnsfns.lddnsfn_gethostbyname = my_gethostbyname;
	dnsfns.lddnsfn_gethostbyaddr = my_gethostbyaddr;
	if ( ldap_set_option( ld, LDAP_OPT_DNS_FN_PTRS, (void *)&dnsfns )
	    != 0 ) {
		ldap_perror( ld, "ldap_set_option: DNS functions" );
		exit( 1 );
	}

#ifdef LDAP_MEMCACHE
	/* create the in-memory cache */
	if (( rc = ldap_memcache_init( MEMCACHE_TTL, MEMCACHE_SIZE, NULL,
	    &tfns, &memcache )) != LDAP_SUCCESS ) {
		fprintf( stderr, "ldap_memcache_init failed - %s\n",
		    ldap_err2string( rc ));
		exit( 1 );
	}
	if (( rc = ldap_memcache_set( ld, memcache )) != LDAP_SUCCESS ) {
		fprintf( stderr, "ldap_memcache_set failed - %s\n",
		    ldap_err2string( rc ));
		exit( 1 );
	}
#endif

	/*
	 * set option so that the next call to ldap_simple_bind_s() after
	 * the server connection is lost will attempt to reconnect.
	 */
	if ( ldap_set_option( ld, LDAP_OPT_RECONNECT, LDAP_OPT_ON ) != 0 ) {
		ldap_perror( ld, "ldap_set_option: reconnect" );
		exit( 1 );
	}

	/* initial bind */
	if ( simplebind( ld, "ldap_simple_bind_s/main", 1 ) != LDAP_SUCCESS ) {
		exit( 1 );
	}

	/* create the operation threads */
	if ( (search_tid = PR_CreateThread( PR_USER_THREAD, search_thread,
	    "1", PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD, PR_UNJOINABLE_THREAD,
	    0 )) == NULL ) {
		perror( "PR_CreateThread search_thread" );
		exit( 1 );
	}
	if ( (modify_tid = PR_CreateThread( PR_USER_THREAD, modify_thread,
	    "2", PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD, PR_UNJOINABLE_THREAD,
	    0 )) == NULL ) {
		perror( "PR_CreateThread modify_thread" );
		exit( 1 );
	}
	if ( (search_tid2 = PR_CreateThread( PR_USER_THREAD, search_thread,
	    "3", PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD, PR_UNJOINABLE_THREAD,
	    0 )) == NULL ) {
		perror( "PR_CreateThread search_thread 2" );
		exit( 1 );
	}
	if ( (add_tid = PR_CreateThread( PR_USER_THREAD, add_thread,
	    "4", PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD, PR_UNJOINABLE_THREAD,
	    0 )) == NULL ) {
		perror( "PR_CreateThread add_thread" );
		exit( 1 );
	}
	if ( (search_tid3 = PR_CreateThread( PR_USER_THREAD, search_thread,
	    "5", PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD, PR_UNJOINABLE_THREAD,
	    0 )) == NULL ) {
		perror( "PR_CreateThread search_thread 3" );
		exit( 1 );
	}
	if ( (delete_tid = PR_CreateThread( PR_USER_THREAD, delete_thread,
	    "6", PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD, PR_UNJOINABLE_THREAD,
	    0 )) == NULL ) {
		perror( "PR_CreateThread delete_thread" );
		exit( 1 );
	}
	if ( (search_tid4 = PR_CreateThread( PR_USER_THREAD, search_thread,
	    "7", PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD, PR_UNJOINABLE_THREAD,
	    0 )) == NULL ) {
		perror( "PR_CreateThread search_thread 4" );
		exit( 1 );
	}

	PR_Cleanup();
	return( 0 );
}


static int
simplebind( LDAP *ld, char *msg, int tries )
{
	int	rc;

	while ( tries-- > 0 ) {
		rc = ldap_simple_bind_s( ld, NAME, PASSWORD );
		if ( rc != LDAP_SUCCESS ) {
			ldap_perror( ld, msg );
		}
		if ( tries == 0 || !CONNECTION_ERROR( rc )) {
			return( rc );
		}
		fprintf( stderr,
		    "%s: sleeping for 5 secs - will try %d more time(s)...\n",
		    msg, tries );
		sleep( 5 );
	}

	return( rc );
}


static void
search_thread( void *arg1 )
{
	LDAPMessage	*res;
	LDAPMessage	*e;
	char		*a;
	char		**v;
	char		*dn;
	BerElement	*ber;
	int		i, rc, msgid;
	void		*tsd;
	char		*id = arg1;

	printf( "search_thread\n" );
	tsd_setup();
	for ( ;; ) {
		printf( "%sSearching...\n", id );
		if ( (msgid = ldap_search( ld, BASE, LDAP_SCOPE_SUBTREE,
		    "(objectclass=*)", NULL, 0 )) == -1 ) {
			ldap_perror( ld, "ldap_search_s" );
			rc =  ldap_get_lderrno( ld, NULL, NULL );
			if ( CONNECTION_ERROR( rc ) && simplebind( ld,
			    "bind-search_thread", 5 ) != LDAP_SUCCESS ) {
				return;
			}
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
					ldap_memfree( a );
				}
				if ( ber != NULL ) {
					ber_free( ber, 0 );
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

		if ( rc == -1 ) {
			rc = ldap_get_lderrno( ld, NULL, NULL );
			if ( CONNECTION_ERROR( rc ) && simplebind( ld,
			    "bind-search_thread", 5 ) != LDAP_SUCCESS ) {
				return;
			}
		}
	}
}

static void
modify_thread( void *arg1 )
{
	LDAPMessage	*res;
	LDAPMessage	*e;
	int		i, modentry, entries, msgid, rc;
	LDAPMod		mod;
	LDAPMod		*mods[2];
	char		*vals[2];
	char		*dn;
	char		*id = arg1;
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
		if (( rc = ldap_modify_s( ld, dn, mods )) != LDAP_SUCCESS ) {
			ldap_perror( ld, "ldap_modify_s" );
			if ( CONNECTION_ERROR( rc ) && simplebind( ld,
			    "bind-modify_thread", 5 ) != LDAP_SUCCESS ) {
				return;
			}
		}
		free( dn );
	}
}

static void
add_thread( void *arg1 )
{
	LDAPMod	mod[5];
	LDAPMod	*mods[6];
	char	dn[BUFSIZ], name[40];
	char	*cnvals[2], *snvals[2], *ocvals[2];
	int	i, rc;
	char	*id = arg1;

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
	ocvals[0] = "person";
	ocvals[1] = NULL;
	mods[3] = NULL;

	for ( ;; ) {
		sprintf( name, "%d", rand() );
		sprintf( dn, "cn=%s, " BASE, name );
		cnvals[0] = name;
		snvals[0] = name;

		printf( "%sAdding entry (%s)\n", id, dn );
		if (( rc = ldap_add_s( ld, dn, mods )) != LDAP_SUCCESS ) {
			ldap_perror( ld, "ldap_add_s" );
			if ( CONNECTION_ERROR( rc ) && simplebind( ld,
			    "bind-add_thread", 5 ) != LDAP_SUCCESS ) {
				return;
			}
		}
	}
}

static void
delete_thread( void *arg1 )
{
	LDAPMessage	*res;
	char		dn[BUFSIZ], name[40];
	int		entries, msgid, rc;
	char		*id = arg1;

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
	if ( rc == -1 || ldap_result2error( ld, res, 1 ) != LDAP_SUCCESS ) {
		ldap_perror( ld, "delete_thread: ldap_search" );
	} else {
		printf( "%sDelete got %d entries\n", id, entries );
	}

	for ( ;; ) {
		sprintf( name, "%d", rand() );
		sprintf( dn, "cn=%s, " BASE, name );

		printf( "%sDeleting entry (%s)\n", id, dn );
		if (( rc = ldap_delete_s( ld, dn )) != LDAP_SUCCESS ) {
			ldap_perror( ld, "ldap_delete_s" );
			if ( CONNECTION_ERROR( rc ) && simplebind( ld,
			    "bind-delete_thread", 5 ) != LDAP_SUCCESS ) {
				return;
			}
		}
	}
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

	tsd = (void *) PR_GetThreadPrivate( tsdindex );
	if ( tsd != NULL ) {
		fprintf( stderr, "tsd non-null!\n" );
		exit( 1 );
	}
	tsd = (void *) calloc( 1, sizeof(struct ldap_error) );
	if ( PR_SetThreadPrivate( tsdindex, tsd ) != 0 ) {
		perror( "PR_SetThreadPrivate" );
		exit( 1 );
	}
}

static void
set_ld_error( int err, char *matched, char *errmsg, void *dummy )
{
	struct ldap_error *le;

	le = (void *) PR_GetThreadPrivate( tsdindex );
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

	le = PR_GetThreadPrivate( tsdindex );
	if ( matchedp != NULL ) {
		*matchedp = le->le_matched;
	}
	if ( errmsgp != NULL ) {
		*errmsgp = le->le_errmsg;
	}
	return( le->le_errno );
}

static void
set_errno( int oserrno )
{
	/* XXXmcs: should this be PR_SetError( oserrno, 0 )? */
	PR_SetError( PR_UNKNOWN_ERROR, oserrno );
}

static int
get_errno( void )
{
	/* XXXmcs: should this be PR_GetError()? */
	return( PR_GetOSError());
}

static void *
my_mutex_alloc( void )
{
	return( (void *)PR_NewLock());
}

static void
my_mutex_free( void *mutex )
{
	PR_DestroyLock( (PRLock *)mutex );
}

static int
my_mutex_lock( void *mutex )
{
	PR_Lock( (PRLock *)mutex );
	return( 0 );
}

static int
my_mutex_unlock( void *mutex )
{
	if ( PR_Unlock( (PRLock *)mutex ) == PR_FAILURE ) {
		return( -1 );
	}

	return( 0 );
}

static LDAPHostEnt *
my_gethostbyname( const char *name, LDAPHostEnt *result,
	char *buffer, int buflen, int *statusp, void *extradata )
{
	PRHostEnt	prhent;

	if ( PR_GetHostByName( name, buffer, buflen,
	    &prhent ) != PR_SUCCESS ) {
		return( NULL );
	}

	return( copyPRHostEnt2LDAPHostEnt( result, &prhent ));
}

static LDAPHostEnt *
my_gethostbyaddr( const char *addr, int length, int type, LDAPHostEnt *result,
	char *buffer, int buflen, int *statusp, void *extradata )
{
	PRHostEnt	prhent;

	if ( PR_GetHostByAddr( (PRNetAddr *)addr, buffer, buflen,
	    &prhent ) != PR_SUCCESS ) {
		return( NULL );
	}

	return( copyPRHostEnt2LDAPHostEnt( result, &prhent ));
}

static LDAPHostEnt *
copyPRHostEnt2LDAPHostEnt( LDAPHostEnt *ldhp, PRHostEnt *prhp )
{
	ldhp->ldaphe_name = prhp->h_name;
	ldhp->ldaphe_aliases = prhp->h_aliases;
	ldhp->ldaphe_addrtype = prhp->h_addrtype;
	ldhp->ldaphe_length =  prhp->h_length;
	ldhp->ldaphe_addr_list =  prhp->h_addr_list;
	return( ldhp );
}
