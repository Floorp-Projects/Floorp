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
#include "ldap-int.h"

#define LDAP_GET_BITOPT( ld, bit ) \
	((ld)->ld_options & bit ) != 0 ? 1 : 0

int
LDAP_CALL
ldap_get_option( LDAP *ld, int option, void *optdata )
{
	int		rc;

	if ( !nsldapi_initialized ) {
		nsldapi_initialize_defaults();
	}

	/*
	 * process global options (not associated with an LDAP session handle)
	 */
	if ( option == LDAP_OPT_MEMALLOC_FN_PTRS ) {
		/* struct copy */
		*((struct ldap_memalloc_fns *)optdata) = nsldapi_memalloc_fns;
		return( 0 );
	}

	/*
	 * if ld is NULL, arrange to return options from our default settings
	 */
	if ( ld == NULL ) {
		ld = &nsldapi_ld_defaults;
	}

	if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
		return( -1 );	/* punt */
	}

	LDAP_MUTEX_LOCK( ld, LDAP_OPTION_LOCK );
	LDAP_SET_LDERRNO( ld, LDAP_SUCCESS, NULL, NULL );
	switch( option ) {
#ifdef LDAP_DNS
	case LDAP_OPT_DNS:
		*((int *) optdata) = LDAP_GET_BITOPT( ld, LDAP_BITOPT_DNS );
		break;
#endif

	case LDAP_OPT_REFERRALS:
		*((int *) optdata) =
		    LDAP_GET_BITOPT( ld, LDAP_BITOPT_REFERRALS );
		break;

#ifdef LDAP_SSLIO_HOOKS
	case LDAP_OPT_SSL:
		*((int *) optdata) = LDAP_GET_BITOPT( ld, LDAP_BITOPT_SSL );
		break;
#endif
	case LDAP_OPT_RESTART:
		*((int *) optdata) = LDAP_GET_BITOPT( ld, LDAP_BITOPT_RESTART );
		break;

	case LDAP_OPT_RECONNECT:
		*((int *) optdata) =
		    LDAP_GET_BITOPT( ld, LDAP_BITOPT_RECONNECT );
		break;

	case LDAP_OPT_ASYNC_CONNECT:
		*((int *) optdata) =
		    LDAP_GET_BITOPT( ld, LDAP_BITOPT_ASYNC );
		break;

	/* stuff in the sockbuf */
	case LDAP_OPT_DESC:
		rc = ber_sockbuf_get_option( ld->ld_sbp,
		    LBER_SOCKBUF_OPT_DESC, optdata );
		LDAP_MUTEX_UNLOCK( ld, LDAP_OPTION_LOCK );
		return( rc );

	/* fields in the LDAP structure */
	case LDAP_OPT_DEREF:
		*((int *) optdata) = ld->ld_deref;
		break;
	case LDAP_OPT_SIZELIMIT:
		*((int *) optdata) = ld->ld_sizelimit;
                break;  
	case LDAP_OPT_TIMELIMIT:
		*((int *) optdata) = ld->ld_timelimit;
                break;
	case LDAP_OPT_REFERRAL_HOP_LIMIT:
		 *((int *) optdata) = ld->ld_refhoplimit;
		break;
	case LDAP_OPT_PROTOCOL_VERSION:
		 *((int *) optdata) = ld->ld_version;
		break;
	case LDAP_OPT_SERVER_CONTROLS:
		*((LDAPControl ***)optdata) = ld->ld_servercontrols;
		break;
	case LDAP_OPT_CLIENT_CONTROLS:
		*((LDAPControl ***)optdata) = ld->ld_clientcontrols;
		break;

	/* rebind proc */
	case LDAP_OPT_REBIND_FN:
		*((LDAP_REBINDPROC_CALLBACK **) optdata) = ld->ld_rebind_fn;
		break;
	case LDAP_OPT_REBIND_ARG:
		*((void **) optdata) = ld->ld_rebind_arg;
		break;

#ifdef LDAP_SSLIO_HOOKS
	/* i/o function pointers */
	case LDAP_OPT_IO_FN_PTRS:
		/* struct copy */
		*((struct ldap_io_fns *) optdata) = ld->ld_io;
		break;
#endif /* LDAP_SSLIO_HOOKS */

	/* thread function pointers */
	case LDAP_OPT_THREAD_FN_PTRS:
		/* struct copy */
		*((struct ldap_thread_fns *) optdata) = ld->ld_thread;
		break;

	/* DNS function pointers */
	case LDAP_OPT_DNS_FN_PTRS:
		/* struct copy */
		*((struct ldap_dns_fns *) optdata) = ld->ld_dnsfn;
		break;

	/* cache function pointers */
	case LDAP_OPT_CACHE_FN_PTRS:
		/* struct copy */
		*((struct ldap_cache_fns *) optdata) = ld->ld_cache;
		break;
	case LDAP_OPT_CACHE_STRATEGY:
		*((int *) optdata) = ld->ld_cache_strategy;
		break;
	case LDAP_OPT_CACHE_ENABLE:
		*((int *) optdata) = ld->ld_cache_on;
		break;

	case LDAP_OPT_ERROR_NUMBER:
		*((int *) optdata) = LDAP_GET_LDERRNO( ld, NULL, NULL );
		break;

	case LDAP_OPT_ERROR_STRING:
		(void)LDAP_GET_LDERRNO( ld, NULL, (char **)optdata );
		break;
	case LDAP_OPT_PREFERRED_LANGUAGE:
		if ( NULL != ld->ld_preferred_language ) {
			*((char **) optdata) =
			    nsldapi_strdup(ld->ld_preferred_language);
		} else {
			*((char **) optdata) = NULL;
		}
		break;

	default:
		LDAP_SET_LDERRNO( ld, LDAP_PARAM_ERROR, NULL, NULL );
		LDAP_MUTEX_UNLOCK( ld, LDAP_OPTION_LOCK );
		return( -1 );
	}

	LDAP_MUTEX_UNLOCK( ld, LDAP_OPTION_LOCK  );
	return( 0 );
}
