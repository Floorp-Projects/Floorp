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
#include "ldap-int.h"

#define LDAP_GET_BITOPT( ld, bit ) \
	((ld)->ld_options & bit ) != 0 ? 1 : 0

static int nsldapi_get_api_info( LDAPAPIInfo *aip );
static int nsldapi_get_feature_info( LDAPAPIFeatureInfo *fip );


int
LDAP_CALL
ldap_get_option( LDAP *ld, int option, void *optdata )
{
	int		rc = 0;

	if ( !nsldapi_initialized ) {
		nsldapi_initialize_defaults();
	}

    /*
     * optdata MUST be a valid pointer...
     */
    if (NULL == optdata)
    {
        return(LDAP_PARAM_ERROR);
    }
	/*
	 * process global options (not associated with an LDAP session handle)
	 */
	if ( option == LDAP_OPT_MEMALLOC_FN_PTRS ) {
		/* struct copy */
		*((struct ldap_memalloc_fns *)optdata) = nsldapi_memalloc_fns;
		return( 0 );
	}

	if ( option == LDAP_OPT_API_INFO ) {
		rc = nsldapi_get_api_info( (LDAPAPIInfo *)optdata );
		if ( rc != LDAP_SUCCESS ) {
			if ( ld != NULL ) {
				LDAP_SET_LDERRNO( ld, rc, NULL, NULL );
			}
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
        *((int *) optdata) = ldap_debug;
#endif /* LDAP_DEBUG */
        return ( 0 );
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


	if (ld != &nsldapi_ld_defaults)
		LDAP_MUTEX_LOCK( ld, LDAP_OPTION_LOCK );
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

	case LDAP_OPT_SSL:
		*((int *) optdata) = LDAP_GET_BITOPT( ld, LDAP_BITOPT_SSL );
		break;

	case LDAP_OPT_RESTART:
		*((int *) optdata) = LDAP_GET_BITOPT( ld, LDAP_BITOPT_RESTART );
		break;

	case LDAP_OPT_RECONNECT:
		*((int *) optdata) =
		    LDAP_GET_BITOPT( ld, LDAP_BITOPT_RECONNECT );
		break;

	case LDAP_OPT_NOREBIND:
		*((int *) optdata) =
		    LDAP_GET_BITOPT( ld, LDAP_BITOPT_NOREBIND );
		break;

#ifdef LDAP_ASYNC_IO
	case LDAP_OPT_ASYNC_CONNECT:
		*((int *) optdata) =
		    LDAP_GET_BITOPT( ld, LDAP_BITOPT_ASYNC );
		break;
#endif /* LDAP_ASYNC_IO */

        /* stuff in the sockbuf */
        case LDAP_X_OPT_SOCKBUF:
                *((Sockbuf **) optdata) = ld->ld_sbp;
                break;

	case LDAP_OPT_DESC:
		if ( ber_sockbuf_get_option( ld->ld_sbp,
		    LBER_SOCKBUF_OPT_DESC, optdata ) != 0 ) {
			LDAP_SET_LDERRNO( ld, LDAP_LOCAL_ERROR, NULL, NULL );
			rc = -1;
		}
		break;

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
		/* fall through */
	case LDAP_OPT_CLIENT_CONTROLS:
		*((LDAPControl ***)optdata) = NULL;
		/* nsldapi_dup_controls returns -1 and sets lderrno on error */
		rc = nsldapi_dup_controls( ld, (LDAPControl ***)optdata,
		    ( option == LDAP_OPT_SERVER_CONTROLS ) ?
		    ld->ld_servercontrols : ld->ld_clientcontrols );
		break;

	/* rebind proc */
	case LDAP_OPT_REBIND_FN:
		*((LDAP_REBINDPROC_CALLBACK **) optdata) = ld->ld_rebind_fn;
		break;
	case LDAP_OPT_REBIND_ARG:
		*((void **) optdata) = ld->ld_rebind_arg;
		break;

	/* i/o function pointers */
	case LDAP_OPT_IO_FN_PTRS:
		if ( ld->ld_io_fns_ptr == NULL ) {
			memset( optdata, 0, sizeof( struct ldap_io_fns ));
		} else {
			/* struct copy */
			*((struct ldap_io_fns *)optdata) = *(ld->ld_io_fns_ptr);
		}
		break;

	/* extended i/o function pointers */
	case LDAP_X_OPT_EXTIO_FN_PTRS:
	  if ( ((struct ldap_x_ext_io_fns *) optdata)->lextiof_size == LDAP_X_EXTIO_FNS_SIZE_REV0) {
	    ((struct ldap_x_ext_io_fns_rev0 *) optdata)->lextiof_close = ld->ld_extclose_fn;
	    ((struct ldap_x_ext_io_fns_rev0 *) optdata)->lextiof_connect = ld->ld_extconnect_fn;
	    ((struct ldap_x_ext_io_fns_rev0 *) optdata)->lextiof_read = ld->ld_extread_fn;
	    ((struct ldap_x_ext_io_fns_rev0 *) optdata)->lextiof_write = ld->ld_extwrite_fn;
	    ((struct ldap_x_ext_io_fns_rev0 *) optdata)->lextiof_poll = ld->ld_extpoll_fn;
	    ((struct ldap_x_ext_io_fns_rev0 *) optdata)->lextiof_newhandle = ld->ld_extnewhandle_fn;
	    ((struct ldap_x_ext_io_fns_rev0 *) optdata)->lextiof_disposehandle = ld->ld_extdisposehandle_fn;
	    ((struct ldap_x_ext_io_fns_rev0 *) optdata)->lextiof_session_arg = ld->ld_ext_session_arg;
	  } else if ( ((struct ldap_x_ext_io_fns *) optdata)->lextiof_size ==
		      LDAP_X_EXTIO_FNS_SIZE ) {
	    /* struct copy */
	    *((struct ldap_x_ext_io_fns *) optdata) = ld->ld_ext_io_fns;
	  } else {       
	    LDAP_SET_LDERRNO( ld, LDAP_PARAM_ERROR, NULL, NULL );
	    rc = -1;
	  }
	  break;
	  
	/* get socketargp in extended i/o function */
	case LDAP_X_OPT_SOCKETARG:
	  if ( ber_sockbuf_get_option( ld->ld_sbp,LBER_SOCKBUF_OPT_SOCK_ARG, optdata)	    
		 != 0 ) {
		LDAP_SET_LDERRNO( ld, LDAP_LOCAL_ERROR, NULL, NULL );
		rc = -1;
	   }
	   	break;

	/* thread function pointers */
	case LDAP_OPT_THREAD_FN_PTRS:
		/* struct copy */
		*((struct ldap_thread_fns *) optdata) = ld->ld_thread;
		break;

	/* extra thread function pointers */
	case LDAP_OPT_EXTRA_THREAD_FN_PTRS:
		/* struct copy */
		*((struct ldap_extra_thread_fns *) optdata) = ld->ld_thread2;
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
		*((char **) optdata) = nsldapi_strdup( *((char **) optdata ));
		break;

	case LDAP_OPT_MATCHED_DN:
		(void)LDAP_GET_LDERRNO( ld, (char **)optdata, NULL );
		*((char **) optdata) = nsldapi_strdup( *((char **) optdata ));
		break;

	case LDAP_OPT_PREFERRED_LANGUAGE:
		if ( NULL != ld->ld_preferred_language ) {
			*((char **) optdata) =
			    nsldapi_strdup(ld->ld_preferred_language);
		} else {
			*((char **) optdata) = NULL;
		}
		break;

	case LDAP_OPT_API_FEATURE_INFO:
		rc = nsldapi_get_feature_info( (LDAPAPIFeatureInfo *)optdata );
		if ( rc != LDAP_SUCCESS ) {
			LDAP_SET_LDERRNO( ld, rc, NULL, NULL );
			rc = -1;
		}
		break;

	case LDAP_OPT_HOST_NAME:
		*((char **) optdata) = nsldapi_strdup( ld->ld_defhost );
		break;

        case LDAP_X_OPT_CONNECT_TIMEOUT:
                *((int *) optdata) = ld->ld_connect_timeout;
                break;

#ifdef LDAP_SASLIO_HOOKS
	/* SASL options */
	case LDAP_OPT_X_SASL_MECH:
		*((char **) optdata) = nsldapi_strdup(ld->ld_def_sasl_mech);
		break;
	case LDAP_OPT_X_SASL_REALM:
		*((char **) optdata) = nsldapi_strdup(ld->ld_def_sasl_realm);
		break;
	case LDAP_OPT_X_SASL_AUTHCID:
		*((char **) optdata) = nsldapi_strdup(ld->ld_def_sasl_authcid);
		break;
	case LDAP_OPT_X_SASL_AUTHZID:
		*((char **) optdata) = nsldapi_strdup(ld->ld_def_sasl_authzid);
		break;
	case LDAP_OPT_X_SASL_SSF:
		{
			int sc;
			sasl_ssf_t      *ssf;
			sasl_conn_t     *ctx;
			if( ld->ld_defconn == NULL ) {
				return -1;
			}
			ctx = (sasl_conn_t *)(ld->ld_defconn->lconn_sasl_ctx);
			if ( ctx == NULL ) {
				return -1;
			}
			sc = sasl_getprop( ctx, SASL_SSF, (const void **) &ssf );
			if ( sc != SASL_OK ) {
				return -1;
			}
			*((sasl_ssf_t *) optdata) = *ssf;
		}
		break;
	case LDAP_OPT_X_SASL_SSF_MIN:
		*((sasl_ssf_t *) optdata) = ld->ld_sasl_secprops.min_ssf;
		break;
	case LDAP_OPT_X_SASL_SSF_MAX:
		*((sasl_ssf_t *) optdata) = ld->ld_sasl_secprops.max_ssf;
		break;
	case LDAP_OPT_X_SASL_MAXBUFSIZE:
		*((sasl_ssf_t *) optdata) = ld->ld_sasl_secprops.maxbufsize;
		break;
	case LDAP_OPT_X_SASL_SSF_EXTERNAL:
	case LDAP_OPT_X_SASL_SECPROPS:
		/*
		 * These options are write only.  Making these options
		 * read/write would expose semi-private interfaces of libsasl
		 * for which there are no cross platform/standardized
		 * definitions.
		 */
		LDAP_SET_LDERRNO( ld, LDAP_PARAM_ERROR, NULL, NULL );
		rc = -1;
		break;
#endif

	default:
		LDAP_SET_LDERRNO( ld, LDAP_PARAM_ERROR, NULL, NULL );
		rc = -1;
	}
	if (ld != &nsldapi_ld_defaults)
		LDAP_MUTEX_UNLOCK( ld, LDAP_OPTION_LOCK  );
	return( rc );
}


/*
 * Table of extended API features we support.
 * The first field is the version of the info. strcuture itself; we do not
 * use the ones from this table so it is okay to leave as zero.
 */
static LDAPAPIFeatureInfo nsldapi_extensions[] = {
    { 0, "SERVER_SIDE_SORT",		LDAP_API_FEATURE_SERVER_SIDE_SORT },
    { 0, "VIRTUAL_LIST_VIEW",		LDAP_API_FEATURE_VIRTUAL_LIST_VIEW },
    { 0, "PERSISTENT_SEARCH",		LDAP_API_FEATURE_PERSISTENT_SEARCH },
    { 0, "PROXY_AUTHORIZATION",		LDAP_API_FEATURE_PROXY_AUTHORIZATION },
    { 0, "X_LDERRNO",			LDAP_API_FEATURE_X_LDERRNO },
    { 0, "X_MEMCACHE",			LDAP_API_FEATURE_X_MEMCACHE },
    { 0, "X_IO_FUNCTIONS",		LDAP_API_FEATURE_X_IO_FUNCTIONS },
    { 0, "X_EXTIO_FUNCTIONS",		LDAP_API_FEATURE_X_EXTIO_FUNCTIONS },
    { 0, "X_DNS_FUNCTIONS",		LDAP_API_FEATURE_X_DNS_FUNCTIONS },
    { 0, "X_MEMALLOC_FUNCTIONS",	LDAP_API_FEATURE_X_MEMALLOC_FUNCTIONS },
    { 0, "X_THREAD_FUNCTIONS",		LDAP_API_FEATURE_X_THREAD_FUNCTIONS },
    { 0, "X_EXTHREAD_FUNCTIONS",	LDAP_API_FEATURE_X_EXTHREAD_FUNCTIONS },
    { 0, "X_GETLANGVALUES",		LDAP_API_FEATURE_X_GETLANGVALUES },
    { 0, "X_CLIENT_SIDE_SORT",		LDAP_API_FEATURE_X_CLIENT_SIDE_SORT },
    { 0, "X_URL_FUNCTIONS",		LDAP_API_FEATURE_X_URL_FUNCTIONS },
    { 0, "X_FILTER_FUNCTIONS",		LDAP_API_FEATURE_X_FILTER_FUNCTIONS },
};

#define NSLDAPI_EXTENSIONS_COUNT	\
	(sizeof(nsldapi_extensions)/sizeof(LDAPAPIFeatureInfo))

/*
 * Retrieve information about this implementation of the LDAP API.
 * Returns an LDAP error code.
 */
static int
nsldapi_get_api_info( LDAPAPIInfo *aip )
{
	int	i;

	if ( aip == NULL ) {
		return( LDAP_PARAM_ERROR );
	}

	aip->ldapai_api_version = LDAP_API_VERSION;

	if ( aip->ldapai_info_version != LDAP_API_INFO_VERSION ) {
		aip->ldapai_info_version = LDAP_API_INFO_VERSION;
		return( LDAP_PARAM_ERROR );
	}

	aip->ldapai_protocol_version = LDAP_VERSION_MAX;
	aip->ldapai_vendor_version = LDAP_VENDOR_VERSION;

	if (( aip->ldapai_vendor_name = nsldapi_strdup( LDAP_VENDOR_NAME ))
	    == NULL ) {
		return( LDAP_NO_MEMORY );
	}

	if ( NSLDAPI_EXTENSIONS_COUNT < 1 ) {
		aip->ldapai_extensions = NULL;
	} else {
		if (( aip->ldapai_extensions = NSLDAPI_CALLOC(
		    NSLDAPI_EXTENSIONS_COUNT + 1, sizeof(char *))) == NULL ) {
			NSLDAPI_FREE( aip->ldapai_vendor_name );
			aip->ldapai_vendor_name = NULL;
			return( LDAP_NO_MEMORY );
		}

		for ( i = 0; i < NSLDAPI_EXTENSIONS_COUNT; ++i ) {
			if (( aip->ldapai_extensions[i] = nsldapi_strdup(
			    nsldapi_extensions[i].ldapaif_name )) == NULL ) {
				ldap_value_free( aip->ldapai_extensions );
				NSLDAPI_FREE( aip->ldapai_vendor_name );
				aip->ldapai_extensions = NULL;
				aip->ldapai_vendor_name = NULL;
				return( LDAP_NO_MEMORY );
			}
		}
	}

	return( LDAP_SUCCESS );
}


/*
 * Retrieves information about a specific extended feature of the LDAP API/
 * Returns an LDAP error code.
 */
static int
nsldapi_get_feature_info( LDAPAPIFeatureInfo *fip )
{
	int	i;

	if ( fip == NULL || fip->ldapaif_name == NULL ) {
		return( LDAP_PARAM_ERROR );
	}

	if ( fip->ldapaif_info_version != LDAP_FEATURE_INFO_VERSION ) {
		fip->ldapaif_info_version = LDAP_FEATURE_INFO_VERSION;
		return( LDAP_PARAM_ERROR );
	}

	for ( i = 0; i < NSLDAPI_EXTENSIONS_COUNT; ++i ) {
		if ( strcmp( fip->ldapaif_name,
		    nsldapi_extensions[i].ldapaif_name ) == 0 ) {
			fip->ldapaif_version =
			    nsldapi_extensions[i].ldapaif_version;
			break;
		}
	}

	return(( i < NSLDAPI_EXTENSIONS_COUNT ) ? LDAP_SUCCESS
	    : LDAP_PARAM_ERROR );
}
