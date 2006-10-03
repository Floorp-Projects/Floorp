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
 * setoption.c - ldap_set_option implementation 
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
ldap_set_option( LDAP *ld, int option, const void *optdata )
{
	int		rc, i;
	char		*matched, *errstr;

	/*
         * if ld is NULL, arrange to modify our default settings
         */
        if ( ld == NULL ) {
		if ( !nsldapi_initialized ) {
			nsldapi_initialize_defaults();
		}
		ld = &nsldapi_ld_defaults;
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
     * LDAP_OPT_DEBUG_LEVEL is global 
     */
    if (LDAP_OPT_DEBUG_LEVEL == option) 
    {
#ifdef LDAP_DEBUG	  
        ldap_debug = *((int *) optdata);
#endif
        return 0;
    }

	/*
	 * process options that are associated with an LDAP session handle
	 */
	if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
		return( -1 );	/* punt */
	}

	rc = 0;
	if ( ld != &nsldapi_ld_defaults
		&& option != LDAP_OPT_EXTRA_THREAD_FN_PTRS
		&& option != LDAP_OPT_THREAD_FN_PTRS ) {
	    LDAP_MUTEX_LOCK( ld, LDAP_OPTION_LOCK );
	}
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

	case LDAP_OPT_SSL:
		LDAP_SETCLR_BITOPT( ld, LDAP_BITOPT_SSL, optdata );
		break;

	case LDAP_OPT_RESTART:
		LDAP_SETCLR_BITOPT( ld, LDAP_BITOPT_RESTART, optdata );
		break;

	case LDAP_OPT_RECONNECT:
		LDAP_SETCLR_BITOPT( ld, LDAP_BITOPT_RECONNECT, optdata );
		break;

	case LDAP_OPT_NOREBIND:
		LDAP_SETCLR_BITOPT( ld, LDAP_BITOPT_NOREBIND, optdata );
		break;

#ifdef LDAP_ASYNC_IO
	case LDAP_OPT_ASYNC_CONNECT:
 		LDAP_SETCLR_BITOPT(ld, LDAP_BITOPT_ASYNC, optdata );
	 	break;
#endif /* LDAP_ASYNC_IO */

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
		/* nsldapi_dup_controls returns -1 and sets lderrno on error */
		rc = nsldapi_dup_controls( ld, &ld->ld_servercontrols,
		    (LDAPControl **)optdata );
		break;
	case LDAP_OPT_CLIENT_CONTROLS:
		/* nsldapi_dup_controls returns -1 and sets lderrno on error */
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

	/* i/o function pointers */
	case LDAP_OPT_IO_FN_PTRS:
		if (( rc = nsldapi_install_compat_io_fns( ld,
		    (struct ldap_io_fns *)optdata )) != LDAP_SUCCESS ) {
			LDAP_SET_LDERRNO( ld, rc, NULL, NULL );
			rc = -1;
		}
		break;

	/* extended i/o function pointers */
	case LDAP_X_OPT_EXTIO_FN_PTRS:
	  /* denotes use of old iofns struct (no writev) */
	  if (((struct ldap_x_ext_io_fns_rev0 *) optdata)->lextiof_size == LDAP_X_EXTIO_FNS_SIZE_REV0) {
	    ld->ld_extio_size = LDAP_X_EXTIO_FNS_SIZE;
	    ld->ld_extclose_fn = ((struct ldap_x_ext_io_fns_rev0 *) optdata)->lextiof_close;
	    ld->ld_extconnect_fn = ((struct ldap_x_ext_io_fns_rev0 *) optdata)->lextiof_connect;
	    ld->ld_extread_fn = ((struct ldap_x_ext_io_fns_rev0 *) optdata)->lextiof_read;
	    ld->ld_extwrite_fn = ((struct ldap_x_ext_io_fns_rev0 *) optdata)->lextiof_write;
	    ld->ld_extpoll_fn = ((struct ldap_x_ext_io_fns_rev0 *) optdata)->lextiof_poll;
	    ld->ld_extnewhandle_fn = ((struct ldap_x_ext_io_fns_rev0 *) optdata)->lextiof_newhandle;
	    ld->ld_extdisposehandle_fn = ((struct ldap_x_ext_io_fns_rev0 *) optdata)->lextiof_disposehandle;
	    ld->ld_ext_session_arg = ((struct ldap_x_ext_io_fns_rev0 *) optdata)->lextiof_session_arg;
	    ld->ld_extwritev_fn = NULL;
	    if ( ber_sockbuf_set_option( ld->ld_sbp, LBER_SOCKBUF_OPT_EXT_IO_FNS,
					 &(ld->ld_ext_io_fns) ) != 0 ) {
	      LDAP_SET_LDERRNO( ld, LDAP_LOCAL_ERROR, NULL, NULL );
	      rc = -1;
	      break;
	    }
	  }
	  else {
	    /* struct copy */
	    ld->ld_ext_io_fns = *((struct ldap_x_ext_io_fns *) optdata);
	  }
	  if (( rc = nsldapi_install_lber_extiofns( ld, ld->ld_sbp ))
	      != LDAP_SUCCESS ) {
	    LDAP_SET_LDERRNO( ld, rc, NULL, NULL );
	    rc = -1;
	  }
	  break;

	 /* set Socket Arg in extended socket i/o functions*/
	case LDAP_X_OPT_SOCKETARG:
	  if ( ber_sockbuf_set_option( ld->ld_sbp,
	    LBER_SOCKBUF_OPT_SOCK_ARG, (void *)optdata ) != 0 ) {
		LDAP_SET_LDERRNO( ld, LDAP_LOCAL_ERROR, NULL, NULL );
		rc = -1;
	  }
		
	  break;
	  
	/* thread function pointers */
	case LDAP_OPT_THREAD_FN_PTRS:
		/*
		 * It is only safe to set the thread function pointers
		 * when one thread is using the LDAP session handle.
		 */
		/* free existing mutexes (some are allocated by ldap_init()) */
		nsldapi_mutex_free_all( ld );

		/* struct copy */
		ld->ld_thread = *((struct ldap_thread_fns *) optdata);

		/* allocate new mutexes */
		nsldapi_mutex_alloc_all( ld );

		/* LDAP_OPTION_LOCK was never locked... so just return */
		return (rc);

	/* extra thread function pointers */
	case LDAP_OPT_EXTRA_THREAD_FN_PTRS:
	/* The extra thread funcs will only pick up the threadid */
	    ld->ld_thread2  = *((struct ldap_extra_thread_fns *) optdata);
	    
	/* Reset the rest of the structure preserving the threadid fn */
	    ld->ld_mutex_trylock_fn =  (LDAP_TF_MUTEX_TRYLOCK_CALLBACK *)NULL;
	    ld->ld_sema_alloc_fn = (LDAP_TF_SEMA_ALLOC_CALLBACK *) NULL;
	    ld->ld_sema_free_fn = (LDAP_TF_SEMA_FREE_CALLBACK *) NULL;
	    ld->ld_sema_wait_fn = (LDAP_TF_SEMA_WAIT_CALLBACK *) NULL;
	    ld->ld_sema_post_fn = (LDAP_TF_SEMA_POST_CALLBACK *) NULL;

	/* We assume that only one thread is active when replacing */
	/* the threadid function.  We will now proceed and reset all */
	/* of the threadid/refcounts */
	    for( i=0; i<LDAP_MAX_LOCK; i++ ) {
                ld->ld_mutex_threadid[i] = (void *) -1;
                ld->ld_mutex_refcnt[i] = 0;
            }

	/* LDAP_OPTION_LOCK was never locked... so just return */
	    return (rc);

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

	case LDAP_OPT_ERROR_NUMBER:
		LDAP_GET_LDERRNO( ld, &matched, &errstr );
		matched = nsldapi_strdup( matched );
		errstr = nsldapi_strdup( errstr );
		LDAP_SET_LDERRNO( ld, *((int *) optdata), matched, errstr );
		break;

	case LDAP_OPT_ERROR_STRING:
		rc = LDAP_GET_LDERRNO( ld, &matched, NULL );
		matched = nsldapi_strdup( matched );
		LDAP_SET_LDERRNO( ld, rc, matched,
		    nsldapi_strdup((char *) optdata));
		rc = LDAP_SUCCESS;
		break;

	case LDAP_OPT_MATCHED_DN:
		rc = LDAP_GET_LDERRNO( ld, NULL, &errstr );
		errstr = nsldapi_strdup( errstr );
		LDAP_SET_LDERRNO( ld, rc,
		    nsldapi_strdup((char *) optdata), errstr );
		rc = LDAP_SUCCESS;
		break;

	case LDAP_OPT_PREFERRED_LANGUAGE:
		if ( NULL != ld->ld_preferred_language ) {
			NSLDAPI_FREE(ld->ld_preferred_language);
		}
		ld->ld_preferred_language = nsldapi_strdup((char *) optdata);
		break;

	case LDAP_OPT_HOST_NAME:
		if ( NULL != ld->ld_defhost ) {
			NSLDAPI_FREE(ld->ld_defhost);
		}
		ld->ld_defhost = nsldapi_strdup((char *) optdata);
		break;

	case LDAP_X_OPT_CONNECT_TIMEOUT:
		ld->ld_connect_timeout = *((int *) optdata);
		break;

#ifdef LDAP_SASLIO_HOOKS
	/* SASL options */
	case LDAP_OPT_X_SASL_MECH:
		NSLDAPI_FREE(ld->ld_def_sasl_mech);
		ld->ld_def_sasl_mech = nsldapi_strdup((char *) optdata);
		break;
	case LDAP_OPT_X_SASL_REALM:
		NSLDAPI_FREE(ld->ld_def_sasl_realm);
		ld->ld_def_sasl_realm = nsldapi_strdup((char *) optdata);
		break;
	case LDAP_OPT_X_SASL_AUTHCID:
		NSLDAPI_FREE(ld->ld_def_sasl_authcid);
		ld->ld_def_sasl_authcid = nsldapi_strdup((char *) optdata);
		break;
	case LDAP_OPT_X_SASL_AUTHZID:
		NSLDAPI_FREE(ld->ld_def_sasl_authzid);
		ld->ld_def_sasl_authzid = nsldapi_strdup((char *) optdata);
		break;
	case LDAP_OPT_X_SASL_SSF_EXTERNAL:
		{
			int sc;
			sasl_ssf_t extprops;
			sasl_conn_t *ctx;
			if( ld->ld_defconn == NULL ) {
				return -1;
			}
			ctx = (sasl_conn_t *)(ld->ld_defconn->lconn_sasl_ctx);
			if ( ctx == NULL ) {
				return -1;
			}
			memset(&extprops, 0L, sizeof(extprops));
			extprops = * ((sasl_ssf_t *) optdata);
			sc = sasl_setprop( ctx, SASL_SSF_EXTERNAL,
					(void *) &extprops );
			if ( sc != SASL_OK ) {
				return -1;
			}
		}
		break;
	case LDAP_OPT_X_SASL_SECPROPS:
		{
			int sc;
			sc = nsldapi_sasl_secprops( (char *) optdata,
					&ld->ld_sasl_secprops );
			return sc == LDAP_SUCCESS ? 0 : -1;
		}
		break;
	case LDAP_OPT_X_SASL_SSF_MIN:
		ld->ld_sasl_secprops.min_ssf = *((sasl_ssf_t *) optdata);
		break;
	case LDAP_OPT_X_SASL_SSF_MAX:
		ld->ld_sasl_secprops.max_ssf = *((sasl_ssf_t *) optdata);
		break;
	case LDAP_OPT_X_SASL_MAXBUFSIZE:
		ld->ld_sasl_secprops.maxbufsize = *((sasl_ssf_t *) optdata);
		break;
	case LDAP_OPT_X_SASL_SSF:       /* read only */
		LDAP_SET_LDERRNO( ld, LDAP_PARAM_ERROR, NULL, NULL );
		rc = -1;
		break;
#endif

	default:
		LDAP_SET_LDERRNO( ld, LDAP_PARAM_ERROR, NULL, NULL );
		rc = -1;
	}

	if ( ld != &nsldapi_ld_defaults ) {
	    LDAP_MUTEX_UNLOCK( ld, LDAP_OPTION_LOCK );
	}
	return( rc );
}
