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
/* control.c - routines to handle ldapv3 controls */

#include "ldap-int.h"

static LDAPControl *ldap_control_dup( LDAPControl *ctrl );
static int ldap_control_copy_contents( LDAPControl *ctrl_dst,
    LDAPControl *ctrl_src );

/*
 * Append a list of LDAPv3 controls to ber.  If ctrls is NULL, use default
 * set of controls from ld.
 * Return an LDAP error code (LDAP_SUCCESS if all goes well).
 * If closeseq is non-zero, we do an extra ber_put_seq() as well.
 */
int
nsldapi_put_controls( LDAP *ld, LDAPControl **ctrls, int closeseq,
    BerElement *ber )
{
	LDAPControl	*c;
	int		rc, i;

	rc = LDAP_ENCODING_ERROR;	/* the most popular error */

	/* if no controls were passed in, use global list from LDAP * */
	LDAP_MUTEX_LOCK( ld, LDAP_CTRL_LOCK );
	if ( ctrls == NULL ) {
		ctrls = ld->ld_servercontrols;
	}

	/* if there are no controls then we are done */
	if ( ctrls == NULL || ctrls[ 0 ] == NULL ) {
		goto clean_exit;
	}

	/*
	 * If we're using LDAPv2 or earlier we can't send any controls, so
	 * we just ignore them unless one is marked critical, in which case
	 * we return an error.
	 */
	if ( NSLDAPI_LDAP_VERSION( ld ) < LDAP_VERSION3 ) {
		for ( i = 0; ctrls != NULL && ctrls[i] != NULL; i++ ) {
			if ( ctrls[i]->ldctl_iscritical ) {
				rc = LDAP_NOT_SUPPORTED;
				goto error_exit;
			}
		}
		goto clean_exit;
	}

	/*
	 * encode the controls as a Sequence of Sequence
	 */
	if ( ber_printf( ber, "t{", LDAP_TAG_CONTROLS ) == -1 ) {
		goto error_exit;
	}

	for ( i = 0; ctrls[i] != NULL; i++ ) {
		c = ctrls[i];

		if ( ber_printf( ber, "{s", c->ldctl_oid ) == -1 ) {
			goto error_exit;
		}

		/* criticality is "BOOLEAN DEFAULT FALSE" */
		/* therefore, it should only be encoded if it exists AND is TRUE */
		if ( c->ldctl_iscritical ) {
			if ( ber_printf( ber, "b", (int)c->ldctl_iscritical )
			    == -1 ) {
				goto error_exit;
			}
		}

		if ( c->ldctl_value.bv_val != NULL ) {
			if ( ber_printf( ber, "o", c->ldctl_value.bv_val,
			    (int)c->ldctl_value.bv_len /* XXX lossy cast */ )
			    == -1 ) {
				goto error_exit;
			}
		}

		if ( ber_put_seq( ber ) == -1 ) {
			goto error_exit;
		}
	}

	if ( ber_put_seq( ber ) == -1 ) {
		goto error_exit;
	}

clean_exit:
	LDAP_MUTEX_UNLOCK( ld, LDAP_CTRL_LOCK );
	if ( closeseq && ber_put_seq( ber ) == -1 ) {
		goto error_exit;
	}
	return( LDAP_SUCCESS );

error_exit:
	LDAP_MUTEX_UNLOCK( ld, LDAP_CTRL_LOCK );
	LDAP_SET_LDERRNO( ld, rc, NULL, NULL );
	return( rc );
}


/*
 * Pull controls out of "ber" (if any present) and return them in "controlsp."
 * Returns an LDAP error code.
 */
int
nsldapi_get_controls( BerElement *ber, LDAPControl ***controlsp )
{
	LDAPControl		*newctrl;
	unsigned long		tag, len;
	int			rc, maxcontrols, curcontrols;
	char			*last;

	/*
	 * Each LDAPMessage can have a set of controls appended
	 * to it. Controls are used to extend the functionality
	 * of an LDAP operation (e.g., add an attribute size limit
	 * to the search operation). These controls look like this:
	 *
	 *	Controls ::= SEQUENCE OF Control
	 *
	 *	Control ::= SEQUENCE {
	 *		controlType	LDAPOID,
	 *		criticality	BOOLEAN DEFAULT FALSE,
	 *		controlValue	OCTET STRING
	 *	}
	 */
	LDAPDebug( LDAP_DEBUG_TRACE, "=> nsldapi_get_controls\n", 0, 0, 0 );

	*controlsp = NULL;

	/*
         * check to see if controls were included
	 */
	if ( ber_get_option( ber, LBER_OPT_REMAINING_BYTES, &len ) != 0 ) {
		return( LDAP_DECODING_ERROR );	/* unexpected error */
	}
	if ( len == 0 ) {
		LDAPDebug( LDAP_DEBUG_TRACE,
		    "<= nsldapi_get_controls no controls\n", 0, 0, 0 );
		return( LDAP_SUCCESS );			/* no controls */
	}
	if (( tag = ber_peek_tag( ber, &len )) != LDAP_TAG_CONTROLS ) {
		if ( tag == LBER_ERROR ) {
			LDAPDebug( LDAP_DEBUG_TRACE,
			    "<= nsldapi_get_controls LDAP_PROTOCOL_ERROR\n",
			    0, 0, 0 );
			return( LDAP_DECODING_ERROR );	/* decoding error */
		}
		/*
		 * We found something other than controls.  This should never
		 * happen in LDAPv3, but we don't treat this is a hard error --
		 * we just ignore the extra stuff.
		 */
		LDAPDebug( LDAP_DEBUG_TRACE,
		    "<= nsldapi_get_controls ignoring unrecognized data in message (tag 0x%x)\n",
		    tag, 0, 0 );
		return( LDAP_SUCCESS );
	}

	maxcontrols = curcontrols = 0;
	for ( tag = ber_first_element( ber, &len, &last );
	    tag != LBER_ERROR && tag != LBER_END_OF_SEQORSET;
	    tag = ber_next_element( ber, &len, last ) ) {
		if ( curcontrols >= maxcontrols - 1 ) {
#define CONTROL_GRABSIZE	5
			maxcontrols += CONTROL_GRABSIZE;
			*controlsp = (struct ldapcontrol **)NSLDAPI_REALLOC(
			    (char *)*controlsp, maxcontrols *
			    sizeof(struct ldapcontrol *) );
			if ( *controlsp == NULL ) {
			    rc = LDAP_NO_MEMORY;
			    goto free_and_return;
			}
		}
		if (( newctrl = (struct ldapcontrol *)NSLDAPI_CALLOC( 1,
		    sizeof(LDAPControl))) == NULL ) {
			rc = LDAP_NO_MEMORY;
			goto free_and_return;
		}
		
		(*controlsp)[curcontrols++] = newctrl;
		(*controlsp)[curcontrols] = NULL;

		if ( ber_scanf( ber, "{a", &newctrl->ldctl_oid )
		    == LBER_ERROR ) {
			rc = LDAP_DECODING_ERROR;
			goto free_and_return;
		}

		/* the criticality is optional */
		if ( ber_peek_tag( ber, &len ) == LBER_BOOLEAN ) {
			int		aint;

			if ( ber_scanf( ber, "b", &aint ) == LBER_ERROR ) {
				rc = LDAP_DECODING_ERROR;
				goto free_and_return;
			}
			newctrl->ldctl_iscritical = (char)aint;	/* XXX lossy cast */
		} else {
			/* absent is synonomous with FALSE */
			newctrl->ldctl_iscritical = 0;
		}

		/* the control value is optional */
		if ( ber_peek_tag( ber, &len ) == LBER_OCTETSTRING ) {
			if ( ber_scanf( ber, "o", &newctrl->ldctl_value )
			    == LBER_ERROR ) {
				rc = LDAP_DECODING_ERROR;
				goto free_and_return;
			}
		} else {
			(newctrl->ldctl_value).bv_val = NULL;
			(newctrl->ldctl_value).bv_len = 0;
		}

	}

	if ( tag == LBER_ERROR ) {
		rc = LDAP_DECODING_ERROR;
		goto free_and_return;
	}

	LDAPDebug( LDAP_DEBUG_TRACE,
	    "<= nsldapi_get_controls found %d controls\n", curcontrols, 0, 0 );
	return( LDAP_SUCCESS );

free_and_return:;
	ldap_controls_free( *controlsp );
	*controlsp = NULL;
	LDAPDebug( LDAP_DEBUG_TRACE,
	    "<= nsldapi_get_controls error 0x%x\n", rc, 0, 0 );
	return( rc );
}


void
LDAP_CALL
ldap_control_free( LDAPControl *ctrl )
{
	if ( ctrl != NULL ) {
		if ( ctrl->ldctl_oid != NULL ) {
			NSLDAPI_FREE( ctrl->ldctl_oid );
		}
		if ( ctrl->ldctl_value.bv_val != NULL ) {
			NSLDAPI_FREE( ctrl->ldctl_value.bv_val );
		}
		NSLDAPI_FREE( (char *)ctrl );
	}
}


void
LDAP_CALL
ldap_controls_free( LDAPControl **ctrls )
{
	int	i;

	if ( ctrls != NULL ) {
		for ( i = 0; ctrls[i] != NULL; i++ ) {
			ldap_control_free( ctrls[i] );
		}
		NSLDAPI_FREE( (char *)ctrls );
	}
}



#if 0
LDAPControl **
LDAP_CALL
ldap_control_append( LDAPControl **ctrl_src, LDAPControl *ctrl )
{
    int nctrls = 0;
	LDAPControl **ctrlp;
	int i;

	if ( NULL == ctrl )
	    return ( NULL );

	/* Count the existing controls */
	if ( NULL != ctrl_src ) {
		while( NULL != ctrl_src[nctrls] ) {
			nctrls++;
		}
	}

	/* allocate the new control structure */
	if ( ( ctrlp = (LDAPControl **)NSLDAPI_MALLOC( sizeof(LDAPControl *)
	    * (nctrls + 2) ) ) == NULL ) {
		return( NULL );
	}
	memset( ctrlp, 0, sizeof(*ctrlp) * (nctrls + 2) );

	for( i = 0; i < (nctrls + 1); i++ ) {
	    if ( i < nctrls ) {
		    ctrlp[i] = ldap_control_dup( ctrl_src[i] );
	    } else {
		    ctrlp[i] = ldap_control_dup( ctrl );
	    }
	    if ( NULL == ctrlp[i] ) {
		    ldap_controls_free( ctrlp );
		    return( NULL );
	    }
	}
	return ctrlp;
}
#endif /* 0 */


/*
 * Replace *ldctrls with a copy of newctrls.
 * returns 0 if successful.
 * return -1 if not and set error code inside LDAP *ld.
 */
int
nsldapi_dup_controls( LDAP *ld, LDAPControl ***ldctrls, LDAPControl **newctrls )
{
	int	count;

	if ( *ldctrls != NULL ) {
		ldap_controls_free( *ldctrls );
	}

	if ( newctrls == NULL || newctrls[0] == NULL ) {
		*ldctrls = NULL;
		return( 0 );
	}

	for ( count = 0; newctrls[ count ] != NULL; ++count ) {
		;
	}

	if (( *ldctrls = (LDAPControl **)NSLDAPI_MALLOC(( count + 1 ) *
	    sizeof( LDAPControl *))) == NULL ) {
		LDAP_SET_LDERRNO( ld, LDAP_NO_MEMORY, NULL, NULL );
		return( -1 );
	}
	(*ldctrls)[ count ] = NULL;

	for ( count = 0; newctrls[ count ] != NULL; ++count ) {
		if (( (*ldctrls)[ count ] =
		    ldap_control_dup( newctrls[ count ] )) == NULL ) {
			ldap_controls_free( *ldctrls );
			*ldctrls = NULL;
			LDAP_SET_LDERRNO( ld, LDAP_NO_MEMORY, NULL, NULL );
			return( -1 );
		}
	}

	return( 0 );
}


/*
 * return a malloc'd copy of "ctrl" (NULL if memory allocation fails)
 */
static LDAPControl *
/* LDAP_CALL */		/* keep this routine internal for now */
ldap_control_dup( LDAPControl *ctrl )
{
	LDAPControl	*rctrl;

	if (( rctrl = (LDAPControl *)NSLDAPI_MALLOC( sizeof( LDAPControl )))
	    == NULL ) {
		return( NULL );
	}

	if ( ldap_control_copy_contents( rctrl, ctrl ) != LDAP_SUCCESS ) {
		NSLDAPI_FREE( rctrl );
		return( NULL );
	}

	return( rctrl );
}


/*
 * duplicate the contents of "ctrl_src" and place in "ctrl_dst"
 */
static int
/* LDAP_CALL */		/* keep this routine internal for now */
ldap_control_copy_contents( LDAPControl *ctrl_dst, LDAPControl *ctrl_src )
{
	size_t	len;

	if ( NULL == ctrl_dst || NULL == ctrl_src ) {
		return( LDAP_PARAM_ERROR );
	}

	ctrl_dst->ldctl_iscritical = ctrl_src->ldctl_iscritical;

	/* fill in the fields of this new control */
	if (( ctrl_dst->ldctl_oid = nsldapi_strdup( ctrl_src->ldctl_oid ))
	    == NULL ) {
		return( LDAP_NO_MEMORY );
	}

	len = (size_t)(ctrl_src->ldctl_value).bv_len;
	if ( ctrl_src->ldctl_value.bv_val == NULL || len <= 0 ) {
		ctrl_dst->ldctl_value.bv_len = 0;
		ctrl_dst->ldctl_value.bv_val = NULL;
	} else {
		ctrl_dst->ldctl_value.bv_len = len;
		if (( ctrl_dst->ldctl_value.bv_val = NSLDAPI_MALLOC( len ))
		    == NULL ) {
			NSLDAPI_FREE( ctrl_dst->ldctl_oid );
			return( LDAP_NO_MEMORY );
		}
		SAFEMEMCPY( ctrl_dst->ldctl_value.bv_val,
		    ctrl_src->ldctl_value.bv_val, len );
	}

	return ( LDAP_SUCCESS );
}



/*
 * build an allocated LDAPv3 control.  Returns an LDAP error code.
 */
int
nsldapi_build_control( char *oid, BerElement *ber, int freeber, char iscritical,
    LDAPControl **ctrlp )
{
	int		rc;
	struct berval	*bvp;

	if ( ber == NULL ) {
		bvp = NULL;
	} else {
		/* allocate struct berval with contents of the BER encoding */
		rc = ber_flatten( ber, &bvp );
		if ( freeber ) {
			ber_free( ber, 1 );
		}
		if ( rc == -1 ) {
			return( LDAP_NO_MEMORY );
		}
	}

	/* allocate the new control structure */
	if (( *ctrlp = (LDAPControl *)NSLDAPI_MALLOC( sizeof(LDAPControl)))
	    == NULL ) {
		if ( bvp != NULL ) {
			ber_bvfree( bvp );
		}
		return( LDAP_NO_MEMORY );
	}

	/* fill in the fields of this new control */
	(*ctrlp)->ldctl_iscritical = iscritical;  
	if (( (*ctrlp)->ldctl_oid = nsldapi_strdup( oid )) == NULL ) {
		NSLDAPI_FREE( *ctrlp ); 
		if ( bvp != NULL ) {
			ber_bvfree( bvp );
		}
		return( LDAP_NO_MEMORY );
	}				

	if ( bvp == NULL ) {
		(*ctrlp)->ldctl_value.bv_len = 0;
		(*ctrlp)->ldctl_value.bv_val = NULL;
	} else {
		(*ctrlp)->ldctl_value = *bvp;	/* struct copy */
		NSLDAPI_FREE( bvp );	/* free container, not contents! */
	}

	return( LDAP_SUCCESS );
}
