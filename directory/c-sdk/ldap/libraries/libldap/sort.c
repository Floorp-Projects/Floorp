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
 * Copyright (c) 1994 Regents of the University of Michigan.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and that due credit is given
 * to the University of Michigan at Ann Arbor. The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission. This software
 * is provided ``as is'' without express or implied warranty.
 */
/*
 * sort.c:  LDAP library entry and value sort routines
 */

#include "ldap-int.h"

/* This xp_qsort fixes a memory problem (ABR) on Solaris for the client.
 * Server is welcome to use it too, but I wasn't sure if it
 * would be ok to use XP code here.  -slamm
 *
 * We don't want to require use of libxp when linking with libldap, so
 * I'll leave use of xp_qsort as a MOZILLA_CLIENT-only thing for now. --mcs
 */
#if defined(MOZILLA_CLIENT) && defined(SOLARIS)
#include "xp_qsort.h"
#else
#define XP_QSORT qsort
#endif

typedef struct keycmp {
    void                 *kc_arg;
    LDAP_KEYCMP_CALLBACK *kc_cmp;
} keycmp_t;

typedef struct keything {
    keycmp_t            *kt_cmp;
    const struct berval *kt_key;
    LDAPMessage         *kt_msg;
} keything_t;

static int LDAP_C LDAP_CALLBACK
ldapi_keycmp( const void *Lv, const void *Rv )
{
    auto keything_t **L = (keything_t**)Lv;
    auto keything_t **R = (keything_t**)Rv;
    auto keycmp_t *cmp = (*L)->kt_cmp;
    return cmp->kc_cmp( cmp->kc_arg, (*L)->kt_key, (*R)->kt_key );
}

int
LDAP_CALL
ldap_keysort_entries(
    LDAP        *ld,
    LDAPMessage **chain,
    void                  *arg,
    LDAP_KEYGEN_CALLBACK  *gen,
    LDAP_KEYCMP_CALLBACK  *cmp,
    LDAP_KEYFREE_CALLBACK *fre)
{
	size_t		count, i;
	keycmp_t	kc = {0};
	keything_t	**kt;
	LDAPMessage	*e, *last;
	LDAPMessage	**ep;

	if ( !NSLDAPI_VALID_LDAP_POINTER( ld )
	    || chain == NULL || cmp == NULL ) {
		return( LDAP_PARAM_ERROR );
	}

	count = ldap_count_entries( ld, *chain );

	if (count < 0) { /* error */
		return( LDAP_PARAM_ERROR );
	}

	if (count < 2) { /* nothing to sort */
		return( 0 );
	}

	kt = (keything_t**)NSLDAPI_MALLOC( count * (sizeof(keything_t*) + sizeof(keything_t)) );
	if ( kt == NULL ) {
		LDAP_SET_LDERRNO( ld, LDAP_NO_MEMORY, NULL, NULL );
		return( -1 );
	}
	for ( i = 0; i < count; i++ ) {
		kt[i] = i + (keything_t*)(kt + count);
	}
	kc.kc_arg = arg;
	kc.kc_cmp = cmp;

	for ( e = *chain, i = 0; i < count; i++, e = e->lm_chain ) {
		kt[i]->kt_msg = e;
		kt[i]->kt_cmp = &kc;
		kt[i]->kt_key = gen( arg, ld, e );
		if ( kt[i]->kt_key == NULL ) {
			if ( fre ) while ( i-- > 0 ) fre( arg, kt[i]->kt_key );
			NSLDAPI_FREE( (char*)kt );
			LDAP_SET_LDERRNO( ld, LDAP_NO_MEMORY, NULL, NULL );
			return( -1 );
		}
	}
	last = e;

	XP_QSORT( (void*)kt, count, (size_t)sizeof(keything_t*), ldapi_keycmp );
    
	ep = chain;
	for ( i = 0; i < count; i++ ) {
		*ep = kt[i]->kt_msg;
		ep = &(*ep)->lm_chain;
		if ( fre ) fre( arg, kt[i]->kt_key );
	}
	*ep = last;
	NSLDAPI_FREE( (char*)kt );
	return( 0 );
}


struct entrything {
	char		**et_vals;
	LDAPMessage	*et_msg;
};

typedef int (LDAP_C LDAP_CALLBACK LDAP_CHARCMP_CALLBACK)(char*, char*);
typedef int (LDAP_C LDAP_CALLBACK LDAP_VOIDCMP_CALLBACK)(const void*, 
	const void*);

static LDAP_CHARCMP_CALLBACK *et_cmp_fn;
static LDAP_VOIDCMP_CALLBACK et_cmp;

int
LDAP_C
LDAP_CALLBACK
ldap_sort_strcasecmp(
    const char	**a,
    const char	**b
)
{
    /* XXXceb
     * I am not 100% sure this is the way this should be handled.  
     * For now we will return a 0 on invalid.
     */    
    if (NULL == a || NULL == b)
        return (0);
	return( strcasecmp( (char *)*a, (char *)*b ) );
}

static int
LDAP_C
LDAP_CALLBACK
et_cmp(
    const void	*aa,
    const void	*bb
)
{
	int			i, rc;
	struct entrything	*a = (struct entrything *)aa;
	struct entrything	*b = (struct entrything *)bb;

	if ( a->et_vals == NULL && b->et_vals == NULL )
		return( 0 );
	if ( a->et_vals == NULL )
		return( -1 );
	if ( b->et_vals == NULL )
		return( 1 );

	for ( i = 0; a->et_vals[i] && b->et_vals[i]; i++ ) {
		if ( (rc = (*et_cmp_fn)( a->et_vals[i], b->et_vals[i] ))
		    != 0 ) {
			return( rc );
		}
	}

	if ( a->et_vals[i] == NULL && b->et_vals[i] == NULL )
		return( 0 );
	if ( a->et_vals[i] == NULL )
		return( -1 );
	return( 1 );
}

int
LDAP_CALL
ldap_multisort_entries(
    LDAP	*ld,
    LDAPMessage	**chain,
    char	**attr,		/* NULL => sort by DN */
    LDAP_CMP_CALLBACK *cmp
)
{
	int			i, count;
	struct entrything	*et;
	LDAPMessage		*e, *last;
	LDAPMessage		**ep;

	if ( !NSLDAPI_VALID_LDAP_POINTER( ld )
	    || chain == NULL || cmp == NULL ) {
		return( LDAP_PARAM_ERROR );
	}

	count = ldap_count_entries( ld, *chain );

	if (count < 0) { /* error, usually with bad ld or malloc */
		return( LDAP_PARAM_ERROR );
	}

	if (count < 2) { /* nothing to sort */
		return( 0 );
	}

	if ( (et = (struct entrything *)NSLDAPI_MALLOC( count *
	    sizeof(struct entrything) )) == NULL ) {
		LDAP_SET_LDERRNO( ld, LDAP_NO_MEMORY, NULL, NULL );
		return( -1 );
	}

	e = *chain;
	for ( i = 0; i < count; i++ ) {
		et[i].et_msg = e;
		et[i].et_vals = NULL;
		if ( attr == NULL ) {
			char	*dn;

			dn = ldap_get_dn( ld, e );
			et[i].et_vals = ldap_explode_dn( dn, 1 );
			NSLDAPI_FREE( dn );
		} else {
			int	attrcnt;
			char	**vals;

			for ( attrcnt = 0; attr[attrcnt] != NULL; attrcnt++ ) {
			    vals = ldap_get_values( ld, e, attr[attrcnt] );
			    if ( ldap_charray_merge( &(et[i].et_vals), vals )
				!= 0 ) {
				int	j;

				/* XXX risky: ldap_value_free( vals ); */
				for ( j = 0; j <= i; j++ )
				    ldap_value_free( et[j].et_vals );
				NSLDAPI_FREE( (char *) et );
				LDAP_SET_LDERRNO( ld, LDAP_NO_MEMORY, NULL, 
				    NULL );
				return( -1 );
			    }
			    if ( vals != NULL ) {
				NSLDAPI_FREE( (char *)vals );
			    }
			}
		}

		e = e->lm_chain;
	}
	last = e;

	et_cmp_fn = (LDAP_CHARCMP_CALLBACK *)cmp;
	XP_QSORT( (void *) et, (size_t) count,
		(size_t) sizeof(struct entrything), et_cmp );

	ep = chain;
	for ( i = 0; i < count; i++ ) {
		*ep = et[i].et_msg;
		ep = &(*ep)->lm_chain;

		ldap_value_free( et[i].et_vals );
	}
	*ep = last;
	NSLDAPI_FREE( (char *) et );

	return( 0 );
}

int
LDAP_CALL
ldap_sort_entries(
    LDAP	*ld,
    LDAPMessage	**chain,
    char	*attr,		/* NULL => sort by DN */
    LDAP_CMP_CALLBACK *cmp
)
{
	char	*attrs[2];

	attrs[0] = attr;
	attrs[1] = NULL;
	return( ldap_multisort_entries( ld, chain, attr ? attrs : NULL, cmp ) );
}

int
LDAP_CALL
ldap_sort_values(
    LDAP	*ld,
    char	**vals,
    LDAP_VALCMP_CALLBACK *cmp
)
{
	int	nel;

	if ( !NSLDAPI_VALID_LDAP_POINTER( ld ) || cmp == NULL ) {
		return( LDAP_PARAM_ERROR );
	}

    if ( NULL == vals) 
    {
		LDAP_SET_LDERRNO( ld, LDAP_PARAM_ERROR, NULL, NULL );
		return( LDAP_PARAM_ERROR );
	}
	for ( nel = 0; vals[nel] != NULL; nel++ )
		;	/* NULL */

	XP_QSORT( vals, nel, sizeof(char *), (LDAP_VOIDCMP_CALLBACK *)cmp );

	return( LDAP_SUCCESS );
}
