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
#include "nsLDAPOperation.h"
#include "nsILDAPMessage.h"
#include "nsIComponentManager.h"
#include "nsXPIDLString.h"
#include "nspr.h"

// constructor
nsLDAPOperation::nsLDAPOperation()
{
    NS_INIT_ISUPPORTS();
}

// destructor
nsLDAPOperation::~nsLDAPOperation()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsLDAPOperation, nsILDAPOperation);

/**
 * Initializes this operation.  Must be called prior to use. 
 *
 * @param aConnection connection this operation should use
 * @param aMessageListener where are the results are called back to. 
 */
NS_IMETHODIMP
nsLDAPOperation::Init(nsILDAPConnection *aConnection,
		      nsILDAPMessageListener *aMessageListener)
{
    if (!aConnection) {
	return NS_ERROR_ILLEGAL_VALUE;
    }

    // so we know that the operation is not yet running (and therefore don't
    // try and call ldap_abandon_ext() on it) or remove it from the queue.
    //
    mMsgId = 0;

    // set the member vars
    //
    mConnection = aConnection;
    mMessageListener = aMessageListener;

    // get and cache the connection handle
    //	
    nsresult rv = mConnection->GetConnectionHandle(&mConnectionHandle);
    if (NS_FAILED(rv)) 
	return NS_ERROR_UNEXPECTED;

    return NS_OK;
}

NS_IMETHODIMP
nsLDAPOperation::GetConnection(nsILDAPConnection* *aConnection)
{
    if (!aConnection) {
	return NS_ERROR_ILLEGAL_VALUE;
    }

    *aConnection = mConnection;
    NS_IF_ADDREF(*aConnection);

    return NS_OK;
}

NS_IMETHODIMP
nsLDAPOperation::GetMessageListener(nsILDAPMessageListener **aMessageListener)
{
    if (!aMessageListener) {
	return NS_ERROR_ILLEGAL_VALUE;
    }

    *aMessageListener = mMessageListener;
    NS_IF_ADDREF(*aMessageListener);

    return NS_OK;
}

// wrapper for ldap_simple_bind()
//
NS_IMETHODIMP
nsLDAPOperation::SimpleBind(const char *passwd)
{
    nsresult rv;
    nsXPIDLCString bindName;

    NS_PRECONDITION(mMessageListener != 0, "MessageListener not set");

    rv = this->mConnection->GetBindName(getter_Copies(bindName));
    if (NS_FAILED(rv))
	return rv;

    this->mMsgId = ldap_simple_bind(this->mConnectionHandle, bindName, passwd);

    if (this->mMsgId == -1) {
        const int lderrno = ldap_get_lderrno(this->mConnectionHandle, 0, 0);
	
	switch (lderrno) {

	case LDAP_ENCODING_ERROR:
	    return NS_ERROR_LDAP_ENCODING_ERROR;

	case LDAP_SERVER_DOWN:
	    // XXXdmose rebind here?
	    return NS_ERROR_FAILURE;

	case LDAP_NO_MEMORY:
	    return NS_ERROR_OUT_OF_MEMORY;

	default:
	    return NS_ERROR_UNEXPECTED;
	}

    } 
  
    // make sure the connection knows where to call back once the messages
    // for this operation start coming in
    //
    rv = mConnection->AddPendingOperation(this);
    switch (rv) {
    case NS_OK:
	break;

	// note that the return value of ldap_abandon_ext() is ignored, as 
	// there's nothing useful to do with it

    case NS_ERROR_OUT_OF_MEMORY:
	(void *)ldap_abandon_ext(mConnectionHandle, mMsgId, 0, 0);
	return NS_ERROR_OUT_OF_MEMORY;
	break;

    case NS_ERROR_UNEXPECTED:
    case NS_ERROR_ILLEGAL_VALUE:
    default:
	(void *)ldap_abandon_ext(mConnectionHandle, mMsgId, 0, 0);
	return NS_ERROR_UNEXPECTED;
    }

    return NS_OK;
}

// wrappers for ldap_search_ext
//
int
nsLDAPOperation::SearchExt(const char *base, // base DN to search
			   int scope, // LDAP_SCOPE_{BASE,ONELEVEL,SUBTREE}
			   const char* filter, // search filter
			   char **attrs, // attribute types to be returned
			   int attrsOnly, // attrs only, or values too?
			   LDAPControl **serverctrls, 
			   LDAPControl **clientctrls,
			   struct timeval *timeoutp, // how long to wait
			   int sizelimit) // max # of entries to return
{
    if (mMessageListener == 0) {
	NS_ERROR("nsLDAPOperation::SearchExt(): mMessageListener not set");
	return NS_ERROR_NOT_INITIALIZED;
    }

    return ldap_search_ext(this->mConnectionHandle, base, scope, 
			   filter, attrs, attrsOnly, serverctrls, 
			   clientctrls, timeoutp, sizelimit, 
			   &(this->mMsgId));
}


/**
 * wrapper for ldap_search_ext(): kicks off an async search request.
 *
 * @param aBaseDn		Base DN to search
 * @param aScope		One of LDAP_SCOPE_{BASE,ONELEVEL,SUBTREE}
 * @param aFilter		Search filter
 * @param aTimeOut		How long to wait
 * @param aSizeLimit		Maximum number of entries to return.
 *
 * XXX doesn't currently handle LDAPControl params
 *
 * void searchExt(in string aBaseDn, in PRInt32 aScope,
 *		  in string aFilter, in PRIntervalTime aTimeOut,
 *		  in PRInt32 aSizeLimit);
 */
NS_IMETHODIMP
nsLDAPOperation::SearchExt(const char *aBaseDn, PRInt32 aScope, 
			   const char *aFilter, PRIntervalTime aTimeOut,
			   PRInt32 aSizeLimit) 
{
    // XXX deal with timeouts
    //
    int retVal = nsLDAPOperation::SearchExt(aBaseDn, aScope, aFilter, NULL, 0, 
					    NULL, NULL, NULL, aSizeLimit);

    switch (retVal) {

    case LDAP_SUCCESS: 
	break;

    case LDAP_ENCODING_ERROR:
	return NS_ERROR_LDAP_ENCODING_ERROR;
	break;

    case LDAP_SERVER_DOWN:
	return NS_ERROR_LDAP_SERVER_DOWN;
	break;

    case LDAP_NO_MEMORY:
	return NS_ERROR_OUT_OF_MEMORY;
	break;

    case LDAP_NOT_SUPPORTED:
	return NS_ERROR_LDAP_NOT_SUPPORTED;
	break;

    case LDAP_PARAM_ERROR:
    default:
	NS_ERROR("nsLDAPOperation::SearchExt(): unexpected return value");
	return NS_ERROR_UNEXPECTED;
    }

    // make sure the connection knows where to call back once the messages
    // for this operation start coming in
    //
    nsresult rv = mConnection->AddPendingOperation(this);
    if (NS_FAILED(rv)) {
	switch (rv) {
	case NS_ERROR_OUT_OF_MEMORY: 
	    (void *)ldap_abandon_ext(mConnectionHandle, mMsgId, 0, 0);
	    return NS_ERROR_OUT_OF_MEMORY;
	    break;

	default: 
	    (void *)ldap_abandon_ext(mConnectionHandle, mMsgId, 0, 0);
	    NS_ERROR("nsLDAPOperation::SearchExt(): unexpected error in "
		     "mConnection->AddPendingOperation");
	    return NS_ERROR_UNEXPECTED;
	    break;
	}
    }

    return NS_OK;
}

// wrapper for ldap_url_search
//
NS_IMETHODIMP
nsLDAPOperation::UrlSearch(const char *aURL, // the search URL
			   PRBool aAttrsOnly) // skip attribute names?
{
    NS_ENSURE_ARG(aURL);
    NS_PRECONDITION(mMessageListener != 0, "MessageListener not set");

    this->mMsgId = ldap_url_search(this->mConnectionHandle, aURL, 
				   aAttrsOnly);
    if (this->mMsgId == -1) {
	// XXX
	PR_Abort();
#ifdef DEBUG
    char *s;

    (void)this->mConnection->GetErrorString(&s);
    PR_fprintf(PR_STDERR, "UrlSearch failed: %s\n", s);
    ldap_memfree(s);
#endif
	return NS_ERROR_FAILURE;
    }

    // make sure the connection knows where to call back once the messages
    // for this operation start coming in
    // XXX should abandon operation if this fails
    nsresult rv = mConnection->AddPendingOperation(this);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

NS_IMETHODIMP
nsLDAPOperation::GetMessageId(PRInt32 *aMsgId)
{
    if (!aMsgId) {
	return NS_ERROR_ILLEGAL_VALUE;
    }

    *aMsgId = this->mMsgId;
   
    return NS_OK;
}

// as far as I can tell from reading the LDAP C SDK code, abandoning something
// that has already been abandoned does not return an error
//
nsresult
nsLDAPOperation::AbandonExt(LDAPControl **serverctrls,
			    LDAPControl **clientctrls)
{
    nsresult rv;
    int retVal;

    if ( mMessageListener == 0 || mMsgId == 0 ) {
	NS_ERROR("nsLDAPOperation::AbandonExt(): mMessageListener or "
		 "mMsgId not initialized");
	return NS_ERROR_NOT_INITIALIZED;
    }

    retVal = ldap_abandon_ext(mConnectionHandle, mMsgId, serverctrls, 
			      clientctrls);
    switch (retVal) {

    case LDAP_SUCCESS:
	break;

    case LDAP_ENCODING_ERROR:
	return NS_ERROR_LDAP_ENCODING_ERROR;
	break;
    
    case LDAP_SERVER_DOWN:
	return NS_ERROR_LDAP_SERVER_DOWN;
	break;

    case LDAP_NO_MEMORY:
	return NS_ERROR_OUT_OF_MEMORY;
	break;

    case LDAP_PARAM_ERROR:
    default: 
	NS_ERROR("nsLDAPOperation::AbandonExt(): unexpected return value from "
		 "ldap_abandon_ext");
	return NS_ERROR_UNEXPECTED;
    }

    // try to remove it from the pendingOperations queue, if it's there.
    // even if something goes wrong here, the abandon() has already succeeded
    // succeeded (and there's nothing else the caller can reasonably do), 
    // so we only pay attention to this in debug builds.
    //
    rv = mConnection->RemovePendingOperation(this);
    if (NS_FAILED(rv)) {
	// XXXdmose should we use keep Abandon from happening on multiple 
	// threads at the same time?  that's when this condition is most 
	// likely to occur.  i _think_ the LDAP C SDK is ok with this; need 
	// to verify.
	//
	NS_WARNING("nsLDAPOperation::AbandonExt: "
		   "mConnection->RemovePendingOperation(this) failed.");
    }

    return NS_OK;
}

/**  
 * wrapper for ldap_abandon_ext() with NULL LDAPControl
 * parameters, equivalent to old-style ldap_abandon(), thus the name.
 *
 * @exception NS_ERROR_NOT_INITIALIZED		operation not initialized
 * @exception NS_ERROR_LDAP_ENCODING_ERROR  	error during BER-encoding
 * @exception NS_ERROR_LDAP_SERVER_DOWN	    	the LDAP server did not
 *					    	receive the request or the
 *					    	connection was lost
 * @exception NS_ERROR_OUT_OF_MEMORY	    	out of memory
 * @exception NS_ERROR_UNEXPECTED		internal error
 */

NS_IMETHODIMP
nsLDAPOperation::Abandon(void)
{
    return nsLDAPOperation::AbandonExt(NULL, NULL);
}
