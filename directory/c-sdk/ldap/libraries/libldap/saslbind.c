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

#ifdef LDAP_SASLIO_HOOKS
/*
 * Global SASL Init data
 */

static int
nsldapi_sasl_fail()
{
        return( SASL_FAIL );
}

static sasl_callback_t client_callbacks[] = {
        { SASL_CB_GETOPT, nsldapi_sasl_fail, NULL },
        { SASL_CB_GETREALM, NULL, NULL },
        { SASL_CB_USER, NULL, NULL },
        { SASL_CB_CANON_USER, NULL, NULL },
        { SASL_CB_AUTHNAME, NULL, NULL },
        { SASL_CB_PASS, NULL, NULL },
        { SASL_CB_ECHOPROMPT, NULL, NULL },
        { SASL_CB_NOECHOPROMPT, NULL, NULL },
        { SASL_CB_LIST_END, NULL, NULL }
};

static int nsldapi_sasl_inited = 0;

static int
nsldapi_sasl_init( void )
{
        if ( nsldapi_sasl_inited ) {
                return( 0 );
        }

        sasl_set_alloc(
                (sasl_malloc_t *)ldap_x_malloc,
                (sasl_calloc_t *)ldap_x_calloc,
                (sasl_realloc_t *)ldap_x_realloc,
                (sasl_free_t *)ldap_x_free );

        if ( sasl_client_init( client_callbacks ) == SASL_OK ) {
                nsldapi_sasl_inited = 1;
                return( 0 );
        }

        return( -1 );
}

int
nsldapi_sasl_is_inited()
{
        return( nsldapi_sasl_inited );
}

int
nsldapi_sasl_cvterrno( LDAP *ld, int err, char *msg )
{
        int rc = LDAP_LOCAL_ERROR;

        switch (err) {
        case SASL_OK:
                rc = LDAP_SUCCESS;
                break;
        case SASL_NOMECH:
                rc = LDAP_AUTH_UNKNOWN;
                break;
        case SASL_BADSERV:
                rc = LDAP_CONNECT_ERROR;
                break;
        case SASL_DISABLED:
        case SASL_ENCRYPT:
        case SASL_EXPIRED:
        case SASL_NOUSERPASS:
        case SASL_NOVERIFY:
        case SASL_PWLOCK:
        case SASL_TOOWEAK:
        case SASL_UNAVAIL:
        case SASL_WEAKPASS:
                rc = LDAP_INAPPROPRIATE_AUTH;
                break;
        case SASL_BADAUTH:
        case SASL_NOAUTHZ:
                rc = LDAP_INVALID_CREDENTIALS;
                break;
        case SASL_NOMEM:
                rc = LDAP_NO_MEMORY;
                break;
        case SASL_NOUSER:
                rc = LDAP_NO_SUCH_OBJECT;
                break;
        case SASL_CONTINUE:
        case SASL_FAIL:
        case SASL_INTERACT:
        default:
                rc = LDAP_LOCAL_ERROR;
                break;
        }

        LDAP_SET_LDERRNO( ld, rc, NULL, msg );
        return( rc );
}

#ifdef LDAP_SASLIO_GET_MECHS_FROM_SERVER
/*
 * Get available SASL Mechanisms supported by the server
 */

static int
nsldapi_get_sasl_mechs ( LDAP *ld, char **pmech )
{
        char *attr[] = { "supportedSASLMechanisms", NULL };
        char **values, **v, *mech, *m;
        LDAPMessage *res, *e;
        struct timeval  timeout;
        int slen, rc;

        if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
                return( LDAP_PARAM_ERROR );
        }

        timeout.tv_sec = SEARCH_TIMEOUT_SECS;
        timeout.tv_usec = 0;

        rc = ldap_search_st( ld, "", LDAP_SCOPE_BASE,
                "objectclass=*", attr, 0, &timeout, &res );

        if ( rc != LDAP_SUCCESS ) {
                return( LDAP_GET_LDERRNO( ld, NULL, NULL ) );
        }

        e = ldap_first_entry( ld, res );
        if ( e == NULL ) {
                ldap_msgfree( res );
                if ( ld->ld_errno == LDAP_SUCCESS ) {
                        LDAP_SET_LDERRNO( ld, LDAP_NO_SUCH_OBJECT, NULL, NULL );
                }
                return( LDAP_GET_LDERRNO( ld, NULL, NULL ) );
        }

        values = ldap_get_values( ld, e, "supportedSASLMechanisms" );
        if ( values == NULL ) {
                ldap_msgfree( res );
                LDAP_SET_LDERRNO( ld, LDAP_NO_SUCH_ATTRIBUTE, NULL, NULL );
                return( LDAP_NO_SUCH_ATTRIBUTE );
        }

        slen = 0;
        for(v = values; *v != NULL; v++ ) {
                slen += strlen(*v) + 1;
        }
        if ( (mech = NSLDAPI_CALLOC(1, slen)) == NULL) {
                ldap_value_free( values );
                ldap_msgfree( res );
                LDAP_SET_LDERRNO( ld, LDAP_NO_MEMORY, NULL, NULL );
                return( LDAP_NO_MEMORY );
        }
        m = mech;
        for(v = values; *v; v++) {
                if (v != values) {
                        *m++ = ' ';
                }
                slen = strlen(*v);
                strncpy(m, *v, slen);
                m += slen;
        }
        *m = '\0';

        ldap_value_free( values );
        ldap_msgfree( res );

        *pmech = mech;

        return( LDAP_SUCCESS );
}
#endif /* LDAP_SASLIO_GET_MECHS_FROM_SERVER */

int
nsldapi_sasl_secprops(
        const char *in,
        sasl_security_properties_t *secprops )
{
        int i;
        char **props = NULL;
        char *inp;
        unsigned sflags = 0;
        sasl_ssf_t max_ssf = 0;
        sasl_ssf_t min_ssf = 0;
        unsigned maxbufsize = 0;
        int got_sflags = 0;
        int got_max_ssf = 0;
        int got_min_ssf = 0;
        int got_maxbufsize = 0;

        if (in == NULL) {
                return LDAP_PARAM_ERROR;
        }
        inp = nsldapi_strdup(in);
        if (inp == NULL) {
                return LDAP_PARAM_ERROR;
        }
        props = ldap_str2charray( inp, "," );
        NSLDAPI_FREE( inp );

        if( props == NULL || secprops == NULL ) {
                return LDAP_PARAM_ERROR;
        }

        for( i=0; props[i]; i++ ) {
                if( strcasecmp(props[i], "none") == 0 ) {
                        got_sflags++;

                } else if( strcasecmp(props[i], "noactive") == 0 ) {
                        got_sflags++;
                        sflags |= SASL_SEC_NOACTIVE;

                } else if( strcasecmp(props[i], "noanonymous") == 0 ) {
                        got_sflags++;
                        sflags |= SASL_SEC_NOANONYMOUS;

                } else if( strcasecmp(props[i], "nodict") == 0 ) {
                        got_sflags++;
                        sflags |= SASL_SEC_NODICTIONARY;

                } else if( strcasecmp(props[i], "noplain") == 0 ) {
                        got_sflags++;
                        sflags |= SASL_SEC_NOPLAINTEXT;

                } else if( strcasecmp(props[i], "forwardsec") == 0 ) {
                        got_sflags++;
                        sflags |= SASL_SEC_FORWARD_SECRECY;

                } else if( strcasecmp(props[i], "passcred") == 0 ) {
                        got_sflags++;
                        sflags |= SASL_SEC_PASS_CREDENTIALS;

                } else if( strncasecmp(props[i],
                        "minssf=", sizeof("minssf")) == 0 ) {
                        if( isdigit( props[i][sizeof("minssf")] ) ) {
                                got_min_ssf++;
                                min_ssf = atoi( &props[i][sizeof("minssf")] );
                        } else {
                                return LDAP_NOT_SUPPORTED;
                        }

                } else if( strncasecmp(props[i],
                        "maxssf=", sizeof("maxssf")) == 0 ) {
                        if( isdigit( props[i][sizeof("maxssf")] ) ) {
                                got_max_ssf++;
                                max_ssf = atoi( &props[i][sizeof("maxssf")] );
                        } else {
                                return LDAP_NOT_SUPPORTED;
                        }

                } else if( strncasecmp(props[i],
                        "maxbufsize=", sizeof("maxbufsize")) == 0 ) {
                        if( isdigit( props[i][sizeof("maxbufsize")] ) ) {
                                got_maxbufsize++;
                                maxbufsize = atoi( &props[i][sizeof("maxbufsize")] );
                                if( maxbufsize &&
                                    (( maxbufsize < SASL_MIN_BUFF_SIZE )
                                    || (maxbufsize > SASL_MAX_BUFF_SIZE ))) {
                                        return( LDAP_PARAM_ERROR );
                                }
                        } else {
                                return( LDAP_NOT_SUPPORTED );
                        }
                } else {
                        return( LDAP_NOT_SUPPORTED );
                }
        }

        if(got_sflags) {
                secprops->security_flags = sflags;
        }
        if(got_min_ssf) {
                secprops->min_ssf = min_ssf;
        }
        if(got_max_ssf) {
                secprops->max_ssf = max_ssf;
        }
        if(got_maxbufsize) {
                secprops->maxbufsize = maxbufsize;
        }

        ldap_charray_free( props );
        return( LDAP_SUCCESS );
}
#endif /* LDAP_SASLIO_HOOKS */

static int
nsldapi_sasl_bind_s(
    LDAP                *ld,
    const char          *dn,
    const char          *mechanism,
    const struct berval *cred,
    LDAPControl         **serverctrls,
    LDAPControl         **clientctrls,
    struct berval       **servercredp,
    LDAPControl         ***responsectrls
)
{
        int             err, msgid;
        LDAPMessage     *result;

        LDAPDebug( LDAP_DEBUG_TRACE, "nsldapi_sasl_bind_s\n", 0, 0, 0 );

        if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
                return( LDAP_PARAM_ERROR );
        }

        if ( NSLDAPI_LDAP_VERSION( ld ) < LDAP_VERSION3 ) {
                LDAP_SET_LDERRNO( ld, LDAP_NOT_SUPPORTED, NULL, NULL );
                return( LDAP_NOT_SUPPORTED );
        }

        if ( ( err = ldap_sasl_bind( ld, dn, mechanism, cred, serverctrls,
            clientctrls, &msgid )) != LDAP_SUCCESS )
                return( err );

        if ( ldap_result( ld, msgid, 1, (struct timeval *) 0, &result ) == -1 )
                return( LDAP_GET_LDERRNO( ld, NULL, NULL ) );

        /* Get the controls sent by the server if requested */
        if ( responsectrls ) {
                if ( ( err = ldap_parse_result( ld, result, &err, NULL, NULL,
                       NULL, responsectrls, 0 )) != LDAP_SUCCESS )
                    return( err );
        }

        err = ldap_parse_sasl_bind_result( ld, result, servercredp, 0 );
        if (err != LDAP_SUCCESS  && err != LDAP_SASL_BIND_IN_PROGRESS) {
                ldap_msgfree( result );
                return( err );
        }

        return( ldap_result2error( ld, result, 1 ) );
}

#ifdef LDAP_SASLIO_HOOKS
static int
nsldapi_sasl_do_bind( LDAP *ld, const char *dn,
        const char *mechs, unsigned flags,
        LDAP_SASL_INTERACT_PROC *callback, void *defaults,
        LDAPControl **sctrl, LDAPControl **cctrl, LDAPControl ***rctrl )
{
        sasl_interact_t *prompts = NULL;
        sasl_conn_t     *ctx = NULL;
        sasl_ssf_t      *ssf = NULL;
        const char      *mech = NULL;
        int             saslrc, rc;
        struct berval   ccred;
        unsigned        credlen;
        int stepnum = 1;
        char *sasl_username = NULL;

        if (rctrl) {
            /* init to NULL so we can call ldap_controls_free below */
            *rctrl = NULL;
        }

        if (NSLDAPI_LDAP_VERSION( ld ) < LDAP_VERSION3) {
                LDAP_SET_LDERRNO( ld, LDAP_NOT_SUPPORTED, NULL, NULL );
                return( LDAP_NOT_SUPPORTED );
        }

        /* shouldn't happen */
        if (callback == NULL) {
                return( LDAP_LOCAL_ERROR );
        }

        if ( (rc = nsldapi_sasl_open(ld, NULL, &ctx, 0)) != LDAP_SUCCESS ) {
            return( rc );
        }

        ccred.bv_val = NULL;
        ccred.bv_len = 0;

        LDAPDebug(LDAP_DEBUG_TRACE, "Starting SASL/%s authentication\n",
                  (mechs ? mechs : ""), 0, 0 );

        do {
                saslrc = sasl_client_start( ctx,
                        mechs,
                        &prompts,
                        (const char **)&ccred.bv_val,
                        &credlen,
                        &mech );

                LDAPDebug(LDAP_DEBUG_TRACE, "Doing step %d of client start for SASL/%s authentication\n",
                          stepnum, (mech ? mech : ""), 0 );
                stepnum++;

                if( saslrc == SASL_INTERACT &&
                    (callback)(ld, flags, defaults, prompts) != LDAP_SUCCESS ) {
                        break;
                }
        } while ( saslrc == SASL_INTERACT );

        ccred.bv_len = credlen;

        if ( (saslrc != SASL_OK) && (saslrc != SASL_CONTINUE) ) {
                return( nsldapi_sasl_cvterrno( ld, saslrc, nsldapi_strdup( sasl_errdetail( ctx ) ) ) );
        }

        stepnum = 1;

        do {
                struct berval *scred;
                int clientstepnum = 1;

                scred = NULL;

                if (rctrl) {
                    /* if we're looping again, we need to free any controls set
                       during the previous loop */
                    /* NOTE that this assumes we only care about the controls
                       returned by the last call to nsldapi_sasl_bind_s - if
                       we care about _all_ controls, we will have to figure out
                       some way to append them each loop go round */
                    ldap_controls_free(*rctrl);
                    *rctrl = NULL;
                }

                LDAPDebug(LDAP_DEBUG_TRACE, "Doing step %d of bind for SASL/%s authentication\n",
                          stepnum, (mech ? mech : ""), 0 );
                stepnum++;

                /* notify server of a sasl bind step */
                rc = nsldapi_sasl_bind_s(ld, dn, mech, &ccred,
                                      sctrl, cctrl, &scred, rctrl); 

                if ( ccred.bv_val != NULL ) {
                        ccred.bv_val = NULL;
                }

                if ( rc != LDAP_SUCCESS && rc != LDAP_SASL_BIND_IN_PROGRESS ) {
                        ber_bvfree( scred );
                        return( rc );
                }

                if( rc == LDAP_SUCCESS && saslrc == SASL_OK ) {
                        /* we're done, no need to step */
                        if( scred ) {
                            if ( scred->bv_len == 0 ) { /* MS AD sends back empty screds */
                                LDAPDebug(LDAP_DEBUG_ANY,
                                          "SASL BIND complete - ignoring empty credential response\n",
                                          0, 0, 0);
                                ber_bvfree( scred );
                            } else {
                                /* but server provided us with data! */
                                LDAPDebug(LDAP_DEBUG_TRACE,
                                          "SASL BIND complete but invalid because server responded with credentials - length [%u]\n",
                                          scred->bv_len, 0, 0);
                                ber_bvfree( scred );
                                LDAP_SET_LDERRNO( ld, LDAP_LOCAL_ERROR,
                                                  NULL, "Error during SASL handshake - invalid server credential response" );
                                return( LDAP_LOCAL_ERROR );
                            }
                        }
                        break;
                }

                /* perform the next step of the sasl bind */
                do {
                        LDAPDebug(LDAP_DEBUG_TRACE, "Doing client step %d of bind step %d for SASL/%s authentication\n",
                                  clientstepnum, stepnum, (mech ? mech : "") );
                        clientstepnum++;
                        saslrc = sasl_client_step( ctx,
                                (scred == NULL) ? NULL : scred->bv_val,
                                (scred == NULL) ? 0 : scred->bv_len,
                                &prompts,
                                (const char **)&ccred.bv_val,
                                &credlen );

                        if( saslrc == SASL_INTERACT &&
                            (callback)(ld, flags, defaults, prompts)
                                                        != LDAP_SUCCESS ) {
                                break;
                        }
                } while ( saslrc == SASL_INTERACT );

                ccred.bv_len = credlen;
                ber_bvfree( scred );

                if ( (saslrc != SASL_OK) && (saslrc != SASL_CONTINUE) ) {
                        return( nsldapi_sasl_cvterrno( ld, saslrc, nsldapi_strdup( sasl_errdetail( ctx ) ) ) );
                }
        } while ( rc == LDAP_SASL_BIND_IN_PROGRESS );

        if ( rc != LDAP_SUCCESS ) {
                return( rc );
        }

        if ( saslrc != SASL_OK ) {
                return( nsldapi_sasl_cvterrno( ld, saslrc, nsldapi_strdup( sasl_errdetail( ctx ) ) ) );
        }

        saslrc = sasl_getprop( ctx, SASL_USERNAME, (const void **) &sasl_username );
        if ( (saslrc == SASL_OK) && sasl_username ) {
                LDAPDebug(LDAP_DEBUG_TRACE, "SASL identity: %s\n", sasl_username, 0, 0);
        }

        saslrc = sasl_getprop( ctx, SASL_SSF, (const void **) &ssf );
        if( saslrc == SASL_OK ) {
                if( ssf && *ssf ) {
                        LDAPDebug(LDAP_DEBUG_TRACE,
                                "SASL install encryption, for SSF: %lu\n",
                                (unsigned long) *ssf, 0, 0 );
                        nsldapi_sasl_install( ld, NULL );
                }
        }

        return( rc );
}
#endif /* LDAP_SASLIO_HOOKS */


/*
 * ldap_sasl_bind - authenticate to the ldap server.  The dn, mechanism,
 * and credentials of the entry to which to bind are supplied. An LDAP
 * error code is returned and if LDAP_SUCCESS is returned *msgidp is set
 * to the id of the request initiated.
 *
 * Example:
 *	struct berval	creds;
 *	LDAPControl	**ctrls;
 *	int		err, msgid;
 *	... fill in creds with credentials ...
 *	... fill in ctrls with server controls ...
 *	err = ldap_sasl_bind( ld, "cn=manager, o=university of michigan, c=us",
 *	    "mechanismname", &creds, ctrls, NULL, &msgid );
 */
int
LDAP_CALL
ldap_sasl_bind(
    LDAP		*ld,
    const char		*dn,
    const char		*mechanism,
    const struct berval	*cred,
    LDAPControl		**serverctrls,
    LDAPControl		**clientctrls,
    int			*msgidp
)
{
	BerElement	*ber;
	int		rc, simple, msgid, ldapversion;

	/*
	 * The ldapv3 bind request looks like this:
	 *	BindRequest ::= SEQUENCE {
	 *		version		INTEGER,
	 *		name		DistinguishedName,	 -- who
	 *		authentication	CHOICE {
	 *			simple		[0] OCTET STRING, -- passwd
	 *			sasl		[3] SaslCredentials -- v3 only
	 *		}
	 *	}
	 *	SaslCredentials ::= SEQUENCE {
	 *		mechanism	LDAPString,
	 * 		credentials	OCTET STRING
	 *	}
	 * all wrapped up in an LDAPMessage sequence.
	 */

	LDAPDebug( LDAP_DEBUG_TRACE, "ldap_sasl_bind\n", 0, 0, 0 );
	
	if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
		return( LDAP_PARAM_ERROR );
	}
	
	if ( msgidp == NULL ) {
		LDAP_SET_LDERRNO( ld, LDAP_PARAM_ERROR, NULL, NULL );
		return( LDAP_PARAM_ERROR );
	}
	
	if ( ( ld->ld_options & LDAP_BITOPT_RECONNECT ) != 0 ) {
		nsldapi_handle_reconnect( ld );
	}	

	simple = ( mechanism == LDAP_SASL_SIMPLE );
	ldapversion = NSLDAPI_LDAP_VERSION( ld );

	/* only ldapv3 or higher can do sasl binds */
	if ( !simple && ldapversion < LDAP_VERSION3 ) {
		LDAP_SET_LDERRNO( ld, LDAP_NOT_SUPPORTED, NULL, NULL );
		return( LDAP_NOT_SUPPORTED );
	}

	LDAP_MUTEX_LOCK( ld, LDAP_MSGID_LOCK );
	msgid = ++ld->ld_msgid;
	LDAP_MUTEX_UNLOCK( ld, LDAP_MSGID_LOCK );

	if ( dn == NULL )
		dn = "";

	if ( ld->ld_cache_on && ld->ld_cache_bind != NULL ) {
		LDAP_MUTEX_LOCK( ld, LDAP_CACHE_LOCK );
		if ( (rc = (ld->ld_cache_bind)( ld, msgid, LDAP_REQ_BIND, dn,
		    cred, LDAP_AUTH_SASL )) != 0 ) {
			*msgidp = rc;
			LDAP_MUTEX_UNLOCK( ld, LDAP_CACHE_LOCK );
			return( LDAP_SUCCESS );
		}
		LDAP_MUTEX_UNLOCK( ld, LDAP_CACHE_LOCK );
	}

	/* create a message to send */
	if (( rc = nsldapi_alloc_ber_with_options( ld, &ber ))
	    != LDAP_SUCCESS ) {
		return( rc );
	}

	/* fill it in */
	if ( simple ) {		/* simple bind; works in LDAPv2 or v3 */
		struct berval	tmpcred;

		if ( cred == NULL ) {
			tmpcred.bv_val = "";
			tmpcred.bv_len = 0;
			cred = &tmpcred;
		}
		rc = ber_printf( ber, "{it{isto}", msgid, LDAP_REQ_BIND,
		    ldapversion, dn, LDAP_AUTH_SIMPLE, cred->bv_val,
		    cred->bv_len );

	} else {		/* SASL bind; requires LDAPv3 or better */
		if ( cred == NULL ) {
			rc = ber_printf( ber, "{it{ist{s}}", msgid,
			    LDAP_REQ_BIND, ldapversion, dn, LDAP_AUTH_SASL,
			    mechanism );
		} else {
			rc = ber_printf( ber, "{it{ist{so}}", msgid,
			    LDAP_REQ_BIND, ldapversion, dn, LDAP_AUTH_SASL,
			    mechanism, cred->bv_val,
			    cred->bv_len );
		}
	}

	if ( rc == -1 ) {
		LDAP_SET_LDERRNO( ld, LDAP_ENCODING_ERROR, NULL, NULL );
		ber_free( ber, 1 );
		return( LDAP_ENCODING_ERROR );
	}

	if ( (rc = nsldapi_put_controls( ld, serverctrls, 1, ber ))
	    != LDAP_SUCCESS ) {
		ber_free( ber, 1 );
		return( rc );
	}

	/* send the message */
	rc = nsldapi_send_initial_request( ld, msgid, LDAP_REQ_BIND,
		(char *)dn, ber );
	*msgidp = rc;
	return( rc < 0 ? LDAP_GET_LDERRNO( ld, NULL, NULL ) : LDAP_SUCCESS );
}

/*
 * ldap_sasl_bind_s - bind to the ldap server using sasl authentication
 * The dn, mechanism, and credentials of the entry to which to bind are
 * supplied.  LDAP_SUCCESS is returned upon success, the ldap error code
 * otherwise.
 *
 * Example:
 *	struct berval	creds;
 *	... fill in creds with credentials ...
 *	ldap_sasl_bind_s( ld, "cn=manager, o=university of michigan, c=us",
 *	    "mechanismname", &creds )
 */
int
LDAP_CALL
ldap_sasl_bind_s(
    LDAP		*ld,
    const char		*dn,
    const char		*mechanism,
    const struct berval	*cred,
    LDAPControl		**serverctrls,
    LDAPControl		**clientctrls,
    struct berval	**servercredp
)
{
        return ( nsldapi_sasl_bind_s( ld, dn, mechanism, cred,
                 serverctrls, clientctrls, servercredp, NULL ) );
}

#ifdef LDAP_SASLIO_HOOKS 
/*
 * SASL Authentication Interface: ldap_sasl_interactive_bind_s
 *
 * This routine takes a DN, SASL mech list, and a SASL callback
 * and performs the necessary sequencing to complete a SASL bind
 * to the LDAP connection ld.  The user provided callback can
 * use an optionally provided set of default values to complete
 * any necessary interactions.
 *
 * Currently imposes the following restrictions:
 *   A mech list must be provided
 *   LDAP_SASL_INTERACTIVE mode requires a callback
 */
int
LDAP_CALL
ldap_sasl_interactive_bind_s( LDAP *ld, const char *dn,
        const char *saslMechanism,
        LDAPControl **sctrl, LDAPControl **cctrl, unsigned flags,
        LDAP_SASL_INTERACT_PROC *callback, void *defaults )
{
        return ldap_sasl_interactive_bind_ext_s( ld, dn, 
                saslMechanism, sctrl, cctrl, flags, callback,
                defaults, NULL );
}

/*
 * ldap_sasl_interactive_bind_ext_s
 *
 * This function extends ldap_sasl_interactive_bind_s by allowing
 * controls received from the server to be returned as rctrl. The
 * caller must pass in a valid LDAPControl** pointer and free the
 * array of controls when finished with them e.g.
 * LDAPControl **retctrls = NULL;
 * ...
 * ldap_sasl_interactive_bind_ext_s(ld, ...., &retctrls);
 * ...
 * ldap_controls_free(retctrls);
 * Only the controls from the server during the last bind step are returned -
 * intermediate controls (if any, usually not) are discarded.
 */
int
LDAP_CALL
ldap_sasl_interactive_bind_ext_s( LDAP *ld, const char *dn,
        const char *saslMechanism,
        LDAPControl **sctrl, LDAPControl **cctrl, unsigned flags,
        LDAP_SASL_INTERACT_PROC *callback, void *defaults, LDAPControl ***rctrl )
{
#ifdef LDAP_SASLIO_GET_MECHS_FROM_SERVER
        char *smechs;
#endif
        int rc;

        LDAPDebug( LDAP_DEBUG_TRACE, "ldap_sasl_interactive_bind_s\n", 0, 0, 0 );

        if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
                return( LDAP_PARAM_ERROR );
        }

        if ((flags == LDAP_SASL_INTERACTIVE) && (callback == NULL)) {
                return( LDAP_PARAM_ERROR );
        }

        LDAP_MUTEX_LOCK(ld, LDAP_SASL_LOCK );

        if( saslMechanism == NULL || *saslMechanism == '\0' ) {
#ifdef LDAP_SASLIO_GET_MECHS_FROM_SERVER
                rc = nsldapi_get_sasl_mechs( ld, &smechs );
                if( rc != LDAP_SUCCESS ) {
                        LDAP_MUTEX_UNLOCK(ld, LDAP_SASL_LOCK );
                        return( rc );
                }
                saslMechanism = smechs;
#else
                LDAP_MUTEX_UNLOCK(ld, LDAP_SASL_LOCK );
                return( LDAP_PARAM_ERROR );
#endif /* LDAP_SASLIO_GET_MECHS_FROM_SERVER */
        }

        /* initialize SASL library */
        if ( nsldapi_sasl_init() < 0 ) {
            return( LDAP_PARAM_ERROR );
        }

        rc = nsldapi_sasl_do_bind( ld, dn, saslMechanism,
                        flags, callback, defaults, sctrl, cctrl, rctrl);

        LDAP_MUTEX_UNLOCK(ld, LDAP_SASL_LOCK );
        return( rc );
}
#else /* LDAP_SASLIO_HOOKS */
/* stubs for platforms that do not support SASL */
int
LDAP_CALL
ldap_sasl_interactive_bind_s( LDAP *ld, const char *dn,
        const char *saslMechanism,
        LDAPControl **sctrl, LDAPControl **cctrl, unsigned flags,
        LDAP_SASL_INTERACT_PROC *callback, void *defaults )
{
  return LDAP_SUCCESS;
}

int
LDAP_CALL
ldap_sasl_interactive_bind_ext_s( LDAP *ld, const char *dn,
        const char *saslMechanism,
        LDAPControl **sctrl, LDAPControl **cctrl, unsigned flags,
        LDAP_SASL_INTERACT_PROC *callback, void *defaults, LDAPControl ***rctrl )
{
  return LDAP_SUCCESS;
}
#endif /* LDAP_SASLIO_HOOKS */


/* returns an LDAP error code that indicates if parse succeeded or not */
int
LDAP_CALL
ldap_parse_sasl_bind_result(
    LDAP		*ld,
    LDAPMessage		*res,
    struct berval	**servercredp,
    int			freeit
)
{
	BerElement	ber;
	int		rc, err;
    ber_int_t	along;
	ber_len_t	len;
	char		*m, *e;

	LDAPDebug( LDAP_DEBUG_TRACE, "ldap_parse_sasl_bind_result\n", 0, 0, 0 );

	/*
	 * the ldapv3 SASL bind response looks like this:
	 *
	 *	BindResponse ::= [APPLICATION 1] SEQUENCE {
	 *		COMPONENTS OF LDAPResult,
	 *		serverSaslCreds [7] OCTET STRING OPTIONAL
	 *	}
	 *
	 * all wrapped up in an LDAPMessage sequence.
	 */

	if ( !NSLDAPI_VALID_LDAP_POINTER( ld ) ||
	    !NSLDAPI_VALID_LDAPMESSAGE_BINDRESULT_POINTER( res )) {
		return( LDAP_PARAM_ERROR );
	}

	/* only ldapv3 or higher can do sasl binds */
	if ( NSLDAPI_LDAP_VERSION( ld ) < LDAP_VERSION3 ) {
		LDAP_SET_LDERRNO( ld, LDAP_NOT_SUPPORTED, NULL, NULL );
		return( LDAP_NOT_SUPPORTED );
	}

	if ( servercredp != NULL ) {
		*servercredp = NULL;
	}

	ber = *(res->lm_ber);	/* struct copy */

	/* skip past message id, matched dn, error message ... */
	rc = ber_scanf( &ber, "{iaa}", &along, &m, &e );

	if ( rc != LBER_ERROR &&
	    ber_peek_tag( &ber, &len ) == LDAP_TAG_SASL_RES_CREDS ) {
		rc = ber_get_stringal( &ber, servercredp );
	}

	if ( freeit ) {
		ldap_msgfree( res );
	}

	if ( rc == LBER_ERROR ) {
		err = LDAP_DECODING_ERROR;
	} else {
		err = (int) along;
	}

	LDAP_SET_LDERRNO( ld, err, m, e );
	/* this is a little kludge for the 3.0 Barracuda/hammerhead relese */
	/* the docs state that the return is either LDAP_DECODING_ERROR */
	/* or LDAP_SUCCESS.  Here we match the docs...  it's cleaner in 3.1 */

	if ( LDAP_DECODING_ERROR == err ) {
		return (LDAP_DECODING_ERROR);
	} else {
		return( LDAP_SUCCESS );
	}
}

