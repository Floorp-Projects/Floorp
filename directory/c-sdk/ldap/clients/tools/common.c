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
 * code that is shared by two or more of the LDAP command line tools
 */

#include "ldaptool.h"
#include "fileurl.h"

#ifdef LDAP_TOOL_ARGPIN
#include "argpin.h"
#include "ntuserpin.h"
#endif /* LDAP_TOOL_ARGPIN */

#include <nspr.h>
#include <stdlib.h>
#include <time.h>	/* for time() and ctime() */

static LDAP_REBINDPROC_CALLBACK get_rebind_credentials;
static void print_library_info( const LDAPAPIInfo *aip, FILE *fp );
static int wait4result( LDAP *ld, int msgid, struct berval **servercredp,
	char *msg );
static int parse_result( LDAP *ld, LDAPMessage *res,
	struct berval **servercredp, char *msg, int freeit );

#ifdef LDAPTOOL_DEBUG_MEMORY   
static void *ldaptool_debug_malloc( size_t size );
static void *ldaptool_debug_calloc( size_t nelem, size_t elsize );
static void *ldaptool_debug_realloc( void *ptr, size_t size );
static void ldaptool_debug_free( void *ptr );
#endif /* LDAPTOOL_DEBUG_MEMORY */

#if defined(NET_SSL) && defined(LDAP_TOOL_PKCS11)
static void ldaptool_setcallbacks( struct ldapssl_pkcs_fns *pfns);
static char * buildTokenCertName( const char *tokenName, const char *certName);
#ifdef FORTEZZA
static int ldaptool_fortezza_init( int exit_on_error );
static int ldaptool_fortezza_alert( void *arg, PRBool onOpen,
	char *string, int value1, void *value2 );
static void * ldaptool_fortezza_getpin( char **passwordp );
static char * ldaptool_fortezza_err2string( int err );
#endif /* FORTEZZA */
#endif

/* copied from ldaprot.h - required to parse the pwpolicy ctrl */
#define LDAP_TAG_PWP_WARNING	0xA0L   /* context specific + constructed */
#define LDAP_TAG_PWP_SECSLEFT	0x80L   /* context specific + primitive */
#define LDAP_TAG_PWP_GRCLOGINS	0x81L   /* context specific + primitive + 1 */
#define LDAP_TAG_PWP_ERROR	0x81L   /* context specific + primitive + 1 */

/*
 * display usage for common options with one exception: -f is not included
 * since the description tends to be tool-specific.
 *
 * As of 1-Jul-1998, of the characters in the set [A-Za-z] the following are
 * not currently used by any of the tools: EJgjqr
 */
void
ldaptool_common_usage( int two_hosts )
{
    fprintf( stderr, "    -n\t\tshow what would be done but don't actually do it\n" );
    fprintf( stderr, "    -v\t\trun in verbose mode (diagnostics to standard output)\n" );
    if ( two_hosts ) {
	fprintf( stderr, "    -h host\tLDAP server1 name or IP address (default: %s)\n", LDAPTOOL_DEFHOST );
	fprintf( stderr, "    -p port\tLDAP server1 TCP port number (default: %d)\n", LDAP_PORT );
	fprintf( stderr, "    -h host\tLDAP server2 name or IP address (default: %s)\n", LDAPTOOL_DEFHOST );
	fprintf( stderr, "    -p port\tLDAP server2 TCP port number (default: %d)\n", LDAP_PORT );
    } else {
	fprintf( stderr, "    -h host\tLDAP server name or IP address (default: %s)\n", LDAPTOOL_DEFHOST );
	fprintf( stderr, "    -p port\tLDAP server TCP port number (default: %d)\n", LDAP_PORT );
    }
    fprintf( stderr,
	    "    -V n\tLDAP protocol version number (%d or %d; default: %d)\n",
	    LDAP_VERSION2, LDAP_VERSION3, LDAP_VERSION3 );
#if defined(NET_SSL)
    fprintf( stderr, "    -Z\t\tmake an SSL-encrypted connection\n" );
    fprintf( stderr, "    -P pathname\tpath to SSL certificate database (default: current directory)\n" );
    fprintf( stderr, "    -N\t\tname of certificate to use for SSL client authentication\n" );
    fprintf( stderr, "    -K pathname\tpath to key database to use for SSL client authentication\n" );
    fprintf( stderr, "    \t\t(default: path to certificate database provided with -P option)\n" );
#ifdef LDAP_TOOL_PKCS11
    fprintf( stderr, "    -m pathname\tpath to security module database\n");
#endif /* LDAP_TOOL_PKCS11 */
    fprintf( stderr, "    -W\t\tSSL key password\n" );
    fprintf( stderr, "    -3\t\tcheck hostnames in SSL certificates\n" );

#ifdef LDAP_TOOL_PKCS11
    fprintf( stderr, "    -Q [token][:certificate name]\tPKCS 11\n" );
    fprintf( stderr, "    -X pathname\tFORTEZZA compromised key list (CKL)\n" );
    fprintf( stderr, "    -I pin\tcard password file\n" );
#endif /* LDAP_TOOL_PKCS11 */

#endif /* NET_SSL */
    fprintf( stderr, "    -D binddn\tbind dn\n" );
    fprintf( stderr, "    -w passwd\tbind passwd (for simple authentication)\n" );
    fprintf( stderr, "    -w - \tprompt for bind passwd (for simple authentication)\n" );
    fprintf( stderr, "    -j file\tread bind passwd from 'file' (for simple authentication)\n" );
    fprintf( stderr, "    -E\t\task server to expose (report) bind identity\n" );
    fprintf( stderr, "    -g\t\tdo not send a password policy request control\n" );
#ifdef LDAP_DEBUG
    fprintf( stderr, "    -d level\tset LDAP debugging level to `level'\n" );
#endif
    fprintf( stderr, "    -R\t\tdo not automatically follow referrals\n" );
    fprintf( stderr, "    -O limit\tmaximum number of referral hops to traverse (default: %d)\n", LDAPTOOL_DEFREFHOPLIMIT );
    fprintf( stderr, "    -M\t\tmanage references (treat them as regular entries)\n" );
    fprintf( stderr, "    -0\t\tignore LDAP library version mismatches\n" );

#ifndef NO_LIBLCACHE
    fprintf( stderr, "    -C cfgfile\tuse local database described by cfgfile\n" );
#endif
    fprintf( stderr, "    -i charset\tcharacter set for command line input (default taken from locale)\n" );
    fprintf( stderr, "    -k dir\tconversion routine directory (default: current directory)\n" );
#if 0
/*
 * Suppress usage for -y (old proxied authorization control) even though
 * we still support it.  We want to encourage people to use -Y instead (the
 * new proxied authorization control).
 */
    fprintf( stderr, "    -y proxydn\tDN used for proxy authorization\n" );
#endif
    fprintf( stderr, "    -Y proxyid\tproxied authorization id,\n" );
    fprintf( stderr, "              \te.g, dn:uid=bjensen,dc=example,dc=com\n" );
    fprintf( stderr, "    -H\t\tdisplay usage information\n" );
    fprintf( stderr, "    -J controloid[:criticality[:value|::b64value|:<fileurl]]\n" );
    fprintf( stderr, "\t\tcriticality is a boolean value (default is false)\n" );
}

/* globals */
char			*ldaptool_charset = NULL;
char			*ldaptool_convdir = NULL;
char			*ldaptool_host = LDAPTOOL_DEFHOST;
char			*ldaptool_host2 = LDAPTOOL_DEFHOST;
int			ldaptool_port = LDAP_PORT;
int			ldaptool_port2 = LDAP_PORT;
int			ldaptool_verbose = 0;
int			ldaptool_not = 0;
FILE			*ldaptool_fp = NULL;
char			*ldaptool_progname = "";
LDAPControl		*ldaptool_request_ctrls[CONTROL_REQUESTS] = {0};
#ifdef LDAP_DEBUG
int			ldaptool_dbg_lvl = 0;
#endif /* LDAP_DEBUG */

/* statics */
static char		*binddn = NULL;
static char		*passwd = NULL;
static int		send_auth_response_ctrl = 0;
static int		user_specified_port = 0;
static int		user_specified_port2 = 0;
static int		chase_referrals = 1;
static int		lib_version_mismatch_is_fatal = 1;
static int		ldversion = -1;	/* use default */
static int		refhoplim = LDAPTOOL_DEFREFHOPLIMIT;
static int		send_manage_dsait_ctrl = 0;
static int		prompt_password = 0;
static FILE		*password_fp = NULL;
static char             *proxyauth_id = NULL;
static int		proxyauth_version = 2;	/* use newer proxy control */
static int		no_pwpolicy_req_ctrl = 0;

#ifndef NO_LIBLCACHE
static char		*cache_config_file = NULL;
#endif /* !NO_LIBLCACHE */
#if defined(NET_SSL)
static int		secure = 0;
static int		isZ = 0;
static int		isN = 0;
static int		isW = 0;
static int		isw = 0;
static int		isD = 0;
static int		isj = 0;
static int		ssl_strength = LDAPTOOL_DEFSSLSTRENGTH;
static char		*ssl_certdbpath = NULL;
static char		*ssl_keydbpath = NULL;
static char		*ssl_certname = NULL;
static char		*ssl_passwd = NULL;

#ifdef LDAP_TOOL_PKCS11
static char     	*ssl_secmodpath = NULL;

static char             *pkcs_token = NULL;

static char             *ssl_donglefile = NULL;

#if 0
static char             *pkcs_pin = NULL;
#endif
static struct ldapssl_pkcs_fns local_pkcs_fns = 
    {0,NULL,NULL,NULL,NULL,NULL,NULL,NULL, NULL };

#ifdef FORTEZZA
static uint32		fortezza_cardmask = 0;
static char		*fortezza_personality = NULL;
static char		*fortezza_krlfile = NULL;
static char		*fortezza_pin = NULL;
#endif /* FORTEZZA */
#endif /* LDAP_TOOL_PKCS11 */
#endif /* NET_SSL */



/*
 * Handle general initialization and options that are common to all of
 * the LDAP tools.
 * Handle options that are common to all of the LDAP tools.
 * Note the the H option is included here but handled via the
 * extra_opt_callback function (along with any "extra_opts" ).
 *
 * Return: final value for optind or -1 if usage should be displayed (for
 * some fatal errors, we call exit here).
 */
int
ldaptool_process_args( int argc, char **argv, char *extra_opts,
	int two_hosts, void (*extra_opt_callback)( int option, char *optarg ))
{
    int		rc, i, hostnum;
    char	*optstring, *common_opts;
    extern char	*optarg;
    extern int	optind;
    LDAPAPIInfo	ldai;
    char *ctrl_arg, *ctrl_oid=NULL, *ctrl_value=NULL;
    int ctrl_criticality=0, vlen;
    LDAPControl *ldctrl;

    /*
     * Set program name global based on argv[0].
     */
    if (( ldaptool_progname = strrchr( argv[ 0 ], '/' )) == NULL ) {
        ldaptool_progname = argv[ 0 ];
    } else {
        ++ldaptool_progname;
    }

#ifdef LDAPTOOL_DEBUG_MEMORY
    {
	struct ldap_memalloc_fns mafns = {
		ldaptool_debug_malloc,
		ldaptool_debug_calloc,
		ldaptool_debug_realloc,
		ldaptool_debug_free
	};

	ldap_set_option( NULL, LDAP_OPT_MEMALLOC_FN_PTRS, &mafns );
    }
#endif	/* LDAPTOOL_DEBUG_MEMORY */

#ifdef LDAP_DEBUG
    i = LDAP_DEBUG_ANY;
    ldap_set_option( NULL, LDAP_OPT_DEBUG_LEVEL, (void *) &i);
#endif

    /*
     * Perform a sanity check on the revision of the LDAP API library to
     * make sure it is at least as new as the one we were compiled against.
     * If the API implementation is from the same vendor as we were compiled
     * against, we also check to make sure the vendor version is at least
     * as new as the library we were compiled against.
     *
     * Version differences are fatal unless the -0 option is passed on the
     * tool command line (that's a zero, not an oh).  We check for the
     * presence of -0 in a crude way to it must appear by itself in argv.
     */
    for ( i = 1; i < argc; ++i ) {
	if ( strcmp( argv[i], "-0" ) == 0 ) {
	    lib_version_mismatch_is_fatal = 0;
	    break;
	}
    }

    memset( &ldai, 0, sizeof(ldai));
    ldai.ldapai_info_version = LDAP_API_INFO_VERSION;
    if (( rc = ldap_get_option( NULL, LDAP_OPT_API_INFO, &ldai )) != 0 ) {
	fprintf( stderr, "%s: unable to retrieve LDAP library version"
		" information;\n\tthis program requires an LDAP library that"
		" implements revision\n\t%d or greater of the LDAP API.\n",
		ldaptool_progname, LDAP_API_VERSION );
	if ( lib_version_mismatch_is_fatal ) {
	    exit( LDAP_LOCAL_ERROR );
	}
    } else if ( ldai.ldapai_api_version < LDAP_API_VERSION ) {
	fprintf( stderr, "%s: this program requires an LDAP library that"
		" implements revision\n\t%d or greater of the LDAP API;"
		" running with revision %d.\n",
		ldaptool_progname, LDAP_API_VERSION, ldai.ldapai_api_version );
	if ( lib_version_mismatch_is_fatal ) {
	    exit( LDAP_LOCAL_ERROR );
	}
    } else if ( strcmp( ldai.ldapai_vendor_name, LDAP_VENDOR_NAME ) != 0 ) {
	fprintf( stderr, "%s: this program requires %s's LDAP\n"
		"\tlibrary version %2.2f or greater; running with\n"
		"\t%s's version %2.2f.\n",
		ldaptool_progname, LDAP_VENDOR_NAME,
		(float)LDAP_VENDOR_VERSION / 100,
		ldai.ldapai_vendor_name,
		(float)ldai.ldapai_vendor_version / 100 );
	if ( lib_version_mismatch_is_fatal ) {
	    exit( LDAP_LOCAL_ERROR );
	}
    } else if ( ldai.ldapai_vendor_version < LDAP_VENDOR_VERSION ) {
	fprintf( stderr, "%s: this program requires %s's LDAP\n"
		"\tlibrary version %2.2f or greater; running with"
		" version %2.2f.\n",
		ldaptool_progname, LDAP_VENDOR_NAME,
		(float)LDAP_VENDOR_VERSION / 100,
		(float)ldai.ldapai_vendor_version / 100 );
	if ( lib_version_mismatch_is_fatal ) {
	    exit( LDAP_LOCAL_ERROR );
	}
    }

    /*
     * Process command line options.
     */
    if ( extra_opts == NULL ) {
	extra_opts = "";
    }

    common_opts = "gnvEMRHZ03d:D:f:h:j:I:K:N:O:P:p:Q:W:w:V:X:m:i:k:y:Y:J:";

    /* note: optstring must include room for liblcache "C:" option */
    if (( optstring = (char *) malloc( strlen( extra_opts ) + strlen( common_opts )
	    + 3 )) == NULL ) {
	perror( "malloc" );
	exit( LDAP_NO_MEMORY );
    }

#ifdef NO_LIBLCACHE
    sprintf( optstring, "%s%s", common_opts, extra_opts );
#else
    sprintf( optstring, "%s%sC:", common_opts, extra_opts );
#endif

    hostnum = 0;
    while ( (i = getopt( argc, argv, optstring )) != EOF ) {
	switch( i ) {
	case 'n':	/* do Not do any LDAP operations */
	    ++ldaptool_not;
	    break;
	case 'v':	/* verbose mode */
	    ++ldaptool_verbose;
	    break;
	case 'd':
#ifdef LDAP_DEBUG
	    ldaptool_dbg_lvl = atoi( optarg );	/* */
	    ber_set_option(NULL, LBER_OPT_DEBUG_LEVEL,
		    (void *)&ldaptool_dbg_lvl);
	    ldaptool_dbg_lvl |= LDAP_DEBUG_ANY;
	    ldap_set_option( NULL, LDAP_OPT_DEBUG_LEVEL,
		    (void *)&ldaptool_dbg_lvl);
#else /* LDAP_DEBUG */
	    fprintf( stderr, "compile with -DLDAP_DEBUG for debugging\n" );
#endif /* LDAP_DEBUG */
	    break;
	case 'R':	/* don't automatically chase referrals */
	    chase_referrals = 0;
	    break;
#ifndef NO_LIBLCACHE
	case 'C':	/* search local database */
	    cache_config_file = strdup( optarg );
	    break;
#endif
	case 'f':	/* input file */
	    if ( optarg[0] == '-' && optarg[1] == '\0' ) {
		ldaptool_fp = stdin;
	    } else if (( ldaptool_fp = ldaptool_open_file( optarg, "r" )) == NULL ) {
			perror( optarg );
			exit( LDAP_PARAM_ERROR );
	    }
	    break;
	case 'h':	/* ldap host */
	    if ( hostnum == 0 ) {
		ldaptool_host = strdup( optarg );
	    } else if ( two_hosts ) {
		ldaptool_host2 = strdup( optarg );
	    } else {
		fprintf( stderr,
			"%s: only one host (-h option) should be specified\n",
			ldaptool_progname );
		return(-1);	/* usage error */
	    }
	    ++hostnum;
	    break;
	case 'D':	/* bind DN */
	    isD = 1;
	    binddn = strdup( optarg );
	    break;
	case 'E':	/* expose bind identity via auth. response control */
	    ++send_auth_response_ctrl;
	    break;

	case 'p':	/* ldap port */
	    if ( !user_specified_port ) {
		++user_specified_port;
		ldaptool_port = atoi( optarg );
	    } else if ( two_hosts ) {
		++user_specified_port2;
		ldaptool_port2 = atoi( optarg );
	    } else {
		fprintf( stderr,
			"%s: only one port (-p option) should be specified\n",
			ldaptool_progname );
		return(-1);	/* usage error */
	    }
		
	    break;
#if defined(NET_SSL)
	case 'P':	/* path to security database */
	    secure = 1; /* do SSL encryption */
	    ssl_certdbpath = strdup( optarg );
	    if (NULL == ssl_certdbpath)
	    {
		perror("malloc");
		exit( LDAP_NO_MEMORY );
	    }
	    break;
	case 'Z':	/* do SSL encryption */
	    secure = 1;
	    isZ = 1;
	    break;
	case 'N':	/* nickname of cert. to use for client auth. */
	    ssl_certname = strdup( optarg );
	    if (NULL == ssl_certname)
	    {
		perror("malloc");
		exit( LDAP_NO_MEMORY );
	    }
	    isN = 1;
	    break;
	case 'K':	/* location of key database */
	    ssl_keydbpath = strdup( optarg );
	    if (NULL == ssl_keydbpath)
	    {
		perror("malloc");
		exit( LDAP_NO_MEMORY );
	    }
	    break;
	case 'W':	/* SSL key password */
	    ssl_passwd = strdup( optarg );
	    if (NULL == ssl_passwd)
	    {
		perror("malloc");
		exit( LDAP_NO_MEMORY );
	    }
	    isW = 1;
	    break;

	case '3':	/* check hostnames in SSL certificates ("no third") */
	    ssl_strength = LDAPSSL_AUTH_CNCHECK;
	    break;

#ifdef LDAP_TOOL_PKCS11
	case 'm':	/* SSL secmod path */
	    ssl_secmodpath = strdup( optarg);
	    if (NULL == ssl_secmodpath)
	    {
		perror("malloc");
		exit( LDAP_NO_MEMORY );
	    }
	    break;

	case 'Q': 	/* FORTEZZA [card][:personality] */
	    pkcs_token = strdup(optarg);
	    if (NULL == pkcs_token)
	    {
		perror("malloc");
		exit( LDAP_NO_MEMORY );
	    }

	    break;
	case 'X':	/* path to FORTEZZA CKL file */
	  /*
	    fortezza_krlfile = strdup( optarg );
	    */

	    break;
	case 'I':	/* FORTEZZA PIN (password file) */
	    ssl_donglefile = strdup( optarg );
	    
	    break;
#endif /* LDAP_TOOL_PKCS11 */

#endif /* NET_SSL */
	case 'w':	/* bind password */
	    isw = 1;
	    if ( optarg[0] == '-' && optarg[1] == '\0' )
		prompt_password = 1;
	    else
		passwd = strdup( optarg );
	    break;
	case 'j':       /* bind password from file */
	    isj = 1;
	    if ((password_fp = fopen( optarg, "r" )) == NULL ) {
		fprintf(stderr, "%s: Unable to open '%s' file\n",
			ldaptool_progname, optarg);
		exit( LDAP_PARAM_ERROR );
	    }
            break;
	case 'O':	/* referral hop limit */
	    refhoplim = atoi( optarg );
	    break;
	case 'V':	/* protocol version */
	    ldversion = atoi (optarg);
	    if ( ldversion != LDAP_VERSION2 && ldversion != LDAP_VERSION3 ) {
		fprintf( stderr, "%s: LDAP protocol version %d is not "
			"supported (use -V%d or -V%d)\n",
			ldaptool_progname, ldversion, LDAP_VERSION2,
			LDAP_VERSION3 );
		exit( LDAP_PARAM_ERROR );
	    }
	    break;
	case 'M':	/* send a manageDsaIT control */
	    send_manage_dsait_ctrl = 1;
	    break;

	case 'i':   /* character set specified */
	    ldaptool_charset = strdup( optarg );
	    if (NULL == ldaptool_charset)
	    {
		perror( "malloc" );
		exit( LDAP_NO_MEMORY );
	    }
		
	    break;
	case 'k':   /* conversion directory */
	    ldaptool_convdir = strdup( optarg );
	    if (NULL == ldaptool_convdir)
	    {
		perror( "malloc" );
		exit( LDAP_NO_MEMORY );
	    }
	    break;
	case 'y':   /* old (version 1) proxied authorization control */
		proxyauth_version = 1;
		/*FALLTHRU*/
	case 'Y':   /* new (version 2 ) proxied authorization control */
	    proxyauth_id = strdup(optarg);
	    if (NULL == proxyauth_id)
	    {
		perror( "malloc" );
		exit( LDAP_NO_MEMORY );
	    }

	    break;

 	case '0':	/* zero -- override LDAP library version check */
	    break;	/* already handled above */

	case 'J':	/* send an arbitrary control */
	    if ( (ctrl_arg = strdup( optarg)) == NULL ) {
		perror ("strdup");
		exit (LDAP_NO_MEMORY);
	    }
	    if (ldaptool_parse_ctrl_arg(ctrl_arg, ':', &ctrl_oid,
		    &ctrl_criticality, &ctrl_value, &vlen)) {
		return (-1);
	    }
	    ldctrl = calloc(1,sizeof(LDAPControl));
	    if (ctrl_value) {
		rc = ldaptool_berval_from_ldif_value( ctrl_value, 
			vlen, &(ldctrl->ldctl_value),
			1 /* recognize file URLs */, 
			0 /* always try file */,
			1 /* report errors */ );
		if ((rc = ldaptool_fileurlerr2ldaperr( rc )) != LDAP_SUCCESS) {
		    fprintf( stderr, "Unable to parse %s\n", ctrl_value);
		    return (-1);
		}
	    }
	    ldctrl->ldctl_oid = ctrl_oid;
	    ldctrl->ldctl_iscritical = ctrl_criticality;
	    ldaptool_add_control_to_array(ldctrl, ldaptool_request_ctrls);
	    break;
	case 'g':	/* do not send password policy request control */
		no_pwpolicy_req_ctrl++;
		break;
	default:
	    (*extra_opt_callback)( i, optarg );
	}
    }

    /* If '-Z' is specified, check if '-P' is specified too. */
    if ( isN || isW ) {
	if ( !isZ ) {
		fprintf( stderr, "%s: with -N, -W options, please specify -Z\n\n", ldaptool_progname ); 
		return (-1);
	}
    }

    /* if '-N' is specified, -W is needed too */
    if ( isN && NULL == ssl_passwd ) {
	fprintf( stderr, "%s: with the -N option, please specify -W also\n\n", ldaptool_progname ); 
	return (-1);
    }

    if ( isj && isw ) {
	fprintf(stderr, "%s: -j and -w options cannot be specified simultaneously\n\n", ldaptool_progname );
	return (-1);
    }

    if ( (isj || isw) && !isD ) {
	fprintf(stderr, "%s: with -j, -w options, please specify -D\n\n", ldaptool_progname );
	return (-1);
    }

    /* use default key and cert DB paths if not set on the command line */
    if ( NULL == ssl_keydbpath ) {
	if ( NULL == ssl_certdbpath ) {
	    ssl_keydbpath = LDAPTOOL_DEFKEYDBPATH;
	} else {
	    /* The ldapssl_.*init() functions, which call NSS_Initialize(),
	     * don't rely on the key DB path ending in the correct filename,
	     * so it is OK to just pass them the cert DB path.
	     */
	    ssl_keydbpath = ssl_certdbpath;
	}
    }
    if ( NULL == ssl_certdbpath ) {
	ssl_certdbpath = LDAPTOOL_DEFCERTDBPATH;
    }

    if (prompt_password != 0) {
	char *password_string = "Enter bind password: ";

#if defined(_WIN32)
	char pbuf[257];
	fputs(password_string,stdout);
	fflush(stdout);
	if (fgets(pbuf,256,stdin) == NULL) {
	    passwd = NULL;
	} else {
	    char *tmp;

	    tmp = strchr(pbuf,'\n');
	    if (tmp) *tmp = '\0';
	    tmp = strchr(pbuf,'\r');
	    if (tmp) *tmp = '\0';
	    passwd = strdup(pbuf);
	}
#else
#if defined(SOLARIS)
	/* 256 characters on Solaris */
	passwd = getpassphrase(password_string);
#else
	/* limited to 16 chars on Tru64, 32 on AIX */
	passwd = getpass(password_string);
#endif
#endif

    } else if (password_fp != NULL) {
	char *linep = NULL;
	int   increment = 0;
	int   c, index;

	/* allocate initial block of memory */
	if ((linep = (char *)malloc(BUFSIZ)) == NULL) {
	    fprintf( stderr, "%s: not enough memory to read password from file\n", ldaptool_progname );
	    exit( LDAP_NO_MEMORY );
	}
	increment++;
	index = 0;
	while ((c = fgetc( password_fp )) != '\n' && c != EOF) {

	    /* check if we will overflow the buffer */
	    if ((c != EOF) && (index == ((increment * BUFSIZ) -1))) {

		/* if we did, add another BUFSIZ worth of bytes */
		if ((linep = (char *)
		    realloc(linep, (increment + 1) * BUFSIZ)) == NULL) {
			fprintf( stderr, "%s: not enough memory to read password from file\n", ldaptool_progname );
			exit( LDAP_NO_MEMORY );
		}
	 	increment++;
	    }
	    linep[index++] = c;
	}
	linep[index] = '\0';
	passwd = linep;
    }

    /*
     * If verbose (-v) flag was passed in, display program name and start time.
     * If the verbose flag was passed at least twice (-vv), also display
     * information about the API library we are running with.
     */
    if ( ldaptool_verbose ) {
	time_t	curtime;

	curtime = time( NULL );
	printf( "%s: started %s\n", ldaptool_progname, ctime( &curtime ));
	if ( ldaptool_verbose > 1 ) {
	    print_library_info( &ldai, stdout );
	}
    }

#ifdef LDAP_TOOL_PKCS11
    if ((NULL != pkcs_token) && (NULL != ssl_certname)) {
	char *result;
	
	if ( (result = buildTokenCertName( pkcs_token, ssl_certname)) != NULL){
	    free( ssl_certname );
	    ssl_certname = result;
	}
    }
#endif /* LDAP_TOOL_PKCS11 */

    free( optstring );

    /*
     * Clean up and return index of first non-option argument.
     */
    if ( ldai.ldapai_extensions != NULL ) {
	ldap_value_free( ldai.ldapai_extensions );
    }
    if ( ldai.ldapai_vendor_name != NULL ) {
	ldap_memfree( ldai.ldapai_vendor_name );
    }

    return( optind );
}


/*
 * Write detailed information about the API library we are running with to fp.
 */
static void
print_library_info( const LDAPAPIInfo *aip, FILE *fp )
{
    int                 i;                                                      
    LDAPAPIFeatureInfo  fi;         

    fprintf( fp, "LDAP Library Information -\n"
	    "    Highest supported protocol version: %d\n"
	    "    LDAP API revision:                  %d\n"
	    "    API vendor name:                    %s\n"
	    "    Vendor-specific version:            %.2f\n",
	    aip->ldapai_protocol_version, aip->ldapai_api_version, 
	    aip->ldapai_vendor_name,
	    (float)aip->ldapai_vendor_version / 100.0 );

    if ( aip->ldapai_extensions != NULL ) {
	fputs( "    LDAP API Extensions:\n", fp );

	for ( i = 0; aip->ldapai_extensions[i] != NULL; i++ )  {
	    fprintf( fp, "        %s", aip->ldapai_extensions[i] );
	    fi.ldapaif_info_version = LDAP_FEATURE_INFO_VERSION;
	    fi.ldapaif_name = aip->ldapai_extensions[i];
	    fi.ldapaif_version = 0;

	    if ( ldap_get_option( NULL, LDAP_OPT_API_FEATURE_INFO, &fi )
		    != 0 ) {
		fprintf( fp, " %s: ldap_get_option( NULL,"
			" LDAP_OPT_API_FEATURE_INFO, ... ) for %s failed"
			" (Feature Info version: %d)\n", ldaptool_progname,
			fi.ldapaif_name, fi.ldapaif_info_version );
	    } else {
		fprintf( fp, " (revision %d)\n", fi.ldapaif_version);
	    }
	}
    }
   fputc( '\n', fp );
}



#ifdef LDAP_TOOL_ARGPIN
static int PinArgRegistration( void )
{
    
    /* pkcs_init was successful  register the pin args */
    
    SVRCOREArgPinObj *ArgPinObj;
    char *tokenName;
#ifndef _WIN32
    SVRCOREStdPinObj *StdPinObj;
#else
    SVRCOREFilePinObj *FilePinObj;
    SVRCOREAltPinObj *AltPinObj;
    SVRCORENTUserPinObj *NTUserPinObj;
    int err;
#endif
    char *pin;
    char *filename;
    /* Create and register the pin object for PKCS 11 */
    local_pkcs_fns.pkcs_getdonglefilename(NULL, &filename);
    local_pkcs_fns.pkcs_getpin(NULL, "", &pin);
#ifndef _WIN32
    if ( SVRCORE_CreateStdPinObj(&StdPinObj, filename, PR_TRUE) !=
	 SVRCORE_Success) {
	fprintf(stderr, "Security Initialization: Unable to create PinObj "
	       "(%d)", PR_GetError());
	return -1;
    }
    if (pin != NULL)
    {
	local_pkcs_fns.pkcs_gettokenname(NULL, &tokenName);
	SVRCORE_CreateArgPinObj(&ArgPinObj, tokenName, pin, (SVRCOREPinObj *)StdPinObj);
	SVRCORE_RegisterPinObj((SVRCOREPinObj *)ArgPinObj);
    }
    else
    {
	SVRCORE_RegisterPinObj((SVRCOREPinObj *)StdPinObj);
    }
#else
    if (NULL != pin)
    {
	local_pkcs_fns.pkcs_gettokenname(NULL, &tokenName);
	if ((err = SVRCORE_CreateNTUserPinObj(&NTUserPinObj)) != SVRCORE_Success){
	    fprintf(stderr, "Security Initialization: Unable to create NTUserPinObj "
		   "(%d)", PR_GetError());
	    exit( LDAP_LOCAL_ERROR );
	}
	if ((err = SVRCORE_CreateArgPinObj(&ArgPinObj, tokenName, pin,
					   (SVRCOREPinObj *)NTUserPinObj)) != SVRCORE_Success)
	{
	    fprintf(stderr, "Security Initialization: Unable to create ArgPinObj "
		   "(%d)", PR_GetError());
	    return -1;

	}
	SVRCORE_RegisterPinObj((SVRCOREPinObj *)ArgPinObj);
	
    }
    else
    {
	if ((err = SVRCORE_CreateNTUserPinObj(&NTUserPinObj)) != SVRCORE_Success){
	    fprintf( stderr, "Security Initialization: Unable to create NTUserPinObj "
		   "(%d)", PR_GetError());
		return -1;
	}
	if (filename && *filename)
	{
	    if ((err = SVRCORE_CreateFilePinObj(&FilePinObj, filename)) !=
		SVRCORE_Success) {
		fprintf( stderr, "Security Initialization: Unable to create FilePinObj "
		       "(%d)", PR_GetError());
		return -1;

	    }
	    if ((err = SVRCORE_CreateAltPinObj(&AltPinObj, (SVRCOREPinObj *)FilePinObj,
					       (SVRCOREPinObj *)NTUserPinObj)) != SVRCORE_Success) {
		fprintf( stderr, "Security Initialization: Unable to create AltPinObj "
		       "(%d)", PR_GetError());
		return -1;
	    }
	    SVRCORE_RegisterPinObj((SVRCOREPinObj *)AltPinObj);
	}
	else
	{
	    SVRCORE_RegisterPinObj((SVRCOREPinObj *)NTUserPinObj);
	}
    }
#endif
    return LDAP_SUCCESS;
    
}
#endif /* LDAP_TOOL_ARGPIN */


/*
 * initialize and return an LDAP session handle.
 * if errors occur, we exit here.
 */
LDAP *
ldaptool_ldap_init( int second_host )
{
    LDAP	*ld = NULL;
    char	*host;
    int		port, rc, user_port;

    if ( ldaptool_not ) {
	return( NULL );
    }
    
    if ( second_host ) {
	host = ldaptool_host2;
	port = ldaptool_port2;
	user_port = user_specified_port2;
    } else {
	host = ldaptool_host;
	port = ldaptool_port;
	user_port = user_specified_port;
    }


    if ( ldaptool_verbose ) {
	printf( "ldap_init( %s, %d )\n", host, port );
    }

#if defined(NET_SSL)
    /*
     * Initialize security libraries and databases and LDAP session.  If
     * ssl_certname is not NULL, then we will attempt to use client auth.
     * if the server supports it.
     */
#ifdef LDAP_TOOL_PKCS11
    ldaptool_setcallbacks( &local_pkcs_fns );

    if ( !second_host 	&& secure  
	 &&(rc = ldapssl_pkcs_init( &local_pkcs_fns))  < 0) {
	    /* secure connection requested -- fail if no SSL */
	    rc = PORT_GetError();
	    fprintf( stderr, "SSL initialization failed: error %d (%s)\n",
		    rc, ldapssl_err2string( rc ));
	    exit( LDAP_LOCAL_ERROR );
    }

#ifdef LDAP_TOOL_ARGPIN
    if (secure) {
	if (PinArgRegistration( )) {
	    exit( LDAP_LOCAL_ERROR);
	}
    }
#endif /* LDAP_TOOL_ARGPIN */

#else /* LDAP_TOOL_PKCS11 */
    if ( !second_host 	&& secure  
	 &&(rc = ldapssl_client_init( ssl_certdbpath, NULL )) < 0) {
	    /* secure connection requested -- fail if no SSL */
	    rc = PORT_GetError();
	    fprintf( stderr, "SSL initialization failed: error %d (%s)\n",
		    rc, ldapssl_err2string( rc ));
	    exit( LDAP_LOCAL_ERROR );
    }
#endif /* LDAP_TOOL_PKCS11 */

    /* set the default SSL strength (used for all future ld's we create) */
    if ( ldapssl_set_strength( NULL, ssl_strength ) < 0 ) {
	perror( "ldapssl_set_strength" );
	exit( LDAP_LOCAL_ERROR );
    }

    if (secure) {
	if ( !user_port ) {
	    port = LDAPS_PORT;
	}
	
	if (( ld = ldapssl_init( host, port,
		secure )) != NULL && ssl_certname != NULL )
	    if (ldapssl_enable_clientauth( ld, ssl_keydbpath, ssl_passwd,
		ssl_certname ) != 0 ) {
		exit ( ldaptool_print_lderror( ld, "ldapssl_enable_clientauth",
		    LDAPTOOL_CHECK4SSL_ALWAYS ));
	    }
    } else {
	/* In order to support IPv6, we use NSPR I/O */
	ld = prldap_init( host, port, 0 /* not shared across threads */ );
    }

#else
    /* In order to support IPv6, we use NSPR I/O */
    ld = prldap_init( host, port, 0 /* not shared across threads */ );
#endif

    if ( ld == NULL ) {
	perror( "ldap_init" );
	exit( LDAP_LOCAL_ERROR );
    }

#ifndef NO_LIBLCACHE
    if ( cache_config_file != NULL ) {
	int	opt;

	if ( lcache_init( ld, cache_config_file ) != 0 ) {
		exit( ldaptool_print_lderror( ld, cache_config_file,
			LDAPTOOL_CHECK4SSL_NEVER ));
	}
	opt = 1;
	(void) ldap_set_option( ld, LDAP_OPT_CACHE_ENABLE, &opt );
	opt = LDAP_CACHE_LOCALDB;
	(void) ldap_set_option( ld, LDAP_OPT_CACHE_STRATEGY, &opt );
	if ( ldversion == -1 ) {	/* not set with -V */
	    ldversion = LDAP_VERSION2;	/* local db only supports v2 */
	}
    }
#endif


    ldap_set_option( ld, LDAP_OPT_REFERRALS, chase_referrals ? LDAP_OPT_ON:
	LDAP_OPT_OFF );
    if ( chase_referrals ) {
	ldap_set_rebind_proc( ld, get_rebind_credentials, NULL );
	ldap_set_option( ld, LDAP_OPT_REFERRAL_HOP_LIMIT, &refhoplim );
    }

    if ( ldversion == -1 ) {	/* not set with -V and not using local db */
	ldversion = LDAP_VERSION3;
    }
    ldap_set_option( ld, LDAP_OPT_PROTOCOL_VERSION, &ldversion );

    return( ld );
}


/*
 * perform a bind to the LDAP server if needed.
 * if an error occurs, we exit here.
 */
void
ldaptool_bind( LDAP *ld )
{
    int		rc, ctrl_index = 0;
    char	*conv;
    LDAPControl	auth_resp_ctrl, *ctrl_array[ 3 ], **bindctrls;
    LDAPControl pwpolicy_req_ctrl;

    if ( ldaptool_not ) {
	return;
    }

    if ( send_auth_response_ctrl ) {
	auth_resp_ctrl.ldctl_oid = LDAP_CONTROL_AUTH_REQUEST;
	auth_resp_ctrl.ldctl_value.bv_val = NULL;
	auth_resp_ctrl.ldctl_value.bv_len = 0;
	auth_resp_ctrl.ldctl_iscritical = 0;
	ctrl_array[ctrl_index++] = &auth_resp_ctrl;
    }

    if ( !no_pwpolicy_req_ctrl ) {
	pwpolicy_req_ctrl.ldctl_oid = LDAP_X_CONTROL_PWPOLICY_REQUEST;
	pwpolicy_req_ctrl.ldctl_value.bv_val = NULL;
	pwpolicy_req_ctrl.ldctl_value.bv_len = 0;
	pwpolicy_req_ctrl.ldctl_iscritical = 0;
	ctrl_array[ctrl_index++] = &pwpolicy_req_ctrl;
    }

    if ( ctrl_index == 0 ) {
	bindctrls = NULL;
    } else {
	ctrl_array[ctrl_index] = NULL;
	bindctrls = ctrl_array;
    }

    /*
     * if using LDAPv3 and not using client auth., omit NULL bind for
     * efficiency.
     */
    if ( ldversion > LDAP_VERSION2 && binddn == NULL && passwd == NULL
	    && ssl_certname == NULL ) {
	return;
    }

    /*
     * do the bind, backing off one LDAP version if necessary
     */
    conv = ldaptool_local2UTF8( binddn, "bind DN" );

    /*
     * if using LDAPv3 and client auth., try a SASL EXTERNAL bind
     */
    if ( ldversion > LDAP_VERSION2 && binddn == NULL && passwd == NULL
	    && ssl_certname != NULL ) {
	rc = ldaptool_sasl_bind_s( ld, NULL, LDAP_SASL_EXTERNAL, NULL,
		bindctrls, NULL, NULL, "ldap_sasl_bind" );
    } else {
	rc = ldaptool_simple_bind_s( ld, conv, passwd, bindctrls, NULL,
		    "ldap_simple_bind" );
    }

    if ( rc == LDAP_SUCCESS ) {
        if ( conv != NULL ) {
           free( conv );
	}
	return;			/* success */
    }

    if ( rc == LDAP_PROTOCOL_ERROR && ldversion > LDAP_VERSION2 ) {
	/*
	 * try again, backing off one LDAP version
	 * this is okay even for client auth. because the way to achieve
	 * client auth. with LDAPv2 is to perform a NULL simple bind.
	 */
	--ldversion;
	fprintf( stderr, "%s: the server doesn't understand LDAPv%d;"
		" trying LDAPv%d instead...\n", ldaptool_progname,
		ldversion + 1, ldversion );
	ldap_set_option( ld, LDAP_OPT_PROTOCOL_VERSION, &ldversion );
	if (( rc = ldaptool_simple_bind_s( ld, conv, passwd,
		bindctrls, NULL, "ldap_simple_bind" )) == LDAP_SUCCESS ) {
            if( conv != NULL )
                free( conv );
	    return;		/* a qualified success */
	}
    }

    if ( conv != NULL ) {
        free( conv );
    }

    /*
     * bind(s) failed -- fatal error
     */
    ldap_unbind( ld );
    exit( rc );
}


/*
 * close open files, unbind, etc.
 */
void
ldaptool_cleanup( LDAP *ld )
{
    if ( ld != NULL ) {
	ldap_unbind( ld );
    }

    if ( ldaptool_fp != NULL && ldaptool_fp != stdin ) {
	fclose( ldaptool_fp );
	ldaptool_fp = NULL;
    }
}


/*
 * Retrieve and print an LDAP error message.  Returns the LDAP error code.
 */
int
ldaptool_print_lderror( LDAP *ld, char *msg, int check4ssl )
{
    int		lderr = ldap_get_lderrno( ld, NULL, NULL );

    ldap_perror( ld, msg );
    if ( secure && check4ssl != LDAPTOOL_CHECK4SSL_NEVER ) {
	if ( check4ssl == LDAPTOOL_CHECK4SSL_ALWAYS
		|| ( lderr == LDAP_SERVER_DOWN )
		|| ( lderr == LDAP_CONNECT_ERROR )) {
	    int		sslerr = PORT_GetError();

	    fprintf( stderr, "\tSSL error %d (%s)\n", sslerr,
		    ldapssl_err2string( sslerr ));
	}
    }

    return( lderr );
}


/*
 * print referrals to stderr
 */
void
ldaptool_print_referrals( char **refs )
{
    int		i;

    if ( refs != NULL ) {
	for ( i = 0; refs[ i ] != NULL; ++i ) {
	    fprintf( stderr, "Referral: %s\n", refs[ i ] );
	}
    }
}


/*
 * print contents of an extended response to stderr
 * this is mainly to support unsolicited notifications
 * Returns an LDAP error code (from the extended result).
 */
int
ldaptool_print_extended_response( LDAP *ld, LDAPMessage *res, char *msg )
{
    char		*oid;
    struct berval	*data;

    if ( ldap_parse_extended_result( ld, res, &oid, &data, 0 )
	    != LDAP_SUCCESS ) {
	(void)ldaptool_print_lderror( ld, msg, LDAPTOOL_CHECK4SSL_IF_APPROP );
    } else {
	if ( oid != NULL ) {
	    if ( strcmp ( oid, LDAP_NOTICE_OF_DISCONNECTION ) == 0 ) {
		fprintf( stderr, "%s: Notice of Disconnection\n", msg );
	    } else {
		fprintf( stderr, "%s: OID %s\n", msg, oid );
	    }
	    ldap_memfree( oid );
	} else {
	    fprintf( stderr, "%s: missing OID\n", msg );
	}

	if ( data != NULL ) {
	    fprintf( stderr, "%s: Data (length %ld):\n", msg, data->bv_len );
#if 0
/* XXXmcs: maybe we should display the actual data? */
	    lber_bprint( data->bv_val, data->bv_len );
#endif
	    ber_bvfree( data );
	}
    }

    return parse_result( ld, res, NULL, msg, 1 );
}


/*
 * Like ldap_sasl_bind_s() but calls wait4result() to display
 * any referrals returned and report errors in a consistent way.
 */
int
ldaptool_sasl_bind_s( LDAP *ld, const char *dn, const char *mechanism,
	const struct berval *cred, LDAPControl **serverctrls,
	LDAPControl **clientctrls, struct berval **servercredp, char *msg )
{
    int		rc, msgid;

    if ( servercredp != NULL ) {
	    *servercredp = NULL;
    }

    if (( rc = ldap_sasl_bind( ld, dn, mechanism, cred, serverctrls,
	    clientctrls, &msgid )) != LDAP_SUCCESS ) {
	(void)ldaptool_print_lderror( ld, msg, LDAPTOOL_CHECK4SSL_IF_APPROP );
    } else {
	rc = wait4result( ld, msgid, servercredp, msg );
    }

    return( rc );
}


/*
 * Like ldap_simple_bind_s() but calls wait4result() to display
 * any referrals returned and report errors in a consistent way.
 */
int
ldaptool_simple_bind_s( LDAP *ld, const char *dn, const char *passwd,
	LDAPControl **serverctrls, LDAPControl **clientctrls, char *msg )
{
    struct berval	bv;

    bv.bv_val = (char *)passwd;		/* XXXmcs: had to cast away const */
    bv.bv_len = ( passwd == NULL ? 0 : strlen( passwd ));
    return( ldaptool_sasl_bind_s( ld, dn, LDAP_SASL_SIMPLE, &bv, serverctrls,
	    clientctrls, NULL, msg ));
}


/*
 * Like ldap_add_ext_s() but calls wait4result() to display
 * any referrals returned and report errors in a consistent way.
 */
int
ldaptool_add_ext_s( LDAP *ld, const char *dn, LDAPMod **attrs,
	LDAPControl **serverctrls, LDAPControl **clientctrls, char *msg )
{
    int		rc, msgid;

    if (( rc = ldap_add_ext( ld, dn, attrs, serverctrls, clientctrls, &msgid ))
	    != LDAP_SUCCESS ) {
	(void)ldaptool_print_lderror( ld, msg, LDAPTOOL_CHECK4SSL_IF_APPROP );
    } else {
	/*
	 * 25-April-2000 Note: the next line used to read:
	 *	rc = wait4result( ld, msgid, NULL, msg );
	 * 'msgid' it was changed to 'LDAP_RES_ANY' in order to receive
	 * unsolicited notifications.
	 */
	rc = wait4result( ld, LDAP_RES_ANY, NULL, msg );
    }

    return( rc );
}


/*
 * Like ldap_modify_ext_s() but calls wait4result() to display
 * any referrals returned and report errors in a consistent way.
 */
int
ldaptool_modify_ext_s( LDAP *ld, const char *dn, LDAPMod **mods,
	LDAPControl **serverctrls, LDAPControl **clientctrls, char *msg )
{
    int		rc, msgid;

    if (( rc = ldap_modify_ext( ld, dn, mods, serverctrls, clientctrls,
	    &msgid )) != LDAP_SUCCESS ) {
	(void)ldaptool_print_lderror( ld, msg, LDAPTOOL_CHECK4SSL_IF_APPROP );
    } else {
	rc = wait4result( ld, msgid, NULL, msg );
    }

    return( rc );
}


/*
 * Like ldap_delete_ext_s() but calls wait4result() to display
 * any referrals returned and report errors in a consistent way.
 */
int
ldaptool_delete_ext_s( LDAP *ld, const char *dn, LDAPControl **serverctrls,
	LDAPControl **clientctrls, char *msg )
{
    int		rc, msgid;

    if (( rc = ldap_delete_ext( ld, dn, serverctrls, clientctrls, &msgid ))
	    != LDAP_SUCCESS ) {
	(void)ldaptool_print_lderror( ld, msg, LDAPTOOL_CHECK4SSL_IF_APPROP );
    } else {
	rc = wait4result( ld, msgid, NULL, msg );
    }

    return( rc );
}


/*
 * Like ldap_compare_ext_s() but calls wait4result() to display
 * any referrals returned and report errors in a consistent way.
 */
int ldaptool_compare_ext_s( LDAP *ld, const char *dn, const char *attrtype,
	    const struct berval *bvalue, LDAPControl **serverctrls,
	    LDAPControl **clientctrls, char *msg )
{
    int		rc, msgid;

    if (( rc = ldap_compare_ext( ld, dn, attrtype, bvalue, serverctrls,
	    clientctrls, &msgid )) != LDAP_SUCCESS ) {
	(void)ldaptool_print_lderror( ld, msg, LDAPTOOL_CHECK4SSL_IF_APPROP );
    } else {
	rc = wait4result( ld, msgid, NULL, msg );
    }

    return( rc );
}


/*
 * Like ldap_rename_s() but calls wait4result() to display
 * any referrals returned and report errors in a consistent way.
 */
int
ldaptool_rename_s(  LDAP *ld, const char *dn, const char *newrdn,
	const char *newparent, int deleteoldrdn, LDAPControl **serverctrls,
	LDAPControl **clientctrls, char *msg )
{
    int		rc, msgid;

    if (( rc = ldap_rename( ld, dn, newrdn, newparent, deleteoldrdn,
	    serverctrls, clientctrls, &msgid )) != LDAP_SUCCESS ) {
	(void)ldaptool_print_lderror( ld, msg, LDAPTOOL_CHECK4SSL_IF_APPROP );
    } else {
	rc = wait4result( ld, msgid, NULL, msg );
    }

    return( rc );
}


/*
 * Wait for a result, check for and display errors and referrals.
 * Also recognize and display "Unsolicited notification" messages.
 * Returns an LDAP error code.
 */
static int
wait4result( LDAP *ld, int msgid, struct berval **servercredp, char *msg )
{
    LDAPMessage	*res;
    int		rc, received_only_unsolicited = 1;

    while ( received_only_unsolicited ) {
	res = NULL;
	if (( rc = ldap_result( ld, msgid, 1, (struct timeval *)NULL, &res ))
		    == -1 ) {
	    (void)ldaptool_print_lderror( ld, msg, LDAPTOOL_CHECK4SSL_IF_APPROP );
	    return( ldap_get_lderrno( ld, NULL, NULL ));
	}

	/*
	 * Special handling for unsolicited notifications:
	 *    1. Parse and display contents.
	 *    2. go back and wait for another (real) result.
	 */
	if ( rc == LDAP_RES_EXTENDED
		    && ldap_msgid( res ) == LDAP_RES_UNSOLICITED ) {
	    rc = ldaptool_print_extended_response( ld, res,
		    "Unsolicited response" );
	} else {
	    rc = parse_result( ld, res, servercredp, msg, 1 );
	    received_only_unsolicited = 0;	/* we're done */
	}
    }

    return( rc );
}


static int
parse_result( LDAP *ld, LDAPMessage *res, struct berval **servercredp,
	char *msg, int freeit )
{
    int		rc, lderr, errno;
    int		pw_days=0, pw_hrs=0, pw_mins=0, pw_secs=0; /* for pwpolicy */
    char	**refs = NULL;
    LDAPControl	**ctrls;
    BerElement *ber = NULL;
    static const char *pwpolicy_err2str[] = {
	"Password has expired.",
	"Account is locked.",
	"Password has been reset by an administrator; you must change it.",
	"Password change not allowed.",
	"Must supply old password.",
	"Invalid password syntax.",
	"Password too short.",
	"Password too young.",
	"Password in history."
    };

    if (( rc = ldap_parse_result( ld, res, &lderr, NULL, NULL, &refs,
	    &ctrls, 0 )) != LDAP_SUCCESS ) {
	(void)ldaptool_print_lderror( ld, msg, LDAPTOOL_CHECK4SSL_IF_APPROP );
	ldap_msgfree( res );
	return( rc );
    }

    /* check for authentication response control & PWPOLICY control*/
    if ( NULL != ctrls ) {
	int		i;
	char		*s;

	for ( i = 0; NULL != ctrls[i]; ++i ) {
	    if ( 0 == strcmp( ctrls[i]->ldctl_oid,
			LDAP_CONTROL_AUTH_RESPONSE )) {
		    s = ctrls[i]->ldctl_value.bv_val;
		    if ( NULL == s ) {
			s = "Null";
		    } else if ( *s == '\0' ) {
			s = "Anonymous";
		    }
		fprintf( stderr, "%s: bound as %s\n", ldaptool_progname, s );
	    }

	    if ( 0 == strcmp( ctrls[i]->ldctl_oid,
			LDAP_CONTROL_PWEXPIRING )) {

		    /* Warn the user his passwd is to expire */
		    errno = 0;	
		    pw_secs = atoi(ctrls[i]->ldctl_value.bv_val);
		    if ( pw_secs > 0  && errno != ERANGE ) {
			if ( pw_secs > 86400 ) {
				pw_days = ( pw_secs / 86400 );
				pw_secs = ( pw_secs % 86400 );
			} 
			if ( pw_secs > 3600 ) {
				pw_hrs = ( pw_secs / 3600 );
				pw_secs = ( pw_secs % 3600 );
			}
			if ( pw_secs > 60 ) {
				pw_mins = ( pw_secs / 60 );
				pw_secs = ( pw_secs % 60 );
			}

			printf("%s: Warning ! Your password will expire after ", ldaptool_progname);
			if ( pw_days ) {
				printf ("%d days, ", pw_days);
			}
			if ( pw_hrs ) {
				printf ("%d hrs, ", pw_hrs);
			}
			if ( pw_mins ) {
				printf ("%d mins, ", pw_mins);
			}
			printf("%d seconds.\n", pw_secs);
			
		   }
		}
	    if ( 0 == strcmp( ctrls[i]->ldctl_oid,
		LDAP_X_CONTROL_PWPOLICY_RESPONSE )) {
		unsigned long tag1=0, tag2=0, tag3=0;
		int warnvalue=0, grclogins=-1, secsleft=-1, errvalue=-1;
		static int err2str_size = sizeof(pwpolicy_err2str)/sizeof(pwpolicy_err2str[0]);

		if ( ( ber = ber_init(&(ctrls[i]->ldctl_value)) ) == NULL ) {
			fprintf(stderr, "%s: not enough memory\n", ldaptool_progname);
			return( LDAP_NO_MEMORY );
		}
		if ( ber_scanf(ber,"{t", &tag1) == LBER_ERROR ) {
			/* error */
			ber_free( ber, 1 );
			return (ldaptool_print_lderror( ld, msg, LDAPTOOL_CHECK4SSL_IF_APPROP ));
		} 
		switch (tag1) {
		case LDAP_TAG_PWP_WARNING:
			if ( ber_scanf(ber, "{ti}", &tag2, &warnvalue)
					== LBER_ERROR ) {
				/* error */
				ber_free( ber, 1 );
				return(ldaptool_print_lderror( ld, msg, LDAPTOOL_CHECK4SSL_IF_APPROP ));
			}
			switch (tag2) {
			case LDAP_TAG_PWP_SECSLEFT:
				secsleft = warnvalue;
				break;
			case LDAP_TAG_PWP_GRCLOGINS:
				grclogins = warnvalue;
				break;
			default:
				/* error */
				ber_free( ber, 1 );
				return(ldaptool_print_lderror( ld, msg, LDAPTOOL_CHECK4SSL_IF_APPROP ));
			}
			/* Now check for the error value if it's present */
			if ( ber_scanf(ber, "te", &tag3, &errvalue) != LBER_ERROR ) {
				if (tag3 != LDAP_TAG_PWP_ERROR) {
					errvalue = -1;
				}
			}
			break;
		case LDAP_TAG_PWP_ERROR:
			if ( ber_scanf(ber, "e}", &errvalue) 
					== LBER_ERROR ) {
				/* error */
				ber_free( ber, 1 );
				return(ldaptool_print_lderror( ld, msg, LDAPTOOL_CHECK4SSL_IF_APPROP ));
			}
			break;
		default : /* error */
			ber_free( ber, 1 );
			return(ldaptool_print_lderror( ld, msg, LDAPTOOL_CHECK4SSL_IF_APPROP ));
		}
		
		/* Now we have all the values */
		if ( secsleft >= 0 ) {
			fprintf(stderr, "%s: Password will expire in %d seconds\n",
				ldaptool_progname, secsleft);
		}
		if ( grclogins >= 0 ) {
			fprintf(stderr, "%s: %d grace login(s) remain\n",
				ldaptool_progname, grclogins);
		}
		if ( errvalue >= 0 && errvalue < err2str_size ) {
			fprintf(stderr, "%s: %s\n",
				ldaptool_progname, pwpolicy_err2str[errvalue]);
		} else if ( errvalue != -1 ) {
			fprintf(stderr, "%s: %s\n",
				ldaptool_progname,
				"Invalid error value in password policy response control");
		}
	    } /* end of LDAP_X_CONTROL_PWPOLICY_RESPONSE */
	}
	ldap_controls_free( ctrls );
    }

    if ( servercredp != NULL && ( rc = ldap_parse_sasl_bind_result( ld, res,
	    servercredp, 0 )) != LDAP_SUCCESS ) {
	(void)ldaptool_print_lderror( ld, msg, LDAPTOOL_CHECK4SSL_IF_APPROP );
	ldap_msgfree( res );
	ber_free( ber, 1 );
	return( rc );
    }

    if ( freeit ) {
	ldap_msgfree( res );
    }

    if ( LDAPTOOL_RESULT_IS_AN_ERROR( lderr )) {
	(void)ldaptool_print_lderror( ld, msg, LDAPTOOL_CHECK4SSL_IF_APPROP );
    }

    if ( refs != NULL ) {
	ldaptool_print_referrals( refs );
	ldap_value_free( refs );
    }

    ber_free( ber, 1 );
    return( lderr );
}


/*
 * if -M was passed on the command line, create and return a "Manage DSA IT"
 * LDAPv3 control.  If not, return NULL.
 */
LDAPControl *
ldaptool_create_manage_dsait_control( void )
{
    LDAPControl	*ctl;

    if ( !send_manage_dsait_ctrl ) {
	return( NULL );
    }

    if (( ctl = (LDAPControl *)calloc( 1, sizeof( LDAPControl ))) == NULL ||
	    ( ctl->ldctl_oid = strdup( LDAP_CONTROL_MANAGEDSAIT )) == NULL ) {
	perror( "calloc" );
	exit( LDAP_NO_MEMORY );
    }

    ctl->ldctl_iscritical = 1;

    return( ctl );
}

/*
 * if -y "dn" was supplied on the command line, create the control
 */
LDAPControl *
ldaptool_create_proxyauth_control( LDAP *ld )
{
    LDAPControl	*ctl = NULL;
    int rc;
    

    if ( !proxyauth_id)
	return( NULL );

    if ( 2 == proxyauth_version ) {
	rc = ldap_create_proxiedauth_control( ld, proxyauth_id, &ctl);
    } else {
	rc = ldap_create_proxyauth_control( ld, proxyauth_id, 1, &ctl);
    }
    if ( rc != LDAP_SUCCESS) 
    {
	if (ctl)
	    ldap_control_free( ctl);
	return NULL;
    }
    return( ctl );
}

void
ldaptool_add_control_to_array( LDAPControl *ctrl, LDAPControl **array)
{
    
    int i;
    for (i=0; i< CONTROL_REQUESTS; i++)
    {
	if (*(array + i) == NULL)
	{
	    *(array + i +1) = NULL;
	    *(array + i) = ctrl;
	    return ;
	}
    }
    fprintf(stderr, "%s: failed to store request control!!!!!!\n",
	    ldaptool_progname);
}

/*
 * Dispose of all controls in array and prepare array for reuse.
 */
void
ldaptool_reset_control_array( LDAPControl **array )
{
    int		i;

    for ( i = 0; i < CONTROL_REQUESTS; i++ ) {
	if ( array[i] != NULL ) {
	    ldap_control_free( array[i] );
	    array[i] = NULL;
	}
    }
}

/*
 * This function calculates control value and its length. *value can
 * be pointing to plain value, ":b64encoded value" or "<fileurl".
 */
static int
calculate_ctrl_value( const char *value,
	char **ctrl_value, int *vlen)
{
    int b64;
    if (*value == ':') {
	value++;
	b64 = 1;
    } else {
	b64 = 0;
    }
    *ctrl_value = (char *)value;

    if ( b64 ) {
	if (( *vlen = ldif_base64_decode( (char *)value,
		(unsigned char *)value )) < 0 ) {
	    fprintf( stderr, 
		"Unable to decode base64 control value \"%s\"\n", value);
	    return( -1 );
	}
    } else {
	*vlen = (int)strlen(*ctrl_value);
    }
    return( 0 );
}

/*
 * Parse the optarg from -J option of ldapsearch
 * and within LDIFfile for ldapmodify. Take ctrl_arg 
 * (the whole string) and divide it into oid, criticality
 * and value. This function breaks down original ctrl_arg
 * with '\0' in places. Also, calculate length of valuestring.
 */
int
ldaptool_parse_ctrl_arg(char *ctrl_arg, char sep,
		char **ctrl_oid, int *ctrl_criticality,
		char **ctrl_value, int *vlen)
{
    char *s, *p;
    int strict;

    /* Initialize passed variables with default values */
    *ctrl_oid = *ctrl_value = NULL;
    *ctrl_criticality = 0;
    *vlen = 0;

    strict = (sep == ' ' ? 1 : 0);
    if(!(s=strchr(ctrl_arg, sep))) {
	/* Possible values of ctrl_arg are 
	 * oid[:value|::b64value|:<fileurl] within LDIF, i.e. sep=' '
	 * oid from command line option, i.e. sep=':'
	 */
	if (sep == ' ') {
	    if (!(s=strchr(ctrl_arg, ':'))) {
		*ctrl_oid = ctrl_arg;
	    }
	    else {
		/* ctrl_arg is of oid:[value|:b64value|<fileurl]
		 * form in the LDIF record. So, grab the oid and then
		 * jump to continue the parsing of ctrl_arg.
		 * 's' is pointing just after oid ends.
		 */
		*s++ = '\0';
		*ctrl_oid = ctrl_arg;
		return (calculate_ctrl_value( s, ctrl_value, vlen ));
	    }
	} else {
		/* oid - from command line option, i.e. sep=':' */
		*ctrl_oid = ctrl_arg;
	}
    }
    else {
	/* Possible values of ctrl_arg are
	 * oid:criticality[:value|::b64value|:<fileurl] - command line
	 * oid criticality[:value|::b64value|:<fileurl] - LDIF
	 * And 's' is pointing just after oid ends.
	 */

	if (*(s+1) == '\0') {
	    fprintf( stderr, "missing value\n" );
	    return( -1 );
	}
	*s = '\0';
	*ctrl_oid = ctrl_arg;
	p = ++s;
	if(!(s=strchr(p, ':'))) {
	    if ( (*ctrl_criticality = ldaptool_boolean_str2value(p, strict))
			== -1 ) {
		fprintf( stderr, "Invalid criticality value\n" );
		return( -1 );
	    }
	}
	else {
	    if (*(s+1) == '\0') { 
	        fprintf( stderr, "missing value\n" );
	        return ( -1 );
	    }
	    *s++ = '\0';
            if ( (*ctrl_criticality = ldaptool_boolean_str2value(p, strict))
			== -1 ) {
		fprintf( stderr, "Invalid criticality value\n" );
		return ( -1 );
	    }
	    return (calculate_ctrl_value( s, ctrl_value, vlen ));
	}
    }

    return( 0 );
}


/*
 * callback function for LDAP bind credentials
 */
/*ARGSUSED*/
static int
LDAP_CALL
LDAP_CALLBACK
get_rebind_credentials( LDAP *ld, char **whop, char **credp,
        int *methodp, int freeit, void* arg )
{
    if ( !freeit ) {
	*whop = binddn;
	*credp = passwd;
	*methodp = LDAP_AUTH_SIMPLE;
    }

    return( LDAP_SUCCESS );
}


/*
 * return pointer to pathname to temporary directory.
 * First we see if the environment variable "TEMP" is set and use it.
 * Then we see if the environment variable "TMP" is set and use it.
 * If this fails, we use "/tmp" on UNIX and fail on Windows.
 */
char *
ldaptool_get_tmp_dir( void )
{
    char	*p;
    int		offset;

    if (( p = getenv( "TEMP" )) == NULL && ( p = getenv( "TMP" )) == NULL ) {
#ifdef _WINDOWS
	fprintf( stderr, "%s: please set the TEMP environment variable.\n",
		ldaptool_progname );
	exit( LDAP_LOCAL_ERROR );
#else
	return( "/tmp" );	/* last resort on UNIX */
#endif
    }

    /*
     * remove trailing slash if present
     */
    offset = strlen( p ) - 1;
    if ( p[offset] == '/'
#ifdef _WINDOWS
	    || p[offset] == '\\'
#endif
	    ) {
	if (( p = strdup( p )) == NULL ) {
	    perror( "strdup" );
	    exit( LDAP_NO_MEMORY );
	}

	p[offset] = '\0';
    }

    return( p );
}


int
ldaptool_berval_is_ascii( const struct berval *bvp )
{
    unsigned long	j;
    int			is_ascii = 1;	 /* optimistic */

    for ( j = 0; j < bvp->bv_len; ++j ) {
	if ( !isascii( bvp->bv_val[ j ] )) {
	    is_ascii = 0;
	    break;
	}
    }

    return( is_ascii );
}


#ifdef LDAP_DEBUG_MEMORY   
#define LDAPTOOL_ALLOC_FREED	0xF001
#define LDAPTOOL_ALLOC_INUSE	0xF002

static void *
ldaptool_debug_alloc( void *ptr, size_t size )
{
    int		*statusp;
    void	*systemptr;

    if ( ptr == NULL ) {
	systemptr = NULL;
    } else {
	systemptr = (void *)((char *)ptr - sizeof(int));
    }

    if (( statusp = (int *)realloc( systemptr, size + sizeof(int))) == NULL ) {
	fprintf( stderr, "%s: realloc( 0x%x, %d) failed\n",
		ldaptool_progname, systemptr, size );
	return( NULL );
    }

    *statusp = LDAPTOOL_ALLOC_INUSE;

    return( (char *)statusp + sizeof(int));
}


static void *
ldaptool_debug_realloc( void *ptr, size_t size )
{
    void	*p;

    if ( ldaptool_dbg_lvl & LDAP_DEBUG_TRACE ) {
	fprintf( stderr, "%s: => realloc( 0x%x, %d )\n",
		ldaptool_progname, ptr, size );
    }

    p = ldaptool_debug_alloc( ptr, size );

    if ( ldaptool_dbg_lvl & LDAP_DEBUG_TRACE ) {
	fprintf( stderr, "%s: 0x%x <= realloc()\n", ldaptool_progname, p );
    }

    return( p );
}


static void *
ldaptool_debug_malloc( size_t size )
{
    void	*p;

    if ( ldaptool_dbg_lvl & LDAP_DEBUG_TRACE ) {
	fprintf( stderr, "%s: => malloc( %d)\n", ldaptool_progname, size );
    }

    p = ldaptool_debug_alloc( NULL, size );

    if ( ldaptool_dbg_lvl & LDAP_DEBUG_TRACE ) {
	fprintf( stderr, "%s: 0x%x <= malloc()\n", ldaptool_progname, p );
    }

    return( p );
}


static void *
ldaptool_debug_calloc( size_t nelem, size_t elsize )
{
    void	*p;

    if ( ldaptool_dbg_lvl & LDAP_DEBUG_TRACE ) {
	fprintf( stderr, "%s: => calloc( %d, %d )\n",
		ldaptool_progname, nelem, elsize );
    }

    if (( p = ldaptool_debug_alloc( NULL, nelem * elsize )) != NULL ) {
	memset( p, 0, nelem * elsize );
    }

    if ( ldaptool_dbg_lvl & LDAP_DEBUG_TRACE ) {
	fprintf( stderr, "%s: 0x%x <= calloc()\n", ldaptool_progname, p );
    }

    return( p );
}


static void
ldaptool_debug_free( void *ptr )
{
    int		*statusp = (int *)((char *)ptr - sizeof(int));

    if ( ldaptool_dbg_lvl & LDAP_DEBUG_TRACE ) {
	fprintf( stderr, "%s: => free( 0x%x )\n", ldaptool_progname, ptr );
    }

    if ( ptr == NULL ) {
	fprintf( stderr, "%s: bad free( 0x0 ) attempted (NULL pointer)\n",
		ldaptool_progname );
    } else if ( *statusp != LDAPTOOL_ALLOC_INUSE ) {
	fprintf( stderr, "%s: bad free( 0x%x ) attempted"
		" (block not in use; status is %d)\n",
		ldaptool_progname, ptr, *statusp );
    } else {
	*statusp = LDAPTOOL_ALLOC_FREED;
	free( statusp );
    }
}
#endif /* LDAP_DEBUG_MEMORY */


#if defined(NET_SSL)
#ifdef LDAP_TOOL_PKCS11
static 
char * 
buildTokenCertName( const char *tokenName, const char *certName)
{
    
    int tokenlen = strlen(tokenName);
    int len = tokenlen + strlen(certName) +2;
    char *result;
    
    if (( result = malloc( len )) != NULL) {
	strcpy(result, tokenName);
	*(result+tokenlen) = ':';
	++tokenlen;
	strcpy(result+tokenlen, certName);
    } else {
	perror("malloc");
	exit( LDAP_NO_MEMORY );
    }
    return result;
}



/*ARGSUSED*/
static int
ldaptool_getcertpath( void *context, char **certlocp )
{
    
    *certlocp = ssl_certdbpath;
    if ( ldaptool_verbose ) {
	if (ssl_certdbpath)
	{
	    printf("ldaptool_getcertpath -- %s\n", ssl_certdbpath );
	}
	else
	{
	    printf("ldaptool_getcertpath -- (null)\n");
	}
	
    }
    return LDAP_SUCCESS;
}

/*ARGSUSED*/
int
ldaptool_getcertname( void *context, char **certnamep )
{ 
    
   *certnamep = ssl_certname;
    if ( ldaptool_verbose ) {
	if (ssl_certname)
	{
	    printf("ldaptool_getcertname -- %s\n", *certnamep);
	}
	else
	{
	    printf("ldaptool_getcertname -- (null)\n");
	}
    }
    return LDAP_SUCCESS;
}

/*ARGSUSED*/
int
ldaptool_getkeypath(void *context, char **keylocp )
{
    *keylocp = ssl_keydbpath;
    if ( ldaptool_verbose ) {
	if (ssl_keydbpath)
	{
	    printf("ldaptool_getkeypath -- %s\n",*keylocp);
	}
	else
	{
	    printf("ldaptool_getkeypath -- (null)\n");
	}
    }
    
    return LDAP_SUCCESS;
}

/*ARGSUSED*/
int
ldaptool_gettokenname( void *context, char **tokennamep )
{
    
    *tokennamep = pkcs_token;
    if ( ldaptool_verbose ) {
	if (pkcs_token)
	{
	    printf("ldaptool_gettokenname -- %s\n",*tokennamep);
	}
	else
	{
	    printf("ldaptool_gettokenname -- (null)\n");
	}
    }

    return LDAP_SUCCESS;
}

/*ARGSUSED*/
int
ldaptool_gettokenpin( void *context, const char *tokennamep, char **tokenpinp)
{
  
#if 0
  char *localtoken;
#endif

/* XXXceb this stuff is removed for the time being.  
 * This function should return the pin from ssl_password
 */


  *tokenpinp = ssl_passwd;
  return LDAP_SUCCESS;
  
#if 0

  ldaptool_gettokenname( NULL, &localtoken);

  if (strcmp( localtoken, tokennamep))

      *tokenpinp = pkcs_pin;
   else 
      *tokenpinp = NULL;

    if ( ldaptool_verbose ) {
	if (pkcs_pin)
	{
	    printf("ldaptool_getokenpin --%s\n", tokenpinp);
	}
	else
	{
	    printf("ldaptool_getokenpin -- (null)\n");
	}
    }
    return LDAP_SUCCESS;
#endif
}

/*ARGSUSED*/
int
ldaptool_getmodpath( void *context, char **modulep )
{
    *modulep = ssl_secmodpath;
    if ( ldaptool_verbose ) {
	if (ssl_secmodpath)
	{
	    printf("ldaptool_getmodpath -- %s\n", *modulep);
	}
	else
	{
	    printf("ldaptool_getmodpath -- (null)\n");
	}
    }
    
    return LDAP_SUCCESS;
}

/*ARGSUSED*/
int
ldaptool_getdonglefilename( void *context, char **filename )
{
    *filename = ssl_donglefile;
    if ( ldaptool_verbose ) {
	if (ssl_donglefile)
	{
	    printf("ldaptool_getdonglefilename -- %s\n", *filename);
	}
	else
	{
	    printf("ldaptool_getdonglefilename -- (null)\n");
	}
	
    }
    
    return LDAP_SUCCESS;
}

static void
ldaptool_setcallbacks( struct ldapssl_pkcs_fns *pfns)
{
  pfns->pkcs_getcertpath = (int (*)(void *, char **))ldaptool_getcertpath;
  pfns->pkcs_getcertname =  (int (*)(void *, char **))ldaptool_getcertname;
  pfns->pkcs_getkeypath =  (int (*)(void *, char **)) ldaptool_getkeypath;
  pfns->pkcs_getmodpath =  (int (*)(void *, char **)) ldaptool_getmodpath;
  pfns->pkcs_getpin =  (int (*)(void *, const char*, char **)) ldaptool_gettokenpin;
  pfns->pkcs_gettokenname =  (int (*)(void *, char **)) ldaptool_gettokenname;
  pfns->pkcs_getdonglefilename =  (int (*)(void *, char **)) ldaptool_getdonglefilename;
  pfns->local_structure_id=PKCS_STRUCTURE_ID;
}



#ifdef FORTEZZA
static int
ldaptool_fortezza_init( int exit_on_error )
{
    int		rc, errcode;

    if ( fortezza_personality == NULL && fortezza_cardmask == 0 ) { /* no FORTEZZA desired */
	SSL_EnableGroup( SSL_GroupFortezza, DSFalse );	/* disable FORTEZZA */
	return( 0 );
    }

    if (( rc = FortezzaConfigureServer( ldaptool_fortezza_getpin, fortezza_cardmask,
	    fortezza_personality, ldaptool_fortezza_alert, NULL, &errcode,
	    fortezza_krlfile )) < 0 ) {
	fprintf( stderr,
		"%s: FORTEZZA initialization failed (error %d - %s)\n",
		ldaptool_progname, errcode,
		ldaptool_fortezza_err2string( errcode ));
	if ( exit_on_error ) {
	    exit( LDAP_LOCAL_ERROR );
	}

	SSL_EnableGroup( SSL_GroupFortezza, DSFalse );	/* disable FORTEZZA */
	return( -1 );
    }

    SSL_EnableGroup( SSL_GroupFortezza, DSTrue );	/* enable FORTEZZA */
    return( 0 );
}


static int
ldaptool_fortezza_alert( void *arg, PRBool onOpen, char *string,
	int value1, void *value2 )
{
    fprintf( stderr, "%s: FORTEZZA alert: ", ldaptool_progname );
    fprintf( stderr, string, value1, value2 );
    fprintf( stderr, "\n" );
    return( 1 );
}


static void *
ldaptool_fortezza_getpin( char **passwordp )
{
    *passwordp = fortezza_pin;
    return( *passwordp );
}


/*
 * convert a Fortezza error code (as returned by FortezzaConfigureServer()
 * into a human-readable string.
 *
 * Error strings are intentionally similar to those found in
 * ns/netsite/lib/libadmin/httpcon.c
 */
static char *
ldaptool_fortezza_err2string( int err )
{
    char	*s;

    switch( err ) {
    case FORTEZZA_BADPASSWD:
	s = "invalid pin number";
	break;
    case FORTEZZA_BADCARD:
	s = "bad or missing card";
	break;
    case FORTEZZA_MISSING_KRL:
	s = "bad or missing compromised key list";
	break;
    case FORTEZZA_CERT_INIT_ERROR:
	s = "unable to initialize certificate cache.  either a cert on "
		"the card is bad, or an old FORTEZZA certificate is in a"
		 "readonly database";
	break;
    case FORTEZZA_EXPIRED_CERT:
	s = "unable to verify certificate";
	break;
    default:
	s = "unknown error";
    }

    return( s );
}

#endif /* FORTEZZA */
#endif /* LDAP_TOOL_PKCS11 */
#endif /* NET_SSL */

int
ldaptool_boolean_str2value ( const char *ptr, int strict )
{
    if (strict) {
	if ( !(strcasecmp(ptr, "true"))) {
	    return 1;
	}
	else if ( !(strcasecmp(ptr, "false"))) {
	    return 0;
	}
	else {
	    return (-1);
	}
    }
    else {
	if ( !(strcasecmp(ptr, "true")) ||
	     !(strcasecmp(ptr, "t")) ||
	     !(strcmp(ptr, "1")) ) {
		return (1);
	}
	else if ( !(strcasecmp(ptr, "false")) ||
	     !(strcasecmp(ptr, "f")) ||
	     !(strcmp(ptr, "0")) ) {
	    	return (0);
	}
	else { 
	    return (-1);
	}	
    }
} 

FILE *
ldaptool_open_file(const char *filename, const char *mode)
{
#ifdef _LARGEFILE64_SOURCE 
	return fopen64(filename, mode);
#else
	return fopen(filename, mode);
#endif
}
