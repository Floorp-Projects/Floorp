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
    } else {
        sasl_ssf_t      ssf;
        unsigned long val = 0;
        if (!ldap_get_option(ld, LDAP_OPT_X_SASL_SSF, &ssf)) {
            val = (unsigned long)ssf;
        }
        printf("Bind successful, security level is %lu\n", val);
    }
}
