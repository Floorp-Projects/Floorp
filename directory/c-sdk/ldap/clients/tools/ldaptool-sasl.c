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
 * Contributor(s):
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
 * File for ldaptool routines for SASL
 */
 
#include <ldap.h>
#include "ldaptool.h"
#include "ldaptool-sasl.h"
#include <sasl.h>
#include <stdio.h>

#ifdef HAVE_SASL_OPTIONS

#define SASL_PROMPT	"SASL"

typedef struct {
        char *mech;
        char *authid;
        char *username;
        char *passwd;
        char *realm;
} ldaptoolSASLdefaults;

static int get_default(ldaptoolSASLdefaults *defaults, sasl_interact_t *interact);
static int get_new_value(sasl_interact_t *interact, unsigned flags);

void *
ldaptool_set_sasl_defaults ( LDAP *ld, char *mech, char *authid, char *username,
				 char *passwd, char *realm )
{
        ldaptoolSASLdefaults *defaults;
         
        if ((defaults = calloc(sizeof(defaults[0]), 1)) == NULL)
		return NULL;

	if (mech)
		defaults->mech = mech;
	else
		ldap_get_option(ld, LDAP_OPT_X_SASL_MECH, &defaults->mech);

	if (authid)
		defaults->authid = authid;
	else
		ldap_get_option(ld, LDAP_OPT_X_SASL_AUTHCID, &defaults->authid);

	if (username)
		defaults->username = username;
	else
		ldap_get_option(ld, LDAP_OPT_X_SASL_AUTHZID, &defaults->username);

        defaults->passwd = passwd;

	if (realm)
		defaults->realm = realm;
	else
		ldap_get_option(ld, LDAP_OPT_X_SASL_REALM, &defaults->realm);

        return defaults;
}

int
ldaptool_sasl_interact( LDAP *ld, unsigned flags, void *defaults, void *prompts ) {
	sasl_interact_t		*interact;
	ldaptoolSASLdefaults	*sasldefaults = defaults;
	int			rc;

	if (prompts == NULL || flags != LDAP_SASL_INTERACTIVE)
		return (LDAP_PARAM_ERROR);

	for (interact = prompts; interact->id != SASL_CB_LIST_END; interact++) {
		/* Obtain the default value */
		if ((rc = get_default(sasldefaults, interact)) != LDAP_SUCCESS)
			return (rc);

		/* If no default, get the new value from stdin */
		if (interact->result == NULL) {
			if ((rc = get_new_value(interact, flags)) != LDAP_SUCCESS)
				return (rc);
		}

	}
	return (LDAP_SUCCESS);
}

static int 
get_default(ldaptoolSASLdefaults *defaults, sasl_interact_t *interact) {
	const char	*defvalue = interact->defresult;

	if (defaults != NULL) {
		switch( interact->id ) {
        	case SASL_CB_AUTHNAME:
			defvalue = defaults->authid;
			break;
        	case SASL_CB_USER:
			defvalue = defaults->username;
			break;
        	case SASL_CB_PASS:
			defvalue = defaults->passwd;
			break;
        	case SASL_CB_GETREALM:
			defvalue = defaults->realm;
			break;
		}
	}

	if (defvalue != NULL) {
		interact->result = (char *)malloc(strlen(defvalue)+1);
		strcpy((char *)interact->result,defvalue);
		
		/* Clear passwd */
		if (interact->id == SASL_CB_PASS && defaults != NULL) {
			/* At this point defaults->passwd is not NULL */
            		memset( defaults->passwd, '\0', strlen(defaults->passwd));
            		defaults->passwd = NULL;
		}

		if ((char *)interact->result == NULL)
			return (LDAP_NO_MEMORY);
		interact->len = strlen((char *)(interact->result));
	}
	return (LDAP_SUCCESS);
}

static int 
get_new_value(sasl_interact_t *interact, unsigned flags) {
	char	*newvalue, str[1024];
	int	len;

	if (interact->id == SASL_CB_ECHOPROMPT || interact->id == SASL_CB_NOECHOPROMPT) {
		if (interact->challenge)
			fprintf(stderr, "Challenge:%s\n", interact->challenge);
	}

#ifdef HAVE_SNPRINTF
	snprintf(str, sizeof(str), "%s:", interact->prompt?interact->prompt:SASL_PROMPT);
#else
	sprintf(str, "%s:", interact->prompt?interact->prompt:SASL_PROMPT);
#endif

	/* Get the new value */
	if (interact->id == SASL_CB_PASS || interact->id == SASL_CB_NOECHOPROMPT) {
#if defined(_WIN32)
		char pbuf[257];
		fputs(str,stdout);
		fflush(stdout);
		if (fgets(pbuf,256,stdin) == NULL) {
			newvalue = NULL;
		} else {
			char *tmp;

			tmp = strchr(pbuf,'\n');
			if (tmp) *tmp = '\0';
			tmp = strchr(pbuf,'\r');
			if (tmp) *tmp = '\0';
			newvalue = strdup(pbuf);
		}
		if ( newvalue == NULL) {
#else
#if defined(SOLARIS)
		if ((newvalue = (char *)getpassphrase(str)) == NULL) {
#else
		if ((newvalue = (char *)getpass(str)) == NULL) {
#endif
#endif
			return (LDAP_UNAVAILABLE);
		}
		len = strlen(newvalue);
	} else {
		fputs(str, stderr);
		if ((newvalue = fgets(str, sizeof(str), stdin)) == NULL)
			return (LDAP_UNAVAILABLE);
		len = strlen(str);
		if (len > 0 && str[len - 1] == '\n')
			str[len - 1] = 0; 
	}

	interact->result = (char *) strdup(newvalue);
	memset(newvalue, '\0', len);
	if (interact->result == NULL)
		return (LDAP_NO_MEMORY);
	interact->len = len;
	return (LDAP_SUCCESS);
}
#endif	/* HAVE_SASL_OPTIONS */
