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
 * The Original Code is Sun LDAP C SDK.
 *
 * The Initial Developer of the Original Code is Sun Microsystems, Inc.
 *
 * Portions created by Sun Microsystems, Inc are Copyright (C) 2005
 * Sun Microsystems, Inc. All Rights Reserved.
 *
 * Contributor(s): abobrov
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
 * ldappasswd.c - generic program to change LDAP users password
 *                by using password modify extended operation.
 */

#include <__version.h>

#include "ldaptool.h"
#include "fileurl.h"

static int				prompt_old_password = 0;
static int				prompt_new_password = 0;
static int				is_file_old = 0;
static int				is_file_new = 0;
static FILE				*old_password_fp = NULL;
static FILE				*new_password_fp = NULL;
static char				*old_password_string = "Old Password: ";
static char				*new_password_string = "New Password: ";
static char				*re_new_password_string = "Re-enter new Password: ";
static struct berval	genpasswd = { 0, NULL };
static struct berval	oldpasswd = { 0, NULL };
static struct berval	newpasswd = { 0, NULL };
static struct berval	userid = { 0, NULL };

static void usage( void );
static void options_callback( int option, char *optarg );

static void
usage( void )
{
	fprintf( stderr, "usage: %s [options] [user]\n", ldaptool_progname );
	fprintf( stderr, "where:\n" );
	fprintf( stderr, "    user\tauthentication id\n" );
	fprintf( stderr, "        \te.g, uid=bjensen,dc=example,dc=com\n" );
	fprintf( stderr, "options:\n" );
	ldaptool_common_usage( 0 );
	fprintf( stderr, "    -a passwd\told password\n" );
	fprintf( stderr, "    -A\t\tprompt for old password\n" );
	fprintf( stderr, "    -t file\tread old password from 'file'\n" );
	fprintf( stderr, "    -s passwd\tnew password\n" );
	fprintf( stderr, "    -S\t\tprompt for new password\n" );
	fprintf( stderr, "    -T file\tread new password from 'file'\n" );
	exit( LDAP_PARAM_ERROR );
}

int
main( int argc, char **argv )
{
	int	optind;
	int	rc = LDAP_SUCCESS; /* being superoptimistic for -n */
	LDAP	*ld;
		
#ifdef notdef
#ifdef HPUX11
#ifndef __LP64__
	_main( argc, argv);
#endif /* __LP64_ */
#endif /* HPUX11 */
#endif

	optind = ldaptool_process_args( argc, argv, "ASa:t:s:T:", 0, options_callback );
	
	if ( (optind == -1) || (argc <= 1) ) {
		usage();
	}
	if ( (argc - optind) >= 1 ) {
		if ( argv[ optind ] ) {
			if ( (userid.bv_val = ldaptool_local2UTF8(argv[ optind ]) ) == NULL ) {
				fprintf( stderr, "%s: not enough memory\n", ldaptool_progname );
				exit( LDAP_NO_MEMORY );
			}
			userid.bv_len = strlen( userid.bv_val );
			++optind;
		}
	}
	
	ld = ldaptool_ldap_init( 0 );
	ldaptool_bind( ld );
	
	if ( ldaptool_nobind && (userid.bv_val == NULL) && (userid.bv_len == 0) ) {
		usage();
	}
		
	if ( !ldaptool_not ) {
		rc = ldap_passwd_s( ld, userid.bv_val ? &userid : NULL, 
								oldpasswd.bv_val ? &oldpasswd : NULL, 
								newpasswd.bv_val ? &newpasswd : NULL, 
								&genpasswd, NULL, NULL );
		if ( rc != LDAP_SUCCESS ) {
			fprintf( stderr, "ldap_passwd_s: %s\n",
					 ldap_err2string( rc ) );
		} else {
			fprintf( stderr, "%s: password successfully changed\n", 
					 ldaptool_progname );
		}
	
		if ( (genpasswd.bv_val != NULL) && (genpasswd.bv_len != 0) ) {
			fprintf( stderr, "New password: %s\n", genpasswd.bv_val );
		}
	}

	ldaptool_cleanup( ld );
	
	return( rc );
}

static void
options_callback( int option, char *optarg )
{	
	char	*old_passwd = NULL;
	char	*new_passwd = NULL;
	char	*re_newpasswd = NULL;
	
    switch( option ) {
	case 'a':	/* old password */
		old_passwd = strdup( optarg );
		if (NULL == old_passwd)
		{
			perror("malloc");
			exit( LDAP_NO_MEMORY );
		}
		break;
	case 'A':	/* prompt old password */
		prompt_old_password = 1;
		break;
	case 't':	/* old password from file */
		if ((old_password_fp = fopen( optarg, "r" )) == NULL ) {
			fprintf(stderr, "%s: Unable to open '%s' file\n",
					ldaptool_progname, optarg);
			exit( LDAP_PARAM_ERROR );
		}
		is_file_old = 1;
		break;
	case 's':	/* new password */
		new_passwd = strdup( optarg );
		if (NULL == new_passwd)
		{
			perror("malloc");
			exit( LDAP_NO_MEMORY );
		}		
		break;
	case 'S':	/* prompt new password */
		prompt_new_password = 1;
		break;
	case 'T':	/* new password from file */
		if ((new_password_fp = fopen( optarg, "r" )) == NULL ) {
			fprintf(stderr, "%s: Unable to open '%s' file\n",
						ldaptool_progname, optarg);
			exit( LDAP_PARAM_ERROR );
		}
		is_file_new = 1;			
		break;
	default:
		usage();
		break;
    }

	if ( (oldpasswd.bv_val == NULL) && (oldpasswd.bv_len == 0)
		 && (prompt_old_password) ) {
		old_passwd = ldaptool_prompt_password( old_password_string );
	} else if ( (oldpasswd.bv_val == NULL) && (oldpasswd.bv_len == 0) 
				&& (is_file_old) ) {
		old_passwd = ldaptool_read_password( old_password_fp );
	}
	
	if ( old_passwd ) {
		if ( !ldaptool_noconv_passwd ) {
			oldpasswd.bv_val = ldaptool_local2UTF8( old_passwd );
		} else {
			oldpasswd.bv_val = strdup( old_passwd );
		}
		if (NULL == oldpasswd.bv_val) 
		{
			perror("malloc");
			exit( LDAP_NO_MEMORY );
		} 				
		oldpasswd.bv_len = strlen( oldpasswd.bv_val );
	}
		
	if ( (newpasswd.bv_val == NULL) && (newpasswd.bv_len == 0)
		 && (prompt_new_password) ) {
try_again:
			new_passwd = ldaptool_prompt_password( new_password_string );
			re_newpasswd = ldaptool_prompt_password( re_new_password_string );
			if ( (NULL == new_passwd) || (NULL == re_newpasswd) ) 
			{
				perror("malloc");
				exit( LDAP_NO_MEMORY );
			}
			if ( (strncmp( new_passwd, re_newpasswd, 
						   strlen( new_passwd ) ) ) ) {
				fprintf( stderr, 
						 "%s: They don't match.\n\nPlease try again\n", 
						 ldaptool_progname );
				free( re_newpasswd );
				free( new_passwd );
				re_newpasswd = NULL;
				new_passwd = NULL;
				goto try_again;
			}
	} else if ( (newpasswd.bv_val == NULL) && (newpasswd.bv_len == 0) 
				&& (is_file_new) ) {
			new_passwd = ldaptool_read_password( new_password_fp );
	}

	if ( new_passwd ) {
		if ( !ldaptool_noconv_passwd ) {
			newpasswd.bv_val = ldaptool_local2UTF8( new_passwd );
		} else {
			newpasswd.bv_val = strdup( new_passwd );
		}
		if (NULL == newpasswd.bv_val) {
			perror("malloc");
			exit( LDAP_NO_MEMORY );
		}				
		newpasswd.bv_len = strlen( newpasswd.bv_val );
	}
	
}

