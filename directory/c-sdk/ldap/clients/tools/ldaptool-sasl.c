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

#if defined(HPUX)
#include <sys/termios.h>  /* for tcgetattr and tcsetattr */
#endif /* HPUX */

#ifdef HAVE_SASL_OPTIONS

#define SASL_PROMPT	"Interact"

typedef struct {
        char *mech;
        char *authid;
        char *username;
        char *passwd;
        char *realm;
} ldaptoolSASLdefaults;

static int get_default(ldaptoolSASLdefaults *defaults, sasl_interact_t *interact, unsigned flags);
static int get_new_value(sasl_interact_t *interact, unsigned flags);

/* WIN32 does not have getlogin() so roll our own */
#if defined( _WINDOWS ) || defined( _WIN32 )
#include "LMCons.h"
static char *getlogin()
{
	LPTSTR lpszSystemInfo; /* pointer to system information string */
	DWORD cchBuff = UNLEN;   /* size of user name */
	static TCHAR tchBuffer[UNLEN + 1]; /* buffer for expanded string */

	lpszSystemInfo = tchBuffer;
	GetUserName(lpszSystemInfo, &cchBuff);

	return lpszSystemInfo;
}
#endif /* _WINDOWS || _WIN32 */

/*
  Note that it is important to use "" (the empty string, length 0) as the default
  username value for non-interactive cases.  This allows the sasl library to find the best
  possible default.  For example, if using GSSAPI, you want the default value for
  the username to be extracted from the Kerberos tgt.  The sasl library will do
  that for you if you set the default username to "".
*/
void *
ldaptool_set_sasl_defaults ( LDAP *ld, unsigned flags, char *mech, char *authid, char *username,
				 char *passwd, char *realm )
{
	ldaptoolSASLdefaults	*defaults;
	char			*login = NULL;

	if ((defaults = calloc(sizeof(ldaptoolSASLdefaults), 1)) == NULL) {
		return NULL;
	}

	/* Try to get the login name */
	if ((login = getlogin()) == NULL) {
		login = "";
	}

	if (mech) {
		defaults->mech = strdup(mech);
	} else {
		ldap_get_option(ld, LDAP_OPT_X_SASL_MECH, &defaults->mech);
	}

	if (authid) { /* use explicit passed in value */
		defaults->authid = strdup(authid);
	} else { /* use option value if any */
		ldap_get_option(ld, LDAP_OPT_X_SASL_AUTHCID, &defaults->authid);
		if (!defaults->authid) {
			/* Default to the login name that is running the command */
			defaults->authid = strdup( login );
		}
	}

	if (username) { /* use explicit passed in value */
		defaults->username = strdup(username);
	} else { /* use option value if any */
		ldap_get_option(ld, LDAP_OPT_X_SASL_AUTHZID, &defaults->username);
		if (!defaults->username && (flags == LDAP_SASL_INTERACTIVE)) {
			/* Default to the login name that is running the command */
			defaults->username = strdup( login );
		} else if (!defaults->username) { /* not interactive - use default sasl value */
			defaults->username = strdup( "" );
		}
	}

	defaults->passwd = passwd;

	if (realm) {
		defaults->realm = realm;
	} else {
		ldap_get_option(ld, LDAP_OPT_X_SASL_REALM, &defaults->realm);
	}

	return defaults;
}

int
ldaptool_sasl_interact( LDAP *ld, unsigned flags, void *defaults, void *prompts ) {
	sasl_interact_t		*interact = NULL;
	ldaptoolSASLdefaults	*sasldefaults = defaults;
	int			rc;

	if (prompts == NULL) {
		return (LDAP_PARAM_ERROR);
	}

	for (interact = prompts; interact->id != SASL_CB_LIST_END; interact++) {
		/* Obtain the default value */
		if ((rc = get_default(sasldefaults, interact, flags)) != LDAP_SUCCESS) {
			return (rc);
		}
		/* always prompt in interactive mode - only prompt in automatic mode
		   if there is no default - never prompt in quiet mode */
		if ( (flags == LDAP_SASL_INTERACTIVE) ||
			 ((interact->result == NULL) && (flags == LDAP_SASL_AUTOMATIC)) ) {
			if ((rc = get_new_value(interact, flags)) != LDAP_SUCCESS)
				return (rc);
		}

	}
	return (LDAP_SUCCESS);
}

static int 
get_default(ldaptoolSASLdefaults *defaults, sasl_interact_t *interact, unsigned flags) {
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

	LDAPToolDebug(LDAP_DEBUG_TRACE, "get_default: id [%lu] sasl default [%s] my default [%s]\n",
                  interact->id,
                  interact->defresult ? interact->defresult : "(null)",
                  defvalue ? defvalue : "(null)");

	if (defvalue != NULL) {
		interact->result = strdup(defvalue);
		
		/* Clear passwd */
		if ((interact->id == SASL_CB_PASS) && (defaults != NULL)) {
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

/*
 * This function should always be called in LDAP_SASL_INTERACTIVE mode, or
 * in LDAP_SASL_AUTOMATIC mode when there is no default value.  This function
 * will print out the challenge, default value, and prompt to get the value.
 * If there is a default value, the user can just press Return/Enter at the
 * prompt to use the default value.  If there is no default, and the user
 * didn't enter anything, this will return "" (the empty string) as the
 * value.
 */
static int 
get_new_value(sasl_interact_t *interact, unsigned flags) {
	char	*newvalue = NULL, str[1024];
	int	len = 0;

	if ((interact->id == SASL_CB_ECHOPROMPT) || (interact->id == SASL_CB_NOECHOPROMPT)) {
		if (interact->challenge) {
			fprintf(stderr, "Challenge: %s\n", interact->challenge);
		}
	}

	if (interact->result) {
		fprintf(stderr, "Default: %s\n", (char *)interact->result);
	}

	snprintf(str, sizeof(str), "%s:", interact->prompt?interact->prompt:SASL_PROMPT);
	str[sizeof(str)-1] = '\0';

	/* Get the new value */
	if ((interact->id == SASL_CB_PASS) || (interact->id == SASL_CB_NOECHOPROMPT)) {
		if ((newvalue = ldaptool_getpass( str )) == NULL) {
			return (LDAP_UNAVAILABLE);
		}
		len = strlen(newvalue);
	} else {
		fputs(str, stderr);
		if ((newvalue = fgets(str, sizeof(str), stdin)) == NULL) {
			return (LDAP_UNAVAILABLE);
		}
		len = strlen(str);
		if ((len > 0) && (str[len - 1] == '\n')) {
			str[len - 1] = '\0';
			len--;
		}
	}

	if (len > 0) { /* user typed in something - use it */
		if (interact->result) {
			free((void *)interact->result);
		}
		interact->result = strdup(newvalue);
		memset(newvalue, '\0', len);

		if (interact->result == NULL) {
			return (LDAP_NO_MEMORY);
		}
		interact->len = len;
	} else { /* use default or "" */
		if (!interact->result) {
			interact->result = "";
		}
		interact->len = strlen(interact->result);
	}
	return (LDAP_SUCCESS);
}
#endif	/* HAVE_SASL_OPTIONS */
