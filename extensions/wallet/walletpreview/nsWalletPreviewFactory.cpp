/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*

  A sample of XPConnect. This file contains the XPCOM factory the
  creates for WalletPreviewImpl objects.

*/

#include "nsCOMPtr.h"
#include "nscore.h"
#include "nsIWalletPreview.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsXPComFactory.h"

static NS_DEFINE_IID(kISupportsIID,        NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID,         NS_IFACTORY_IID);
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kWalletPreviewCID,            NS_WALLETPREVIEW_CID);

class WalletPreviewFactoryImpl : public nsIFactory
{
public:
    WalletPreviewFactoryImpl(const nsCID &aClass, const char* className, const char* contractID);

    // nsISupports methods
    NS_DECL_ISUPPORTS

    // nsIFactory methods
    NS_IMETHOD CreateInstance(nsISupports *aOuter,
                              const nsIID &aIID,
                              void **aResult);

    NS_IMETHOD LockFactory(PRBool aLock);

protected:
    virtual ~WalletPreviewFactoryImpl();

protected:
    nsCID       mClassID;
    const char* mClassName;
    const char* mContractID;
};

////////////////////////////////////////////////////////////////////////

WalletPreviewFactoryImpl::WalletPreviewFactoryImpl(const nsCID &aClass, 
                                   const char* className,
                                   const char* contractID)
    : mClassID(aClass), mClassName(className), mContractID(contractID)
{
    NS_INIT_REFCNT();
}

WalletPreviewFactoryImpl::~WalletPreviewFactoryImpl()
{
}

NS_IMETHODIMP
WalletPreviewFactoryImpl::QueryInterface(const nsIID &aIID, void **aResult)
{
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    // Always NULL result, in case of failure
    *aResult = nsnull;

    if (aIID.Equals(kISupportsIID)) {
        *aResult = NS_STATIC_CAST(nsISupports*, this);
        AddRef();
        return NS_OK;
    } else if (aIID.Equals(kIFactoryIID)) {
        *aResult = NS_STATIC_CAST(nsIFactory*, this);
        AddRef();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(WalletPreviewFactoryImpl);
NS_IMPL_RELEASE(WalletPreviewFactoryImpl);

NS_IMETHODIMP
WalletPreviewFactoryImpl::CreateInstance(nsISupports *aOuter,
                                 const nsIID &aIID,
                                 void **aResult)
{
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    *aResult = nsnull;

    nsresult rv;

    nsISupports *inst = nsnull;
    if (mClassID.Equals(kWalletPreviewCID)) {
        if (NS_FAILED(rv = NS_NewWalletPreview((nsIWalletPreview**) &inst)))
            return rv;
    }
    else {
        return NS_ERROR_NO_INTERFACE;
    }

    if (! inst)
        return NS_ERROR_OUT_OF_MEMORY;

    if (NS_FAILED(rv = inst->QueryInterface(aIID, aResult))) {
        // We didn't get the right interface.
        NS_ERROR("didn't support the interface you wanted");
    }

    NS_IF_RELEASE(inst);
    return rv;
}

nsresult WalletPreviewFactoryImpl::LockFactory(PRBool aLock)
{
    // Not implemented in simplest case.
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////



// return the proper factory to the caller
extern "C" PR_IMPLEMENT(nsresult)
NSGetFactory(nsISupports* aServMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aContractID,
             nsIFactory **aFactory)
{
    if (! aFactory)
        return NS_ERROR_NULL_POINTER;

    WalletPreviewFactoryImpl* factory = new WalletPreviewFactoryImpl(aClass, aClassName, aContractID);
    if (factory == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(factory);
    *aFactory = factory;
    return NS_OK;
}



extern "C" PR_IMPLEMENT(nsresult)
NSRegisterSelf(nsISupports* aServMgr , const char* aPath)
{
    nsresult rv;

    nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIComponentManager> compMgr = 
             do_GetService(kComponentManagerCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = compMgr->RegisterComponent(kWalletPreviewCID,
                                    "WalletPreview World Component",
                                    "@mozilla.org/walletpreview/walletpreview-world;1",
                                    aPath, PR_TRUE, PR_TRUE);

    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


extern "C" PR_IMPLEMENT(nsresult)
NSUnregisterSelf(nsISupports* aServMgr, const char* aPath)
{
    nsresult rv;

    nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIComponentManager> compMgr = 
             do_GetService(kComponentManagerCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = compMgr->UnregisterComponent(kWalletPreviewCID, aPath);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}
