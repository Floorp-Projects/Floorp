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

struct ldaperror {
	int	e_code;
	char	*e_reason;
};

static struct ldaperror ldap_errlist[] = {
	{ LDAP_SUCCESS, 			"Success" },
	{ LDAP_OPERATIONS_ERROR, 		"Operations error" },
	{ LDAP_PROTOCOL_ERROR, 			"Protocol error" },
	{ LDAP_TIMELIMIT_EXCEEDED,		"Timelimit exceeded" },
	{ LDAP_SIZELIMIT_EXCEEDED, 		"Sizelimit exceeded" },
	{ LDAP_COMPARE_FALSE, 			"Compare false" },
	{ LDAP_COMPARE_TRUE, 			"Compare true" },
	{ LDAP_STRONG_AUTH_NOT_SUPPORTED,	"Authentication method not supported" },
	{ LDAP_STRONG_AUTH_REQUIRED, 		"Strong authentication required" },
	{ LDAP_PARTIAL_RESULTS, 		"Partial results and referral received" },
	{ LDAP_REFERRAL, 			"Referral received" },
	{ LDAP_ADMINLIMIT_EXCEEDED,		"Administrative limit exceeded" },
	{ LDAP_UNAVAILABLE_CRITICAL_EXTENSION,	"Unavailable critical extension" },
	{ LDAP_CONFIDENTIALITY_REQUIRED,	"Confidentiality required" },
	{ LDAP_SASL_BIND_IN_PROGRESS,		"SASL bind in progress" },

	{ LDAP_NO_SUCH_ATTRIBUTE, 		"No such attribute" },
	{ LDAP_UNDEFINED_TYPE, 			"Undefined attribute type" },
	{ LDAP_INAPPROPRIATE_MATCHING, 		"Inappropriate matching" },
	{ LDAP_CONSTRAINT_VIOLATION, 		"Constraint violation" },
	{ LDAP_TYPE_OR_VALUE_EXISTS, 		"Type or value exists" },
	{ LDAP_INVALID_SYNTAX, 			"Invalid syntax" },

	{ LDAP_NO_SUCH_OBJECT, 			"No such object" },
	{ LDAP_ALIAS_PROBLEM, 			"Alias problem" },
	{ LDAP_INVALID_DN_SYNTAX,		"Invalid DN syntax" },
	{ LDAP_IS_LEAF, 			"Object is a leaf" },
	{ LDAP_ALIAS_DEREF_PROBLEM, 		"Alias dereferencing problem" },

	{ LDAP_INAPPROPRIATE_AUTH, 		"Inappropriate authentication" },
	{ LDAP_INVALID_CREDENTIALS, 		"Invalid credentials" },
	{ LDAP_INSUFFICIENT_ACCESS, 		"Insufficient access" },
	{ LDAP_BUSY, 				"DSA is busy" },
	{ LDAP_UNAVAILABLE, 			"DSA is unavailable" },
	{ LDAP_UNWILLING_TO_PERFORM, 		"DSA is unwilling to perform" },
	{ LDAP_LOOP_DETECT, 			"Loop detected" },
    { LDAP_SORT_CONTROL_MISSING,    "Sort Control is missing"  },
    { LDAP_INDEX_RANGE_ERROR,              "Search results exceed the range specified by the offsets" }, 
    
    { LDAP_NAMING_VIOLATION, 		"Naming violation" },
	{ LDAP_OBJECT_CLASS_VIOLATION, 		"Object class violation" },
	{ LDAP_NOT_ALLOWED_ON_NONLEAF, 		"Operation not allowed on nonleaf" },
	{ LDAP_NOT_ALLOWED_ON_RDN, 		"Operation not allowed on RDN" },
	{ LDAP_ALREADY_EXISTS, 			"Already exists" },
	{ LDAP_NO_OBJECT_CLASS_MODS, 		"Cannot modify object class" },
	{ LDAP_RESULTS_TOO_LARGE,		"Results too large" },
	{ LDAP_AFFECTS_MULTIPLE_DSAS,		"Affects multiple servers" },

	{ LDAP_OTHER, 				"Unknown error" },
	{ LDAP_SERVER_DOWN,			"Can't contact LDAP server" },
	{ LDAP_LOCAL_ERROR,			"Local error" },
	{ LDAP_ENCODING_ERROR,			"Encoding error" },
	{ LDAP_DECODING_ERROR,			"Decoding error" },
	{ LDAP_TIMEOUT,				"Timed out" },
	{ LDAP_AUTH_UNKNOWN,			"Unknown authentication method" },
	{ LDAP_FILTER_ERROR,			"Bad search filter" },
	{ LDAP_USER_CANCELLED,			"User cancelled operation" },
	{ LDAP_PARAM_ERROR,			"Bad parameter to an ldap routine" },
	{ LDAP_NO_MEMORY,			"Out of memory" },
	{ LDAP_CONNECT_ERROR,			"Can't connect to the LDAP server" },
	{ LDAP_NOT_SUPPORTED,			"Not supported by this version of the LDAP protocol" },
	{ LDAP_CONTROL_NOT_FOUND,		"Requested LDAP control not found" },
	{ LDAP_NO_RESULTS_RETURNED,		"No results returned" },
	{ LDAP_MORE_RESULTS_TO_RETURN,		"More results to return" },
	{ LDAP_CLIENT_LOOP,			"Client detected loop" },
	{ LDAP_REFERRAL_LIMIT_EXCEEDED,		"Referral hop limit exceeded" },
	{ -1, 0 }
};

char *
LDAP_CALL
ldap_err2string( int err )
{
	int	i;

	LDAPDebug( LDAP_DEBUG_TRACE, "ldap_err2string\n", 0, 0, 0 );

	for ( i = 0; ldap_errlist[i].e_code != -1; i++ ) {
		if ( err == ldap_errlist[i].e_code )
			return( ldap_errlist[i].e_reason );
	}

	return( "Unknown error" );
}


static char *
nsldapi_safe_strerror( int e )
{
	char *s;

	if (( s = strerror( e )) == NULL ) {
		s = "unknown error";
	}

	return( s );
}


void
LDAP_CALL
ldap_perror( LDAP *ld, const char *s )
{
	int		i, err;
	char	*matched = NULL;
	char	*errmsg = NULL; 
	char	*separator;
	char    msg[1024];

	LDAPDebug( LDAP_DEBUG_TRACE, "ldap_perror\n", 0, 0, 0 );

	if ( s == NULL ) {
		s = separator = "";
	} else {
		separator = ": ";
	}

	if ( ld == NULL ) {
		snprintf( msg, sizeof( msg ), 
			"%s%s%s", s, separator, nsldapi_safe_strerror( errno ) );
		ber_err_print( msg );
		return;
	}

	LDAP_MUTEX_LOCK( ld, LDAP_ERR_LOCK );
	err = LDAP_GET_LDERRNO( ld, &matched, &errmsg );
	for ( i = 0; ldap_errlist[i].e_code != -1; i++ ) {
		if ( err == ldap_errlist[i].e_code ) {
			snprintf( msg, sizeof( msg ), 
				"%s%s%s", s, separator, ldap_errlist[i].e_reason );
			ber_err_print( msg );
			if ( err == LDAP_CONNECT_ERROR ) {
				ber_err_print( " - " );
				ber_err_print( nsldapi_safe_strerror(
				    LDAP_GET_ERRNO( ld )));
			}
			ber_err_print( "\n" );
			if ( matched != NULL && *matched != '\0' ) {
				snprintf( msg, sizeof( msg ), 
					"%s%smatched: %s\n", s, separator, matched );
				ber_err_print( msg );
			}
			if ( errmsg != NULL && *errmsg != '\0' ) {
				snprintf( msg, sizeof( msg ), 
					"%s%sadditional info: %s\n", s, separator, errmsg );
				ber_err_print( msg );
			}
			LDAP_MUTEX_UNLOCK( ld, LDAP_ERR_LOCK );
			return;
		}
	}
	snprintf( msg, sizeof( msg ), 
		"%s%sNot an LDAP errno %d\n", s, separator, err );
	ber_err_print( msg );
	LDAP_MUTEX_UNLOCK( ld, LDAP_ERR_LOCK );
}

int
LDAP_CALL
ldap_result2error( LDAP *ld, LDAPMessage *r, int freeit )
{
	int	lderr_parse, lderr;

	lderr_parse = ldap_parse_result( ld, r, &lderr, NULL, NULL, NULL,
	    NULL, freeit );

	if ( lderr_parse != LDAP_SUCCESS ) {
		return( lderr_parse );
	}

	return( lderr );
}

int
LDAP_CALL
ldap_get_lderrno( LDAP *ld, char **m, char **s )
{
	if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
		return( LDAP_PARAM_ERROR );	/* punt */
	}

	if ( ld->ld_get_lderrno_fn == NULL ) {
		if ( m != NULL ) {
			*m = ld->ld_matched;
		}
		if ( s != NULL ) {
			*s = ld->ld_error;
		}
		return( ld->ld_errno );
	} else {
		return( ld->ld_get_lderrno_fn( m, s, ld->ld_lderrno_arg ) );
	}
}


/*
 * Note: there is no need for callers of ldap_set_lderrno() to lock the
 * ld mutex.  If applications intend to share an LDAP session handle
 * between threads they *must* perform their own locking around the
 * session handle or they must install a "set lderrno" thread callback
 * function.
 * 
 */
int
LDAP_CALL
ldap_set_lderrno( LDAP *ld, int e, char *m, char *s )
{
	if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
		return( LDAP_PARAM_ERROR );
	}

	if ( ld->ld_set_lderrno_fn != NULL ) {
		ld->ld_set_lderrno_fn( e, m, s, ld->ld_lderrno_arg );
	} else {
        LDAP_MUTEX_LOCK( ld, LDAP_ERR_LOCK );
		ld->ld_errno = e;
		if ( ld->ld_matched ) {
			NSLDAPI_FREE( ld->ld_matched );
		}
		ld->ld_matched = m;
		if ( ld->ld_error ) {
			NSLDAPI_FREE( ld->ld_error );
		}
		ld->ld_error = s;
        LDAP_MUTEX_UNLOCK( ld, LDAP_ERR_LOCK );
	}

	return( LDAP_SUCCESS );
}


/*
 * Returns an LDAP error that says whether parse succeeded.  The error code
 * from the LDAP result itself is returned in the errcodep result parameter.
 * If any of the result params. (errcodep, matchednp, errmsgp, referralsp,
 * or serverctrlsp) are NULL we don't return that info.
 */
int
LDAP_CALL
ldap_parse_result( LDAP *ld, LDAPMessage *res, int *errcodep, char **matchednp,
	char **errmsgp, char ***referralsp, LDAPControl ***serverctrlsp,
	int freeit )
{
	LDAPMessage		*lm;
	int			err, errcode;
	char			*m, *e;
	m = e = NULL;

	LDAPDebug( LDAP_DEBUG_TRACE, "ldap_parse_result\n", 0, 0, 0 );

	if ( !NSLDAPI_VALID_LDAP_POINTER( ld ) ||
	    !NSLDAPI_VALID_LDAPMESSAGE_POINTER( res )) {
		return( LDAP_PARAM_ERROR );
	}

	/* skip over entries and references to find next result in this chain */
	for ( lm = res; lm != NULL; lm = lm->lm_chain ) {	
		if ( lm->lm_msgtype != LDAP_RES_SEARCH_ENTRY &&
		    lm->lm_msgtype != LDAP_RES_SEARCH_REFERENCE ) {
			break;
		}
	}

	if ( lm == NULL ) {
		err = LDAP_NO_RESULTS_RETURNED;
		LDAP_SET_LDERRNO( ld, err, NULL, NULL );
		return( err );
	}

	err = nsldapi_parse_result( ld, lm->lm_msgtype, lm->lm_ber, &errcode,
	    &m, &e, referralsp, serverctrlsp );

	if ( err == LDAP_SUCCESS ) {
		if ( errcodep != NULL ) {
			*errcodep = errcode;
		}
		if ( matchednp != NULL ) {
			*matchednp = nsldapi_strdup( m );
		}
		if ( errmsgp != NULL ) {
			*errmsgp = nsldapi_strdup( e );
		}

		/*
		 * if there are more result messages in the chain, arrange to
		 * return the special LDAP_MORE_RESULTS_TO_RETURN "error" code.
		 */
		for ( lm = lm->lm_chain; lm != NULL; lm = lm->lm_chain ) {	
			if ( lm->lm_msgtype != LDAP_RES_SEARCH_ENTRY &&
			    lm->lm_msgtype != LDAP_RES_SEARCH_REFERENCE ) {
				err = LDAP_MORE_RESULTS_TO_RETURN;
				break;
			}
		}
	} else { 
	    /* In this case, m and e were already freed by ber_scanf */
	    m = e = NULL; 
	} 

	if ( freeit ) {
		ldap_msgfree( res );
	}

	LDAP_SET_LDERRNO( ld, ( err == LDAP_SUCCESS ) ? errcode : err, m, e );

	/* nsldapi_parse_result set m and e, so we have to delete them if they exist.
	   Only delete them if they haven't been reused for matchednp or errmsgp.
	   if ( err == LDAP_SUCCESS ) {
	if ( (m != NULL) && (matchednp == NULL) ) {
	NSLDAPI_FREE( m );
	}
	if ( (e != NULL) && (errmsgp == NULL) ) {
	NSLDAPI_FREE( e );
	}
	} */

	return( err );
}

/*
 * returns an LDAP error code indicating success or failure of parsing
 * does NOT set any error information inside "ld"
 */
int
nsldapi_parse_result( LDAP *ld, int msgtype, BerElement *rber, int *errcodep,
    char **matchednp, char **errmsgp, char ***referralsp,
    LDAPControl ***serverctrlsp )
{
	BerElement	ber;
	ber_len_t	len;
	ber_int_t   errcode;
	int			berrc, err;
	char		*m, *e;

	/*
	 * Parse the result message.  LDAPv3 result messages look like this:
	 *
	 *	LDAPResult ::= SEQUENCE {
	 *		resultCode	ENUMERATED { ... },
	 *		matchedDN	LDAPDN,
	 *		errorMessage	LDAPString,
	 *		referral	[3] Referral OPTIONAL
	 *		opSpecificStuff	OPTIONAL
	 *	}
	 *
	 * all wrapped up in an LDAPMessage sequence which looks like this:
	 *	LDAPMessage ::= SEQUENCE {
	 *		messageID	MessageID,
	 *		LDAPResult	CHOICE { ... },	// message type
	 *		controls	[0] Controls OPTIONAL
	 *	}
	 *
	 * LDAPv2 messages don't include referrals or controls.
	 * LDAPv1 messages don't include matchedDN, referrals, or controls.
	 *
	 * ldap_result() pulls out the message id, so by the time a result
	 * message gets here we are sitting at the start of the LDAPResult.
	 */

	err = LDAP_SUCCESS;	/* optimistic */
	m = e = NULL;
	if ( matchednp != NULL ) {
		*matchednp = NULL;
	}
	if ( errmsgp != NULL ) {
		*errmsgp = NULL;
	}
	if ( referralsp != NULL ) {
		*referralsp = NULL;
	}
	if ( serverctrlsp != NULL ) {
		*serverctrlsp = NULL;
	}
	ber = *rber;		/* struct copy */

	if ( NSLDAPI_LDAP_VERSION( ld ) < LDAP_VERSION2 ) {
		berrc = ber_scanf( &ber, "{ia}", &errcode, &e );
	} else {
		if (( berrc = ber_scanf( &ber, "{iaa", &errcode, &m, &e ))
		    != LBER_ERROR ) {
			/* check for optional referrals */
			if ( ber_peek_tag( &ber, &len ) == LDAP_TAG_REFERRAL ) {
				if ( referralsp == NULL ) {
					/* skip referrals */
					berrc = ber_scanf( &ber, "x" );
				} else {
					/* suck out referrals */
					berrc = ber_scanf( &ber, "v",
					    referralsp );
				}
			} else if ( referralsp != NULL ) {
				*referralsp = NULL;
			}
		}

		if ( berrc != LBER_ERROR ) {
			/*
			 * skip past optional operation-specific elements:
			 *   bind results - serverSASLcreds
			 *   extendedop results -  OID plus value
			 */
			if ( msgtype == LDAP_RES_BIND ) {
				if ( ber_peek_tag( &ber, &len ) == 
				    LDAP_TAG_SASL_RES_CREDS ) {
					berrc = ber_scanf( &ber, "x" );
				}
			} else if ( msgtype == LDAP_RES_EXTENDED ) {
				if ( ber_peek_tag( &ber, &len ) ==
				    LDAP_TAG_EXOP_RES_OID ) {
					berrc = ber_scanf( &ber, "x" );
				}
				if ( berrc != LBER_ERROR &&
				    ber_peek_tag( &ber, &len ) ==
				    LDAP_TAG_EXOP_RES_VALUE ) {
					berrc = ber_scanf( &ber, "x" );
				}
			}
		}

		/* pull out controls (if requested and any are present) */
		if ( berrc != LBER_ERROR && serverctrlsp != NULL &&
		    ( berrc = ber_scanf( &ber, "}" )) != LBER_ERROR ) {
			err = nsldapi_get_controls( &ber, serverctrlsp );
		}
	}

	if ( berrc == LBER_ERROR && err == LDAP_SUCCESS ) {
		err = LDAP_DECODING_ERROR;
	}

	if ( errcodep != NULL ) {
		*errcodep = errcode;
	}
	if ( matchednp != NULL ) {
		*matchednp = m;
	} else if ( m != NULL ) {
		NSLDAPI_FREE( m );
	}
	if ( errmsgp != NULL ) {
		*errmsgp = e;
	} else if ( e != NULL ) {
		NSLDAPI_FREE( e );
	}

	return( err );
}
