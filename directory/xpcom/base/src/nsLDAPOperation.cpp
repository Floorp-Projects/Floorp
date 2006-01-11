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

#include "nsLDAPInternal.h"
#include "nsLDAPOperation.h"
#include "nsLDAPConnection.h"
#include "nsILDAPMessage.h"
#include "nsIComponentManager.h"
#include "nsXPIDLString.h"
#include "nspr.h"
#include "nsISimpleEnumerator.h"
#include "nsLDAPControl.h"
#include "nsILDAPErrors.h"

// constructor
nsLDAPOperation::nsLDAPOperation()
{
}

// destructor
nsLDAPOperation::~nsLDAPOperation()
{
}


NS_IMPL_THREADSAFE_ADDREF(nsLDAPOperation)
NS_IMPL_THREADSAFE_RELEASE(nsLDAPOperation)
NS_INTERFACE_MAP_BEGIN(nsLDAPOperation)
  NS_INTERFACE_MAP_ENTRY(nsILDAPOperation)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsILDAPOperation)
  NS_IMPL_QUERY_CLASSINFO(nsLDAPOperation)
NS_INTERFACE_MAP_END_THREADSAFE
NS_IMPL_CI_INTERFACE_GETTER1(nsLDAPOperation, nsILDAPOperation)


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

/**
 * Given an nsIArray of nsILDAPControls, return the appropriate
 * zero-terminated array of LDAPControls ready to pass in to the C SDK.
 */
static nsresult
convertControlArray(nsIArray *aXpcomArray, LDAPControl ***aArray)
{
    // get the size of the original array
    PRUint32 length;
    nsresult rv  = aXpcomArray->GetLength(&length);
    NS_ENSURE_SUCCESS(rv, rv);

    // don't allocate an array if someone passed us in an empty one
    if (!length) {
        *aArray = 0;
        return NS_OK;
    }

    // allocate a local array of the form understood by the C-SDK;
    // +1 is to account for the final null terminator.  PR_Calloc is
    // is used so that ldap_controls_free will work anywhere during the
    // iteration
    LDAPControl **controls = 
        NS_STATIC_CAST(LDAPControl **,
                       PR_Calloc(length+1, sizeof(LDAPControl)));

    // prepare to enumerate the array
    nsCOMPtr<nsISimpleEnumerator> enumerator;
    rv = aXpcomArray->Enumerate(getter_AddRefs(enumerator));
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool moreElements;
    rv = enumerator->HasMoreElements(&moreElements);
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint32 i = 0;
    while (moreElements) {

        // get the next array element
        nsCOMPtr<nsISupports> isupports;
        rv = enumerator->GetNext(getter_AddRefs(isupports));
        if (NS_FAILED(rv)) {
            ldap_controls_free(controls);
            return rv;
        }
        nsCOMPtr<nsILDAPControl> control = do_QueryInterface(isupports, &rv);
        if (NS_FAILED(rv)) {
            ldap_controls_free(controls);
            return NS_ERROR_INVALID_ARG; // bogus element in the array
        }
        nsLDAPControl *ctl = NS_STATIC_CAST(nsLDAPControl *,
                                            NS_STATIC_CAST(nsILDAPControl *,
                                                           control.get()));

        // convert it to an LDAPControl structure placed in the new array
        rv = ctl->ToLDAPControl(&controls[i]);
        if (NS_FAILED(rv)) {
            ldap_controls_free(controls);
            return rv;
        }

        // on to the next element
        rv = enumerator->HasMoreElements(&moreElements);
        if (NS_FAILED(rv)) {
            ldap_controls_free(controls);
            return NS_ERROR_UNEXPECTED;
        }
        ++i;
    }

    *aArray = controls;
    return NS_OK;
}

NS_IMETHODIMP
nsLDAPOperation::SearchExt(const nsACString& aBaseDn, PRInt32 aScope, 
                           const nsACString& aFilter, 
                           PRUint32 aAttrCount, const char **aAttributes,
                           PRIntervalTime aTimeOut, PRInt32 aSizeLimit) 
{
    if (!mMessageListener) {
        NS_ERROR("nsLDAPOperation::SearchExt(): mMessageListener not set");
        return NS_ERROR_NOT_INITIALIZED;
    }

    // XXX add control logging
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

    LDAPControl **serverctls = 0;
    nsresult rv;
    if (mServerControls) {
        rv = convertControlArray(mServerControls, &serverctls);
        if (NS_FAILED(rv)) {
            PR_LOG(gLDAPLogModule, PR_LOG_ERROR,
                   ("nsLDAPOperation::SearchExt(): error converting server "
                    "control array: %x", rv));

            if (attrs) {
                nsMemory::Free(attrs);
            }
            return rv;
        }
    }


    LDAPControl **clientctls = 0;
    if (mClientControls) {
        rv = convertControlArray(mClientControls, &clientctls);
        if (NS_FAILED(rv)) {
            PR_LOG(gLDAPLogModule, PR_LOG_ERROR,
                   ("nsLDAPOperation::SearchExt(): error converting client "
                    "control array: %x", rv));
            if (attrs) {
                nsMemory::Free(attrs);
            }
            ldap_controls_free(serverctls);
            return rv;
        }
    }

    // XXX deal with timeout here
    int retVal = ldap_search_ext(mConnectionHandle, 
                                 PromiseFlatCString(aBaseDn).get(),
                                 aScope, PromiseFlatCString(aFilter).get(), 
                                 attrs, 0, serverctls, clientctls, 0,
                                 aSizeLimit, &mMsgID);

    // clean up
    ldap_controls_free(serverctls);        
    ldap_controls_free(clientctls);
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
    rv = NS_STATIC_CAST(nsLDAPConnection *, NS_STATIC_CAST(
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
NS_IMETHODIMP
nsLDAPOperation::AbandonExt()
{
    nsresult rv;
    nsresult retStatus = NS_OK;

    if ( mMessageListener == 0 || mMsgID == 0 ) {
        NS_ERROR("nsLDAPOperation::AbandonExt(): mMessageListener or "
                 "mMsgId not initialized");
        return NS_ERROR_NOT_INITIALIZED;
    }

    // XXX handle controls here
    if (mServerControls || mClientControls) {
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    rv = ldap_abandon_ext(mConnectionHandle, mMsgID, 0, 0);
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
          // XXXdmose should we keep AbandonExt from happening on multiple 
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
nsLDAPOperation::GetClientControls(nsIMutableArray **aControls)
{
    NS_IF_ADDREF(*aControls = mClientControls);
    return NS_OK;
}

NS_IMETHODIMP
nsLDAPOperation::SetClientControls(nsIMutableArray *aControls)
{
    mClientControls = aControls;
    return NS_OK;
}

NS_IMETHODIMP nsLDAPOperation::GetServerControls(nsIMutableArray **aControls)
{
    NS_IF_ADDREF(*aControls = mServerControls);
    return NS_OK;
}

NS_IMETHODIMP nsLDAPOperation::SetServerControls(nsIMutableArray *aControls)
{
    mServerControls = aControls;
    return NS_OK;
}
