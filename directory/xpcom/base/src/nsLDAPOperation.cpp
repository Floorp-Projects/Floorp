/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the mozilla.org LDAP XPCOM SDK.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dan Mosedale <dmose@mozilla.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsLDAP.h"
#include "nsLDAPOperation.h"
#include "nsLDAPConnection.h"
#include "nsILDAPMessage.h"
#include "nsIComponentManager.h"
#include "nsXPIDLString.h"
#include "nspr.h"

// constructor
nsLDAPOperation::nsLDAPOperation()
{
}

// destructor
nsLDAPOperation::~nsLDAPOperation()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsLDAPOperation, nsILDAPOperation)

/**
 * Initializes this operation.  Must be called prior to use. 
 *
 * @param aConnection connection this operation should use
 * @param aMessageListener where are the results are called back to. 
 */
NS_IMETHODIMP
nsLDAPOperation::Init(nsILDAPConnection *aConnection,
                      nsILDAPMessageListener *aMessageListener,
                      nsISupports *aClosure)
{
    if (!aConnection) {
        return NS_ERROR_ILLEGAL_VALUE;
    }

    // so we know that the operation is not yet running (and therefore don't
    // try and call ldap_abandon_ext() on it) or remove it from the queue.
    //
    mMsgID = 0;

    // set the member vars
    //
    mConnection = aConnection;
    mMessageListener = aMessageListener;
    mClosure = aClosure;

    // cache the connection handle
    //
    mConnectionHandle = 
        NS_STATIC_CAST(nsLDAPConnection *, aConnection)->mConnectionHandle;

    return NS_OK;
}

NS_IMETHODIMP
nsLDAPOperation::GetClosure(nsISupports **_retval)
{
    if (!_retval) {
        return NS_ERROR_ILLEGAL_VALUE;
    }
    NS_IF_ADDREF(*_retval = mClosure);
    return NS_OK;
}

NS_IMETHODIMP
nsLDAPOperation::SetClosure(nsISupports *aClosure)
{
    mClosure = aClosure;
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

void
nsLDAPOperation::Clear()
{
  mMessageListener = nsnull;
  mClosure = nsnull;
  mConnection = nsnull;
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
nsLDAPOperation::SimpleBind(const nsACString& passwd)
{
    nsresult rv;
    nsCAutoString bindName;
    PRBool originalMsgID = mMsgID;
    // Ugly hack alert:
    // the first time we get called with a passwd, remember it.
    // Then, if we get called again w/o a password, use the 
    // saved one. Getting called again means we're trying to
    // fall back to VERSION2.
    // Since LDAP operations are thrown away when done, it won't stay
    // around in memory.
    if (!passwd.IsEmpty())
      mSavePassword = passwd;

    NS_PRECONDITION(mMessageListener != 0, "MessageListener not set");

    rv = mConnection->GetBindName(bindName);
    if (NS_FAILED(rv))
        return rv;

    PR_LOG(gLDAPLogModule, PR_LOG_DEBUG, 
           ("nsLDAPOperation::SimpleBind(): called; bindName = '%s'; ",
            bindName.get()));

    // If this is a second try at binding, remove the operation from pending ops
    // because msg id has changed...
    if (originalMsgID)
      NS_STATIC_CAST(nsLDAPConnection *, 
                        NS_STATIC_CAST(nsILDAPConnection *, 
                        mConnection.get()))->RemovePendingOperation(this);

    mMsgID = ldap_simple_bind(mConnectionHandle, bindName.get(),
                              PromiseFlatCString(mSavePassword).get());

    if (mMsgID == -1) {
        const int lderrno = ldap_get_lderrno(mConnectionHandle, 0, 0);
        
        switch (lderrno) {

        case LDAP_ENCODING_ERROR:
            return NS_ERROR_LDAP_ENCODING_ERROR;

        case LDAP_CONNECT_ERROR:
            return NS_ERROR_LDAP_CONNECT_ERROR;

        case LDAP_SERVER_DOWN:
            // XXXdmose rebind here?
            return NS_ERROR_LDAP_SERVER_DOWN;

        case LDAP_NO_MEMORY:
            return NS_ERROR_OUT_OF_MEMORY;

        default:
            return NS_ERROR_UNEXPECTED;
        }

    } 
  
    // make sure the connection knows where to call back once the messages
    // for this operation start coming in
    rv = NS_STATIC_CAST(nsLDAPConnection *, 
                        NS_STATIC_CAST(nsILDAPConnection *, 
                        mConnection.get()))->AddPendingOperation(this);
    switch (rv) {
    case NS_OK:
        break;

        // note that the return value of ldap_abandon_ext() is ignored, as 
        // there's nothing useful to do with it

    case NS_ERROR_OUT_OF_MEMORY:
        (void *)ldap_abandon_ext(mConnectionHandle, mMsgID, 0, 0);
        return NS_ERROR_OUT_OF_MEMORY;
        break;

    case NS_ERROR_UNEXPECTED:
    case NS_ERROR_ILLEGAL_VALUE:
    default:
        (void *)ldap_abandon_ext(mConnectionHandle, mMsgID, 0, 0);
        return NS_ERROR_UNEXPECTED;
    }

    return NS_OK;
}

// wrappers for ldap_search_ext
//
int
nsLDAPOperation::SearchExt(const nsACString& base, // base DN to search
                           int scope, // SCOPE_{BASE,ONELEVEL,SUBTREE}
                           const nsACString& filter, // search filter
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

    return ldap_search_ext(mConnectionHandle, PromiseFlatCString(base).get(),
                           scope, PromiseFlatCString(filter).get(), attrs,
                           attrsOnly, serverctrls, clientctrls, timeoutp,
                           sizelimit, &mMsgID);
}


/**
 * wrapper for ldap_search_ext(): kicks off an async search request.
 *
 * @param aBaseDn               Base DN to search
 * @param aScope                One of SCOPE_{BASE,ONELEVEL,SUBTREE}
 * @param aFilter               Search filter
 * @param aAttrCount            Number of attributes we request (0 for all)
 * @param aAttributes           Array of strings, holding the attributes we need
 * @param aTimeOut              How long to wait
 * @param aSizeLimit            Maximum number of entries to return.
 *
 * XXX doesn't currently handle LDAPControl params
 *
 * void searchExt(in AUTF8String aBaseDn, in PRInt32 aScope,
 *                in AUTF8String aFilter, PRUint32 aAttrCount,
 *                const char **aAttributes, in PRIntervalTime aTimeOut,
 *                in PRInt32 aSizeLimit);
 */
NS_IMETHODIMP
nsLDAPOperation::SearchExt(const nsACString& aBaseDn, PRInt32 aScope, 
                           const nsACString& aFilter, PRUint32 aAttrCount,
                           const char **aAttributes, PRIntervalTime aTimeOut,
                           PRInt32 aSizeLimit) 
{
    PR_LOG(gLDAPLogModule, PR_LOG_DEBUG, 
           ("nsLDAPOperation::SearchExt(): called with aBaseDn = '%s'; "
            "aFilter = '%s', aAttrCounts = %u, aSizeLimit = %d", 
            PromiseFlatCString(aBaseDn).get(),
            PromiseFlatCString(aFilter).get(),
            aAttrCount, aSizeLimit));

    char **attrs = 0;

    // Convert our XPCOM style C-Array to one that the C-SDK will like, i.e.
    // add a last NULL element.
    //
    if (aAttrCount && aAttributes) {
        attrs = NS_STATIC_CAST(char **,
                    nsMemory::Alloc((aAttrCount + 1) * sizeof(char *)));
        if (!attrs) {
            NS_ERROR("nsLDAPOperation::SearchExt: out of memory ");
            return NS_ERROR_OUT_OF_MEMORY;
        }
        memcpy(attrs, aAttributes, aAttrCount * sizeof(char *));
        attrs[aAttrCount] = 0;
    }

    // XXX deal with timeouts
    //
    int retVal = SearchExt(aBaseDn, aScope, aFilter,
                           attrs, 0, 0, 0, 0, aSizeLimit);

    if (attrs) {
        nsMemory::Free(attrs);
    }

    switch (retVal) {

    case LDAP_SUCCESS: 
        break;

    case LDAP_ENCODING_ERROR:
        return NS_ERROR_LDAP_ENCODING_ERROR;

    case LDAP_SERVER_DOWN:
        return NS_ERROR_LDAP_SERVER_DOWN;

    case LDAP_NO_MEMORY:
        return NS_ERROR_OUT_OF_MEMORY;

    case LDAP_NOT_SUPPORTED:
        return NS_ERROR_LDAP_NOT_SUPPORTED;

    case LDAP_PARAM_ERROR:
        return NS_ERROR_INVALID_ARG;

    case LDAP_FILTER_ERROR:
        return NS_ERROR_LDAP_FILTER_ERROR;

    default:
        NS_ERROR("nsLDAPOperation::SearchExt(): unexpected return value");
        return NS_ERROR_UNEXPECTED;
    }

    // make sure the connection knows where to call back once the messages
    // for this operation start coming in
    //
    nsresult rv = NS_STATIC_CAST(nsLDAPConnection *, NS_STATIC_CAST(
        nsILDAPConnection *, mConnection.get()))->AddPendingOperation(this);
    if (NS_FAILED(rv)) {
        switch (rv) {
        case NS_ERROR_OUT_OF_MEMORY: 
            (void *)ldap_abandon_ext(mConnectionHandle, mMsgID, 0, 0);
            return NS_ERROR_OUT_OF_MEMORY;

        default: 
            (void *)ldap_abandon_ext(mConnectionHandle, mMsgID, 0, 0);
            NS_ERROR("nsLDAPOperation::SearchExt(): unexpected error in "
                     "mConnection->AddPendingOperation");
            return NS_ERROR_UNEXPECTED;
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsLDAPOperation::GetMessageID(PRInt32 *aMsgID)
{
    if (!aMsgID) {
        return NS_ERROR_ILLEGAL_VALUE;
    }

    *aMsgID = mMsgID;
   
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
    nsresult retStatus = NS_OK;

    if ( mMessageListener == 0 || mMsgID == 0 ) {
        NS_ERROR("nsLDAPOperation::AbandonExt(): mMessageListener or "
                 "mMsgId not initialized");
        return NS_ERROR_NOT_INITIALIZED;
    }

    rv = ldap_abandon_ext(mConnectionHandle, mMsgID, serverctrls, 
                          clientctrls);
    switch (rv) {

    case LDAP_SUCCESS:
        break;

    case LDAP_ENCODING_ERROR:
        return NS_ERROR_LDAP_ENCODING_ERROR;
    
    case LDAP_SERVER_DOWN:
        retStatus = NS_ERROR_LDAP_SERVER_DOWN;
        break;

    case LDAP_NO_MEMORY:
        return NS_ERROR_OUT_OF_MEMORY;

    case LDAP_PARAM_ERROR:
        return NS_ERROR_INVALID_ARG;

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
    // check mConnection in case we're getting bit by 
    // http://bugzilla.mozilla.org/show_bug.cgi?id=239729, wherein we 
    // theorize that ::Clearing the operation is nulling out the mConnection
    // from another thread.
    if (mConnection)
    {
      rv = NS_STATIC_CAST(nsLDAPConnection *, NS_STATIC_CAST(
          nsILDAPConnection *, mConnection.get()))->RemovePendingOperation(this);

      if (NS_FAILED(rv)) {
          // XXXdmose should we use keep Abandon from happening on multiple 
          // threads at the same time?  that's when this condition is most 
          // likely to occur.  i _think_ the LDAP C SDK is ok with this; need 
          // to verify.
          //
          NS_WARNING("nsLDAPOperation::AbandonExt: "
                     "mConnection->RemovePendingOperation(this) failed.");
      }
    }

    return retStatus;
}

NS_IMETHODIMP
nsLDAPOperation::Abandon(void)
{
    return nsLDAPOperation::AbandonExt(0, 0);
}
