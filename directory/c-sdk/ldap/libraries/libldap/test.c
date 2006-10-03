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

/* test.c - a simple test harness. */
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#ifdef MACOS
#ifdef THINK_C
#include <console.h>
#include <unix.h>
#include <fcntl.h>
#endif /* THINK_C */
#include "macos.h"
#else /* MACOS */
#if defined( DOS )
#include "msdos.h"
#if defined( WINSOCK ) 
#include "console.h"
#endif /* WINSOCK */
#else /* DOS */
#ifdef _WINDOWS
#include <windows.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
//#include "console.h"
#else /* _WINDOWS */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/file.h>
#ifndef VMS
#include <fcntl.h>
#include <unistd.h>
#endif /* VMS */
#endif /* _WINDOWS */
#endif /* DOS */
#endif /* MACOS */

#include "ldap.h"
#include "disptmpl.h"
#include "ldaplog.h"
#include "portable.h"
#ifndef NO_LIBLCACHE
#include "lcache.h"
#endif /* !NO_LIBLCACHE */

#undef NET_SSL
#if defined(NET_SSL)
#include <nss.h>
#include <ldap_ssl.h>
#endif


#if !defined( PCNFS ) && !defined( WINSOCK ) && !defined( MACOS )
#define MOD_USE_BVALS
#endif /* !PCNFS && !WINSOCK && !MACOS */

static void handle_result( LDAP *ld, LDAPMessage *lm, int onlyone );
static void print_ldap_result( LDAP *ld, LDAPMessage *lm, char *s );
static void print_controls( LDAPControl **ctrls, int freeit );
static void print_referrals( char **refs, int freeit );
static void print_search_entry( LDAP *ld, LDAPMessage *res, int onlyone );
static char *changetype_num2string( ber_int_t chgtype );
static void print_search_reference( LDAP *ld, LDAPMessage *res, int onlyone );
static void free_list( char **list );
static int entry2textwrite( void *fp, char *buf, int len );
static void bprint( char *data, int len );
static char **string2words( char *str, char *delims );
static const char * url_parse_err2string( int e );

char *dnsuffix;

#ifndef WINSOCK
static char *
getline( char *line, int len, FILE *fp, char *prompt )
{
	printf(prompt);

	if ( fgets( line, len, fp ) == NULL )
		return( NULL );

	line[ strlen( line ) - 1 ] = '\0';

	return( line );
}
#endif /* WINSOCK */

static char **
get_list( char *prompt )
{
	static char	buf[256];
	int		num;
	char		**result;

	num = 0;
	result = (char **) 0;
	while ( 1 ) {
		getline( buf, sizeof(buf), stdin, prompt );

		if ( *buf == '\0' )
			break;

		if ( result == (char **) 0 )
			result = (char **) malloc( sizeof(char *) );
		else
			result = (char **) realloc( result,
			    sizeof(char *) * (num + 1) );

		result[num++] = (char *) strdup( buf );
	}
	if ( result == (char **) 0 )
		return( NULL );
	result = (char **) realloc( result, sizeof(char *) * (num + 1) );
	result[num] = NULL;

	return( result );
}


static void
free_list( char **list )
{
	int	i;

	if ( list != NULL ) {
		for ( i = 0; list[ i ] != NULL; ++i ) {
			free( list[ i ] );
		}
		free( (char *)list );
	}
}


#ifdef MOD_USE_BVALS
static int
file_read( char *path, struct berval *bv )
{
	FILE		*fp;
	long		rlen;
	int		eof;

	if (( fp = NSLDAPI_FOPEN( path, "r" )) == NULL ) {
	    	perror( path );
		return( -1 );
	}

	if ( fseek( fp, 0L, SEEK_END ) != 0 ) {
		perror( path );
		fclose( fp );
		return( -1 );
	}

	bv->bv_len = ftell( fp );

	if (( bv->bv_val = (char *)malloc( bv->bv_len )) == NULL ) {
		perror( "malloc" );
		fclose( fp );
		return( -1 );
	}

	if ( fseek( fp, 0L, SEEK_SET ) != 0 ) {
		perror( path );
		fclose( fp );
		return( -1 );
	}

	rlen = fread( bv->bv_val, 1, bv->bv_len, fp );
	eof = feof( fp );
	fclose( fp );

	if ( (unsigned long)rlen != bv->bv_len ) {
		perror( path );
		free( bv->bv_val );
		return( -1 );
	}

	return( bv->bv_len );
}
#endif /* MOD_USE_BVALS */


static LDAPMod **
get_modlist( char *prompt1, char *prompt2, char *prompt3 )
{
	static char	buf[256];
	int		num;
	LDAPMod		tmp;
	LDAPMod		**result;
#ifdef MOD_USE_BVALS
	struct berval	**bvals;
#endif /* MOD_USE_BVALS */

	num = 0;
	result = NULL;
	while ( 1 ) {
		if ( prompt1 ) {
			getline( buf, sizeof(buf), stdin, prompt1 );
			tmp.mod_op = atoi( buf );

			if ( tmp.mod_op == -1 || buf[0] == '\0' )
				break;
		} else {
			tmp.mod_op = 0;
		}

		getline( buf, sizeof(buf), stdin, prompt2 );
		if ( buf[0] == '\0' )
			break;
		tmp.mod_type = strdup( buf );

		tmp.mod_values = get_list( prompt3 );
#ifdef MOD_USE_BVALS
		if ( tmp.mod_values != NULL ) {
			int	i;

			for ( i = 0; tmp.mod_values[i] != NULL; ++i )
				;
			bvals = (struct berval **)calloc( i + 1,
			    sizeof( struct berval *));
			for ( i = 0; tmp.mod_values[i] != NULL; ++i ) {
				bvals[i] = (struct berval *)malloc(
				    sizeof( struct berval ));
				if ( strncmp( tmp.mod_values[i], "{FILE}",
				    6 ) == 0 ) {
					if ( file_read( tmp.mod_values[i] + 6,
					    bvals[i] ) < 0 ) {
						return( NULL );
					}
				} else {
					bvals[i]->bv_val = tmp.mod_values[i];
					bvals[i]->bv_len =
					    strlen( tmp.mod_values[i] );
				}
			}
			tmp.mod_bvalues = bvals;
			tmp.mod_op |= LDAP_MOD_BVALUES;
		}
#endif /* MOD_USE_BVALS */

		if ( result == NULL )
			result = (LDAPMod **) malloc( sizeof(LDAPMod *) );
		else
			result = (LDAPMod **) realloc( result,
			    sizeof(LDAPMod *) * (num + 1) );

		result[num] = (LDAPMod *) malloc( sizeof(LDAPMod) );
		*(result[num]) = tmp;	/* struct copy */
		num++;
	}
	if ( result == NULL )
		return( NULL );
	result = (LDAPMod **) realloc( result, sizeof(LDAPMod *) * (num + 1) );
	result[num] = NULL;

	return( result );
}


int LDAP_CALL LDAP_CALLBACK
bind_prompt( LDAP *ld, char **dnp, char **passwdp, int *authmethodp,
	int freeit, void *dummy )
{
	static char	dn[256], passwd[256];

	if ( !freeit ) {
#ifdef KERBEROS
		getline( dn, sizeof(dn), stdin,
		    "re-bind method (0->simple, 1->krbv41, 2->krbv42, 3->krbv41&2)? " );
		if (( *authmethodp = atoi( dn )) == 3 ) {
			*authmethodp = LDAP_AUTH_KRBV4;
		} else {
			*authmethodp |= 0x80;
		}
#else /* KERBEROS */
		*authmethodp = LDAP_AUTH_SIMPLE;
#endif /* KERBEROS */

		getline( dn, sizeof(dn), stdin, "re-bind dn? " );
		strcat( dn, dnsuffix );
		*dnp = dn;

		if ( *authmethodp == LDAP_AUTH_SIMPLE && dn[0] != '\0' ) {
			getline( passwd, sizeof(passwd), stdin,
			    "re-bind password? " );
		} else {
			passwd[0] = '\0';
		}
		*passwdp = passwd;
	}

	return( LDAP_SUCCESS );
}


#define HEX2BIN( h )	( (h) >= '0' && (h) <='9' ? (h) - '0' : (h) - 'A' + 10 )

void
berval_from_hex( struct berval *bvp, char *hexstr )
{
    char	*src, *dst, c;
	unsigned char abyte;

    dst = bvp->bv_val;
    bvp->bv_len = 0;
    src = hexstr;
    while ( *src != '\0' ) {
	c = *src;
	if ( isupper( c )) {
		c = tolower( c );
	}
	abyte = HEX2BIN( c ) << 4;

	++src;
	c = *src;
	if ( isupper( c )) {
		c = tolower( c );
	}
	abyte |= HEX2BIN( c );
	++src;

	*dst++ = abyte;
	++bvp->bv_len;
    }
}


static void
add_control( LDAPControl ***ctrlsp, LDAPControl *newctrl )
{
	int	i;

	if ( *ctrlsp == NULL ) {
		*ctrlsp = (LDAPControl **) calloc( 2, sizeof(LDAPControl *) );
		i = 0;
	} else {
		for ( i = 0; (*ctrlsp)[i] != NULL; i++ ) {
			;	/* NULL */
		}
		*ctrlsp = (LDAPControl **) realloc( *ctrlsp,
		    (i + 2) * sizeof(LDAPControl *) );
	}
	(*ctrlsp)[i] = newctrl;
	(*ctrlsp)[i+1] = NULL;
}


#ifdef TEST_CUSTOM_MALLOC

typedef struct my_malloc_info {
	long	mmi_magic;
	size_t	mmi_actualsize;
} MyMallocInfo;
#define MY_MALLOC_MAGIC_NUMBER		0x19940618

#define MY_MALLOC_CHECK_MAGIC( p )	if ( ((MyMallocInfo *)( (p) - sizeof()

void *
my_malloc( size_t size )
{
	void		*p;
	MyMallocInfo	*mmip;

	if (( p = malloc( size + sizeof( struct my_malloc_info ))) != NULL ) {
		mmip = (MyMallocInfo *)p;
		mmip->mmi_magic = MY_MALLOC_MAGIC_NUMBER;
		mmip->mmi_actualsize = size;
	}

	fprintf( stderr, "my_malloc: allocated ptr 0x%x, size %ld\n",
	    p,  mmip->mmi_actualsize );

	return( (char *)p + sizeof( MyMallocInfo ));
}


void *
my_calloc( size_t nelem, size_t elsize )
{
	void	*p;

	if (( p = my_malloc( nelem * elsize )) != NULL ) {
		memset( p, 0, nelem * elsize );
	}

	return( p );
}


void
my_free( void *ptr )
{
	char		*p;
	MyMallocInfo	*mmip;

	p = (char *)ptr;
	p -= sizeof( MyMallocInfo );
	mmip = (MyMallocInfo *)p;
	if ( mmip->mmi_magic != MY_MALLOC_MAGIC_NUMBER ) {
		fprintf( stderr,
		    "my_malloc_check_magic: ptr 0x%x bad magic number\n", ptr );
		exit( 1 );
	}

	fprintf( stderr, "my_free: freeing ptr 0x%x, size %ld\n",
	    p,  mmip->mmi_actualsize );

	memset( p, 0, mmip->mmi_actualsize + sizeof( MyMallocInfo ));
	free( p );
}


void *
my_realloc( void *ptr, size_t size )
{
	void		*p;
	MyMallocInfo	*mmip;

	if ( ptr == NULL ) {
		return( my_malloc( size ));
	}

	mmip = (MyMallocInfo *)( (char *)ptr - sizeof( MyMallocInfo ));
	if ( mmip->mmi_magic != MY_MALLOC_MAGIC_NUMBER ) {
		fprintf( stderr,
		    "my_malloc_check_magic: ptr 0x%x bad magic number\n", ptr );
		exit( 1 );
	}

	if ( size <= mmip->mmi_actualsize ) {	/* current block big enough? */
		return( ptr );
	}

	if (( p = my_malloc( size )) != NULL ) {
		memcpy( p, ptr, mmip->mmi_actualsize );
		my_free( ptr );
	}

	return( p );
}
#endif /* TEST_CUSTOM_MALLOC */

int
#ifdef WINSOCK
ldapmain(
#else /* WINSOCK */
main(
#endif /* WINSOCK */
	int argc, char **argv )
{
	LDAP		*ld;
	int		rc, i, c, port, cldapflg, errflg, method, id, msgtype;
	int		version;
	char		line[256], command1, command2, command3;
	char		passwd[64], dn[256], rdn[64], attr[64], value[256];
	char		filter[256], *host, **types;
	char		**exdn, *fnname;
	int		bound, all, scope, attrsonly, optval, ldapversion;
	LDAPMessage	*res;
	LDAPMod		**mods, **attrs;
	struct timeval	timeout, *tvp;
	char		*copyfname = NULL;
	int		copyoptions = 0;
	LDAPURLDesc	*ludp;
	struct ldap_disptmpl	*tmpllist = NULL;
	int		changetypes, changesonly, return_echg_ctls;
	LDAPControl	**tmpctrls, *newctrl, **controls = NULL;
	char		*usage = "usage: %s [-u] [-h host] [-d level] [-s dnsuffix] [-p port] [-t file] [-T file] [-V protocolversion]\n";

	extern char	*optarg;
	extern int	optind;

#ifdef MACOS
	if (( argv = get_list( "cmd line arg?" )) == NULL ) {
		exit( 1 );
	}
	for ( argc = 0; argv[ argc ] != NULL; ++argc ) {
		;
	}
#endif /* MACOS */

#ifdef TEST_CUSTOM_MALLOC
        {
		struct ldap_memalloc_fns	memalloc_fns;

		memalloc_fns.ldapmem_malloc = my_malloc;
		memalloc_fns.ldapmem_calloc = my_calloc;
		memalloc_fns.ldapmem_realloc = my_realloc;
		memalloc_fns.ldapmem_free = my_free;

		if ( ldap_set_option( NULL, LDAP_OPT_MEMALLOC_FN_PTRS,
		    &memalloc_fns ) != 0 ) {
			fputs( "ldap_set_option failed\n", stderr );
			exit( 1 );
		}
	}
#endif /* TEST_CUSTOM_MALLOC */

	host = NULL;
	port = LDAP_PORT;
	dnsuffix = "";
	cldapflg = errflg = 0;
	ldapversion = 0;	/* use default */
#ifndef _WIN32
#ifdef LDAP_DEBUG
	ldap_debug = LDAP_DEBUG_ANY;
#endif
#endif

	while (( c = getopt( argc, argv, "uh:d:s:p:t:T:V:" )) != -1 ) {
		switch( c ) {
		case 'u':
#ifdef CLDAP
			cldapflg++;
#else /* CLDAP */
			printf( "Compile with -DCLDAP for UDP support\n" );
#endif /* CLDAP */
			break;

		case 'd':
#ifndef _WIN32
#ifdef LDAP_DEBUG
			ldap_debug = atoi( optarg ) | LDAP_DEBUG_ANY;
			if ( ldap_debug & LDAP_DEBUG_PACKETS ) {
				ber_set_option( NULL, LBER_OPT_DEBUG_LEVEL,
					&ldap_debug );
			}
#else
			printf( "Compile with -DLDAP_DEBUG for debugging\n" );
#endif
#endif
			break;

		case 'h':
			host = optarg;
			break;

		case 's':
			dnsuffix = optarg;
			break;

		case 'p':
			port = atoi( optarg );
			break;

#if !defined(MACOS) && !defined(DOS)
		case 't':	/* copy ber's to given file */
			copyfname = strdup( optarg );
			copyoptions = LBER_SOCKBUF_OPT_TO_FILE;
			break;

		case 'T':	/* only output ber's to given file */
			copyfname = strdup( optarg );
			copyoptions = (LBER_SOCKBUF_OPT_TO_FILE |
			    LBER_SOCKBUF_OPT_TO_FILE_ONLY);
			break;
#endif
		case 'V':	/* LDAP protocol version */
			ldapversion = atoi( optarg );
			break;

		default:
		    ++errflg;
		}
	}

	if ( host == NULL && optind == argc - 1 ) {
		host = argv[ optind ];
		++optind;
	}

	if ( errflg || optind < argc - 1 ) {
		fprintf( stderr, usage, argv[ 0 ] );
		exit( 1 );
	}
	
	printf( "%sldap_init( %s, %d )\n", cldapflg ? "c" : "",
		host == NULL ? "(null)" : host, port );

	if ( cldapflg ) {
#ifdef CLDAP
		ld = cldap_open( host, port );
#endif /* CLDAP */
	} else {
		ld = ldap_init( host, port );
	}

	if ( ld == NULL ) {
		perror( "ldap_init" );
		exit(1);
	}

	if ( ldapversion != 0 && ldap_set_option( ld,
	    LDAP_OPT_PROTOCOL_VERSION, (void *)&ldapversion ) != 0 ) {
		ldap_perror( ld, "ldap_set_option (protocol version)" );
		exit(1);
	}

#ifdef notdef
#if !defined(MACOS) && !defined(DOS)
	if ( copyfname != NULL ) {
		int	fd;
		Sockbuf	*sb;

		if ( (fd = open( copyfname, O_WRONLY | O_CREAT, 0600 ))
		    == -1 ) {
			perror( copyfname );
			exit ( 1 );
		}
		ldap_get_option( ld, LDAP_OPT_SOCKBUF, &sb );
		ber_sockbuf_set_option( sb, LBER_SOCKBUF_OPT_COPYDESC,
		    (void *) &fd );
		ber_sockbuf_set_option( sb, copyoptions, LBER_OPT_ON );
	}
#endif
#endif

	bound = 0;
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	tvp = &timeout;

	(void) memset( line, '\0', sizeof(line) );
	while ( getline( line, sizeof(line), stdin, "\ncommand? " ) != NULL ) {
		command1 = line[0];
		command2 = line[1];
		command3 = line[2];

		switch ( command1 ) {
		case 'a':	/* add or abandon */
			switch ( command2 ) {
			case 'd':	/* add */
				getline( dn, sizeof(dn), stdin, "dn? " );
				strcat( dn, dnsuffix );
				if ( (attrs = get_modlist( NULL, "attr? ",
				    "value? " )) == NULL )
					break;
				if ( (id = ldap_add( ld, dn, attrs )) == -1 )
					ldap_perror( ld, "ldap_add" );
				else
					printf( "Add initiated with id %d\n",
					    id );
				break;

			case 'b':	/* abandon */
				getline( line, sizeof(line), stdin, "msgid? " );
				id = atoi( line );
				if ( ldap_abandon( ld, id ) != 0 )
					ldap_perror( ld, "ldap_abandon" );
				else
					printf( "Abandon successful\n" );
				break;
			default:
				printf( "Possibilities: [ad]d, [ab]ort\n" );
			}
			break;

		case 'v':	/* ldap protocol version */
			getline( line, sizeof(line), stdin,
			    "ldap version? " );
			version = atoi( line );
			if ( ldap_set_option( ld, LDAP_OPT_PROTOCOL_VERSION,
			    (void *) &version ) != 0 ) {
				ldap_perror( ld, "ldap_set_option" );
			}
			break;

		case 'b':	/* asynch bind */
			getline( line, sizeof(line), stdin,
			    "method 0->simple 3->sasl? " );
			method = atoi( line );
			if ( method == 0 ) {
				method = LDAP_AUTH_SIMPLE;
			} else if ( method == 3 ) {
				method = LDAP_AUTH_SASL;
			}
			getline( dn, sizeof(dn), stdin, "dn? " );
			strcat( dn, dnsuffix );

			if ( method == LDAP_AUTH_SIMPLE && dn[0] != '\0' ) {
			} else {
				passwd[0] = '\0';
			}

			if ( method == LDAP_AUTH_SIMPLE ) {
				if ( dn[0] != '\0' ) {
					getline( passwd, sizeof(passwd), stdin,
					    "password? " );
				} else {
					passwd[0] = '\0';
				}
				rc = ldap_simple_bind( ld, dn, passwd );
			} else {
				struct berval	cred;
				char		mechanism[BUFSIZ];

				getline( mechanism, sizeof(mechanism), stdin,
				    "mechanism? " );
				getline( passwd, sizeof(passwd), stdin,
				    "credentials? " );
				cred.bv_val = passwd;
				cred.bv_len = strlen( passwd );
				if ( ldap_sasl_bind( ld, dn, mechanism, &cred,
				    NULL, NULL, &rc ) != LDAP_SUCCESS ) {
					rc = -1;
				}
			}
			if ( rc == -1 ) {
				fprintf( stderr, "ldap_bind failed\n" );
				ldap_perror( ld, "ldap_bind" );
			} else {
				printf( "Bind initiated\n" );
				bound = 1;
			}
			break;

		case 'B':	/* synch bind */
			getline( line, sizeof(line), stdin,
			    "method 0->simple 3->sasl? " );
			method = atoi( line );
			if ( method == 0 ) {
				method = LDAP_AUTH_SIMPLE;
			} else if ( method == 3 ) {
				method = LDAP_AUTH_SASL;
			}
			getline( dn, sizeof(dn), stdin, "dn? " );
			strcat( dn, dnsuffix );

			if ( method == LDAP_AUTH_SIMPLE && dn[0] != '\0' ) {
			} else {
				passwd[0] = '\0';
			}

			if ( method == LDAP_AUTH_SIMPLE ) {
				if ( dn[0] != '\0' ) {
					getline( passwd, sizeof(passwd), stdin,
					    "password? " );
				} else {
					passwd[0] = '\0';
				}
				rc = ldap_simple_bind_s( ld, dn, passwd );
				fnname = "ldap_simple_bind_s";
			} else {
				struct berval	cred;
				char		mechanism[BUFSIZ];

				getline( mechanism, sizeof(mechanism), stdin,
				    "mechanism? " );
				getline( passwd, sizeof(passwd), stdin,
				    "credentials? " );
				cred.bv_val = passwd;
				cred.bv_len = strlen( passwd );
				rc = ldap_sasl_bind_s( ld, dn, mechanism,
					&cred, NULL, NULL, NULL );
				fnname = "ldap_sasl_bind_s";
			}
			if ( rc != LDAP_SUCCESS ) {
				fprintf( stderr, "%s failed\n", fnname );
				ldap_perror( ld, fnname );
			} else {
				printf( "Bind successful\n" );
				bound = 1;
			}
			break;

		case 'c':	/* compare */
			getline( dn, sizeof(dn), stdin, "dn? " );
			strcat( dn, dnsuffix );
			getline( attr, sizeof(attr), stdin, "attr? " );
			getline( value, sizeof(value), stdin, "value? " );

			if ( (id = ldap_compare( ld, dn, attr, value )) == -1 )
				ldap_perror( ld, "ldap_compare" );
			else
				printf( "Compare initiated with id %d\n", id );
			break;

		case 'x':	/* extended operation */
			{
			char		oid[100];
			struct berval	val;

			getline( oid, sizeof(oid), stdin, "oid? " );
			getline( value, sizeof(value), stdin, "value? " );
			if ( strncmp( value, "0x", 2 ) == 0 ) {
				val.bv_val = (char *)malloc( strlen( value ) / 2 );
				berval_from_hex( &val, value + 2 );
			} else {
				val.bv_val = strdup( value );
				val.bv_len = strlen( value );
			}
			if ( ldap_extended_operation( ld, oid, &val, NULL,
			    NULL, &id ) != LDAP_SUCCESS ) {
				ldap_perror( ld, "ldap_extended_operation" );
			} else {
				printf( "Extended op initiated with id %d\n",
				    id );
			}
			free( val.bv_val );
			}
			break;

		case 'C':	/* set cache parameters */
#ifdef NO_LIBLCACHE
			getline( line, sizeof(line), stdin,
			    "cache init (memcache 0)? " );
#else
			getline( line, sizeof(line), stdin,
			    "cache init (memcache 0, lcache 1)? " );
#endif
			i = atoi( line );
			if ( i == 0 ) {		/* memcache */
				unsigned long	ttl, size;
				char		**basedns, *dnarray[2];
				LDAPMemCache	*mc;

				getline( line, sizeof(line), stdin,
				    "memcache ttl? " );
				ttl = atoi( line );
				getline( line, sizeof(line), stdin,
				    "memcache size? " );
				size = atoi( line );
				getline( line, sizeof(line), stdin,
				    "memcache baseDN? " );
				if ( *line == '\0' ) {
					basedns = NULL;
				} else {
					dnarray[0] = line;
					dnarray[1] = NULL;
					basedns = dnarray;
				}
				if (( rc = ldap_memcache_init( ttl, size,
				    basedns, NULL, &mc )) != LDAP_SUCCESS ) {
					fprintf( stderr,
					    "ldap_memcache_init: %s\n",
					    ldap_err2string( rc ));
				} else if (( rc = ldap_memcache_set( ld, mc ))
				    != LDAP_SUCCESS ) {
					fprintf( stderr,
					    "ldap_memcache_set: %s\n",
					    ldap_err2string( rc ));
				}

#ifndef NO_LIBLCACHE
			} else if ( i == 1 ) {
				getline( line, sizeof(line), stdin,
				    "cache config file? " );
				if ( line[0] != '\0' ) {
					if ( lcache_init( ld, line ) != 0 ) {
						perror( "ldap_cache_init" );
						break;
					}
				}
				getline( line, sizeof(line), stdin,
				    "cache on/off (on 1, off 0)? " );
				if ( line[0] != '\0' ) {
					i = atoi( line );
					if ( ldap_set_option( ld,
					    LDAP_OPT_CACHE_ENABLE, &i ) != 0 ) {
						ldap_perror( ld, "ldap_cache_enable" );
						break;
					}
				}
				getline( line, sizeof(line), stdin,
				  "cache strategy (check 0, populate 1, localdb 2)? " );
				if ( line[0] != '\0' ) {
					i = atoi( line );
					if ( ldap_set_option( ld,
					    LDAP_OPT_CACHE_STRATEGY, &i )
					    != 0 ) {
						ldap_perror(ld, "ldap_cache_strategy");
						break;
					}
				}
#endif /* !NO_LIBLCACHE */

			} else {
				fprintf( stderr, "unknown cachetype %d\n", i );
			}
			break;

		case 'd':	/* turn on debugging */
#ifndef _WIN32
#ifdef LDAP_DEBUG
			getline( line, sizeof(line), stdin, "debug level? " );
			ldap_debug = atoi( line ) | LDAP_DEBUG_ANY;
			if ( ldap_debug & LDAP_DEBUG_PACKETS ) {
				ber_set_option( NULL, LBER_OPT_DEBUG_LEVEL,
					&ldap_debug );
			}
#else
			printf( "Compile with -DLDAP_DEBUG for debugging\n" );
#endif
#endif
			break;

		case 'E':	/* explode a dn */
			getline( line, sizeof(line), stdin, "dn? " );
			exdn = ldap_explode_dn( line, 0 );
			for ( i = 0; exdn != NULL && exdn[i] != NULL; i++ ) {
				printf( "\t\"%s\"\n", exdn[i] );
			}
			break;

		case 'R':	/* explode an rdn */
			getline( line, sizeof(line), stdin, "rdn? " );
			exdn = ldap_explode_rdn( line, 0 );
			for ( i = 0; exdn != NULL && exdn[i] != NULL; i++ ) {
				printf( "\t\"%s\"\n", exdn[i] );
			}
			break;

		case 'm':	/* modify or modifyrdn */
			if ( strncmp( line, "modify", 4 ) == 0 ) {
				getline( dn, sizeof(dn), stdin, "dn? " );
				strcat( dn, dnsuffix );
				if ( (mods = get_modlist(
				    "mod (0=>add, 1=>delete, 2=>replace -1=>done)? ",
				    "attribute type? ", "attribute value? " ))
				    == NULL )
					break;
				if ( (id = ldap_modify( ld, dn, mods )) == -1 )
					ldap_perror( ld, "ldap_modify" );
				else
					printf( "Modify initiated with id %d\n",
					    id );
			} else if ( strncmp( line, "modrdn", 4 ) == 0 ) {
				getline( dn, sizeof(dn), stdin, "dn? " );
				strcat( dn, dnsuffix );
				getline( rdn, sizeof(rdn), stdin, "newrdn? " );
				getline( line, sizeof(line), stdin,
				    "deleteoldrdn? " );
				if ( (id = ldap_modrdn2( ld, dn, rdn,
				    atoi(line) )) == -1 )
					ldap_perror( ld, "ldap_modrdn" );
				else
					printf( "Modrdn initiated with id %d\n",
					    id );
			} else {
				printf( "Possibilities: [modi]fy, [modr]dn\n" );
			}
			break;

		case 'q':	/* quit */
#ifdef CLDAP
			if ( cldapflg )
				cldap_close( ld );
#endif /* CLDAP */
			if ( !cldapflg )
				ldap_unbind( ld );
			exit( 0 );
			break;

		case 'r':	/* result or remove */
			switch ( command3 ) {
			case 's':	/* result */
				getline( line, sizeof(line), stdin,
				    "msgid (-1=>any)? " );
				if ( line[0] == '\0' )
					id = -1;
				else
					id = atoi( line );
				getline( line, sizeof(line), stdin,
				    "all (0=>any, 1=>all)? " );
				if ( line[0] == '\0' )
					all = 1;
				else
					all = atoi( line );
				if (( msgtype = ldap_result( ld, id, all,
				    tvp, &res )) < 1 ) {
					ldap_perror( ld, "ldap_result" );
					break;
				}
				printf( "\nresult: msgtype %d msgid %d\n",
				    msgtype, ldap_msgid( res ) );
				handle_result( ld, res, 0 );
				res = NULL;
				break;

			case 'm':	/* remove */
				getline( dn, sizeof(dn), stdin, "dn? " );
				strcat( dn, dnsuffix );
				if ( (id = ldap_delete( ld, dn )) == -1 )
					ldap_perror( ld, "ldap_delete" );
				else
					printf( "Remove initiated with id %d\n",
					    id );
				break;

			default:
				printf( "Possibilities: [rem]ove, [res]ult\n" );
				break;
			}
			break;

		case 's':	/* search */
			getline( dn, sizeof(dn), stdin, "searchbase? " );
			strcat( dn, dnsuffix );
			getline( line, sizeof(line), stdin,
			    "scope (0=Base, 1=One Level, 2=Subtree)? " );
			scope = atoi( line );
			getline( filter, sizeof(filter), stdin,
			    "search filter (e.g. sn=jones)? " );
			types = get_list( "attrs to return? " );
			getline( line, sizeof(line), stdin,
			    "attrsonly (0=attrs&values, 1=attrs only)? " );
			attrsonly = atoi( line );

			if ( cldapflg ) {
#ifdef CLDAP
			    getline( line, sizeof(line), stdin,
				"Requestor DN (for logging)? " );
			    if ( cldap_search_s( ld, dn, scope, filter, types,
				    attrsonly, &res, line ) != 0 ) {
				ldap_perror( ld, "cldap_search_s" );
			    } else {
				printf( "\nresult: msgid %d\n",
				    res->lm_msgid );
				handle_result( ld, res, 0 );
				res = NULL;
			    }
#endif /* CLDAP */
			} else {
			    if (( id = ldap_search( ld, dn, scope, filter,
				    types, attrsonly  )) == -1 ) {
				ldap_perror( ld, "ldap_search" );
			    } else {
				printf( "Search initiated with id %d\n", id );
			    }
			}
			free_list( types );
			break;

		case 't':	/* set timeout value */
			getline( line, sizeof(line), stdin, "timeout (-1=infinite)? " );
			timeout.tv_sec = atoi( line );
			if ( timeout.tv_sec < 0 ) {
				tvp = NULL;
			} else {
				tvp = &timeout;
			}
			break;

		case 'U':	/* set ufn search prefix */
			getline( line, sizeof(line), stdin, "ufn prefix? " );
			ldap_ufn_setprefix( ld, line );
			break;

		case 'u':	/* user friendly search w/optional timeout */
			getline( dn, sizeof(dn), stdin, "ufn? " );
			strcat( dn, dnsuffix );
			types = get_list( "attrs to return? " );
			getline( line, sizeof(line), stdin,
			    "attrsonly (0=attrs&values, 1=attrs only)? " );
			attrsonly = atoi( line );

			if ( command2 == 't' ) {
				id = ldap_ufn_search_c( ld, dn, types,
				    attrsonly, &res, ldap_ufn_timeout,
				    &timeout );
			} else {
				id = ldap_ufn_search_s( ld, dn, types,
				    attrsonly, &res );
			}
			if ( res == NULL )
				ldap_perror( ld, "ldap_ufn_search" );
			else {
				printf( "\nresult: err %d\n", id );
				handle_result( ld, res, 0 );
				res = NULL;
			}
			free_list( types );
			break;

		case 'l':	/* URL search */
			getline( line, sizeof(line), stdin,
			    "attrsonly (0=attrs&values, 1=attrs only)? " );
			attrsonly = atoi( line );
			getline( line, sizeof(line), stdin, "LDAP URL? " );
			if (( id = ldap_url_search( ld, line, attrsonly  ))
				== -1 ) {
			    ldap_perror( ld, "ldap_url_search" );
			} else {
			    printf( "URL search initiated with id %d\n", id );
			}
			break;

		case 'p':	/* parse LDAP URL */
			getline( line, sizeof(line), stdin, "LDAP URL? " );
			if (( i = ldap_url_parse( line, &ludp )) != 0 ) {
			    fprintf( stderr, "ldap_url_parse: error %d (%s)\n", i,
						url_parse_err2string( i ));
			} else {
			    printf( "\t  host: " );
			    if ( ludp->lud_host == NULL ) {
				printf( "DEFAULT\n" );
			    } else {
				printf( "<%s>\n", ludp->lud_host );
			    }
			    printf( "\t  port: " );
			    if ( ludp->lud_port == 0 ) {
				printf( "DEFAULT\n" );
			    } else {
				printf( "%d\n", ludp->lud_port );
			    }
			    printf( "\tsecure: %s\n", ( ludp->lud_options &
				    LDAP_URL_OPT_SECURE ) != 0 ? "Yes" : "No" );
			    printf( "\t    dn: " );
			    if ( ludp->lud_dn == NULL ) {
				printf( "ROOT\n" );
			    } else {
				printf( "%s\n", ludp->lud_dn );
			    }
			    printf( "\t attrs:" );
			    if ( ludp->lud_attrs == NULL ) {
				printf( " ALL" );
			    } else {
				for ( i = 0; ludp->lud_attrs[ i ] != NULL; ++i ) {
				    printf( " <%s>", ludp->lud_attrs[ i ] );
				}
			    }
			    printf( "\n\t scope: %s\n", ludp->lud_scope == LDAP_SCOPE_ONELEVEL ?
				"ONE" : ludp->lud_scope == LDAP_SCOPE_BASE ? "BASE" :
				ludp->lud_scope == LDAP_SCOPE_SUBTREE ? "SUB" : "**invalid**" );
			    printf( "\tfilter: <%s>\n", ludp->lud_filter );
			    ldap_free_urldesc( ludp );
			}
			    break;

		case 'n':	/* set dn suffix, for convenience */
			getline( line, sizeof(line), stdin, "DN suffix? " );
			strcpy( dnsuffix, line );
			break;

		case 'N':	/* add an LDAPv3 control */
			getline( line, sizeof(line), stdin,
			    "Control oid (. to clear list)? " );
			if ( *line == '.' && *(line+1) == '\0' ) {
			    controls = NULL;
			} else {
			    newctrl = (LDAPControl *) malloc(
				sizeof(LDAPControl) );
			    newctrl->ldctl_oid = strdup( line );
			    getline( line, sizeof(line), stdin,
				"Control value? " );
			    if ( strncmp( line, "0x", 2 ) == 0 ) {
				    newctrl->ldctl_value.bv_val =
				    (char *)malloc( strlen( line ) / 2 );
				    berval_from_hex( &(newctrl->ldctl_value), 
					    line + 2 );
			    } else {
				    newctrl->ldctl_value.bv_val
					= strdup( line );
			    }
			    newctrl->ldctl_value.bv_len = strlen( line );
			    getline( line, sizeof(line), stdin,
				"Critical (0=no, 1=yes)? " );
			    newctrl->ldctl_iscritical = atoi( line );
			    add_control( &controls, newctrl );
			}
			ldap_set_option( ld, LDAP_OPT_SERVER_CONTROLS,
			    controls );
			ldap_get_option( ld, LDAP_OPT_SERVER_CONTROLS,
			    &tmpctrls );
			print_controls( tmpctrls, 0 );
			break;

		case 'P':	/* add a persistent search control */
			getline( line, sizeof(line), stdin, "Changetypes to "
			    " return (additive - add (1), delete (2), "
			    "modify (4), modDN (8))? " );
			changetypes = atoi(line);
			getline( line, sizeof(line), stdin,
			    "Return changes only (0=no, 1=yes)? " );
			changesonly = atoi(line);
			getline( line, sizeof(line), stdin, "Return entry "
			    "change controls (0=no, 1=yes)? " );
			return_echg_ctls = atoi(line);
			getline( line, sizeof(line), stdin,
				"Critical (0=no, 1=yes)? " );
			if ( ldap_create_persistentsearch_control( ld,
			    changetypes, changesonly, return_echg_ctls,
			    (char)atoi(line), &newctrl ) != LDAP_SUCCESS ) {
				ldap_perror( ld, "ldap_create_persistent"
				    "search_control" );
			} else {
			    add_control( &controls, newctrl );
			    ldap_set_option( ld, LDAP_OPT_SERVER_CONTROLS,
				controls );
			    ldap_get_option( ld, LDAP_OPT_SERVER_CONTROLS,
				&tmpctrls );
			    print_controls( tmpctrls, 0 );
			}
			break;
			
		case 'o':	/* set ldap options */
			getline( line, sizeof(line), stdin, "alias deref (0=never, 1=searching, 2=finding, 3=always)?" );
			i = atoi( line );
			ldap_set_option( ld, LDAP_OPT_DEREF, &i );
			getline( line, sizeof(line), stdin, "timelimit?" );
			i = atoi( line );
			ldap_set_option( ld, LDAP_OPT_TIMELIMIT, &i );
			getline( line, sizeof(line), stdin, "sizelimit?" );
			i = atoi( line );
			ldap_set_option( ld, LDAP_OPT_SIZELIMIT, &i );

#ifdef STR_TRANSLATION
			getline( line, sizeof(line), stdin,
				"Automatic translation of T.61 strings (0=no, 1=yes)?" );
			if ( atoi( line ) == 0 ) {
				ld->ld_lberoptions &= ~LBER_OPT_TRANSLATE_STRINGS;
			} else {
				ld->ld_lberoptions |= LBER_OPT_TRANSLATE_STRINGS;
#ifdef LDAP_CHARSET_8859
				getline( line, sizeof(line), stdin,
					"Translate to/from ISO-8859 (0=no, 1=yes?" );
				if ( atoi( line ) != 0 ) {
					ldap_set_string_translators( ld,
					    ldap_8859_to_t61,
					    ldap_t61_to_8859 );
				}
#endif /* LDAP_CHARSET_8859 */
			}
#endif /* STR_TRANSLATION */

#ifdef LDAP_DNS
			getline( line, sizeof(line), stdin,
				"Use DN & DNS to determine where to send requests (0=no, 1=yes)?" );
			optval = ( atoi( line ) != 0 );
			ldap_set_option( ld, LDAP_OPT_DNS, (void *) optval );
#endif /* LDAP_DNS */

			getline( line, sizeof(line), stdin,
				"Recognize and chase referrals (0=no, 1=yes)?" );
			optval = ( atoi( line ) != 0 );
			ldap_set_option( ld, LDAP_OPT_REFERRALS,
			    (void *) optval );
			if ( optval ) {
				getline( line, sizeof(line), stdin,
					"Prompt for bind credentials when chasing referrals (0=no, 1=yes)?" );
				if ( atoi( line ) != 0 ) {
					ldap_set_rebind_proc( ld, bind_prompt,
					    NULL );
				}
			}
#ifdef NET_SSL
			getline( line, sizeof(line), stdin,
				"Use Secure Sockets Layer - SSL (0=no, 1=yes)?" );
			optval = ( atoi( line ) != 0 );
			if ( optval ) {
				getline( line, sizeof(line), stdin,
				    "security DB path?" ); 
				if ( ldapssl_client_init( (*line == '\0') ?
				    NULL : line, NULL ) < 0 ) {
					perror( "ldapssl_client_init" );
					optval = 0;     /* SSL not avail. */
				} else if ( ldapssl_install_routines( ld )
				    < 0 ) {
					ldap_perror( ld,
					    "ldapssl_install_routines" );
					optval = 0;     /* SSL not avail. */
				}
			}

			ldap_set_option( ld, LDAP_OPT_SSL,
			    optval ? LDAP_OPT_ON : LDAP_OPT_OFF );

			getline( line, sizeof(line), stdin,
				"Set SSL options (0=no, 1=yes)?" );
			optval = ( atoi( line ) != 0 );
			while ( 1 ) {
			    PRInt32 sslopt;
			    PRBool  on;

			    getline( line, sizeof(line), stdin,
				    "Option to set (0 if done)?" );
			    sslopt = atoi(line);
			    if ( sslopt == 0 ) {
				break;
			    }
			    getline( line, sizeof(line), stdin,
				    "On=1, Off=0?" );
			    on = ( atoi( line ) != 0 );
			    if ( ldapssl_set_option( ld, sslopt, on ) != 0 ) {
				ldap_perror( ld, "ldapssl_set_option" );
			    }
			}
#endif

			getline( line, sizeof(line), stdin, "Reconnect?" );
			ldap_set_option( ld, LDAP_OPT_RECONNECT,
			    ( atoi( line ) == 0 ) ? LDAP_OPT_OFF :
			    LDAP_OPT_ON );

			getline( line, sizeof(line), stdin, "Async I/O?" );
			ldap_set_option( ld, LDAP_OPT_ASYNC_CONNECT,
			    ( atoi( line ) == 0 ) ? LDAP_OPT_OFF :
			    LDAP_OPT_ON );
			break;

		case 'I':	/* initialize display templates */
			getline( line, sizeof(line), stdin,	
				"Template file [ldaptemplates.conf]?" );
			if (( i = ldap_init_templates( *line == '\0' ?
				"ldaptemplates.conf" : line, &tmpllist ))
				!= 0 ) {
			    fprintf( stderr, "ldap_init_templates: %s\n",
				    ldap_tmplerr2string( i ));
			}
			break;

		case 'T':	/* read & display using template */
			getline( dn, sizeof(dn), stdin, "entry DN? " );
			strcat( dn, dnsuffix );
			if (( i = ldap_entry2text_search( ld, dn, NULL, NULL,
				tmpllist, NULL, NULL, entry2textwrite, stdout,
				"\n", 0, 0 )) != LDAP_SUCCESS ) {
			    fprintf( stderr, "ldap_entry2text_search: %s\n",
				    ldap_err2string( i ));
			}
			break;

		case 'L':	/* set preferred language */
			getline( line, sizeof(line), stdin,
				"Preferred language? " );
			if ( *line == '\0' ) {
				ldap_set_option( ld,
				    LDAP_OPT_PREFERRED_LANGUAGE, NULL );
			} else {
				ldap_set_option( ld,
				    LDAP_OPT_PREFERRED_LANGUAGE, line );
			}
			break;

		case 'F':	/* create filter */
			{
				char	filtbuf[ 512 ], pattern[ 512 ];
				char	prefix[ 512 ], suffix[ 512 ];
				char	attr[ 512 ], value[ 512 ];
				char	*dupvalue, **words;

				getline( pattern, sizeof(pattern), stdin,
				    "pattern? " );
				getline( prefix, sizeof(prefix), stdin,
				    "prefix? " );
				getline( suffix, sizeof(suffix), stdin,
				    "suffix? " );
				getline( attr, sizeof(attr), stdin,
				    "attribute? " );
				getline( value, sizeof(value), stdin,
				    "value? " );
				
				if (( dupvalue = strdup( value )) != NULL ) {
					words = string2words( value, " " );
				} else {
					words = NULL;
				}
				if ( ldap_create_filter( filtbuf,
				    sizeof(filtbuf), pattern, prefix, suffix,
				    attr, value, words) != 0 ) {
					fprintf( stderr,
						"ldap_create_filter failed\n" );
				} else {
					printf( "filter is \"%s\"\n", filtbuf );
				}
				if ( dupvalue != NULL ) free( dupvalue );
				if ( words != NULL ) free( words );
			}
			break;

		case '?':	/* help */
		case '\0':	/* help */
    printf( "Commands: [ad]d         [ab]andon         [b]ind\n" );
    printf( "          synch [B]ind  [c]ompare         [l]URL search\n" );
    printf( "          [modi]fy      [modr]dn          [rem]ove\n" );
    printf( "          [res]ult      [s]earch          [q]uit/unbind\n\n" );
    printf( "          [u]fn search  [ut]fn search with timeout\n" );
    printf( "          [d]ebug       [C]set cache parms[g]set msgid\n" );
    printf( "          d[n]suffix    [t]imeout         [v]ersion\n" );
    printf( "          [U]fn prefix  [?]help           [o]ptions\n" );
    printf( "          [E]xplode dn  [p]arse LDAP URL  [R]explode RDN\n" );
    printf( "          e[x]tended op [F]ilter create\n" );
    printf( "          set co[N]trols    set preferred [L]anguage\n" );
    printf( "          add a [P]ersistent search control\n" );
    printf( "          [I]nitialize display templates\n" );
    printf( "          [T]read entry and display using template\n" );
			break;

		default:
			printf( "Invalid command.  Type ? for help.\n" );
			break;
		}

		(void) memset( line, '\0', sizeof(line) );
	}

	return( 0 );
}

static void
handle_result( LDAP *ld, LDAPMessage *lm, int onlyone )
{
	int	msgtype;

	switch ( (msgtype = ldap_msgtype( lm )) ) {
	case LDAP_RES_COMPARE:
		printf( "Compare result\n" );
		print_ldap_result( ld, lm, "compare" );
		break;

	case LDAP_RES_SEARCH_RESULT:
		printf( "Search result\n" );
		print_ldap_result( ld, lm, "search" );
		break;

	case LDAP_RES_SEARCH_ENTRY:
		printf( "Search entry\n" );
		print_search_entry( ld, lm, onlyone );
		break;

	case LDAP_RES_SEARCH_REFERENCE:
		printf( "Search reference\n" );
		print_search_reference( ld, lm, onlyone );
		break;

	case LDAP_RES_ADD:
		printf( "Add result\n" );
		print_ldap_result( ld, lm, "add" );
		break;

	case LDAP_RES_DELETE:
		printf( "Delete result\n" );
		print_ldap_result( ld, lm, "delete" );
		break;

	case LDAP_RES_MODIFY:
		printf( "Modify result\n" );
		print_ldap_result( ld, lm, "modify" );
		break;

	case LDAP_RES_MODRDN:
		printf( "ModRDN result\n" );
		print_ldap_result( ld, lm, "modrdn" );
		break;

	case LDAP_RES_BIND:
		printf( "Bind result\n" );
		print_ldap_result( ld, lm, "bind" );
		break;
	case LDAP_RES_EXTENDED:
		if ( ldap_msgid( lm ) == LDAP_RES_UNSOLICITED ) {
			printf( "Unsolicited result\n" );
			print_ldap_result( ld, lm, "unsolicited" );
		} else {
			printf( "ExtendedOp result\n" );
			print_ldap_result( ld, lm, "extendedop" );
		}
		break;

	default:
		printf( "Unknown result type 0x%x\n", msgtype );
		print_ldap_result( ld, lm, "unknown" );
	}

	if ( !onlyone ) {
		ldap_msgfree( lm );
	}
}

static void
print_ldap_result( LDAP *ld, LDAPMessage *lm, char *s )
{
	int		lderr;
	char		*matcheddn, *errmsg, *oid, **refs;
	LDAPControl	**ctrls;
	struct berval	*servercred, *data;

	if ( ldap_parse_result( ld, lm, &lderr, &matcheddn, &errmsg, &refs,
	    &ctrls, 0 ) != LDAP_SUCCESS ) {
		ldap_perror( ld, "ldap_parse_result" );
	} else {
		fprintf( stderr, "%s: %s", s, ldap_err2string( lderr ));
		if ( lderr == LDAP_CONNECT_ERROR ) {
			perror( " - " );
		} else {
			fputc( '\n', stderr );
		}
		if ( errmsg != NULL ) {
			if ( *errmsg != '\0' ) {
				fprintf( stderr, "Additional info: %s\n",
				    errmsg );
			}
			ldap_memfree( errmsg );
		}
		if ( matcheddn != NULL ) {
			if ( NAME_ERROR( lderr )) {
				fprintf( stderr, "Matched DN: %s\n",
				    matcheddn );
			}
			ldap_memfree( matcheddn );
		}
		print_referrals( refs, 1 );
		print_controls( ctrls, 1 );
	}

	/* if SASL bind response, get and show server credentials */
	if ( ldap_msgtype( lm ) == LDAP_RES_BIND &&
	    ldap_parse_sasl_bind_result( ld, lm, &servercred, 0 ) ==
	    LDAP_SUCCESS && servercred != NULL ) {
		fputs( "\tSASL server credentials:\n", stderr );
		bprint( servercred->bv_val, servercred->bv_len );
		ber_bvfree( servercred );
	}

	/* if ExtendedOp response, get and show oid plus data */
	if ( ldap_msgtype( lm ) == LDAP_RES_EXTENDED &&
	    ldap_parse_extended_result( ld, lm, &oid, &data, 0 ) ==
	    LDAP_SUCCESS ) {
		if ( oid != NULL ) {
			if ( strcmp ( oid, LDAP_NOTICE_OF_DISCONNECTION )
			    == 0 ) {
				printf(
				    "\t%s Notice of Disconnection (OID: %s)\n",
				    s, oid );
			} else {
				printf( "\t%s OID: %s\n", s, oid );
			}
			ldap_memfree( oid );
		}
		if ( data != NULL ) {
			printf( "\t%s data:\n", s );
			bprint( data->bv_val, data->bv_len );
			ber_bvfree( data );
		}
	}
}

static void
print_search_entry( LDAP *ld, LDAPMessage *res, int onlyone )
{
	BerElement	*ber;
	char		*a, *dn, *ufn;
	struct berval	**vals;
	int		i, count;
	LDAPMessage	*e, *msg;
	LDAPControl	**ectrls;

	count = 0;
	for ( msg = ldap_first_message( ld, res );
	    msg != NULL && ( !onlyone || count == 0 );
	    msg = ldap_next_message( ld, msg ), ++count ) {
		if ( ldap_msgtype( msg ) != LDAP_RES_SEARCH_ENTRY ) {
		    handle_result( ld, msg, 1 );	/* something else */
		    continue;
		}
		e = msg;

		dn = ldap_get_dn( ld, e );
		printf( "\tDN: %s\n", dn );

		ufn = ldap_dn2ufn( dn );
		printf( "\tUFN: %s\n", ufn );
#ifdef WINSOCK
		ldap_memfree( dn );
		ldap_memfree( ufn );
#else /* WINSOCK */
		free( dn );
		free( ufn );
#endif /* WINSOCK */

		for ( a = ldap_first_attribute( ld, e, &ber ); a != NULL;
		    a = ldap_next_attribute( ld, e, ber ) ) {
			printf( "\t\tATTR: %s\n", a );
			if ( (vals = ldap_get_values_len( ld, e, a ))
			    == NULL ) {
				printf( "\t\t\t(no values)\n" );
			} else {
				for ( i = 0; vals[i] != NULL; i++ ) {
					int		nonascii = 0;
					unsigned long	j;

					for ( j = 0; j < vals[i]->bv_len; j++ )
						if ( !isascii( vals[i]->bv_val[j] ) ) {
							nonascii = 1;
							break;
						}

					if ( nonascii ) {
						printf( "\t\t\tlength (%ld) (not ascii)\n", vals[i]->bv_len );
#ifdef BPRINT_NONASCII
						bprint( vals[i]->bv_val,
						    vals[i]->bv_len );
#endif /* BPRINT_NONASCII */
						continue;
					}
					printf( "\t\t\tlength (%ld) %s\n",
					    vals[i]->bv_len, vals[i]->bv_val );
				}
				ber_bvecfree( vals );
			}
			ldap_memfree( a );
		}
		if ( ldap_get_lderrno( ld, NULL, NULL ) != LDAP_SUCCESS ) {
			ldap_perror( ld,
			    "ldap_first_attribute/ldap_next_attribute" );
		}
		if ( ber != NULL ) {
			ber_free( ber, 0 );
		}

		if ( ldap_get_entry_controls( ld, e, &ectrls )
		    != LDAP_SUCCESS ) {
			ldap_perror( ld, "ldap_get_entry_controls" );
		} else {
			int	changenumpresent;
            ber_int_t changetype;
			char	*prevdn;
			ber_int_t  changenum;

			if ( ldap_parse_entrychange_control( ld, ectrls,
			    &changetype, &prevdn, &changenumpresent,
			    &changenum ) == LDAP_SUCCESS ) {
				fprintf( stderr, "EntryChangeNotification\n"
				    "\tchangeType:   %s\n", 
				    changetype_num2string( changetype ));
				if ( prevdn != NULL ) {
					fprintf( stderr,
					    "\tpreviousDN:   \"%s\"\n",
					    prevdn );
				}
				if ( changenumpresent ) {
					fprintf( stderr, "\tchangeNumber: %d\n",
					    changenum );
				}
				if ( prevdn != NULL ) {
					free( prevdn );
				}
			}
			print_controls( ectrls, 1 );
		}
	}
}


static char *
changetype_num2string( ber_int_t chgtype )
{
    static char buf[ 25 ];
    char	*s;

    switch( chgtype ) {
    case LDAP_CHANGETYPE_ADD:
	s = "add";
	break;
    case LDAP_CHANGETYPE_DELETE:
	s = "delete";
	break;
    case LDAP_CHANGETYPE_MODIFY:
	s = "modify";
	break;
    case LDAP_CHANGETYPE_MODDN:
	s = "moddn";
	break;
    default:
	s = buf;
	sprintf( s, "unknown (%d)", chgtype );
    }

    return( s );
}


static void
print_search_reference( LDAP *ld, LDAPMessage *res, int onlyone )
{
	LDAPMessage	*msg;
	LDAPControl	**ctrls;
	char		**refs;
	int		count;

	count = 0;
	for ( msg = ldap_first_message( ld, res );
	    msg != NULL && ( !onlyone || count == 0 );
	    msg = ldap_next_message( ld, msg ), ++count ) {
		if ( ldap_msgtype( msg ) != LDAP_RES_SEARCH_REFERENCE ) {
			handle_result( ld, msg, 1 );	/* something else */
			continue;
		}

		if ( ldap_parse_reference( ld, msg, &refs, &ctrls, 0 ) !=
		    LDAP_SUCCESS ) {
			ldap_perror( ld, "ldap_parse_reference" );
		} else {
			print_referrals( refs, 1 );
			print_controls( ctrls, 1 );
		}
	}
}


static void
print_referrals( char **refs, int freeit )
{
	int	i;

	if ( refs == NULL ) {
		return;
	}

	fprintf( stderr, "Referrals:\n" );
	for ( i = 0; refs[ i ] != NULL; ++i ) {
		fprintf( stderr, "\t%s\n", refs[ i ] );
	}

	if ( freeit ) {
		ldap_value_free( refs );
	}
}


static void
print_controls( LDAPControl **ctrls, int freeit )
{
	int	i;

	if ( ctrls == NULL ) {
		return;
	}

	fprintf( stderr, "Controls:\n" );
	for ( i = 0; ctrls[ i ] != NULL; ++i ) {
		if ( i > 0 ) {
			fputs( "\t-----------\n", stderr );
		}
		fprintf( stderr, "\toid:      %s\n", ctrls[ i ]->ldctl_oid );
		fprintf( stderr, "\tcritical: %s\n",
		    ctrls[ i ]->ldctl_iscritical ? "YES" : "NO" );
		fputs( "\tvalue:\n", stderr );
		bprint( ctrls[ i ]->ldctl_value.bv_val,
		    ctrls[ i ]->ldctl_value.bv_len );
	}

	if ( freeit ) {
		ldap_controls_free( ctrls );
	}
}


static int
entry2textwrite( void *fp, char *buf, int len )
{
        return( fwrite( buf, len, 1, (FILE *)fp ) == 0 ? -1 : len );
}


/* similar to getfilter.c:break_into_words() */
static char **
string2words( char *str, char *delims )
{
    char        *word, **words;
    int         count;
    char        *lasts;

    if (( words = (char **)calloc( 1, sizeof( char * ))) == NULL ) {
	    return( NULL );
    }
    count = 0;
    words[ count ] = NULL;

    word = ldap_utf8strtok_r( str, delims, &lasts );
    while ( word != NULL ) {
        if (( words = (char **)realloc( words,
                ( count + 2 ) * sizeof( char * ))) == NULL ) {
	    free( words );
            return( NULL );
        }

        words[ count ] = word;
        words[ ++count ] = NULL;
        word = ldap_utf8strtok_r( NULL, delims, &lasts );
    }

    return( words );
}


static const char *
url_parse_err2string( int e )
{
    const char	*s = "unknown";

    switch( e ) {
    case LDAP_URL_ERR_NOTLDAP:
	s = "URL doesn't begin with \"ldap://\"";
	break;
    case LDAP_URL_ERR_NODN:
	s = "URL has no DN (required)";
	break;
    case LDAP_URL_ERR_BADSCOPE:
	s = "URL scope string is invalid";
	break;
    case LDAP_URL_ERR_MEM:
	s = "can't allocate memory space";
	break;
    case LDAP_URL_ERR_PARAM:
	s = "bad parameter to an URL function";
	break;
    case LDAP_URL_UNRECOGNIZED_CRITICAL_EXTENSION:
	s = "unrecognized critical URL extension";
	break;
    }

    return( s );
}


/*
 * Print arbitrary stuff, for debugging.
 */

#define BPLEN	48
static void
bprint( char *data, int len )
{
    static char	hexdig[] = "0123456789abcdef";
    char	out[ BPLEN ];
    int		i = 0;

    memset( out, 0, BPLEN );
    for ( ;; ) {
	if ( len < 1 ) {
		fprintf( stderr, "\t%s\n", ( i == 0 ) ? "(end)" : out );
	    break;
	}

#ifndef HEX
	if ( isgraph( (unsigned char)*data )) {
	    out[ i ] = ' ';
	    out[ i+1 ] = *data;
	} else {
#endif
	    out[ i ] = hexdig[ ( *data & 0xf0 ) >> 4 ];
	    out[ i+1 ] = hexdig[ *data & 0x0f ];
#ifndef HEX
	}
#endif
	i += 2;
	len--;
	data++;

	if ( i > BPLEN - 2 ) {
	    fprintf( stderr, "\t%s\n", out );
	    memset( out, 0, BPLEN );
	    i = 0;
	    continue;
	}
	out[ i++ ] = ' ';
    }

    fflush( stderr );
}
