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

/* ldapsearch.c - generic program to search LDAP */

#include "ldaptool.h"
#include "fileurl.h"

#define VLV_PARAM_SEP ':'

static void usage( void );
static int dosearch( LDAP *ld, char *base, int scope, char **attrs, 
			 int attrsonly, char *filtpatt, char *value);
static void write_string_attr_value( char *attrname, char *strval,
	unsigned long opts );
#define LDAPTOOL_WRITEVALOPT_SUPPRESS_NAME	0x01
static int write_ldif_value( char *type, char *value, unsigned long vallen,
	unsigned long ldifoptions );
static void print_entry( LDAP *ld, LDAPMessage *entry, int attrsonly );
static void options_callback( int option, char *optarg );
static void parse_and_display_reference( LDAP *ld, LDAPMessage *ref );
static char *sortresult2string(unsigned long result);
static char *changetype_num2string( int chgtype );
static char *msgtype2str( int msgtype );

/*
 * Prefix used in names of pseudo attributes added to the entry LDIF
 * output if we receive an entryChangeNotification control with an entry
 * (requested using persistent search).
 */
#define LDAPTOOL_PSEARCH_ATTR_PREFIX	"persistentSearch-"


static void
usage( void )
{
    fprintf( stderr, "usage: %s -b basedn [options] filter [attributes...]\n", ldaptool_progname );
    fprintf( stderr, "       %s -b basedn [options] -f file [attributes...]\nwhere:\n", ldaptool_progname );
    fprintf( stderr, "    basedn\tbase dn for search\n" );
    fprintf( stderr, "\t\t(if the environment variable LDAP_BASEDN is set,\n" );
    fprintf( stderr, "\t\tthen the -b flag is not required)\n" );
    fprintf( stderr, "    filter\tRFC-2254 compliant LDAP search filter\n" );
    fprintf( stderr, "    file\tfile containing a sequence of LDAP search filters to use\n" );
    fprintf( stderr, "    attributes\twhitespace-separated list of attributes to retrieve\n" );
    fprintf( stderr, "\t\t(if no attribute list is given, all are retrieved)\n" );
    fprintf( stderr, "options:\n" );
    ldaptool_common_usage( 0 );
#if defined( XP_WIN32 )
    fprintf( stderr, "    -t\t\twrite values to files in temp directory.\n" );
#else
    fprintf( stderr, "    -t\t\twrite values to files in /tmp\n" );
#endif
    fprintf( stderr, "    -U\t\tproduce file URLs in conjunction with -t\n" );
    fprintf( stderr, "    -e\t\tminimize base-64 encoding of values\n" );
    fprintf( stderr, "    -u\t\tinclude User Friendly entry names in the output\n" );
    fprintf( stderr, "    -o\t\tprint entries using old format (default is LDIF)\n" );
    fprintf( stderr, "    -T\t\tdon't fold (wrap) long lines (default is to fold)\n" );
    fprintf( stderr, "    -1\t\tomit leading \"version: %d\" line in LDIF output\n", LDIF_VERSION_ONE );
    fprintf( stderr, "    -A\t\tretrieve attribute names only (no values)\n" );
    fprintf( stderr, "    -B\t\tprint non-ASCII values when old format (-o) is used\n" );
    fprintf( stderr, "    -x\t\tperforming sorting on server\n" );
    fprintf( stderr, "    -F sep\tprint `sep' instead of `%s' between attribute names\n", LDAPTOOL_DEFSEP );
    fprintf( stderr, "          \tand values\n" );
    fprintf( stderr, "    -S attr\tsort the results by attribute `attr'\n" );
    fprintf( stderr, "    -s scope\tone of base, one, or sub (default is sub)\n" );
    fprintf( stderr, "    -a deref\tone of never, always, search, or find (default: never)\n" );
    fprintf( stderr, "            \t(alias dereferencing)\n" );
    fprintf( stderr, "    -l time lim\ttime limit (in seconds) for search\n" );
    fprintf( stderr, "    -z size lim\tsize limit (in entries) for search\n" );
    fprintf( stderr, "    -C ps:changetype[:changesonly[:entrychgcontrols]]\n" );
    fprintf( stderr, "\t\tchangetypes are add,delete,modify,moddn,any\n" );
    fprintf( stderr, "\t\tchangesonly and  entrychgcontrols are boolean values\n" );
    fprintf( stderr, "\t\t(default is 1)\n" );
    fprintf( stderr, "    -G before%cafter%cindex%ccount | before%cafter%cvalue where 'before' and\n", VLV_PARAM_SEP, VLV_PARAM_SEP, VLV_PARAM_SEP, VLV_PARAM_SEP, VLV_PARAM_SEP );
    fprintf( stderr, "\t\t'after' are the number of entries surrounding 'index.'\n");
    fprintf( stderr, "\t\t'count' is the content count, 'value' is the search value.\n");

    exit( LDAP_PARAM_ERROR );
}

static char	*base = NULL;
static char	*sep = LDAPTOOL_DEFSEP;
static char	**sortattr = NULL;
static char     *vlv_value = NULL;
static int	sortsize = 0;
static int	*skipsortattr = NULL;
static int	includeufn, allow_binary, vals2tmp, ldif, scope, deref;
static int	attrsonly, timelimit, sizelimit, server_sort, fold;
static int	minimize_base64, produce_file_urls;
static int	use_vlv = 0, vlv_before, vlv_after, vlv_index, vlv_count;
static int	use_psearch=0;
static int	write_ldif_version = 1;

/* Persistent search variables */
static int	chgtype=0, changesonly=1, return_echg_ctls=1;


int
main( int argc, char **argv )
{
    char		*filtpattern, **attrs;
    int			rc, optind, i, first;
    LDAP		*ld;

    deref = LDAP_DEREF_NEVER;
    allow_binary = vals2tmp = attrsonly = 0;
    minimize_base64 = produce_file_urls = 0;
    ldif = 1;
    fold = 1;
    sizelimit = timelimit = 0;
    scope = LDAP_SCOPE_SUBTREE;
	server_sort = 0;

#ifdef notdef
#ifdef HPUX11
#ifndef __LP64__
	_main( argc, argv);
#endif /* __LP64_ */
#endif /* HPUX11 */
#endif


    ldaptool_reset_control_array( ldaptool_request_ctrls );
    optind = ldaptool_process_args( argc, argv, "ABLTU1eotuxa:b:F:G:l:S:s:z:C:",
        0, options_callback );

    if ( optind == -1 ) {
	usage();
    }

    if ( base == NULL ) {
	if (( base = getenv( "LDAP_BASEDN" )) == NULL ) {
	    usage();
	}
    }    
    if ( sortattr ) {
	for ( sortsize = 0; sortattr[sortsize] != NULL; sortsize++ ) {
	    ;       /* NULL */
	}
	sortsize++;             /* add in the final NULL field */
	skipsortattr = (int *) malloc( sortsize * sizeof(int *) );
	if ( skipsortattr == NULL ) {
    		fprintf( stderr, "Out of memory\n" );
		exit( LDAP_NO_MEMORY );
	}
	memset( (char *) skipsortattr, 0, sortsize * sizeof(int *) );
    } else if ( server_sort ) {
	server_sort = 0;   /* ignore this option if no sortattrs were given */
    }

    if ( argc - optind < 1 ) {
	if ( ldaptool_fp == NULL ) {
	    usage();
	}
	attrs = NULL;
	filtpattern = "%s";
    } else {	/* there are additional args (filter + attrs) */
	if ( ldaptool_fp == NULL || strstr( argv[ optind ], "%s" ) != NULL ) {
	    filtpattern = ldaptool_local2UTF8( argv[ optind ] );
	    ++optind;
	} else {
	    filtpattern = "%s";
	}

	if ( argv[ optind ] == NULL ) {
	    attrs = NULL;
	} else if ( sortattr == NULL || *sortattr == '\0' || server_sort) {
	    attrs = &argv[ optind ];
	} else {
	    attrs = ldap_charray_dup( &argv[ optind ] );
	    if ( attrs == NULL ) {
		fprintf( stderr, "Out of memory\n" );
		exit( LDAP_NO_MEMORY );
	    }
	    for ( i = 0; i < (sortsize - 1); i++ ) {
                if ( !ldap_charray_inlist( attrs, sortattr[i] ) ) {
                    if ( ldap_charray_add( &attrs, sortattr[i] ) != 0 ) {
    			fprintf( stderr, "Out of memory\n" );
			exit( LDAP_NO_MEMORY );
		    }
                    /*
                     * attribute in the search list only for the purpose of
                     * sorting
                     */
                    skipsortattr[i] = 1;
                }
	    }
        }
    }

    ld = ldaptool_ldap_init( 0 );

    if ( !ldaptool_not ) {
	ldap_set_option( ld, LDAP_OPT_DEREF, &deref );
	ldap_set_option( ld, LDAP_OPT_TIMELIMIT, &timelimit );
	ldap_set_option( ld, LDAP_OPT_SIZELIMIT, &sizelimit );
    }

    ldaptool_bind( ld );

    if ( ldaptool_verbose ) {
	printf( "filter pattern: %s\nreturning: ", filtpattern );
	if ( attrs == NULL ) {
	    printf( "ALL" );
	} else {
	    for ( i = 0; attrs[ i ] != NULL; ++i ) {
		printf( "%s ", attrs[ i ] );
	    }
	}
	putchar( '\n' );
    }

    if ( ldaptool_fp == NULL ) {
	char *conv;

	conv = ldaptool_local2UTF8( base );
	rc = dosearch( ld, conv, scope, attrs, attrsonly, filtpattern, "" );
	if( conv != NULL )
            free( conv );
    } else {
	int done = 0;

	rc = LDAP_SUCCESS;
	first = 1;
	while ( rc == LDAP_SUCCESS && !done ) {
	    char *linep = NULL;
	    int   increment = 0;
	    int	  c, index;
	 
	    /* allocate initial block of memory */ 
	    if ((linep = (char *)malloc(BUFSIZ)) == NULL) {
	        fprintf( stderr, "Out of memory\n" );
		exit( LDAP_NO_MEMORY );
	    }
	    increment++;
	    index = 0;
	    while ((c = fgetc( ldaptool_fp )) != '\n' && c != EOF) {

	        /* check if we will overflow the buffer */
	        if ((c != EOF) && (index == ((increment * BUFSIZ) -1))) {

		    /* if we did, add another BUFSIZ worth of bytes */
	   	    if ((linep = (char *)
			realloc(linep, (increment + 1) * BUFSIZ)) == NULL) {
			fprintf( stderr, "Out of memory\n" );
			exit( LDAP_NO_MEMORY );
		    }
		    increment++;
		}
		linep[index++] = c;
	    }

	    if (c == EOF) {
		done = 1;
		break;
	    }
	    
	    linep[index] = '\0';

	    if ( !first ) {
		putchar( '\n' );
	    } else {
		first = 0;
	    }
	    rc = dosearch( ld, base, scope, attrs, attrsonly, filtpattern,
		    linep );
	    free (linep);
	}
    }

    ldaptool_cleanup( ld );
    return( rc );
}


static void
options_callback( int option, char *optarg )
{
    char *s, *p, *temp_arg, *ps_ptr, *ps_arg;
    int i=0;
    
    switch( option ) {
    case 'u':	/* include UFN */
	++includeufn;
	break;
    case 't':	/* write attribute values to /tmp files */
	++vals2tmp;
	break;
    case 'U':	/* produce file URLs in conjunction with -t */
	++produce_file_urls;
	break;
    case 'e':	/* minimize base-64 encoding of values */
	++minimize_base64;
	break;
    case 'A':	/* retrieve attribute names only -- no values */
	++attrsonly;
	break;
    case 'L':       /* print entries in LDIF format -- now the default */
	break;
    case 'o':	/* print entries using old ldapsearch format */
	ldif = 0;
	break;
    case 'B':	/* allow binary values to be printed, even if -o used */
	++allow_binary;
	break;
    case '1':	/* omit leading "version: #" line from LDIF output */
	write_ldif_version = 0;
	break;
    case 's':	/* search scope */
	if ( strncasecmp( optarg, "base", 4 ) == 0 ) {
	    scope = LDAP_SCOPE_BASE;
	} else if ( strncasecmp( optarg, "one", 3 ) == 0 ) {
	    scope = LDAP_SCOPE_ONELEVEL;
	} else if ( strncasecmp( optarg, "sub", 3 ) == 0 ) {
	    scope = LDAP_SCOPE_SUBTREE;
	} else {
	    fprintf( stderr, "scope should be base, one, or sub\n" );
	    usage();
	}
	break;

    case 'a':	/* set alias deref option */
	if ( strncasecmp( optarg, "never", 5 ) == 0 ) {
	    deref = LDAP_DEREF_NEVER;
	} else if ( strncasecmp( optarg, "search", 5 ) == 0 ) {
	    deref = LDAP_DEREF_SEARCHING;
	} else if ( strncasecmp( optarg, "find", 4 ) == 0 ) {
	    deref = LDAP_DEREF_FINDING;
	} else if ( strncasecmp( optarg, "always", 6 ) == 0 ) {
	    deref = LDAP_DEREF_ALWAYS;
	} else {
	    fprintf( stderr, "alias deref should be never, search, find, or always\n" );
	    usage();
	}
	break;

    case 'F':	/* field separator */
	sep = strdup( optarg );
	break;
    case 'b':	/* searchbase */
	base = strdup( optarg );
	break;
    case 'l':	/* time limit */
	timelimit = atoi( optarg );
	break;
    case 'x':	/* server sorting requested */
	server_sort = 1;
	break;
    case 'z':	/* size limit */
	sizelimit = atoi( optarg );
	break;
    case 'S':	/* sort attribute */
	ldap_charray_add( &sortattr, strdup( optarg ) );
	break;
    case 'T':	/* don't fold lines */
	fold = 0;
	break;
    case 'G':  /* do the virtual list setup */
	use_vlv++;
	s = strchr(optarg, VLV_PARAM_SEP );
	
	if (s != NULL)
	{
	    vlv_before = atoi(optarg);
	    s++;
	    vlv_after = atoi( s );
	    s = strchr(s, VLV_PARAM_SEP );
	    if (s != NULL)
	    {
		s++;
		/* below is a small set of logic to implement the following cases
		 * -G23:23:wilber
		 * -G23:23:"wilber:wright"
		 * -G23:23:'wilber'
		 * -G23:23:wilber wright
		 * all of the above are before, after, value -  NOTE: a colon not in a quoted 
		 * string will break the parser!!!!
		 * -G23:23:45:600
		 * above is index, count encoding
		 */

		if (*s == '\'' || *s == '"')
		{
		    vlv_value = strdup( s );
		}
		else
		{
		    if (strchr( s, VLV_PARAM_SEP ))
		    {
			/* we have an index + count option */
			vlv_index = atoi( s );
			vlv_count = atoi( strchr( s, VLV_PARAM_SEP) + 1);
		    }
		    else
		    {
			/* we don't have a quote surrounding the assertion value 
			 * do we need to??? 
			 */
			vlv_value = strdup( s );
		    }
		}
	    }
	    else
	    {
		fprintf( stderr,"Illegal 'after' paramater for virtual list\n" );
		exit( LDAP_PARAM_ERROR );
	    }
	    
	}
	else
	{
	    fprintf( stderr,"Illegal 'before' paramater for virtual list\n" );
	    exit( LDAP_PARAM_ERROR );
	}
	break;
    case 'C':
	use_psearch++;
	if ( (ps_arg = strdup( optarg)) == NULL ) {
	    perror ("strdup");
	    exit (LDAP_NO_MEMORY);
	}
	
	ps_ptr=strtok(ps_arg, ":");
	if (ps_ptr == NULL || (strcasecmp(ps_ptr, "ps")) ) {
	    fprintf (stderr, "Invalid argument for -C\n");
	    usage();
	}
	if (ps_ptr=strtok(NULL, ":")) {
	    if ( (temp_arg = strdup( ps_ptr )) == NULL ) {
	        perror ("strdup");
	    	exit (LDAP_NO_MEMORY);
	    } 
	} else {
	    fprintf (stderr, "Invalid argument for -C\n");
	    usage();
	}
	if (ps_ptr=strtok(NULL, ":")) {
	    if ( (changesonly = ldaptool_boolean_str2value(ps_ptr, 0)) == -1) {
		fprintf(stderr, "Invalid option value: %s\n", ps_ptr);
		usage();
	    }
	}    
	if (ps_ptr=strtok(NULL, ":")) {
	    if ( (return_echg_ctls = ldaptool_boolean_str2value(ps_ptr, 0)) == -1) {
		fprintf(stderr, "Invalid option value: %s\n", ps_ptr);
		usage();
	    }
	}    

	/* Now parse the temp_arg and build chgtype as
	 * the changetypes are encountered */

	if ((ps_ptr = strtok( temp_arg, "," )) == NULL) {
		usage();
	} else {
	    while ( ps_ptr ) {
		if ((strcasecmp(ps_ptr, "add"))==0) 
		    chgtype |= LDAP_CHANGETYPE_ADD;
		else if ((strcasecmp(ps_ptr, "delete"))==0) 
		    chgtype |= LDAP_CHANGETYPE_DELETE;
		else if ((strcasecmp(ps_ptr, "modify"))==0) 
		    chgtype |= LDAP_CHANGETYPE_MODIFY;
		else if ((strcasecmp(ps_ptr, "moddn"))==0) 
		    chgtype |= LDAP_CHANGETYPE_MODDN;
		else if ((strcasecmp(ps_ptr, "any"))==0) 
		    chgtype = LDAP_CHANGETYPE_ANY;
		else {
			fprintf(stderr, "Unknown changetype: %s\n", ps_ptr);
			usage();
		}
		ps_ptr = strtok( NULL, "," );
	    }
	  }
	break;
    default:
	usage();
	break;
    }
}


static int
dosearch( ld, base, scope, attrs, attrsonly, filtpatt, value )
    LDAP	*ld;
    char	*base;
    int		scope;
    char	**attrs;
    int		attrsonly;
    char	*filtpatt;
    char	*value;
{
    char		**refs = NULL, filter[ BUFSIZ ], *filterp = NULL;
    int			rc, first, matches;
    LDAPMessage		*res, *e;
    LDAPControl		*ldctrl;
    LDAPControl		**ctrl_response_array = NULL;
    LDAPVirtualList	vlv_data;
    int			msgid = 0;
    int			length = 0;

    length = strlen( filtpatt ) + strlen ( value ) +1;
    if ( length > BUFSIZ ) {
	if ((filterp = (char *)
		malloc ( length )) == NULL) {
		perror( "filter and/or pattern too long?" );
		exit (LDAP_PARAM_ERROR);
	}
    } else {
	filterp = filter;
    }
 
#ifdef HAVE_SNPRINTF
    if ( snprintf( filterp, length, filtpatt, value ) < 0 ) {
	perror( "snprintf filter (filter and/or pattern too long?)" );
	exit( LDAP_PARAM_ERROR );
    }
#else
    sprintf( filterp, filtpatt, value );
#endif

    if ( *filterp == '\0' ) {	/* treat empty filter is a shortcut for oc=* */
	strcpy( filterp, "(objectclass=*)" );
    }

    if ( ldaptool_verbose ) {
	/*
	 * Display the filter that will be used.  Add surrounding parens.
	 * if they are missing.
	 */
	if ( '(' == *filterp ) {
	    printf( "filter is: %s\n", filterp );
	} else {
	    printf( "filter is: (%s)\n", filterp );
	}
    }

    if ( ldaptool_not ) {
	if (filterp != filter)
		free (filterp);
	return( LDAP_SUCCESS );
    }

    if (( ldctrl = ldaptool_create_manage_dsait_control()) != NULL ) {
	ldaptool_add_control_to_array(ldctrl, ldaptool_request_ctrls);
    }

    if ((ldctrl = ldaptool_create_proxyauth_control(ld)) !=NULL) {
	ldaptool_add_control_to_array(ldctrl, ldaptool_request_ctrls);
    }
    
    if (use_psearch) {
	if ( ldap_create_persistentsearch_control( ld, chgtype,
                changesonly, return_echg_ctls, 
		1, &ldctrl ) != LDAP_SUCCESS )
	{
		ldap_perror( ld, "ldap_create_persistentsearch_control" );
		return (1);
	}
	ldaptool_add_control_to_array(ldctrl, ldaptool_request_ctrls);
    }


    if (server_sort) {
	/* First make a sort key list from the attribute list we have */
	LDAPsortkey **keylist = NULL;
	int i = 0;
	char *sortattrs = NULL;
	char *s = NULL;
	int string_length = 0;

	/* Count the sort strings */
	for (i = 0; i < sortsize - 1 ; i++) {
	    string_length += strlen(sortattr[i]) + 1;
	}

	sortattrs = (char *) malloc(string_length + 1);
	if (NULL == sortattrs) {
	    fprintf( stderr, "Out of memory\n" );
	    exit( LDAP_NO_MEMORY );
	}
	
	s = sortattrs;
	for (i = 0; i < sortsize - 1 ; i++) {
	    memcpy(s, sortattr[i], strlen(sortattr[i]));
	    s += strlen(sortattr[i]);
	    *s++ = ' ';
	}

	sortattrs[string_length] = '\0';

	ldap_create_sort_keylist(&keylist,sortattrs);
	free(sortattrs);
	sortattrs = NULL;

	/* Then make a control for the sort attributes we have */
	rc = ldap_create_sort_control(ld,keylist,0,&ldctrl);
	ldap_free_sort_keylist(keylist);
	if ( rc != LDAP_SUCCESS ) {
	    if (filterp != filter)
		free (filterp);
	    return( ldaptool_print_lderror( ld, "ldap_create_sort_control",
		LDAPTOOL_CHECK4SSL_IF_APPROP ));
	}

	ldaptool_add_control_to_array(ldctrl, ldaptool_request_ctrls);

    }
    /* remember server side sorting must be available for vlv!!!! */

    if (use_vlv)
    {
	vlv_data.ldvlist_before_count = vlv_before;
	vlv_data.ldvlist_after_count = vlv_after;
	if ( ldaptool_verbose ) {
	    printf( "vlv data %lu, %lu, ", 
		    vlv_data.ldvlist_before_count,
		    vlv_data.ldvlist_after_count 
		);
	}
	if (vlv_value)
	{
	    vlv_data.ldvlist_attrvalue = vlv_value;
	    vlv_data.ldvlist_size = 0;
	    vlv_data.ldvlist_index = 0;
	    if ( ldaptool_verbose ) {
		printf( "%s, 0, 0\n", vlv_data.ldvlist_attrvalue);
	    }
	}
	else
	{
	    vlv_data.ldvlist_attrvalue = NULL;
	    vlv_data.ldvlist_size = vlv_count;
	    vlv_data.ldvlist_index = vlv_index;
	    if ( ldaptool_verbose ) {
		printf( "(null), %lu, %lu\n", vlv_data.ldvlist_size, vlv_data.ldvlist_index );
	    }
	}
	
	if ( rc != LDAP_SUCCESS ) {
	    if (filterp != filter)
		free (filterp);
	    return( ldaptool_print_lderror( ld, "ldap_create_sort_control",
		LDAPTOOL_CHECK4SSL_IF_APPROP ));
	}
	if (LDAP_SUCCESS != (rc = ldap_create_virtuallist_control(ld,
		&vlv_data, &ldctrl)))
	{
	    if (filterp != filter)
		free (filterp);
	    return( ldaptool_print_lderror( ld,
		"ldap_create_virtuallist_control",
		LDAPTOOL_CHECK4SSL_IF_APPROP ));
	}

	ldaptool_add_control_to_array(ldctrl, ldaptool_request_ctrls);
	
    }

    if ( ldap_search_ext( ld, base, scope, filterp, attrs, attrsonly, 
	    ldaptool_request_ctrls, NULL, NULL, -1, &msgid )
	    != LDAP_SUCCESS ) {
	if (filterp != filter)
		free (filterp);
	return( ldaptool_print_lderror( ld, "ldap_search",
		LDAPTOOL_CHECK4SSL_IF_APPROP ));
    }


    matches = 0;
    first = 1;
    if ( sortattr && !server_sort ) {
        rc = ldap_result( ld, LDAP_RES_ANY, 1, NULL, &res );
    } else {
        while ( (rc = ldap_result( ld, LDAP_RES_ANY, 0, NULL, &res )) != 
		LDAP_RES_SEARCH_RESULT && rc != -1 ) {
	    if ( rc != LDAP_RES_SEARCH_ENTRY ) {
		if ( rc == LDAP_RES_SEARCH_REFERENCE ) {
		    parse_and_display_reference( ld, res );
		} else if ( rc == LDAP_RES_EXTENDED
			&& ldap_msgid( res ) == LDAP_RES_UNSOLICITED ) {
		    ldaptool_print_extended_response( ld, res,
			    "Unsolicited response" );
		} else {
		    fprintf( stderr, "%s: ignoring LDAP response message"
			    " type 0x%x (%s)\n",
			    ldaptool_progname, rc, msgtype2str( rc ));
		}
		ldap_msgfree( res );
		continue;
	    }
	    matches++;
	    e = ldap_first_entry( ld, res );
	    if ( !first ) {
	        putchar( '\n' );
	    } else {
	        first = 0;
	    }
	    print_entry( ld, e, attrsonly );
	    ldap_msgfree( res );
        }
    }
    if ( rc == -1 ) {
	if (filterp != filter)
		free (filterp);
	return( ldaptool_print_lderror( ld, "ldap_result",
		LDAPTOOL_CHECK4SSL_IF_APPROP ));
    }

    if ( ldap_parse_result( ld, res, &rc, NULL, NULL, &refs,
	    &ctrl_response_array, 0 ) != LDAP_SUCCESS ) {
	ldaptool_print_lderror( ld, "ldap_parse_result",
		LDAPTOOL_CHECK4SSL_IF_APPROP );
    } else if ( rc != LDAP_SUCCESS ) {                                          
	ldaptool_print_lderror( ld, "ldap_search",
		LDAPTOOL_CHECK4SSL_IF_APPROP );
    } 
    /* Parse the returned sort control */
    if (server_sort) {
	unsigned long result = 0;
	char *attribute;
	
	if ( LDAP_SUCCESS != ldap_parse_sort_control(ld,ctrl_response_array,&result,&attribute) ) {
	    ldaptool_print_lderror(ld, "ldap_parse_sort_control",
		    LDAPTOOL_CHECK4SSL_IF_APPROP );
	    ldap_controls_free(ctrl_response_array);
	    ldap_msgfree(res);
	    if (filterp != filter)
		free (filterp);
	    return ( ldap_get_lderrno( ld, NULL, NULL ) );
	}
	
	if (0 == result) {
	    if ( ldaptool_verbose ) {
		printf( "Server indicated results sorted OK\n");
	    }
	} else {
	    if (NULL != attribute) {
		printf("Server reported sorting error %ld: %s, attribute in error\"%s\"\n",result,sortresult2string(result),attribute);
	    } else {
		printf("Server reported sorting error %ld: %s\n",result,sortresult2string(result));
	    }
	}

    }

    if (use_vlv)
    {
	unsigned long vpos, vcount;
	int vresult;
	if ( LDAP_SUCCESS != ldap_parse_virtuallist_control(ld,ctrl_response_array,&vpos, &vcount,&vresult) ) {
	    ldaptool_print_lderror( ld, "ldap_parse_virtuallist_control",
		    LDAPTOOL_CHECK4SSL_IF_APPROP );
	    ldap_controls_free(ctrl_response_array);
	    ldap_msgfree(res);
	    if (filterp != filter)
		free (filterp);
	    return ( ldap_get_lderrno( ld, NULL, NULL ) );
	}
	
	if (0 == vresult) {
	    if ( ldaptool_verbose ) {
		printf( "Server indicated virtual list positioning OK\n");
	    }
	    printf("index %lu content count %lu\n", vpos, vcount);
	    
	} else {
	    printf("Server reported sorting error %d: %s\n",vresult,sortresult2string(vresult));

	}

    }
    
    ldap_controls_free(ctrl_response_array);   
    
    if ( sortattr != NULL && !server_sort) {
	
	(void) ldap_multisort_entries( ld, &res,
				       ( *sortattr == NULL ) ? NULL : sortattr, 
				       (LDAP_CMP_CALLBACK *)strcasecmp );
	matches = 0;
	first = 1;
	for ( e = ldap_first_entry( ld, res ); e != NULLMSG;
	      e = ldap_next_entry( ld, e ) ) {
	    matches++;
	    if ( !first ) {
		putchar( '\n' );
	    } else {
		first = 0;
	    }
	    print_entry( ld, e, attrsonly );
	}
    }
    
    if ( ldaptool_verbose ) {
	printf( "%d matches\n", matches );
    }

    if ( refs != NULL ) {
	ldaptool_print_referrals( refs );
	ldap_value_free( refs );
    }

    if (filterp != filter)
	free (filterp);

    ldap_msgfree( res );
    return( rc );
}


static void
print_entry( ld, entry, attrsonly )
    LDAP	*ld;
    LDAPMessage	*entry;
    int		attrsonly;
{
    char		*a, *dn, *ufn, tmpfname[ BUFSIZ ];
    int			i, notascii;
    BerElement		*ber;
    struct berval	**bvals;
    FILE		*tmpfp;
#if defined( XP_WIN32 )
	char	mode[20] = "w+b";
#else
	char	mode[20] = "w";
#endif

    dn = ldap_get_dn( ld, entry );
    write_string_attr_value( "dn", dn, LDAPTOOL_WRITEVALOPT_SUPPRESS_NAME );
    if ( includeufn ) {
	ufn = ldap_dn2ufn( dn );
	write_string_attr_value( "ufn", ufn,
			LDAPTOOL_WRITEVALOPT_SUPPRESS_NAME );
	free( ufn );
    }
    ldap_memfree( dn );

    if ( use_psearch ) {
	LDAPControl	**ectrls;
	int		chgtype, chgnumpresent;
	long		chgnum;
	char		*prevdn, intbuf[ 128 ];

	if ( ldap_get_entry_controls( ld, entry, &ectrls ) == LDAP_SUCCESS ) {
	    if ( ldap_parse_entrychange_control( ld, ectrls, &chgtype,
			&prevdn, &chgnumpresent, &chgnum ) == LDAP_SUCCESS ) {
		write_string_attr_value(
			LDAPTOOL_PSEARCH_ATTR_PREFIX "changeType",
			changetype_num2string( chgtype ), 0 );
		if ( chgnumpresent ) {
		    sprintf( intbuf, "%d", chgnum );
		    write_string_attr_value(
			    LDAPTOOL_PSEARCH_ATTR_PREFIX "changeNumber",
			    intbuf, 0 );
		}
		if ( NULL != prevdn ) {
		    write_string_attr_value(
			    LDAPTOOL_PSEARCH_ATTR_PREFIX "previousDN",
			    prevdn, 0 );
		    ldap_memfree( prevdn );
		}
	    }
	}
    }

    for ( a = ldap_first_attribute( ld, entry, &ber ); a != NULL;
	    a = ldap_next_attribute( ld, entry, ber ) ) {
	if ( ldap_charray_inlist(sortattr, a) &&            /* in the list*/
	    skipsortattr[ldap_charray_position(sortattr, a)] ) {/* and skip it*/
	    continue;                                       /* so skip it! */
	}
	if ( attrsonly ) {
	    if ( ldif ) {
		write_ldif_value( a, "", 0, 0 );
	    } else {
		printf( "%s\n", a );
	    }
	} else if (( bvals = ldap_get_values_len( ld, entry, a )) != NULL ) {
	    for ( i = 0; bvals[i] != NULL; i++ ) {
		if ( vals2tmp ) {
#ifdef HAVE_SNPRINTF
		    if ( snprintf( tmpfname, sizeof(tmpfname),
			    "%s/ldapsearch-%s-XXXXXX",
			    ldaptool_get_tmp_dir(), a ) < 0 ) {
			perror( "snprintf tmpfname (attribute name too long?)" );
			exit( LDAP_PARAM_ERROR );
		    }
#else
		    sprintf( tmpfname, "%s/ldapsearch-%s-XXXXXX",
			    ldaptool_get_tmp_dir(), a );
#endif
		    tmpfp = NULL;

		    if ( mktemp( tmpfname ) == NULL ) {
			perror( tmpfname );
		    } else if (( tmpfp = fopen( tmpfname, mode)) == NULL ) {
			perror( tmpfname );
		    } else if ( bvals[ i ]->bv_len > 0 &&
			    fwrite( bvals[ i ]->bv_val,
			    bvals[ i ]->bv_len, 1, tmpfp ) == 0 ) {
			perror( tmpfname );
		    } else if ( ldif ) {
			if ( produce_file_urls ) {
			    char	*url;

			    if ( ldaptool_path2fileurl( tmpfname, &url ) !=
				LDAPTOOL_FILEURL_SUCCESS ) {
				    perror( "ldaptool_path2fileurl" );
				} else {
				    write_ldif_value( a, url, strlen( url ),
					    LDIF_OPT_VALUE_IS_URL );
				    free( url );
				}
			} else {
			    write_ldif_value( a, tmpfname, strlen( tmpfname ),
				    0 );
			}
		    } else {
			printf( "%s%s%s\n", a, sep, tmpfname );
		    }

		    if ( tmpfp != NULL ) {
			fclose( tmpfp );
		    }
		} else {
		    notascii = 0;
		    if ( !ldif && !allow_binary ) {
			notascii = !ldaptool_berval_is_ascii( bvals[i] );
		    }

		    if ( ldif ) {
			write_ldif_value( a, bvals[ i ]->bv_val,
				bvals[ i ]->bv_len, 0 );
		    } else {
			printf( "%s%s%s\n", a, sep,
				notascii ? "NOT ASCII" : bvals[ i ]->bv_val );
		    }
		}
	    }
	    ber_bvecfree( bvals );
	}
	ldap_memfree( a );
    }

    if ( ldap_get_lderrno( ld, NULL, NULL ) != LDAP_SUCCESS ) {
	ldaptool_print_lderror( ld, "ldap_first_attribute/ldap_next_attribute",
		LDAPTOOL_CHECK4SSL_IF_APPROP );
    }

    if ( ber != NULL ) {
	ber_free( ber, 0 );
    }
}


static void
write_string_attr_value( char *attrname, char *strval, unsigned long opts )
{
    if ( strval == NULL ) {
	strval = "";
    }
    if ( ldif ) {
	write_ldif_value( attrname, strval, strlen( strval ), 0 );
    } else if ( 0 != ( opts & LDAPTOOL_WRITEVALOPT_SUPPRESS_NAME )) {
	printf( "%s\n", strval );
    } else {
	printf( "%s%s%s\n", attrname, sep, strval );
    }
}


static int
write_ldif_value( char *type, char *value, unsigned long vallen,
	unsigned long ldifoptions )
{
    char	*ldif;
    static int	wrote_version = 0;

    if ( write_ldif_version && !wrote_version ) {
	char	versionbuf[ 64 ];

	wrote_version = 1;
	sprintf( versionbuf, "%d", LDIF_VERSION_ONE );
	write_ldif_value( "version", versionbuf, strlen( versionbuf ), 0 );
    }

    if ( !fold ) {
	ldifoptions |= LDIF_OPT_NOWRAP;
    }
    if ( minimize_base64 ) {
	ldifoptions |= LDIF_OPT_MINIMAL_ENCODING;
    }

    if (( ldif = ldif_type_and_value_with_options( type, value, (int)vallen,
	    ldifoptions )) == NULL ) {
	return( -1 );
    }

    fputs( ldif, stdout );
    free( ldif );

    return( 0 );
}


static char *
sortresult2string(unsigned long result)
{
	/*
            success                   (0), -- results are sorted
            operationsError           (1), -- server internal failure
            timeLimitExceeded         (3), -- timelimit reached before
                                           -- sorting was completed
            strongAuthRequired        (8), -- refused to return sorted
                                           -- results via insecure
                                           -- protocol
            adminLimitExceeded       (11), -- too many matching entries
                                           -- for the server to sort
            noSuchAttribute          (16), -- unrecognized attribute
                                           -- type in sort key
            inappropriateMatching    (18), -- unrecognized or inappro-
                                           -- priate matching rule in
                                           -- sort key
            insufficientAccessRights (50), -- refused to return sorted
                                           -- results to this client
            busy                     (51), -- too busy to process
            unwillingToPerform       (53), -- unable to sort
            other                    (80)
	*/

	switch (result) {
	case 0: return ("success");
	case 1: return ("operations error");
	case 3: return ("time limit exceeded");
	case 8: return ("strong auth required");
	case 11: return ("admin limit exceeded");
	case 16: return ("no such attribute");
	case 18: return ("unrecognized or inappropriate matching rule");
	case 50: return ("insufficient access rights");
	case 51: return ("too busy");
	case 53: return ("unable to sort");
	case 80:
        default: return ("Er...Other ?");
	}
}


static void
parse_and_display_reference( LDAP *ld, LDAPMessage *ref )
{
    int		i;
    char	**refs;

    if ( ldap_parse_reference( ld, ref, &refs, NULL, 0 ) != LDAP_SUCCESS ) {
	ldaptool_print_lderror( ld, "ldap_parse_reference",
		LDAPTOOL_CHECK4SSL_IF_APPROP );
    } else if ( refs != NULL && refs[ 0 ] != NULL ) {
	fputs( "Unfollowed continuation reference(s):\n", stderr );
	for ( i = 0; refs[ i ] != NULL; ++i ) {
	    fprintf( stderr, "    %s\n", refs[ i ] );
	}
	ldap_value_free( refs );
    }
}


/*possible operations a client can invoke -- copied from ldaprot.h */          

#ifndef LDAP_REQ_BIND                                                           
#define LDAP_REQ_BIND                   0x60L   /* application + constructed */ 
#define LDAP_REQ_UNBIND                 0x42L   /* application + primitive   */ 
#define LDAP_REQ_SEARCH                 0x63L   /* application + constructed */ 
#define LDAP_REQ_MODIFY                 0x66L   /* application + constructed */ 
#define LDAP_REQ_ADD                    0x68L   /* application + constructed */ 
#define LDAP_REQ_DELETE                 0x4aL   /* application + primitive   */ 
#define LDAP_REQ_RENAME                 0x6cL   /* application + constructed */ 
#define LDAP_REQ_COMPARE                0x6eL   /* application + constructed */ 
#define LDAP_REQ_ABANDON                0x50L   /* application + primitive   */ 
#define LDAP_REQ_EXTENDED               0x77L   /* application + constructed */ 
#endif /* LDAP_REQ_BIND */                                                      



struct ldapsearch_type2str {
    
    int         ldst2s_type;    /* message type */ 
    char        *ldst2s_string; /* descriptive string */     
};  


static struct ldapsearch_type2str ldapsearch_msgtypes[] = {
    
    /* results: */    
    { LDAP_RES_BIND,                    "bind result" },
    { LDAP_RES_SEARCH_REFERENCE,        "continuation reference" },
    { LDAP_RES_SEARCH_ENTRY,            "entry" },
    { LDAP_RES_SEARCH_RESULT,           "search result" },
    { LDAP_RES_MODIFY,                  "modify result" },
    { LDAP_RES_ADD,                     "add result" }, 
    { LDAP_RES_DELETE,                  "delete result" },
    { LDAP_RES_MODDN,                   "rename result" },
    { LDAP_RES_COMPARE,                 "compare result" },
    { LDAP_RES_EXTENDED,                "extended operation result" },
    /* requests: */ 
    { LDAP_REQ_BIND,                    "bind request" }, 
    { LDAP_REQ_UNBIND,                  "unbind request" }, 
    { LDAP_REQ_SEARCH,                  "search request" }, 
    { LDAP_REQ_MODIFY,                  "modify request" }, 
    { LDAP_REQ_ADD,                     "add request" },
    { LDAP_REQ_DELETE,                  "delete request" }, 
    { LDAP_REQ_RENAME,                  "rename request" }, 
    { LDAP_REQ_COMPARE,                 "compare request" }, 
    { LDAP_REQ_ABANDON,                 "abandon request" }, 
    { LDAP_REQ_EXTENDED,                "extended request" },
    
};


#define LDAPSEARCHTOOL_NUMTYPES (sizeof(ldapsearch_msgtypes) \
				 / sizeof(struct ldapsearch_type2str))


/*
* Return a descriptive string given an LDAP result message type (tag). 
*/     
static char * 
msgtype2str( int msgtype )
{    
    char        *s = "unknown"; 
    int         i;
    
    s = "unknown";
    for ( i = 0; i < LDAPSEARCHTOOL_NUMTYPES; ++i ) {
	if ( msgtype == ldapsearch_msgtypes[ i ].ldst2s_type ) {
	    s = ldapsearch_msgtypes[ i ].ldst2s_string;	
	}	
    }    
    return( s );
}


/*
 * Return a descriptive string given a Persistent Search change type
 */
static char *
changetype_num2string( int chgtype )
{
    char	*s = "unknown";

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
    }

    return( s );
}
