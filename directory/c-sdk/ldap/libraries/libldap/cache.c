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
 *  Copyright (c) 1993 The Regents of the University of Michigan.
 *  All rights reserved.
 */
/*
 *  cache.c - generic caching support for LDAP
 */

#include "ldap-int.h"

/*
 * ldap_cache_flush - flush part of the LDAP cache. returns an
 * ldap error code (LDAP_SUCCESS, LDAP_NO_SUCH_OBJECT, etc.).
 */

int
LDAP_CALL
ldap_cache_flush( LDAP *ld, const char *dn, const char *filter )
{
	if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
		return( LDAP_PARAM_ERROR );
	}

	if ( dn == NULL ) {
		dn = "";
	}

	return( (ld->ld_cache_flush)( ld, dn, filter ) );
}

/*
 * nsldapi_add_result_to_cache - add an ldap entry we just read off the network
 * to the ldap cache. this routine parses the ber for the entry and
 * constructs the appropriate add request. this routine calls the
 * cache add routine to actually add the entry.
 */

void
nsldapi_add_result_to_cache( LDAP *ld, LDAPMessage *m )
{
	char		*dn;
	LDAPMod		**mods;
	int		i, max, rc;
	char		*a;
	BerElement	*ber;
	char		buf[50];
	struct berval	bv;
	struct berval	*bvp[2];

	LDAPDebug( LDAP_DEBUG_TRACE, "=> nsldapi_add_result_to_cache id %d type %d\n",
	    m->lm_msgid, m->lm_msgtype, 0 );
	if ( m->lm_msgtype != LDAP_RES_SEARCH_ENTRY ||
	    ld->ld_cache_add == NULL ) {
		LDAPDebug( LDAP_DEBUG_TRACE,
		    "<= nsldapi_add_result_to_cache not added\n", 0, 0, 0 );
		return;
	}

#define GRABSIZE	5

	dn = ldap_get_dn( ld, m );
	mods = (LDAPMod **)NSLDAPI_MALLOC( GRABSIZE * sizeof(LDAPMod *) );
	max = GRABSIZE;
	for ( i = 0, a = ldap_first_attribute( ld, m, &ber ); a != NULL;
	    a = ldap_next_attribute( ld, m, ber ), i++ ) {
		if ( i == (max - 1) ) {
			max += GRABSIZE;
			mods = (LDAPMod **)NSLDAPI_REALLOC( mods,
			    sizeof(LDAPMod *) * max );
		}

		mods[i] = (LDAPMod *)NSLDAPI_CALLOC( 1, sizeof(LDAPMod) );
		mods[i]->mod_op = LDAP_MOD_BVALUES;
		mods[i]->mod_type = a;
		mods[i]->mod_bvalues = ldap_get_values_len( ld, m, a );
	}
	if ( ber != NULL ) {
		ber_free( ber, 0 );
	}
	if (( rc = LDAP_GET_LDERRNO( ld, NULL, NULL )) != LDAP_SUCCESS ) {
		LDAPDebug( LDAP_DEBUG_TRACE,
		    "<= nsldapi_add_result_to_cache error: failed to construct mod list (%s)\n",
		    ldap_err2string( rc ), 0, 0 );
		ldap_mods_free( mods, 1 );
		return;
	}

	/* update special cachedtime attribute */
	if ( i == (max - 1) ) {
		max++;
		mods = (LDAPMod **)NSLDAPI_REALLOC( mods,
		    sizeof(LDAPMod *) * max );
	}
	mods[i] = (LDAPMod *)NSLDAPI_CALLOC( 1, sizeof(LDAPMod) );
	mods[i]->mod_op = LDAP_MOD_BVALUES;
	mods[i]->mod_type = "cachedtime";
	sprintf( buf, "%ld", time( NULL ) );
	bv.bv_val = buf;
	bv.bv_len = strlen( buf );
	bvp[0] = &bv;
	bvp[1] = NULL;
	mods[i]->mod_bvalues = bvp;
	mods[++i] = NULL;

	/* msgid of -1 means don't send the result */
	rc = (ld->ld_cache_add)( ld, -1, m->lm_msgtype, dn, mods );
	LDAPDebug( LDAP_DEBUG_TRACE,
	    "<= nsldapi_add_result_to_cache added (rc %d)\n", rc, 0, 0 );
}
