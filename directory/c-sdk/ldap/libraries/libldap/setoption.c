/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/*
 * `setoption.c - ldap_set_option implementation 
 */

#include "ldap-int.h"

#define LDAP_SETCLR_BITOPT( ld, bit, optdata ) \
	if ( optdata != NULL ) {		\
		(ld)->ld_options |= bit;	\
	} else {				\
		(ld)->ld_options &= ~bit;	\
	}


int
LDAP_CALL
ldap_set_option( LDAP *ld, int option, void *optdata )
{
	int		rc;
	struct ldap	copyld;

	if ( !nsldapi_initialized ) {
		nsldapi_initialize_defaults();
	}

	/*
	 * process global options (not associated with an LDAP session handle)
	 */
	if ( option == LDAP_OPT_MEMALLOC_FN_PTRS ) {
		struct lber_memalloc_fns	memalloc_fns;

		/* set libldap ones via a struct copy */
		nsldapi_memalloc_fns = *((struct ldap_memalloc_fns *)optdata);

		/* also set liblber memory allocation callbacks */
		memalloc_fns.lbermem_malloc =
		    nsldapi_memalloc_fns.ldapmem_malloc;
		memalloc_fns.lbermem_calloc =
		    nsldapi_memalloc_fns.ldapmem_calloc;
		memalloc_fns.lbermem_realloc =
		    nsldapi_memalloc_fns.ldapmem_realloc;
		memalloc_fns.lbermem_free =
		    nsldapi_memalloc_fns.ldapmem_free;
		if ( ber_set_option( NULL, LBER_OPT_MEMALLOC_FN_PTRS,
		    &memalloc_fns ) != 0 ) {
			return( -1 );
		}

		return( 0 );
	}

	/*
	 * if ld is NULL, arrange to modify our default settings
	 */
	if ( ld == NULL ) {
		ld = &nsldapi_ld_defaults;
	}

	/*
	 * process options that are associated with an LDAP session handle
	 */
	if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
		return( -1 );	/* punt */
	}

	rc = 0;
	LDAP_MUTEX_LOCK( ld, LDAP_OPTION_LOCK );
	LDAP_SET_LDERRNO( ld, LDAP_SUCCESS, NULL, NULL );
	switch( option ) {
	/* options that can be turned on and off */
#ifdef LDAP_DNS
	case LDAP_OPT_DNS:
		LDAP_SETCLR_BITOPT( ld, LDAP_BITOPT_DNS, optdata );
		break;
#endif

	case LDAP_OPT_REFERRALS:
		LDAP_SETCLR_BITOPT( ld, LDAP_BITOPT_REFERRALS, optdata );
		break;

#ifdef LDAP_SSLIO_HOOKS
	case LDAP_OPT_SSL:
		LDAP_SETCLR_BITOPT( ld, LDAP_BITOPT_SSL, optdata );
		break;
#endif

	case LDAP_OPT_RESTART:
		LDAP_SETCLR_BITOPT( ld, LDAP_BITOPT_RESTART, optdata );
		break;

	case LDAP_OPT_RECONNECT:
		LDAP_SETCLR_BITOPT( ld, LDAP_BITOPT_RECONNECT, optdata );
		break;
	case LDAP_OPT_ASYNC_CONNECT:
 		LDAP_SETCLR_BITOPT(ld, LDAP_BITOPT_ASYNC, optdata );
	 	break;

	/* fields in the LDAP structure */
	case LDAP_OPT_DEREF:
		ld->ld_deref = *((int *) optdata);
		break;
	case LDAP_OPT_SIZELIMIT:
		ld->ld_sizelimit = *((int *) optdata);
                break;  
	case LDAP_OPT_TIMELIMIT:
		ld->ld_timelimit = *((int *) optdata);
                break;
	case LDAP_OPT_REFERRAL_HOP_LIMIT:
		ld->ld_refhoplimit = *((int *) optdata);
		break;
	case LDAP_OPT_PROTOCOL_VERSION:
		ld->ld_version = *((int *) optdata);
		if ( ld->ld_defconn != NULL ) {	/* also set in default conn. */
			ld->ld_defconn->lconn_version = ld->ld_version;
		}
		break;
	case LDAP_OPT_SERVER_CONTROLS:
		rc = nsldapi_dup_controls( ld, &ld->ld_servercontrols,
		    (LDAPControl **)optdata );
		break;
	case LDAP_OPT_CLIENT_CONTROLS:
		rc = nsldapi_dup_controls( ld, &ld->ld_clientcontrols,
		    (LDAPControl **)optdata );
		break;

	/* rebind proc */
	case LDAP_OPT_REBIND_FN:
		ld->ld_rebind_fn = (LDAP_REBINDPROC_CALLBACK *) optdata;
		break;
	case LDAP_OPT_REBIND_ARG:
		ld->ld_rebind_arg = (void *) optdata;
		break;

#ifdef LDAP_SSLIO_HOOKS
	/* i/o function pointers */
	case LDAP_OPT_IO_FN_PTRS:
		/* struct copy */
		ld->ld_io = *((struct ldap_io_fns *) optdata);
		rc = ber_sockbuf_set_option( ld->ld_sbp,
		    LBER_SOCKBUF_OPT_READ_FN, (void *) ld->ld_read_fn );
		rc |= ber_sockbuf_set_option( ld->ld_sbp,
		    LBER_SOCKBUF_OPT_WRITE_FN, (void *) ld->ld_write_fn );
		break;
#endif

	/* thread function pointers */
	case LDAP_OPT_THREAD_FN_PTRS:
		/* struct copy */
		ld->ld_thread = *((struct ldap_thread_fns *) optdata);
		if ( ld->ld_mutex_alloc_fn != NULL &&
		    ld != &nsldapi_ld_defaults &&
		    ld->ld_mutex != NULL ) {
			int i;
			for( i=0; i<LDAP_MAX_LOCK; i++ )
				ld->ld_mutex[i] = (ld->ld_mutex_alloc_fn)();
		}
		break;

	/* extra thread function pointers */
	case LDAP_OPT_EXTRA_THREAD_FN_PTRS:
		ld->ld_thread2 = *((struct ldap_extra_thread_fns *) optdata);
		break;

	/* DNS function pointers */
	case LDAP_OPT_DNS_FN_PTRS:
		/* struct copy */
		ld->ld_dnsfn = *((struct ldap_dns_fns *) optdata);
		break;

	/* cache function pointers */
	case LDAP_OPT_CACHE_FN_PTRS:
		/* struct copy */
		ld->ld_cache = *((struct ldap_cache_fns *) optdata);
		break;
	case LDAP_OPT_CACHE_STRATEGY:
		ld->ld_cache_strategy = *((int *) optdata);
		break;
	case LDAP_OPT_CACHE_ENABLE:
		ld->ld_cache_on = *((int *) optdata);
		break;
	case LDAP_OPT_PREFERRED_LANGUAGE:
		if ( NULL != ld->ld_preferred_language )
			NSLDAPI_FREE(ld->ld_preferred_language);
		if ( NULL == optdata ) {
			ld->ld_preferred_language = NULL;
		} else {
			ld->ld_preferred_language =
			    nsldapi_strdup((char *) optdata);
		}
		break;

	default:
		LDAP_SET_LDERRNO( ld, LDAP_PARAM_ERROR, NULL, NULL );
		rc = -1;
	}

	LDAP_MUTEX_UNLOCK( ld, LDAP_OPTION_LOCK );
	return( rc );
}
