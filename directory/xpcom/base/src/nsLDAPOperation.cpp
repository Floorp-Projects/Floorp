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

#include "ldap.h"
#include "nsLDAPOperation.h"
#include "nsILDAPMessage.h"
#include "nsIComponentManager.h"

struct timeval nsLDAPOperation::sNullTimeval = {0, 0};

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

// the connection this operation is on
//
// attribute nsILDAPConnection connection;
//
NS_IMETHODIMP
nsLDAPOperation::SetConnection(nsILDAPConnection *aConnection)
{
    nsresult rv;

    // set the connection
    //
    mConnection = aConnection;

    // get and cache the connection handle
    //	
    rv = this->mConnection->GetConnectionHandle(&this->mConnectionHandle);
    if (NS_FAILED(rv)) 
	return rv;

    return NS_OK;
}

NS_IMETHODIMP
nsLDAPOperation::GetConnection(nsILDAPConnection* *aConnection)
{
    NS_ENSURE_ARG_POINTER(aConnection);

    *aConnection = mConnection;
    NS_IF_ADDREF(*aConnection);

    return NS_OK;
}

// wrapper for ldap_simple_bind()
//
NS_IMETHODIMP
nsLDAPOperation::SimpleBind(const char *who, const char *passwd)
{
    NS_ENSURE_ARG(who);
    NS_ENSURE_ARG(passwd);

    this->mMsgId = ldap_simple_bind(this->mConnectionHandle, who, 
				   passwd);

    if (this->mMsgId == -1) {
        return NS_ERROR_FAILURE;
    } else {
        return NS_OK;
    }
}

// wrapper for ldap_result
NS_IMETHODIMP
nsLDAPOperation::Result(PRInt32 aAll, 
			PRTime timeout, 
			nsILDAPMessage* *aMessage,
			PRInt32 *_retval)
{
    LDAPMessage *msgHandle;
    nsCOMPtr<nsILDAPMessage> msg;

    nsresult rv;

    NS_ENSURE_ARG_POINTER(aMessage);
    NS_ENSURE_ARG_POINTER(_retval);

    // make the call
    //
    *_retval =	ldap_result(mConnectionHandle, mMsgId,
			    aAll, &sNullTimeval, &msgHandle);
    
    // if we didn't error or timeout, create an nsILDAPMessage
    //	
    if (*_retval != 0 && *_retval != -1) {
	
	// create the message
	//
	msg = do_CreateInstance("mozilla.network.ldapmessage", &rv);
	NS_ENSURE_SUCCESS(rv, rv);

	// initialize it
        //	
	rv = msg->Init(this, msgHandle);
	NS_ENSURE_SUCCESS(rv, rv);

    }

    *aMessage = msg;
    NS_IF_ADDREF(*aMessage);

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
    return ldap_search_ext(this->mConnectionHandle, base, scope, 
			   filter, attrs, attrsOnly, serverctrls, 
			   clientctrls, timeoutp, sizelimit, &(this->mMsgId));
}

int
nsLDAPOperation::SearchExt(const char *base, // base DN to search
			   int scope, // LDAP_SCOPE_{BASE,ONELEVEL,SUBTREE}
			   const char* filter, // search filter
			   struct timeval *timeoutp, // how long to wait
			   int sizelimit) // max # of entries to return
{
    return nsLDAPOperation::SearchExt(base, scope, filter, NULL, 0, NULL,
				      NULL, timeoutp, sizelimit);
}

// wrapper for ldap_url_search
//
NS_IMETHODIMP
nsLDAPOperation::URLSearch(const char *aURL, // the search URL
			   PRBool aAttrsOnly) // skip attribute names?
{
    NS_ENSURE_ARG(aURL);

    this->mMsgId = ldap_url_search(this->mConnectionHandle, aURL, 
				   aAttrsOnly);
    if (this->mMsgId == -1) {
	return NS_ERROR_FAILURE;
    }

    return NS_OK;
}
