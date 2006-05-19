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
/*
 *  Copyright (c) 1994 The Regents of the University of Michigan.
 *  All rights reserved.
 */
/*
 *  free.c - some free routines are included here to avoid having to
 *           link in lots of extra code when not using certain features
 */

#if 0
#ifndef lint 
static char copyright[] = "@(#) Copyright (c) 1994 The Regents of the University of Michigan.\nAll rights reserved.\n";
#endif
#endif

#include "ldap-int.h"

void
LDAP_CALL
ldap_getfilter_free( LDAPFiltDesc *lfdp )
{
    LDAPFiltList	*flp, *nextflp;
    LDAPFiltInfo	*fip, *nextfip;

    if ( lfdp == NULL ) {
	return;
    }

    for ( flp = lfdp->lfd_filtlist; flp != NULL; flp = nextflp ) {
	for ( fip = flp->lfl_ilist; fip != NULL; fip = nextfip ) {
	    nextfip = fip->lfi_next;
	    NSLDAPI_FREE( fip->lfi_filter );
	    NSLDAPI_FREE( fip->lfi_desc );
	    NSLDAPI_FREE( fip );
	}
	nextflp = flp->lfl_next;
	NSLDAPI_FREE( flp->lfl_pattern );
	NSLDAPI_FREE( flp->lfl_delims );
	NSLDAPI_FREE( flp->lfl_tag );
	NSLDAPI_FREE( flp );
    }

    if ( lfdp->lfd_curvalcopy != NULL ) {
	NSLDAPI_FREE( lfdp->lfd_curvalcopy );
    }
    if ( lfdp->lfd_curvalwords != NULL ) {
	NSLDAPI_FREE( lfdp->lfd_curvalwords );
    }
    if ( lfdp->lfd_filtprefix != NULL ) {
	NSLDAPI_FREE( lfdp->lfd_filtprefix );
    }
    if ( lfdp->lfd_filtsuffix != NULL ) {
	NSLDAPI_FREE( lfdp->lfd_filtsuffix );
    }

    NSLDAPI_FREE( lfdp );
}


/*
 * free a null-terminated array of pointers to mod structures. the
 * structures are freed, not the array itself, unless the freemods
 * flag is set.
 */
void
LDAP_CALL
ldap_mods_free( LDAPMod **mods, int freemods )
{
	int	i;

	if ( !NSLDAPI_VALID_LDAPMOD_ARRAY( mods )) {
		return;
	}

	for ( i = 0; mods[i] != NULL; i++ ) {
		if ( mods[i]->mod_op & LDAP_MOD_BVALUES ) {
			if ( mods[i]->mod_bvalues != NULL ) {
				ber_bvecfree( mods[i]->mod_bvalues );
			}
		} else if ( mods[i]->mod_values != NULL ) {
			ldap_value_free( mods[i]->mod_values );
		}
		if ( mods[i]->mod_type != NULL ) {
			NSLDAPI_FREE( mods[i]->mod_type );
		}
		NSLDAPI_FREE( (char *) mods[i] );
	}

	if ( freemods )
		NSLDAPI_FREE( (char *) mods );
}


/*
 * ldap_memfree() is needed to ensure that memory allocated by the C runtime
 * assocated with libldap is freed by the same runtime code.
 */
void
LDAP_CALL
ldap_memfree( void *s )
{
	if ( s != NULL ) {
		NSLDAPI_FREE( s );
	}
}


/*
 * ldap_ber_free() is just a cover for ber_free()
 * ber_free() checks for ber == NULL, so we don't bother.
 */
void
LDAP_CALL
ldap_ber_free( BerElement *ber, int freebuf )
{
	ber_free( ber, freebuf );
}
