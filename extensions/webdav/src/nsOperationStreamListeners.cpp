/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:expandtab:ts=4 sw=4:
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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is Oracle Corporation.
 * Portions created by Oracle Corporation are  Copyright (C) 2004
 * by Oracle Corporation.  All Rights Reserved.
 *
 * Contributor(s):
 *   Mike Shaver <shaver@off.net> (original author)
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

#include "nsIHttpChannel.h"
#include "nsIIOService.h"
#include "nsNetUtil.h"

#include "nsIWebDAVResource.h"
#include "nsIWebDAVListener.h"

#include "nsWebDAVInternal.h"

class OperationStreamListener : public nsIStreamListener
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER
    
    enum OperationMode {
        PUT, GET
    };
    
    OperationStreamListener(nsIWebDAVResource *resource,
                            nsIWebDAVOperationListener *listener,
                            OperationMode mode) :
        mResource(resource), mListener(listener), mMode(mode) { }
    virtual ~OperationStreamListener() { }
    
protected:
    nsCOMPtr<nsIWebDAVResource>          mResource;
    nsCOMPtr<nsIWebDAVOperationListener> mListener;
    OperationMode                        mMode;
};

NS_IMPL_ADDREF(OperationStreamListener)
NS_IMPL_RELEASE(OperationStreamListener)
NS_INTERFACE_MAP_BEGIN(OperationStreamListener)
  NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
  NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIStreamListener)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
OperationStreamListener::OnStartRequest(nsIRequest *aRequest,
                                           nsISupports *aContext)
{
  return NS_OK;
}

NS_IMETHODIMP
OperationStreamListener::OnStopRequest(nsIRequest *aRequest,
                                       nsISupports *aContext,
                                       nsresult aStatusCode)
{
    switch (mMode) {
    case PUT:
        mListener->OnPutResult(aStatusCode, mResource);
        break;
    case GET:
        mListener->OnGetResult(aStatusCode, mResource);
        break;
    }
    return NS_OK;
}

NS_IMETHODIMP
OperationStreamListener::OnDataAvailable(nsIRequest *aRequest,
                                         nsISupports *aContext,
                                         nsIInputStream *aInputStream,
                                         PRUint32 offset, PRUint32 count)
{
    aRequest->Cancel(NS_BINDING_ABORTED);
    return NS_BINDING_ABORTED;
}

nsresult
NS_WD_NewPutOperationStreamListener(nsIWebDAVResource *resource,
                                    nsIWebDAVOperationListener *listener,
                                    nsIStreamListener **streamListener)
{
    nsCOMPtr<nsIRequestObserver> osl = 
        new OperationStreamListener(resource, listener,
                                    OperationStreamListener::PUT);
    if (!osl)
        return NS_ERROR_OUT_OF_MEMORY;
    return CallQueryInterface(osl, streamListener);
}

nsresult
NS_WD_NewGetOperationRequestObserver(nsIWebDAVResource *resource,
                                     nsIWebDAVOperationListener *listener,
                                     nsIRequestObserver **observer)
{
    nsCOMPtr<nsIRequestObserver> osl = 
        new OperationStreamListener(resource, listener,
                                    OperationStreamListener::GET);
    if (!osl)
        return NS_ERROR_OUT_OF_MEMORY;
    return CallQueryInterface(osl, observer);
}
