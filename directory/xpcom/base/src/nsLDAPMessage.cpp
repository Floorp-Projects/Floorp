/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the mozilla.org LDAP XPCOM component.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): Dan Mosedale <dmose@mozilla.org>
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#include "nsLDAP.h"
#include "nsLDAPMessage.h"
#include "nspr.h"

NS_IMPL_THREADSAFE_ISUPPORTS1(nsLDAPMessage, nsILDAPMessage);

// constructor
//
nsLDAPMessage::nsLDAPMessage()
{
    NS_INIT_ISUPPORTS();

    mMsgHandle = NULL;
    mConnectionHandle = NULL;
    mPosition = NULL;
}

// destructor
// XXX better error-handling than fprintf
//
nsLDAPMessage::~nsLDAPMessage(void)
{
    int rc;

    if (mMsgHandle != NULL) {
	rc = ldap_msgfree(mMsgHandle);

	switch(rc) {
	case LDAP_RES_BIND:
	case LDAP_RES_SEARCH_ENTRY:
	case LDAP_RES_SEARCH_RESULT:
	case LDAP_RES_MODIFY:
	case LDAP_RES_ADD:
	case LDAP_RES_DELETE:
	case LDAP_RES_MODRDN:
	case LDAP_RES_COMPARE:
	case LDAP_RES_SEARCH_REFERENCE:
	case LDAP_RES_EXTENDED:
	case LDAP_RES_ANY:
	    // success
	    break;
	case LDAP_SUCCESS:
	    // timed out (dunno why LDAP_SUCCESS is used to indicate this) 
	    PR_LOG(gLDAPLogModule, PR_LOG_WARNING, 
		   ("nsLDAPMessage::~nsLDAPMessage: ldap_msgfree() "
		    "timed out\n"));
	    break;
	default:
	    // other failure
	    PR_LOG(gLDAPLogModule, PR_LOG_WARNING, 
		   ("nsLDAPMessage::~nsLDAPMessage: ldap_msgfree() "
		    "failed: %s\n", ldap_err2string(rc)));
	    break;
	}
    }

    if ( mPosition != NULL ) {
	ldap_ber_free(mPosition, 0);
    }

}

// associate this message with an existing operation
//
NS_IMETHODIMP
nsLDAPMessage::Init(nsILDAPConnection *aConnection, LDAPMessage *aMsgHandle)
{
    nsresult rv;

    NS_ENSURE_ARG_POINTER(aConnection);
    NS_ENSURE_ARG_POINTER(aMsgHandle);
    
    // initialize the appropriate member vars
    //
    mConnection = aConnection;
    mMsgHandle = aMsgHandle;

    // cache the connection handle associated with this operation
    //
    rv = mConnection->GetConnectionHandle(&mConnectionHandle);
    if (NS_FAILED(rv))
	return NS_ERROR_FAILURE;

    return NS_OK;
}

// XXX - both this and GetErrorString should be based on a separately
// broken out ldap_parse_result
//
NS_IMETHODIMP
nsLDAPMessage::GetErrorCode(PRInt32 *aErrCode)
{
    PRInt32 rc;

    rc = ldap_parse_result(mConnectionHandle, mMsgHandle, aErrCode, 
			   NULL, NULL, NULL, NULL, 0);

    if (rc != LDAP_SUCCESS) {

#ifdef DEBUG
	PR_fprintf(PR_STDERR,
		   "nsLDAPMessage::ErrorToString: ldap_parse_result: %s\n",
		   ldap_err2string(rc));
#endif	
	return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

// XXX deal with extra params (make client not have to use ldap_memfree() on 
// result)
// XXX better error-handling than fprintf()
//
char *
nsLDAPMessage::GetErrorString(void)
{
  int errcode;
  char *matcheddn;
  char *errmsg;
  char **referrals;
  LDAPControl **serverctrls;

  int rc;

  rc = ldap_parse_result(mConnectionHandle, mMsgHandle, &errcode, &matcheddn,
			 &errmsg, &referrals, &serverctrls, 0);
  if (rc != LDAP_SUCCESS) {
      PR_fprintf(PR_STDERR, 
		 "nsLDAPMessage::ErrorToString: ldap_parse_result: %s\n",
		 ldap_err2string(rc));
      // XXX need real err handling here
  }

  if (matcheddn) ldap_memfree(matcheddn);
  if (errmsg) ldap_memfree(errmsg);
  if (referrals) ldap_memfree(referrals);
  if (serverctrls) ldap_memfree(serverctrls);

  return(ldap_err2string(errcode));
}

// wrapper for ldap_first_attribute 
//
NS_IMETHODIMP
nsLDAPMessage::FirstAttribute(char* *aAttribute)
{
    NS_ENSURE_ARG_POINTER(aAttribute);

    *aAttribute = ldap_first_attribute(mConnectionHandle, mMsgHandle, 
				       &mPosition);
    if (*aAttribute) {
	return NS_OK;
    } else {
	return NS_ERROR_FAILURE;
    }
}

// wrapper for ldap_next_attribute()
//
NS_IMETHODIMP
nsLDAPMessage::NextAttribute(char* *aAttribute)
{
    NS_ENSURE_ARG_POINTER(aAttribute);

    *aAttribute = ldap_next_attribute(mConnectionHandle, mMsgHandle, 
				      mPosition);
    if (*aAttribute) {
	return NS_OK;
    } else {
	// figure out whether this returned NULL because it was the
	// last attribute (which is OK), or because there was an error
	//
	nsresult rv;
	PRInt32 lderr;

	rv = mConnection->GetLdErrno(NULL, NULL, &lderr);
	if (NS_FAILED(rv)) { 
	    // some sort of internal error; propagate upwards
	    //
	    return rv;
        }

	if (lderr == LDAP_SUCCESS) {
	    return NS_OK;
	} else {
	    // XXX should really propagate lderr & associated strings upwards..
	    // maybe need to turn LdErrno into an interface
	    //
	    return NS_ERROR_FAILURE;
	}
    }
}

// wrapper for ldap_msgtype()
//
int
nsLDAPMessage::Type(void)
{
    return (ldap_msgtype(mMsgHandle));
}

// wrapper for ldap_get_dn
//
NS_IMETHODIMP
nsLDAPMessage::GetDn(char* *aDN)
{
    NS_ENSURE_ARG_POINTER(aDN);

    *aDN = ldap_get_dn(mConnectionHandle, mMsgHandle);

    if (*aDN) {
	return NS_OK;
    } else {
	return NS_ERROR_FAILURE;
    }

}

// wrapper for ldap_get_values()
//
NS_IMETHODIMP
nsLDAPMessage::GetValues(const char *aAttr, PRUint32 *aCount, 
			 char** *aValues)
{
    PRUint32 i;
    char **values;

    values = ldap_get_values(mConnectionHandle, mMsgHandle, aAttr);

    // bail out if there was a problem
    // XXX - better err handling
    //
    if (!values) {
	return NS_ERROR_FAILURE;	
    }

    // count the values
    //
    for ( i=0 ; values[i] != NULL; i++ ) {
    }

    *aCount = i + 1;    // include the NULL-terminator in our count
    *aValues = values;
    return NS_OK;
}

// returns an LDIF-like string representation of this message
//
// string toString();
//
NS_IMETHODIMP
nsLDAPMessage::ToString(char* *aString)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
