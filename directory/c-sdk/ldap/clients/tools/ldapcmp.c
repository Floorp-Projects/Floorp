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

/* tool to compare the contents of two LDAP directory subtrees */

#include "ldaptool.h"

typedef struct attr {
    char *name;
    char **vals;
    struct attr *next;
} ATTR;			/* used for comparing two entries */

static void options_callback( int option, char *optarg );
static int docompare( LDAP *ld1, LDAP *ld2, char *base );
static int cmp2(LDAP *ld1, LDAP *ld2, LDAPMessage *e1, int findonly );
static void notfound(char *base, int dbaseno);
ATTR* get_attrs( LDAP *ld, LDAPMessage *e );
char* cmp_attrs( ATTR *a1, ATTR *a2 );
static void attr_free(ATTR *at);
#if 0 /* these functions are not used */
static void print_entry( LDAP *ld, LDAPMessage *entry, int attrsonly );
static void print_dn( LDAP *ld, LDAPMessage *entry );
static int write_ldif_value( char *type, char *value, unsigned long vallen );
#endif /* 0 */

static void
usage( void )
{
    fprintf( stderr,
	    "usage: %s -b basedn [options] [attributes...]\nwhere:\n",
	    ldaptool_progname );
    fprintf( stderr, "    basedn\tbase dn for search\n" );
    fprintf( stderr, "\t\t(if the environment variable LDAP_BASEDN is set,\n" );
    fprintf( stderr, "\t\tthen the -b flag is not required)\n" );
    fprintf( stderr, "options:\n" );
    fprintf( stderr, "    -s scope\tone of base, one, or sub (default is sub)\n" );
    ldaptool_common_usage( 1 );
    exit( LDAP_PARAM_ERROR );
}

static char	*base = NULL;
static int	 allow_binary, vals2tmp, ldif, scope, deref, differ=0;
static int	attrsonly, timelimit, sizelimit;
#if 0 /* these statics are referenced only by unused functions */
static char	*sep = LDAPTOOL_DEFSEP;
static char	**sortattr = NULL;
static int	*skipsortattr = NULL;
static int	includeufn;
#endif /* 0 */


int
main( int argc, char **argv )
{
    int			rc, optind;
    LDAP		*ld1, *ld2;

#ifdef notdef
#ifdef HPUX11
#ifndef __LP64__
	_main( argc, argv);
#endif /* __LP64_ */
#endif /* HPUX11 */
#endif

    deref = LDAP_DEREF_NEVER;
    allow_binary = vals2tmp = attrsonly = 0;
    ldif = 1;
    sizelimit = timelimit = 0;
    scope = LDAP_SCOPE_SUBTREE;

    optind = ldaptool_process_args( argc, argv, "Bb:l:s:z:", 0,
	    options_callback );

    if ( optind == -1 ) {
	usage();
    }

    if ( base == NULL ) {
	if (( base = getenv( "LDAP_BASEDN" )) == NULL ) {
	    usage();
	}
    }

    ld1 = ldaptool_ldap_init( 0 );

    ldap_set_option( ld1, LDAP_OPT_DEREF, &deref );
    ldap_set_option( ld1, LDAP_OPT_TIMELIMIT, &timelimit );
    ldap_set_option( ld1, LDAP_OPT_SIZELIMIT, &sizelimit );

    ldaptool_bind( ld1 );

    ld2 = ldaptool_ldap_init( 1 );

    ldap_set_option( ld2, LDAP_OPT_DEREF, &deref );
    ldap_set_option( ld2, LDAP_OPT_TIMELIMIT, &timelimit );
    ldap_set_option( ld2, LDAP_OPT_SIZELIMIT, &sizelimit );

    ldaptool_bind( ld2 );
    if ( ldaptool_verbose ) {
	printf( "Connections to servers established.  Beginning comparison.\n" );
    }

    rc = docompare( ld1, ld2, base );

    ldaptool_cleanup( ld1 );
    ldaptool_cleanup( ld2 );
    if ( ldaptool_verbose && !rc ) {
	if ( !differ ) {
	    printf( "compare completed: no differences found\n" );
	} else {
	    printf( "compare completed: ****differences were found****\n" );
	}
    }
    return( rc );
}


static void
options_callback( int option, char *optarg )
{
    switch( option ) {
    case 'B':	/* allow binary values to be printed, even if -o used */
	++allow_binary;
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
    case 'b':	/* searchbase */
	base = strdup( optarg );
	break;
    case 'l':	/* time limit */
	timelimit = atoi( optarg );
	break;
    case 'z':	/* size limit */
	sizelimit = atoi( optarg );
	break;
    default:
	usage();
	break;
    }
}


/*
 * Returns an LDAP error code.
 */
static int
docompare( LDAP *ld1, LDAP *ld2, char *base )
{
    int			rc, msgid;
    LDAPMessage		*res, *e;
    LDAPControl		*ctrls[2], **serverctrls;

    if ( ldaptool_verbose ) {
        printf( "Base: %s\n\n", base );
    }
    if ( ldaptool_not ) {
	return( LDAP_SUCCESS );
    }

    if (( ctrls[0] = ldaptool_create_manage_dsait_control()) != NULL ) {
	ctrls[1] = NULL;
	serverctrls = ctrls;
    } else {
	serverctrls = NULL;
    }

    if ( ldap_search_ext( ld1, base, scope, "objectClass=*", NULL,
	    0, serverctrls, NULL, NULL, -1, &msgid ) != LDAP_SUCCESS ) {
	return( ldaptool_print_lderror( ld1, "ldap_search",
	    LDAPTOOL_CHECK4SSL_IF_APPROP ));
    }
/* XXXmcs: this code should be modified to display referrals and references */
    while ( (rc = ldap_result( ld1, LDAP_RES_ANY, 0, NULL, &res )) == 
        LDAP_RES_SEARCH_ENTRY ) {
        e = ldap_first_entry( ld1, res );
        rc = cmp2( ld1, ld2, e , 0);
        ldap_msgfree( res );
    }
    if ( rc == -1 ) {
	return( ldaptool_print_lderror( ld1, "ldap_result",
		LDAPTOOL_CHECK4SSL_IF_APPROP ));
    }
    if (( rc = ldap_result2error( ld1, res, 0 )) != LDAP_SUCCESS ) {
        ldaptool_print_lderror( ld1, "ldap_search",
		LDAPTOOL_CHECK4SSL_IF_APPROP );
    }
    ldap_msgfree( res );

    if ( ldap_search_ext( ld2, base, scope, "objectClass=*", NULL,
	    0, serverctrls, NULL, NULL, -1, &msgid  ) == -1 ) {
	return( ldaptool_print_lderror( ld2, "ldap_search",
		LDAPTOOL_CHECK4SSL_IF_APPROP ));
    }
/* XXXmcs: this code should be modified to display referrals and references */
    while ( (rc = ldap_result( ld2, LDAP_RES_ANY, 0, NULL, &res )) == 
	    LDAP_RES_SEARCH_ENTRY ) {
	e = ldap_first_entry( ld2, res );
        rc = cmp2( ld2, ld1, e , 1);
        ldap_msgfree( res );
    }
    if ( rc == -1 ) {
	return( ldaptool_print_lderror( ld2, "ldap_result",
		LDAPTOOL_CHECK4SSL_IF_APPROP ));
    }
    if (( rc = ldap_result2error( ld1, res, 0 )) != LDAP_SUCCESS ) {
        ldaptool_print_lderror( ld1, "ldap_search",
		LDAPTOOL_CHECK4SSL_IF_APPROP );
    }
    ldap_msgfree( res );

    return( rc );
}


/*
 * Returns an LDAP error code.
 */
static int
cmp2( LDAP *ld1, LDAP *ld2, LDAPMessage *e1, int findonly)
{
    LDAPMessage		*e2, *res;
    char		*dn, *attrcmp;
    int			found=0, rc, msgid;
    ATTR		*a1, *a2;

    dn = ldap_get_dn( ld1, e1 );

    if ( ldaptool_verbose ) {
	if ( findonly ) {
	    printf( "Checking that %s exists on both servers\n", dn );
	} else {
	    printf("Comparing entry %s on both servers\n", dn );
	}
    }

    if ( ldap_search( ld2, dn, LDAP_SCOPE_BASE, "objectClass=*", NULL, 0 ) == -1 ) {
        return( ldaptool_print_lderror( ld2, "ldap_search",
		LDAPTOOL_CHECK4SSL_IF_APPROP ));
    }
/* XXXmcs: this code should be modified to display referrals and references */
    while ( (rc = ldap_result( ld2, LDAP_RES_ANY, 0, NULL, &res )) == 
        LDAP_RES_SEARCH_ENTRY ) {
        e2 = ldap_first_entry( ld1, res );
	found = 1;
	if ( !findonly ) {
	    a1 = get_attrs( ld1, e1 );
	    a2 = get_attrs( ld2, e2 );
	    attrcmp = cmp_attrs( a1, a2 );
	    if ( strcmp( attrcmp, "") != 0 ) {
	        printf("\n%s%s\n", dn, attrcmp);
	    }
	}
        ldap_msgfree( res );
    }
    if ( !found ) {
	notfound( dn, findonly );
	differ = 1;
    }
    if ( rc == -1 ) {
        return( ldaptool_print_lderror( ld2, "ldap_result",
		LDAPTOOL_CHECK4SSL_IF_APPROP ));
    }
    ldap_msgfree( res );
    ldap_memfree( dn );
    return(rc);
}


ATTR*
get_attrs( LDAP *ld, LDAPMessage *e )
{
    char		*a;
    ATTR		*head, *tail, *tmp;
    BerElement		*ber;

    head=tail=tmp=NULL;
    for ( a = ldap_first_attribute( ld, e, &ber ); a != NULL; 
         a = ldap_next_attribute( ld, e, ber ) ) {
        tmp = (ATTR*)malloc(sizeof(ATTR));
	if(head == NULL)
	    head = tail = tmp;
	else {
	    tail->next = tmp;
	    tail = tmp;
	}
	tmp->name = a;
	tmp->vals = ldap_get_values( ld, e, a );
	tmp->next = NULL;
    }
   if ( ber != NULL ) {
      ber_free( ber, 0 );
   }
   /*   used for debugging
   tmp=head;
   while(tmp!= NULL) {
       printf("\n%s :", tmp->name);
       for(i=0; tmp->vals[i] != NULL; i++)
	   printf("\n\t%d  %s", i, tmp->vals[i]);
       tmp = tmp->next;
       }
   */
   return(head);
}


char*
cmp_attrs( ATTR *a1, ATTR *a2 )
{
    static char result[5000];
    char res[1000], partial[1000], *name = "";
    ATTR *head1, *head2, *tmp, *prev, *start;
    int i, j, found;

    head1 = a1;
    head2 = a2;
    tmp = a2;
    prev = NULL;
    strcpy(result, "");
    while(head1 != NULL) {
        name = head1->name;
	if(head2 == NULL) {
	    while(head1 != NULL) {
	        sprintf(partial, "\ndifferent: %s(*)", head1->name);
		strcat(result, partial);
		for(i=0; head1->vals[i] != NULL; i++) {
		    sprintf(partial,"\n\t1: %s", head1->vals[i]);
		    strcat(result, partial);
		}
		tmp = head1;
		head1 = head1->next;
		attr_free(tmp);
	    }
	    differ = 1;
	    break;
	}
        name = head1->name;
	start = tmp;
	while(tmp != NULL) {
	    if(!strcmp(name, tmp->name)) {	/* attr found  */
	        strcpy(res, "");
	        for(i=0; (head1->vals[i]) != NULL; i++) {
		    found = 0;
		    for(j=0; (tmp->vals[j]) != NULL; j++)
		        if(!strcmp(head1->vals[i], tmp->vals[j])) {
			    found = 1;
			    tmp->vals[j][0] = 7;
			    break;
			}
		    if(!found) {
		        sprintf(partial, "\n\t1: %s", head1->vals[i]);
			strcat(res, partial);
		    }
		}
	        for(j=0; tmp->vals[j] != NULL; j++)
		  if(tmp->vals[j][0] != 7){
		        sprintf(partial, "\n\t2: %s", tmp->vals[j]);
			strcat(res, partial);
		    }

		if(strcmp(res, "")) {
		    sprintf(partial, "\ndifferent: %s%s", name, res);
		    differ = 1;
		    strcat(result, partial);
		}
		if(prev == NULL) {		/* tmp = head2 */
		    head2 = head2->next;
		    attr_free(tmp);
		    tmp = head2;
		}
	        else {
		    prev->next = tmp->next;
		    attr_free(tmp);
		    tmp = prev->next;
		    if(tmp == NULL) {
		        tmp = head2;
			prev = NULL;
		    }
		}
		break;
	    }
	    else {		        /* attr not found */
	        if(prev == NULL)
		    prev = head2;
		else
		    prev = tmp;
		tmp = tmp->next;
	        if(tmp == NULL)	{     	/* end of list */
		    tmp = head2;
		    prev = NULL;
		}
		if(tmp == start) {	/* attr !exist in 2 */
		    sprintf(partial, "\ndifferent: %s(*)", name);
		    differ = 1;
		    strcat(result, partial);
		    for(i=0; head1->vals[i] != NULL; i++) {
		        sprintf(partial, "\n\t1: %s", head1->vals[i]);
			strcat(result, partial);
		    }
		    break;
		}    
	    }
	}
	start = head1;
	head1 = head1->next;
	attr_free(start);
    }
    while(head2 != NULL) {
        sprintf(partial, "\ndifferent: %s(*)", head2->name);
	differ = 1;
       	strcat(result, partial);
       	for(i=0; head2->vals[i] != NULL; i++) {
       	    sprintf(partial, "\n\t2: %s", head2->vals[i]);
       	    strcat(result, partial);
       	}
	tmp = head2;
       	head2 = head2->next;
       	attr_free(tmp);
    }
    return(result);
}


static void
attr_free(ATTR *at)
{
    ldap_memfree(at->name);
    ldap_value_free(at->vals);
    free(at);
}


static void
notfound(char *base, int dbaseno)
{
    printf("%donly: %s\n", dbaseno+1, base);
}


#if 0	/* these function is not used */
/* used for debugging */
static void
print_dn( LDAP *ld, LDAPMessage *entry )
{
    char		*dn, *ufn;

    dn = ldap_get_dn( ld, entry );
    if ( ldif ) {
	write_ldif_value( "dn", dn, strlen( dn ));
    } else {
	printf( "%s\n", dn );
    }
    if ( includeufn ) {
	ufn = ldap_dn2ufn( dn );
	if ( ldif ) {
	    write_ldif_value( "ufn", ufn, strlen( ufn ));
	} else {
	    printf( "%s\n", ufn );
	}
	free( ufn );
    }
    ldap_memfree( dn );
}


static void
print_entry( ld, entry, attrsonly )
    LDAP	*ld;
    LDAPMessage	*entry;
    int		attrsonly;
{
    char		*a, *dn, *ufn, tmpfname[ 256 ];
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
    if ( ldif ) {
	write_ldif_value( "dn", dn, strlen( dn ));
    } else {
	printf( "%s\n", dn );
    }
    if ( includeufn ) {
	ufn = ldap_dn2ufn( dn );
	if ( ldif ) {
	    write_ldif_value( "ufn", ufn, strlen( ufn ));
	} else {
	    printf( "%s\n", ufn );
	}
	free( ufn );
    }
    ldap_memfree( dn );

    for ( a = ldap_first_attribute( ld, entry, &ber ); a != NULL;
	    a = ldap_next_attribute( ld, entry, ber ) ) {
	if ( ldap_charray_inlist(sortattr, a) &&            /* in the list*/
	    skipsortattr[ldap_charray_position(sortattr, a)] ) {/* and skip it*/
	    continue;                                       /* so skip it! */
	}
	if ( attrsonly ) {
	    if ( ldif ) {
		write_ldif_value( a, "", 0 );
	    } else {
		printf( "%s\n", a );
	    }
	} else if (( bvals = ldap_get_values_len( ld, entry, a )) != NULL ) {
	    for ( i = 0; bvals[i] != NULL; i++ ) {
		if ( vals2tmp ) {
		    sprintf( tmpfname, "%s/ldapcmp-%s-XXXXXX",
			    ldaptool_get_tmp_dir(), a );
		    tmpfp = NULL;

		    if ( mktemp( tmpfname ) == NULL ) {
			perror( tmpfname );
		    } else if (( tmpfp = fopen( tmpfname, mode)) == NULL ) {
			perror( tmpfname );
		    } else if ( fwrite( bvals[ i ]->bv_val,
			    bvals[ i ]->bv_len, 1, tmpfp ) == 0 ) {
			perror( tmpfname );
		    } else if ( ldif ) {
			write_ldif_value( a, tmpfname, strlen( tmpfname ));
		    } else {
			printf( "%s%s%s\n", a, sep, tmpfname );
		    }

		    if ( tmpfp != NULL ) {
			fclose( tmpfp );
		    }
		} else {
		    notascii = 0;
		    if ( !ldif && !allow_binary ) {
			notascii = !ldaptool_berval_is_ascii( bvals[ i ] );
		    }

		    if ( ldif ) {
			write_ldif_value( a, bvals[ i ]->bv_val,
				bvals[ i ]->bv_len );
		    } else {
			printf( "%s%s%s\n", a, sep,
				notascii ? "NOT ASCII" : bvals[ i ]->bv_val );
		    }
		}
	    }
	    ber_bvecfree( bvals );
	}
    }
    if ( ber != NULL ) {
	ber_free( ber, 0 );
    }
}


static int
write_ldif_value( char *type, char *value, unsigned long vallen )
{
    char	*ldif;

    if (( ldif = ldif_type_and_value( type, value, (int)vallen )) == NULL ) {
	return( -1 );
    }

    fputs( ldif, stdout );
    free( ldif );

    return( 0 );
}
#endif /* 0 */
