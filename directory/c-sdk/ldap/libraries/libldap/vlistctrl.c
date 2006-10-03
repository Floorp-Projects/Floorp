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

/* vlistctrl.c - virtual list control implementation. */
#include "ldap-int.h"



/*
 * function to create a VirtualListViewRequest control that can be passed
 * to ldap_search_ext() or ldap_search_ext_s().  *ctrlp will be set to a
 * freshly allocated LDAPControl structure.  Returns an LDAP error code
 * (LDAP_SUCCESS if all goes well).
 * 
 *  Parameters
 *   ld              LDAP pointer to the desired connection 
 *
 *   ldvlistp        the control structure.
 *
 *   ctrlp           the address of a place to put the constructed control 

  The controlValue is an OCTET STRING
  whose value is the BER-encoding of the following SEQUENCE:
  
       VirtualListViewRequest ::= SEQUENCE {
               beforeCount    INTEGER (0 .. maxInt),
               afterCount     INTEGER (0 .. maxInt),
               CHOICE {
                       byIndex [0] SEQUENCE {
                       index           INTEGER (0 .. maxInt),
                       contentCount    INTEGER (0 .. maxInt) }
                       byValue [1] greaterThanOrEqual assertionValue }
  
  beforeCount indicates how many  entries  before  the  target  entry  the
  client  wants  the  server  to  send. afterCount indicates the number of
  entries after the target entry the client  wants  the  server  to  send.
  index  and contentCount identify the target entry
  greaterThanOrEqual  is  an  attribute  assertion  value  defined  in
  [LDAPv3].  If  present, the value supplied in greaterThanOrEqual is used
  to determine the target entry by  comparison  with  the  values  of  the
  attribute  specified as the primary sort key. The first list entry who's
  value is no less than the supplied value is the target entry.

 */

int 
LDAP_CALL
ldap_create_virtuallist_control( 
    LDAP *ld, 
    LDAPVirtualList *ldvlistp,
    LDAPControl **ctrlp 
)
{
    BerElement *ber;
    int rc;
    
    if (!NSLDAPI_VALID_LDAP_POINTER( ld )) {
	return( LDAP_PARAM_ERROR );
    }


    if ( NULL == ctrlp || NULL == ldvlistp ) {
        LDAP_SET_LDERRNO( ld, LDAP_PARAM_ERROR, NULL, NULL );
        return ( LDAP_PARAM_ERROR );
    }

    /* create a ber package to hold the controlValue */
    if ( LDAP_SUCCESS != nsldapi_alloc_ber_with_options( ld, &ber )  ) 
    {
        LDAP_SET_LDERRNO( ld, LDAP_NO_MEMORY, NULL, NULL );
        return( LDAP_NO_MEMORY );
    }

    if ( LBER_ERROR == ber_printf( ber, 
                                   "{ii", 
                                   ldvlistp->ldvlist_before_count,
                                   ldvlistp->ldvlist_after_count )) 
				    /* XXX lossy casts */
    {
        LDAP_SET_LDERRNO( ld, LDAP_ENCODING_ERROR, NULL, NULL );
        ber_free( ber, 1 );
        return( LDAP_ENCODING_ERROR );
    }

    if (NULL == ldvlistp->ldvlist_attrvalue)
    {
        if ( LBER_ERROR == ber_printf( ber, 
                                       "t{ii}}", 
				       LDAP_TAG_VLV_BY_INDEX,
                                       ldvlistp->ldvlist_index, 
                                       ldvlistp->ldvlist_size ) ) 
				       /* XXX lossy casts */
        {
            LDAP_SET_LDERRNO( ld, LDAP_ENCODING_ERROR, NULL, NULL );
            ber_free( ber, 1 );
            return( LDAP_ENCODING_ERROR );
        }
    } 
    else 
    {
        if ( LBER_ERROR == ber_printf( ber, 
                                      "to}", 
				       LDAP_TAG_VLV_BY_VALUE,
                                      ldvlistp->ldvlist_attrvalue,
				       (int)strlen( ldvlistp->ldvlist_attrvalue )) ) {
            LDAP_SET_LDERRNO( ld, LDAP_ENCODING_ERROR, NULL, NULL );
            ber_free( ber, 1 );
            return( LDAP_ENCODING_ERROR );
        }
    }


    rc = nsldapi_build_control( LDAP_CONTROL_VLVREQUEST , 
                                ber, 
                                1,
                                1, 
                                ctrlp );

    LDAP_SET_LDERRNO( ld, rc, NULL, NULL );
    return( rc );

}


/*
 * function to find and parse a VirtualListViewResponse control contained in
 * "ctrls"  *target_posp, *list_sizep, and *errcodep are set based on its
 * contents.  Returns an LDAP error code that indicates whether the parsing
 * itself was successful (LDAP_SUCCESS if all goes well).

  The controlValue is an OCTET STRING, whose value
  is the BER encoding of a value of the following SEQUENCE:
  
       VirtualListViewResponse ::= SEQUENCE {
               targetPosition    INTEGER (0 .. maxInt),
               contentCount     INTEGER (0 .. maxInt),
               virtualListViewResult ENUMERATED {
                       success (0),
                       operatonsError (1),
                       unwillingToPerform (53),
                       insufficientAccessRights (50),
                       busy (51),
                       timeLimitExceeded (3),
                       adminLimitExceeded (11),
                       sortControlMissing (60),
                       indexRangeError (61),
                       other (80) }  }

 */
int 
LDAP_CALL
ldap_parse_virtuallist_control
( 
    LDAP *ld, 
    LDAPControl **ctrls,
    ber_int_t *target_posp, 
    ber_int_t *list_sizep, 
    int *errcodep
)
{
    BerElement		*ber;
    int			i, foundListControl;
    ber_int_t		errcode;
    LDAPControl		*listCtrlp;
    ber_int_t	target_pos, list_size;

    if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
        return( LDAP_PARAM_ERROR );
    }

    /* only ldapv3 or higher can do virtual lists. */
    if ( NSLDAPI_LDAP_VERSION( ld ) < LDAP_VERSION3 ) {
        LDAP_SET_LDERRNO( ld, LDAP_NOT_SUPPORTED, NULL, NULL );
        return( LDAP_NOT_SUPPORTED );
    }

    /* find the listControl in the list of controls if it exists */
    if ( ctrls == NULL ) {
        LDAP_SET_LDERRNO( ld, LDAP_CONTROL_NOT_FOUND, NULL, NULL );
        return ( LDAP_CONTROL_NOT_FOUND );
    } 
    
    foundListControl = 0;
    for ( i = 0; (( ctrls[i] != NULL ) && ( !foundListControl )); i++ ) {
        foundListControl = !strcmp( ctrls[i]->ldctl_oid, 
                                    LDAP_CONTROL_VLVRESPONSE );
    }
    if ( !foundListControl ) {
        LDAP_SET_LDERRNO( ld, LDAP_CONTROL_NOT_FOUND, NULL, NULL );
        return ( LDAP_CONTROL_NOT_FOUND );
    } else {
        /* let local var point to the listControl */
        listCtrlp = ctrls[i-1];                 
    }

    /*  allocate a Ber element with the contents of the list_control's struct berval */
    if ( ( ber = ber_init( &listCtrlp->ldctl_value ) ) == NULL ) {
        LDAP_SET_LDERRNO( ld, LDAP_NO_MEMORY, NULL, NULL );
        return( LDAP_NO_MEMORY );
    }           

    /* decode the result from the Berelement */
    if (  LBER_ERROR == ber_scanf( ber, "{iie}", &target_pos, &list_size,
	    &errcode ) ) {
        LDAP_SET_LDERRNO( ld, LDAP_DECODING_ERROR, NULL, NULL );
        ber_free( ber, 1 );
        return( LDAP_DECODING_ERROR );
    }

    if ( target_posp != NULL ) {
        *target_posp = target_pos;
    }
    if ( list_sizep != NULL ) {
        *list_sizep = list_size;
    }
    if ( errcodep != NULL ) {
        *errcodep = (int)errcode;
    }

    /* the ber encoding is no longer needed */
    ber_free(ber,1);

    return(LDAP_SUCCESS);

}
