/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

//#include "nsRepository.h"
#include "nsWalletService.h"
//#include "nsINetService.h"
#include "nsIServiceManager.h"
//static NS_DEFINE_IID(kINetServiceIID, NS_INETSERVICE_IID);
//static NS_DEFINE_IID(kNetServiceCID, NS_NETSERVICE_CID);

//#include "nsIWebShell.h"
//static NS_DEFINE_IID(kIWebShellIID, NS_IWEB_SHELL_IID);
//static NS_DEFINE_IID(kWebShellCID, NS_WEB_SHELL_CID);

static NS_DEFINE_IID(kIWalletServiceIID, NS_IWALLETSERVICE_IID);

#include "wallet.h"

//#if defined(HAS_C_PLUS_PLUS_CASTS)
//#define NS_STATIC_CAST(__type, __ptr)	   static_cast<__type>(__ptr)
//#else
//#define NS_STATIC_CAST(__type, __ptr)	   ((__type)(__ptr))
//#endif

nsWalletlibService::nsWalletlibService()
{
    NS_INIT_REFCNT();
}

nsWalletlibService::~nsWalletlibService()
{
}

/* calls into the wallet module */

NS_IMETHODIMP
nsWalletlibService::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
	return NS_ERROR_NULL_POINTER;

    *result = nsnull;
    if (iid.Equals(kIWalletServiceIID)) {
	*result = NS_STATIC_CAST(nsIWalletService*, this);
	AddRef();
	return NS_OK;
    }
    return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsWalletlibService);
NS_IMPL_RELEASE(nsWalletlibService);

NS_IMETHODIMP nsWalletlibService::WALLET_PreEdit(nsIURL* url) {
    ::WLLT_PreEdit(url);
    return NS_OK;
}

NS_IMETHODIMP nsWalletlibService::WALLET_Prefill(nsIPresShell* shell, PRBool quick) {
    ::WLLT_Prefill(shell, quick);
    return NS_OK;
}

NS_IMETHODIMP nsWalletlibService::WALLET_OKToCapture
        (PRBool* result, PRInt32 count, char* URLName) {
    ::WLLT_OKToCapture(result, count, URLName);
    return NS_OK;
}

NS_IMETHODIMP nsWalletlibService::WALLET_Capture(
        nsIDocument* doc,
        nsString name,
        nsString value) {
    ::WLLT_Capture(doc, name, value);
    return NS_OK;
}


/* call to create the wallet object */

nsresult
NS_NewWalletService(nsIWalletService** result)
{
    nsIWalletService* wallet = new nsWalletlibService();
    if (! wallet)
	return NS_ERROR_NULL_POINTER;

    *result = wallet;
    NS_ADDREF(*result);
    return NS_OK;
}
