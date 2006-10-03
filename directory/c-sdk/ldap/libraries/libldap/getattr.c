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
 *  Copyright (c) 1990 Regents of the University of Michigan.
 *  All rights reserved.
 */
/*
 *  getattr.c
 */

#if 0
#ifndef lint 
static char copyright[] = "@(#) Copyright (c) 1990 Regents of the University of Michigan.\nAll rights reserved.\n";
#endif
#endif

#include "ldap-int.h"


static unsigned long
bytes_remaining( BerElement *ber )
{
	ber_len_t	len;

	if ( ber_get_option( ber, LBER_OPT_REMAINING_BYTES, &len ) != 0 ) {
		return( 0 );	/* not sure what else to do.... */
	}
	return( len );
}


char *
LDAP_CALL
ldap_first_attribute( LDAP *ld, LDAPMessage *entry, BerElement **ber )
{
	char		*attr;
	int			err;
	ber_len_t	seqlength;

	LDAPDebug( LDAP_DEBUG_TRACE, "ldap_first_attribute\n", 0, 0, 0 );

	if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
		return( NULL );		/* punt */
	}

	if ( ber == NULL || !NSLDAPI_VALID_LDAPMESSAGE_ENTRY_POINTER( entry )) {
		LDAP_SET_LDERRNO( ld, LDAP_PARAM_ERROR, NULL, NULL );
		return( NULL );
	}
	
	if ( nsldapi_alloc_ber_with_options( ld, ber ) != LDAP_SUCCESS ) {
		return( NULL );
	}

	**ber = *entry->lm_ber;

	attr = NULL;			/* pessimistic */
	err = LDAP_DECODING_ERROR;	/* ditto */

	/* 
	 * Skip past the sequence, dn, and sequence of sequence.
	 * Reset number of bytes remaining so we confine the rest of our
	 * decoding to the current sequence.
	 */
	if ( ber_scanf( *ber, "{xl{", &seqlength ) != LBER_ERROR &&
	     ber_set_option( *ber, LBER_OPT_REMAINING_BYTES, &seqlength )
	    == 0 ) {
		/* snarf the attribute type, and skip the set of values,
		 * leaving us positioned right before the next attribute
		 * type/value sequence.
		 */
		if ( ber_scanf( *ber, "{ax}", &attr ) != LBER_ERROR ||
		    bytes_remaining( *ber ) == 0 ) {
			err = LDAP_SUCCESS;
		}
	}

	LDAP_SET_LDERRNO( ld, err, NULL, NULL );
	if ( attr == NULL || err != LDAP_SUCCESS ) {
		ber_free( *ber, 0 );
		*ber = NULL;
	}
	return( attr );
}

/* ARGSUSED */
char *
LDAP_CALL
ldap_next_attribute( LDAP *ld, LDAPMessage *entry, BerElement *ber )
{
	char	*attr;
	int	err;

	LDAPDebug( LDAP_DEBUG_TRACE, "ldap_next_attribute\n", 0, 0, 0 );

	if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
		return( NULL );		/* punt */
	}

	if ( ber == NULL || !NSLDAPI_VALID_LDAPMESSAGE_ENTRY_POINTER( entry )) {
		LDAP_SET_LDERRNO( ld, LDAP_PARAM_ERROR, NULL, NULL );
		return( NULL );
	}

	attr = NULL;			/* pessimistic */
	err = LDAP_DECODING_ERROR;	/* ditto */

	/* skip sequence, snarf attribute type, skip values */
	if ( ber_scanf( ber, "{ax}", &attr ) != LBER_ERROR ||
	    bytes_remaining( ber ) == 0 ) {
		err = LDAP_SUCCESS;
	}

	LDAP_SET_LDERRNO( ld, err, NULL, NULL );
	return( attr );
}
