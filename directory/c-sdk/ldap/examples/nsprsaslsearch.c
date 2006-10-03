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
 * Test a bind and search using SASL for authentication.
 * The mechanisms and all other parameters should be passed in,
 * and LDAP_SASL_QUIET mode should be used (no user interaction).
 * If you want to test with user interaction, just use ldapsearch,
 * or see ldap/clients/tools/ldaptool-sasl.c for more information.
 *
 */

#include "examples.h"
#include "ldappr.h"

#include <sasl.h>

static int sasl_flags = LDAP_SASL_QUIET;
static char *sasl_mech = "GSSAPI";

/* warning! - the following requires intimate knowledge of sasl.h */
static char *default_values[] = {
    "", /* SASL_CB_USER         0x4001 */
    "", /* SASL_CB_AUTHNAME     0x4002 */
    "", /* SASL_CB_LANGUAGE     0x4003 */ /* not used */
    "", /* SASL_CB_PASS         0x4004 */
    "", /* SASL_CB_ECHOPROMPT   0x4005 */
    "", /* SASL_CB_NOECHOPROMPT   0x4005 */
    "", /* SASL_CB_CNONCE       0x4007 */
    ""  /* SASL_CB_GETREALM     0x4008 */
};

/* this is so we can use SASL_CB_USER etc. to index into default_values */
#define VALIDVAL(n) ((n >= SASL_CB_USER) && (n <= SASL_CB_GETREALM))
#define VAL(n) default_values[n-0x4001]

static int
example_sasl_interact( LDAP *ld, unsigned flags, void *defaults, void *prompts ) {
	sasl_interact_t		*interact = NULL;
	int			rc;

	if (prompts == NULL) {
		return (LDAP_PARAM_ERROR);
	}

	for (interact = prompts; interact->id != SASL_CB_LIST_END; interact++) {
        if (VALIDVAL(interact->id)) {
            interact->result = VAL(interact->id);
            interact->len = strlen((char *)interact->result);
        }
	}
	return (LDAP_SUCCESS);
}

static int
usage(char *progname)
{
    fprintf(stderr, "Usage: %s [mechanism|""] [username|""] [authname|""] [password|""] [realm|""]\n",
            progname);
    return 1;
}

int
main(int argc, char *argv[])
{
    int ii;
    int index;
    int rc;
    LDAP *ld;
    LDAPControl     **ctrls = NULL;
    int ldversion = LDAP_VERSION3;
    int debuglevel = 0;
    int messages[10];
    int nmsg = sizeof(messages)/sizeof(messages[0]);
    int finished = 0;

    /* set the default sasl args from the user input */
    if (argv[1] && *argv[1]) {
        sasl_mech = argv[1];
    }
    for (ii = 2; ii < argc; ++ii) {
        char *val = argv[ii];
        switch(ii) {
        case 2:
            index = SASL_CB_USER;
            break;
        case 3:
            index = SASL_CB_AUTHNAME;
            break;
        case 4:
            index = SASL_CB_PASS;
            break;
        case 5:
            index = SASL_CB_GETREALM;
            break;
        default:
            return usage(argv[0]);
            break;
        }
        VAL(index) = val;
    }

    ldap_set_option(NULL, LDAP_OPT_DEBUG_LEVEL, &debuglevel);
	/* get a handle to an LDAP connection */
	if ( (ld = prldap_init( MY_HOST, MY_PORT, 0 )) == NULL ) {
		perror( "ldap_init" );
		return( 1 );
	}

    ldap_set_option( ld, LDAP_OPT_PROTOCOL_VERSION, &ldversion );

    rc = ldap_sasl_interactive_bind_ext_s( ld, "", sasl_mech,
                                           NULL, NULL, sasl_flags,
                                           example_sasl_interact, NULL, &ctrls );
    if (rc != LDAP_SUCCESS ) {
        ldap_perror( ld, "Bind Error" );
        return rc;
    } else {
        sasl_ssf_t      ssf;
        unsigned long val = 0;
        if (!ldap_get_option(ld, LDAP_OPT_X_SASL_SSF, &ssf)) {
            val = (unsigned long)ssf;
        }
        printf("Bind successful, security level is %lu\n", val);
    }

    for (ii = 0; ii < nmsg; ++ii) {
        /* start an async search */
        if (( messages[ii] = ldap_search( ld, MY_SEARCHBASE, LDAP_SCOPE_SUBTREE,
                                          MY_FILTER, NULL, 0 )) < 0 ) {
            ldap_perror( ld, "ldap_search" );
            return( 1 );
        }
    }

    while (!finished) {
        finished = 1;
        for (ii = 0; ii < nmsg; ++ii) {
            struct timeval	zerotime;
            LDAPMessage *result, *e;

            zerotime.tv_sec = zerotime.tv_usec = 0L;
            if (messages[ii] < 0) {
                continue;
            }
            finished = 0; /* have at least one result to get */
            result = NULL;
            rc = ldap_result( ld, messages[ii], LDAP_MSG_ALL, &zerotime, &result );
            if ( rc < 0 ) {
                /* some error occurred */
                ldap_perror( ld, "ldap_result" );
                return( 1 );
            }
            if (rc == 0) {
                /* Timeout was exceeded.  No entries are ready for retrieval. */
                if ( result != NULL ) {
                    ldap_msgfree( result );
                }
                continue;
            }
            printf("Got results for message[%d] %d\n", ii, messages[ii]);
            for ( e = ldap_first_entry( ld, result ); e != NULL;
                  e = ldap_next_entry( ld, e ) ) {
                BerElement	*ber;
                char		*a, *dn;
                char		**vals;
                if ( (dn = ldap_get_dn( ld, e )) != NULL ) {
                    printf( "dn: %s\n", dn );
                    ldap_memfree( dn );
                }
                for ( a = ldap_first_attribute( ld, e, &ber );
                      a != NULL; a = ldap_next_attribute( ld, e, ber ) ) {
                    if ((vals = ldap_get_values( ld, e, a)) != NULL ) {
                        int jj;
                        for ( jj = 0; vals[jj] != NULL; jj++ ) {
                            printf( "%s: %s\n", a, vals[jj] );
                        }
                        ldap_value_free( vals );
                    }
                    ldap_memfree( a );
                }
                if ( ber != NULL ) {
                    ber_free( ber, 0 );
                }
                printf( "\n" );
            }
            ldap_msgfree( result );
            messages[ii] = -1;
        }
    }
    ldap_unbind(ld);
}
