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

/* ldapmodify.c - generic program to modify or add entries using LDAP */

#include <__version.h>

#include "ldaptool.h"
#include "fileurl.h"

static int		newval, contoper, force, valsfromfiles, display_binary_values;
static int		ldif_version = -1;	/* -1 => unknown version */
static LDAP		*ld;
static char		*rejfile = NULL;
static char		*bulkimport_suffix = NULL;
static int		ldapmodify_quiet = 0;

#define LDAPMOD_MAXLINE		4096

/* strings found in replog/LDIF entries (mostly lifted from slurpd/slurp.h) */
#define T_REPLICA_STR		"replica"
#define T_DN_STR		"dn"
#define T_VERSION_STR		"version"
#define T_CHANGETYPESTR         "changetype"
#define T_ADDCTSTR		"add"
#define T_MODIFYCTSTR		"modify"
#define T_DELETECTSTR		"delete"
#define T_RENAMECTSTR		"rename"	/* non-standard */
#define T_MODDNCTSTR		"moddn"
#define T_MODRDNCTSTR		"modrdn"
#define T_MODOPADDSTR		"add"
#define T_MODOPREPLACESTR	"replace"
#define T_MODOPDELETESTR	"delete"
#define T_MODSEPSTR		"-"
#define T_NEWRDNSTR		"newrdn"
#define	T_NEWSUPERIORSTR	"newsuperior"
#define	T_NEWPARENTSTR		"newparent"
#define T_DELETEOLDRDNSTR	"deleteoldrdn"
#define T_NEWSUPERIORSTR        "newsuperior"
#define T_NEWPARENTSTR          "newparent"	/* non-standard */

/* bulk import */
#define	BULKIMPORT_START_OID	"2.16.840.1.113730.3.5.7"
#define	BULKIMPORT_STOP_OID	"2.16.840.1.113730.3.5.8"

static void options_callback( int option, char *optarg );
static int process_ldapmod_rec( char *rbuf );
static int process_ldif_rec( char *rbuf );
static void addmodifyop( LDAPMod ***pmodsp, int modop, char *attr,
	char *value, int vlen );
static int domodify( char *dn, LDAPMod **pmods, int newentry );
static int dodelete( char *dn );
static int dorename( char *dn, char *newrdn, char *newparent,
	int deleteoldrdn );
static void freepmods( LDAPMod **pmods );
static char *read_one_record( FILE *fp );
static char *strdup_and_trim( char *s );

static void
usage( void )
{
    fprintf( stderr, "usage: %s [options]\n", ldaptool_progname );
    fprintf( stderr, "options:\n" );
    ldaptool_common_usage( 0 );
    fprintf( stderr, "    -c\t\tcontinuous mode (do not stop on errors)\n" );
    fprintf( stderr, "    -A\t\tdisplay non-ASCII values in conjunction with -v\n" );
    fprintf( stderr, "    -f file\tread modifications from file (default: standard input)\n" );
    if ( strcmp( ldaptool_progname, "ldapmodify" ) == 0 ){
	fprintf( stderr, "    -a\t\tadd entries\n" );
    }
    fprintf( stderr, "    -b\t\tread values that start with / from files (for bin attrs)\n" );
    fprintf( stderr, "    -F\t\tforce application of all changes, regardless of\n" );
    fprintf( stderr, "      \t\treplica lines\n" );
    fprintf( stderr, "    -e rejfile\tsave rejected entries in \"rejfile\"\n" );
    fprintf( stderr, "    -B suffix\tbulk import to \"suffix\"\n");
    fprintf( stderr, "    -q\t\tbe quiet when adding/modifying entries\n" );
    exit( LDAP_PARAM_ERROR );
}


int
main( int argc, char **argv )
{
    char	*rbuf, *saved_rbuf, *start, *p, *q;
    FILE	*rfp = NULL;
    int		rc, use_ldif, deref, optind;
    LDAPControl	*ldctrl;


#ifdef notdef
#ifdef HPUX11
#ifndef __LP64__
	_main( argc, argv);
#endif /* __LP64_ */
#endif /* HPUX11 */
#endif

    valsfromfiles = display_binary_values = 0;

    optind = ldaptool_process_args( argc, argv, "aAbcFe:B:q", 0,
	    options_callback );


    if ( optind == -1 ) {
	usage();
    }

    if ( !newval && strcmp( ldaptool_progname, "ldapadd" ) == 0 ) {
	newval = 1;
    }

    if ( ldaptool_fp == NULL ) {
	ldaptool_fp = stdin;
    }

    if ( argc - optind != 0 ) {
	usage();
    }

    ld = ldaptool_ldap_init( 0 );

    if ( !ldaptool_not ) {
	deref = LDAP_DEREF_NEVER;	/* this seems prudent */
	ldap_set_option( ld, LDAP_OPT_DEREF, &deref );
    }

    ldaptool_bind( ld );

    if (( ldctrl = ldaptool_create_manage_dsait_control()) != NULL ) {
	ldaptool_add_control_to_array( ldctrl, ldaptool_request_ctrls);
    } 

    if ((ldctrl = ldaptool_create_proxyauth_control(ld)) !=NULL) {
	ldaptool_add_control_to_array( ldctrl, ldaptool_request_ctrls);
    }

    rc = 0;

    /* turn on bulk import?*/
    if (bulkimport_suffix) {
	struct berval	bv, *retdata;
	char		*retoid;

	bv.bv_val = bulkimport_suffix;
	bv.bv_len = strlen(bulkimport_suffix);
	if ((rc = ldap_extended_operation_s(ld,
	    BULKIMPORT_START_OID, &bv, NULL,
	    NULL, &retoid, &retdata)) != 0) {
		fprintf(stderr, "Error: unable to service "
		    "extended operation request\n\t'%s' for "
		    "bulk import\n\t(error:%d:'%s')\n",
		    BULKIMPORT_START_OID, rc, ldap_err2string(rc));
		return (rc);
	}
	if (retoid)
		ldap_memfree(retoid);
	if (retdata)
		ber_bvfree(retdata);
    }

    while (( rc == 0 || contoper ) &&
		( rbuf = read_one_record( ldaptool_fp )) != NULL ) {
	/*
	 * we assume record is ldif/slapd.replog if the first line
	 * has a colon that appears to the left of any equal signs, OR
	 * if the first line consists entirely of digits (an entry id)
	 */
	use_ldif = ( p = strchr( rbuf, ':' )) != NULL &&
		( q = strchr( rbuf, '\n' )) != NULL && p < q &&
		(( q = strchr( rbuf, '=' )) == NULL || p < q );

	start = rbuf;
	saved_rbuf = strdup( rbuf );

	if ( !use_ldif && ( q = strchr( rbuf, '\n' )) != NULL ) {
	    for ( p = rbuf; p < q; ++p ) {
		if ( !isdigit( *p )) {
		    break;
		}
	    }
	    if ( p >= q ) {
		use_ldif = 1;
		start = q + 1;
	    }
	}

	if ( use_ldif ) {
	    rc = process_ldif_rec( start );
	} else {
	    rc = process_ldapmod_rec( start );
	}
	if ( rc != LDAP_SUCCESS && rejfile != NULL ) {
	    /* Write this record to the reject file */
	    int newfile = 0;
	    struct stat stbuf;
	    if ( stat( rejfile, &stbuf ) < 0 ) {
		if ( errno == ENOENT ) {
		    newfile = 1;
		}
	    }
	    if (( rfp = ldaptool_open_file( rejfile, "a" )) == NULL ) {
		fprintf( stderr, "Cannot open error file \"%s\" - "
			"erroneous entries will not be saved\n", rejfile );
		rejfile = NULL;
	    } else {
		if ( newfile == 0 ) {
		    fputs( "\n", rfp );
		}
		fprintf( rfp, "# Error: %s\n", ldap_err2string( rc ));
		fputs( saved_rbuf, rfp );
		fclose( rfp );
		rfp = NULL;
	    }
	}

	free( rbuf );
	free( saved_rbuf );
    }
    ldaptool_reset_control_array( ldaptool_request_ctrls );

    /* turn off bulk import?*/
    if (bulkimport_suffix) {
	struct berval	bv, *retdata;
	char		*retoid;

	bv.bv_val = "";
	bv.bv_len = 0;
	if ((rc = ldap_extended_operation_s(ld,
	    BULKIMPORT_STOP_OID, &bv, NULL,
	    NULL, &retoid, &retdata)) != 0) {

		fprintf(stderr, "Error: unable to service "
		    "extended operation request\n\t '%s' for "
		    "bulk import\n\t(rc:%d:'%s')\n",
		    BULKIMPORT_STOP_OID, rc, ldap_err2string(rc));
		return (rc);
	}
	if (retoid)
		ldap_memfree(retoid);
	if (retdata)
		ber_bvfree(retdata);
    }

    ldaptool_cleanup( ld );
    return( rc );
}


static void
options_callback( int option, char *optarg )
{
    switch( option ) {
    case 'a':	/* add */
	newval = 1;
	break;
    case 'b':	/* read values from files (for binary attributes) */
	valsfromfiles = 1;
	break;
    case 'A':	/* display non-ASCII values when -v is used */
	display_binary_values = 1;
	break;
    case 'c':	/* continuous operation */
	contoper = 1;
	break;
    case 'F':	/* force all changes records to be used */
	force = 1;
	break;
    case 'e':
	rejfile = strdup( optarg );
	break;
    case 'B':	/* bulk import option */
	bulkimport_suffix = strdup( optarg );
	break;
    case 'q':	/* quiet mode on add/modify operations */
	ldapmodify_quiet = 1;
	break;
    default:
	usage();
    }
}



static int
process_ldif_rec( char *rbuf )
{
    char	*line, *dn, *type, *value, *newrdn, *newparent, *p;
    char	*ctrl_oid=NULL, *ctrl_value=NULL;
    int 	ctrl_criticality=1;
    LDAPControl *ldctrl;
    int		rc, linenum, vlen, modop, replicaport;
    int		expect_modop, expect_sep, expect_chgtype_or_control, expect_newrdn;
    int		expect_deleteoldrdn, expect_newparent, rename, moddn;
    int		deleteoldrdn, saw_replica, use_record, new_entry, delete_entry;
    int         got_all, got_value;
    LDAPMod	**pmods;

    new_entry = newval;

    rc = got_all = saw_replica = delete_entry = expect_modop = 0;
    expect_deleteoldrdn = expect_newrdn = expect_newparent = expect_sep = 0;
    expect_chgtype_or_control = linenum = got_value = rename = moddn = 0;
    deleteoldrdn = 1;
    use_record = force;
    pmods = NULL;
    dn = newrdn = newparent = NULL;

    while ( rc == 0 && ( line = ldif_getline( &rbuf )) != NULL ) {
	++linenum;
	if ( expect_sep && strcasecmp( line, T_MODSEPSTR ) == 0 ) {
	    expect_sep = 0;
	    expect_modop = 1;
	    
	    /*If we see a separator in the input stream,
	     but we didn't get a value from the last modify
	     then we have to fill pmods with an empty value*/
	    if (modop == LDAP_MOD_REPLACE && !got_value){
	      addmodifyop( &pmods, modop, value, NULL, 0);
	    }

	    got_value = 0;
	    continue;
	}
	
	if ( ldif_parse_line( line, &type, &value, &vlen ) < 0 ) {
	    fprintf( stderr, "%s: invalid format (line %d of entry: %s)\n",
		    ldaptool_progname, linenum, dn == NULL ? "" : dn );
	    fprintf( stderr, "%s: line contents: (%s)\n",
		    ldaptool_progname, line );
	    rc = LDAP_PARAM_ERROR;
	    break;
	}

evaluate_line:
	if ( dn == NULL ) {
	    if ( !use_record && strcasecmp( type, T_REPLICA_STR ) == 0 ) {
		++saw_replica;
		if (( p = strchr( value, ':' )) == NULL ) {
		    replicaport = LDAP_PORT;
		} else {
		    *p++ = '\0';
		    replicaport = atoi( p );
		}
		if ( strcasecmp( value, ldaptool_host ) == 0 &&
			replicaport == ldaptool_port ) {
		    use_record = 1;
		}

	    } else if ( strcasecmp( type, T_DN_STR ) == 0 ) {
		if (( dn = strdup( value )) == NULL ) {
		    perror( "strdup" );
		    exit( LDAP_NO_MEMORY );
		}
		expect_chgtype_or_control = 1;

	    } else if ( strcasecmp( type, T_VERSION_STR ) == 0 ) {
		ldif_version = atoi( value );
		if ( ldif_version != LDIF_VERSION_ONE ) {
		    fprintf( stderr, "%s:  LDIF version %d is not supported;"
			" use version: %d\n", ldaptool_progname, ldif_version,
			LDIF_VERSION_ONE );
		    exit( LDAP_PARAM_ERROR );
		}
		if ( ldaptool_verbose ) {
		    printf( "Processing a version %d LDIF file...\n",
			    ldif_version );
		}
		
		/* Now check if there's something left to process   */
		/* and if not, go get the new record, else continue */
		if ( *rbuf == '\0' ) {
			return( 0 );
		}

	    } else if ( !saw_replica ) {
		printf( "%s: skipping change record: no dn: line\n",
			ldaptool_progname );
		return( 0 );
	    }

	    continue; /* skip all lines until we see "dn:" */
	}

	if ( expect_chgtype_or_control ) {
	    expect_chgtype_or_control = 0;
	    if ( !use_record && saw_replica ) {
		printf( "%s: skipping change record for entry: %s\n\t(LDAP host/port does not match replica: lines)\n",
			ldaptool_progname, dn );
		free( dn );
		return( 0 );
	    }

	    if ( strcasecmp( type, "control" ) == 0 ) {
		value = strdup_and_trim( value );
		if (ldaptool_parse_ctrl_arg(value, ' ', &ctrl_oid, 
			&ctrl_criticality, &ctrl_value, &vlen)) {
			    usage();
		}
        	ldctrl = calloc(1,sizeof(LDAPControl));
        	if (ctrl_value) {
        	    rc = ldaptool_berval_from_ldif_value( ctrl_value, vlen,
			 &(ldctrl->ldctl_value),
			 1 /* recognize file URLs */, 0 /* always try file */,
            		 1 /* report errors */ );
        	    if ((rc = ldaptool_fileurlerr2ldaperr( rc )) != LDAP_SUCCESS) {
            		fprintf( stderr, "Unable to parse %s\n", ctrl_value);
            		usage();
        	    }
        	}
        	ldctrl->ldctl_oid = ctrl_oid;
        	ldctrl->ldctl_iscritical = ctrl_criticality;
        	ldaptool_add_control_to_array(ldctrl, ldaptool_request_ctrls);
		expect_chgtype_or_control = 1;
		continue;
	    }

	    if ( strcasecmp( type, T_CHANGETYPESTR ) == 0 ) {
		value = strdup_and_trim( value );
		if ( strcasecmp( value, T_MODIFYCTSTR ) == 0 ) {
		    new_entry = 0;
		    expect_modop = 1;
		} else if ( strcasecmp( value, T_ADDCTSTR ) == 0 ) {
		    new_entry = 1;
		    modop = LDAP_MOD_ADD;
		} else if ( strcasecmp( value, T_MODRDNCTSTR ) == 0 ) {
		    expect_newrdn = 1;
		    moddn = 1;
		} else if ( strcasecmp( value, T_MODDNCTSTR ) == 0 ) {
		    expect_newrdn = 1;
		    moddn = 1;
		} else if ( strcasecmp( value, T_RENAMECTSTR ) == 0 ) {
		    expect_newrdn = 1;
		    rename = 1;
		} else if ( strcasecmp( value, T_DELETECTSTR ) == 0 ) {
		    got_all = delete_entry = 1;
		} else {
		    fprintf( stderr,
			    "%s:  unknown %s \"%s\" (line %d of entry: %s)\n",
			    ldaptool_progname, T_CHANGETYPESTR, value,
			    linenum, dn );
		    rc = LDAP_PARAM_ERROR;
		}
		free( value );
		continue;
	    } else if ( newval ) {		/*  missing changetype => add */
		new_entry = 1;
		modop = LDAP_MOD_ADD;
	    } else {
	      /*The user MUST put in changetype: blah
	       unless adding a new entry with either -a or ldapadd*/
		fprintf(stderr, "%s: Missing changetype operation specification.\n\tThe dn line must be followed by \"changetype: operation\"\n\t(unless ldapmodify is called with -a option)\n\twhere operation is add|delete|modify|modrdn|moddn|rename\n\t\"%s\" is not a valid changetype operation specification\n\t(line %d of entry %s)\n", 
		ldaptool_progname, type, linenum, dn);
		rc = LDAP_PARAM_ERROR;
		/*expect_modop = 1;	 missing changetype => modify */
	    }
	}

	if ( expect_modop ) {
	    expect_modop = 0;
	    expect_sep = 1;
	    if ( strcasecmp( type, T_MODOPADDSTR ) == 0 ) {
		modop = LDAP_MOD_ADD;
		continue;
	    } else if ( strcasecmp( type, T_MODOPREPLACESTR ) == 0 ) {
		modop = LDAP_MOD_REPLACE;
		continue;
	    } else if ( strcasecmp( type, T_MODOPDELETESTR ) == 0 ) {
		modop = LDAP_MOD_DELETE;
		addmodifyop( &pmods, modop, value, NULL, 0 );
		continue;
	    }  else { /*Bug 27479. Remove default add operation*/ 
	      fprintf(stderr, "%s: Invalid parameter \"%s\" specified for changetype modify (line %d of entry %s)\n", 
		      ldaptool_progname, type, linenum, dn);
	      rc = LDAP_PARAM_ERROR;
	    }

	  }

	if ( expect_newrdn ) {
	    if ( strcasecmp( type, T_NEWRDNSTR ) == 0 ) {
		if ( *value == '\0' ) {
		    fprintf( stderr,
			    "%s: newrdn value missing (line %d of entry: %s)\n",
			    ldaptool_progname, linenum, dn == NULL ? "" : dn );
		    rc = LDAP_PARAM_ERROR;
		} else if (( newrdn = strdup( value )) == NULL ) {
		    perror( "strdup" );
		    exit( LDAP_NO_MEMORY );
		} else {
		    expect_newrdn = 0;
		    if ( rename ) {
			expect_newparent = 1;
		    } else {
			expect_deleteoldrdn = 1;
		    }
		}
	    } else {
		fprintf( stderr, "%s: expecting \"%s:\" but saw \"%s:\" (line %d of entry %s)\n",
			ldaptool_progname, T_NEWRDNSTR, type, linenum, dn );
		rc = LDAP_PARAM_ERROR;
	    }
	} else if ( expect_newparent ) {
	    expect_newparent = 0;
	    if ( rename ) {
		expect_deleteoldrdn = 1;
	    }
	    if ( strcasecmp( type, T_NEWPARENTSTR ) == 0
		    || strcasecmp( type, T_NEWSUPERIORSTR ) == 0 ) {
		if (( newparent = strdup( value )) == NULL ) {
		    perror( "strdup" );
		    exit( LDAP_NO_MEMORY );
		}
	    } else {
		/* Since this is an optional argument for rename/moddn, cause
		 * the current line to be re-evaluated if newparent doesn't
		 * follow deleteoldrdn.
		 */
		newparent = NULL;  
		goto evaluate_line;
	    }
	} else if ( expect_deleteoldrdn ) {
	    if ( strcasecmp( type, T_DELETEOLDRDNSTR ) == 0 ) {
		if ( *value == '\0' ) {
		    fprintf( stderr,
			    "%s: missing 0 or 1 (line %d of entry: %s)\n",
			    ldaptool_progname, linenum, dn == NULL ? "" : dn );
		    rc = LDAP_PARAM_ERROR;
		} else {
		    deleteoldrdn = ( *value == '0' ) ? 0 : 1;
		    expect_deleteoldrdn = 0;
		    if ( moddn ) {
			expect_newparent = 1;
		    }
		}
	    } else {
		fprintf( stderr, "%s: expecting \"%s:\" but saw \"%s:\" (line %d of entry %s)\n",
			ldaptool_progname, T_DELETEOLDRDNSTR, type, linenum,
			dn );
		rc = LDAP_PARAM_ERROR;
	    }
	    got_all = 1;
	} else if ( got_all ) {
	    fprintf( stderr,
		    "%s: extra lines at end (line %d of entry %s)\n",
		    ldaptool_progname, linenum, dn );
	    rc = LDAP_PARAM_ERROR;
	    got_all = 1;
	} else {
	    addmodifyop( &pmods, modop, type, value, vlen );
	    /*There was a value to replace*/
	    got_value = 1;

	}
    }

    if ( rc == 0 ) {
	if ( delete_entry ) {
	    rc = dodelete( dn );
	} else if ( newrdn != NULL ) {
	    rc = dorename( dn, newrdn, newparent, deleteoldrdn );
	    rename = 0;
	} else {

	  /*Patch to fix Bug 22183
	    If pmods is null, then there is no
	    attribute to replace, so we alloc
	    an empty pmods*/
	  if (modop == LDAP_MOD_REPLACE && !got_value && expect_sep){
	    addmodifyop( &pmods, modop, value, NULL, 0);
	  }/*End Patch*/
	  
	  
	  rc = domodify( dn, pmods, new_entry );
	}

	if ( rc == LDAP_SUCCESS ) {
	    rc = 0;
	}
    }

    if ( dn != NULL ) {
	free( dn );
    }
    if ( newrdn != NULL ) {
	free( newrdn );
    }
    if ( newparent != NULL ) {
	free( newparent );
    }
    if ( pmods != NULL ) {
	freepmods( pmods );
    }

    return( rc );
}


static int
process_ldapmod_rec( char *rbuf )
{
    char	*line, *dn, *p, *q, *attr, *value;
    int		rc, linenum, modop;
    LDAPMod	**pmods;

    pmods = NULL;
    dn = NULL;
    linenum = 0;
    line = rbuf;
    rc = 0;

    while ( rc == 0 && rbuf != NULL && *rbuf != '\0' ) {
	++linenum;
	if (( p = strchr( rbuf, '\n' )) == NULL ) {
	    rbuf = NULL;
	} else {
	    if ( *(p-1) == '\\' ) {	/* lines ending in '\' are continued */
		strcpy( p - 1, p );
		rbuf = p;
		continue;
	    }
	    *p++ = '\0';
	    rbuf = p;
	}

	if ( dn == NULL ) {	/* first line contains DN */
	    if (( dn = strdup( line )) == NULL ) {
		perror( "strdup" );
		exit( LDAP_NO_MEMORY );
	    }
	} else {
	    if (( p = strchr( line, '=' )) == NULL ) {
		value = NULL;
		p = line + strlen( line );
	    } else {
		*p++ = '\0';
		value = p;
	    }

	    for ( attr = line; *attr != '\0' && isspace( *attr ); ++attr ) {
		;	/* skip attribute leading white space */
	    }

	    for ( q = p - 1; q > attr && isspace( *q ); --q ) {
		*q = '\0';	/* remove attribute trailing white space */
	    }

	    if ( value != NULL ) {
		while ( isspace( *value )) {
		    ++value;		/* skip value leading white space */
		}
		for ( q = value + strlen( value ) - 1; q > value &&
			isspace( *q ); --q ) {
		    *q = '\0';	/* remove value trailing white space */
		}
		if ( *value == '\0' ) {
		    value = NULL;
		}

	    }

	    if ( value == NULL && newval ) {
		fprintf( stderr, "%s: missing value on line %d (attr is %s)\n",
			ldaptool_progname, linenum, attr );
		rc = LDAP_PARAM_ERROR;
	    } else {
		 switch ( *attr ) {
		case '-':
		    modop = LDAP_MOD_DELETE;
		    ++attr;
		    break;
		case '+':
		    modop = LDAP_MOD_ADD;
		    ++attr;
		    break;
		default:
		    /*Bug 27479. Remove the add default*/
		      fprintf(stderr, "%s: Invalid parameter specified for changetype modify (line %d of entry %s)\n", 
		      ldaptool_progname, linenum, dn);
		      rc = LDAP_PARAM_ERROR;
		}

		addmodifyop( &pmods, modop, attr, value,
			( value == NULL ) ? 0 : strlen( value ));
	    }
	}

	line = rbuf;
    }

    if ( rc == 0 ) {
	if ( dn == NULL ) {
	    rc = LDAP_PARAM_ERROR;
	} else if (( rc = domodify( dn, pmods, newval )) == LDAP_SUCCESS ){
	  rc = 0;
	}
      }
    
    if ( pmods != NULL ) {
	freepmods( pmods );
    }
    if ( dn != NULL ) {
	free( dn );
    }

    return( rc );
}


static void
addmodifyop( LDAPMod ***pmodsp, int modop, char *attr, char *value, int vlen )
{
    LDAPMod		**pmods;
    int			i, j, rc;
    struct berval	*bvp;

    pmods = *pmodsp;
    modop |= LDAP_MOD_BVALUES;

    i = 0;
    if ( pmods != NULL ) {
	for ( ; pmods[ i ] != NULL; ++i ) {
	    if ( strcasecmp( pmods[ i ]->mod_type, attr ) == 0 &&
		    pmods[ i ]->mod_op == modop ) {
		break;
	    }
	}
    }

    if ( pmods == NULL || pmods[ i ] == NULL ) {
	if (( pmods = (LDAPMod **)LDAPTOOL_SAFEREALLOC( pmods, (i + 2) *
		sizeof( LDAPMod * ))) == NULL ) {
	    perror( "realloc" );
	    exit( LDAP_NO_MEMORY );
	}
	*pmodsp = pmods;
	pmods[ i + 1 ] = NULL;
	if (( pmods[ i ] = (LDAPMod *)calloc( 1, sizeof( LDAPMod )))
		== NULL ) {
	    perror( "calloc" );
	    exit( LDAP_NO_MEMORY );
	}
	pmods[ i ]->mod_op = modop;
	if (( pmods[ i ]->mod_type = strdup( attr )) == NULL ) {
	    perror( "strdup" );
	    exit( LDAP_NO_MEMORY );
	}
    }

    if ( value != NULL ) {
	j = 0;
	if ( pmods[ i ]->mod_bvalues != NULL ) {
	    for ( ; pmods[ i ]->mod_bvalues[ j ] != NULL; ++j ) {
		;
	    }
	}
	if (( pmods[ i ]->mod_bvalues = (struct berval **)
		LDAPTOOL_SAFEREALLOC( pmods[ i ]->mod_bvalues,
		(j + 2) * sizeof( struct berval * ))) == NULL ) {
	    perror( "realloc" );
	    exit( LDAP_NO_MEMORY );
	}
	pmods[ i ]->mod_bvalues[ j + 1 ] = NULL;
	if (( bvp = (struct berval *)malloc( sizeof( struct berval )))
		== NULL ) {
	    perror( "malloc" );
	    exit( LDAP_NO_MEMORY );
	}
	pmods[ i ]->mod_bvalues[ j ] = bvp;

#ifdef notdef 
	if (ldaptool_verbose) {
		printf("%s: value: %s vlen: %d\n", "ldapmodify", value, vlen);
	}
#endif
	rc = ldaptool_berval_from_ldif_value( value, vlen, bvp,
		    ( ldif_version >= LDIF_VERSION_ONE ), valsfromfiles,
			1 /* report errors */ );
	if ( rc != LDAPTOOL_FILEURL_SUCCESS ) {
	    exit( ldaptool_fileurlerr2ldaperr( rc ));
	}
    }
}


static int
domodify( char *dn, LDAPMod **pmods, int newentry )
{
    int			i, j, notascii, op;
    struct berval	*bvp;

    if ( pmods == NULL ) {
	fprintf( stderr, "%s: no attributes to change or add (entry %s)\n",
		ldaptool_progname, dn );
	return( LDAP_PARAM_ERROR );
    }

    if ( ldaptool_verbose ) {
	for ( i = 0; pmods[ i ] != NULL; ++i ) {
	    op = pmods[ i ]->mod_op & ~LDAP_MOD_BVALUES;
	    printf( "%s %s:\n", op == LDAP_MOD_REPLACE ?
		    "replace" : op == LDAP_MOD_ADD ?
		    "add" : "delete", pmods[ i ]->mod_type );
	    if ( pmods[ i ]->mod_bvalues != NULL ) {
		for ( j = 0; pmods[ i ]->mod_bvalues[ j ] != NULL; ++j ) {
		    bvp = pmods[ i ]->mod_bvalues[ j ];
		    notascii = 0;
		    if ( !display_binary_values ) {
			notascii = !ldaptool_berval_is_ascii( bvp );
		    }
		    if ( notascii ) {
			printf( "\tNOT ASCII (%ld bytes)\n", bvp->bv_len );
		    } else {
			printf( "\t%s\n", bvp->bv_val );
		    }
		}
	    }
	}
    }

    if ( !ldapmodify_quiet) {
	if ( newentry ) {
	    printf( "%sadding new entry %s\n",
		ldaptool_not ? "!" : "", dn );
	} else {
	    printf( "%smodifying entry %s\n",
		ldaptool_not ? "!" : "", dn );
	}
    }

    if ( !ldaptool_not ) {
	if ( newentry ) {
	unsigned int	sleep_interval = 2; /* seconds */

	    while ((i = ldaptool_add_ext_s( ld, dn, pmods,
			ldaptool_request_ctrls, NULL, "ldap_add" ))
			== LDAP_BUSY) {
		if ( sleep_interval > 3600 ) {
			printf("ldap_add: Unable to complete request.  ");
			printf("Server is too ");
			printf("busy servicing other requests\n");
			break;
		}
		if ( !ldapmodify_quiet ) {
			printf("ldap_add: LDAP_BUSY returned by server.  ");
			printf("Will retry operation ");
			printf("in %d seconds\n", sleep_interval); 
		}
		sleep( sleep_interval );
		sleep_interval *= 2;
	    }
	} else {
	    i = ldaptool_modify_ext_s( ld, dn, pmods, ldaptool_request_ctrls,
		    NULL, "ldap_modify" );
	}
	if ( i == LDAP_SUCCESS && ldaptool_verbose ) {
	    printf( "modify complete\n" );
	}
    } else {
	i = LDAP_SUCCESS;
    }

    if ( !ldapmodify_quiet) {
	putchar( '\n' );
    }

    return( i );
}


static int
dodelete( char *dn )
{
    int	rc;

    printf( "%sdeleting entry %s\n", ldaptool_not ? "!" : "", dn );
    if ( !ldaptool_not ) {
	if (( rc = ldaptool_delete_ext_s( ld, dn, ldaptool_request_ctrls,
		NULL, "ldap_delete" )) == LDAP_SUCCESS && ldaptool_verbose ) {
	    printf( "delete complete" );
	}
    } else {
	rc = LDAP_SUCCESS;
    }

    putchar( '\n' );

    return( rc );
}


static int
dorename( char *dn, char *newrdn, char *newparent, int deleteoldrdn )
{
    int	rc;

    if ( ldaptool_verbose ) {
	if ( newparent == NULL ) {
	    printf( "new RDN: %s (%skeep existing values)\n",
		    newrdn, deleteoldrdn ? "do not " : "" );
	} else {
	    printf( "new RDN: %s, new parent %s (%skeep existing values)\n",
		    newrdn, newparent, deleteoldrdn ? "do not " : "" );
	}
    }

    printf( "%smodifying RDN of entry %s%s\n",
	    ldaptool_not ? "!" : "", dn, ( newparent == NULL ) ? "" :
	    " and/or moving it beneath a new parent\n" );

    if ( !ldaptool_not ) {
	if (( rc = ldaptool_rename_s( ld, dn, newrdn, newparent, deleteoldrdn,
		ldaptool_request_ctrls, NULL, "ldap_rename" )) == LDAP_SUCCESS
		&& ldaptool_verbose ) {
	    printf( "rename completed\n" );
	}
    } else {
	rc = LDAP_SUCCESS;
    }

    putchar( '\n' );

    return( rc );
}


static void
freepmods( LDAPMod **pmods )
{
    int	i;

    for ( i = 0; pmods[ i ] != NULL; ++i ) {
	if ( pmods[ i ]->mod_bvalues != NULL ) {
	    ber_bvecfree( pmods[ i ]->mod_bvalues );
	}
	if ( pmods[ i ]->mod_type != NULL ) {
	    free( pmods[ i ]->mod_type );
	}
	free( pmods[ i ] );
    }
    free( pmods );
}


static char *
read_one_record( FILE *fp )
{
    int         len, gotnothing;
    char        *buf, line[ LDAPMOD_MAXLINE ];
    int		lcur, lmax;

    lcur = lmax = 0;
    buf = NULL;
    gotnothing = 1;

    while ( fgets( line, sizeof(line), fp ) != NULL ) {
	if ( (len = strlen( line )) < 2 ) {
	    if ( gotnothing ) {
		continue;
	    } else {
		break;
	    }
	}

	/* Check if the blank line starts with '\r' (CR) */
	if ( ((len = strlen( line )) == 2) && (line[0] == '\r') ) {
	    if ( gotnothing ) {
		continue;
	    } else {
		break; 
	      }
	}

	if ( *line == '#' ) {
	    continue;			/* skip comment lines */
	}

	gotnothing = 0;
        if ( lcur + len + 1 > lmax ) {
            lmax = LDAPMOD_MAXLINE
		    * (( lcur + len + 1 ) / LDAPMOD_MAXLINE + 1 );
	    if (( buf = (char *)LDAPTOOL_SAFEREALLOC( buf, lmax )) == NULL ) {
		perror( "realloc" );
		exit( LDAP_NO_MEMORY );
	    }
        }
        strcpy( buf + lcur, line );
        lcur += len;
    }

    return( buf );
}


/*
 * strdup and trim trailing blanks
 */
static char *
strdup_and_trim( char *s )
{
    char	*p;

    if (( s = strdup( s )) == NULL ) {
	perror( "strdup" );
	exit( LDAP_NO_MEMORY );
    }

    p = s + strlen( s ) - 1;
    while ( p >= s && isspace( *p )) {
	--p;
    }
    *++p = '\0';

    return( s );
}
