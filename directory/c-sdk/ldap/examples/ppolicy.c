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
 * Attempt to bind to the directory, and report back any password
 * expiration information received.
 */
#include "examples.h"

#define NO_PASSWORD_CONTROLS 0
#define PASSWORD_EXPIRED -1

static void doUsage() {
	printf( "Usage: ppolicy HOST PORT DN PASSWORD\n" );
}

static int
check_controls( LDAPControl **ctrls ) {
	int		i;
	char buf[256];
	int status = NO_PASSWORD_CONTROLS;

	if ( ctrls == NULL ) {
		return NO_PASSWORD_CONTROLS;
	}

	for ( i = 0; ctrls[ i ] != NULL; ++i ) {
		memcpy( buf, ctrls[ i ]->ldctl_value.bv_val,
				ctrls[ i ]->ldctl_value.bv_len );
		buf[ctrls[ i ]->ldctl_value.bv_len] = 0;
		if( !strcmp( LDAP_CONTROL_PWEXPIRED, ctrls[ i ]->ldctl_oid ) ) {
			status = PASSWORD_EXPIRED;
		} else if ( !strcmp( LDAP_CONTROL_PWEXPIRING,
							 ctrls[ i ]->ldctl_oid ) ) {
			status = atoi( buf );
		}
	}

	return status;
}

static void
process_other_errors( int lderr ) {
	fprintf( stderr, "ldap_parse_result: %s",
			 ldap_err2string( lderr ));
	if ( LDAP_CONNECT_ERROR == lderr ) {
		perror( " - " );
	} else {
		fputc( '\n', stderr );
	}
}

static void
process_other_messages( char *errmsg ) {
	if ( errmsg != NULL ) {
		if ( *errmsg != '\0' ) {
			fprintf( stderr, "Additional info: %s\n",
					 errmsg );
		}
		ldap_memfree( errmsg );
	}
}


int
main( int argc, char **argv ) {
    LDAP	    	*ld;
    char	    	*dn;
	char            *password;
	char            *host;
	int             port;
    int 		   	rc = 0;
    int             version = LDAP_VERSION3;
	int             msgid;
	LDAPMessage     *result;
	LDAPControl	    **ctrls;
	int		        lderr;
	int             password_status = 0;
	char		    *matcheddn, *errmsg, **refs;

	if ( argc == 1 ) {
		host = MY_HOST;
		port = MY_PORT;
		dn = USER_DN;
		password = USER_PW;
	} else if ( argc == 5 ) {
		host = argv[1];
		port = atoi( argv[2] );
		dn = argv[3];
		password = argv[4];
	} else {
		doUsage();
		return( 1 );
	}

    /* get a handle to an LDAP connection */
    if ( (ld = ldap_init( host, port )) == NULL ) {
		perror( "ldap_init" );
		return( 1 );
    }
    
    if (ldap_set_option( ld, LDAP_OPT_PROTOCOL_VERSION, &version ) != 0) {
		ldap_perror( ld, "ldap_set_option");
		return ( 1 );
    }

    /* authenticate to the directory */
#ifdef SYNCHRONOUS_BIND
	/* Synchronous bind */
	ldap_simple_bind_s( ld, dn, password );
	lderr = ldap_get_lderrno( ld, NULL, &errmsg );
	if ( LDAP_SUCCESS == lderr ) {
		printf( "Authentication successful\n" );
	} else {
		rc = -1;
		if ( LDAP_INVALID_CREDENTIALS == lderr ) {
			fprintf( stderr, "Invalid credentials\n" );
		} else {
			process_other_errors( lderr );
		}
		if ( errmsg != NULL ) {
			if ( strstr( errmsg, "password expired" ) != NULL ) {
				fprintf( stderr, "Password expired\n" );
			} else {
				fprintf( stderr, "Additional info: %s\n",
						 errmsg );
			}
			ldap_memfree( errmsg );
		}
	}
	/* You can't get the controls with a synchronous bind, so we
	   can't report if the password is about to expire */

#else
	/* Asynchronous bind */
	if ( msgid = ldap_simple_bind( ld, dn, password ) < 0 ) {
		ldap_perror( ld, "ldap_simple_bind" );
		rc = -1;
	} else {
		rc = ldap_result( ld, LDAP_RES_ANY, LDAP_MSG_ONE,
				(struct timeval *)NULL, &result );
		if ( rc == LDAP_RES_BIND ) {
			if ( ldap_parse_result( ld, result, &lderr, &matcheddn, &errmsg,
									&refs, &ctrls, 0 ) != LDAP_SUCCESS ) {
				ldap_perror( ld, "ldap_parse_result" );
			} else {
				if ( LDAP_SUCCESS == lderr ) {
					printf( "Authentication successful\n" );
				} else {
					if ( LDAP_INVALID_CREDENTIALS == lderr ) {
						fprintf( stderr, "Invalid credentials\n" );
					} else {
						process_other_errors( lderr );
					}
					if ( errmsg != NULL ) {
						if ( strstr( errmsg, "password expired" ) != NULL ) {
							fprintf( stderr, "Password expired\n" );
						} else {
							fprintf( stderr, "Additional info: %s\n",
									 errmsg );
						}
						ldap_memfree( errmsg );
					}
				}

				password_status = check_controls( ctrls );
				ldap_controls_free( ctrls );
				if ( password_status == PASSWORD_EXPIRED ) {
					fprintf( stderr,
							 "Password expired and must be reset\n" );
				} else if ( password_status > 0 ) {
					fprintf( stderr,
							 "Password will expire in %d seconds\n",
							 password_status );
				}
				rc = 0;
			}
		} else {
			fprintf( stderr, "ldap_result returned %d\n", rc );
			ldap_perror( ld, "ldap_result" );
			rc = -1;
		}
	}
#endif

	if ( LDAP_SUCCESS == lderr ) {
		ldap_unbind( ld );
	}

    return rc;
}
