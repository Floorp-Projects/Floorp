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

#include "nsWalletService.h"
#include "nsIServiceManager.h"
#include "wallet.h"
#include "singsign.h"

static NS_DEFINE_IID(kIWalletServiceIID, NS_IWALLETSERVICE_IID);

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

NS_IMETHODIMP nsWalletlibService::SI_DisplaySignonInfoAsHTML(){
    ::SINGSIGN_DisplaySignonInfoAsHTML();
    return NS_OK;
}

NS_IMETHODIMP nsWalletlibService::SI_RememberSignonData
        (char* URLName, char** name_array, char** value_array, char** type_array, PRInt32 value_cnt) {
    ::SINGSIGN_RememberSignonData(URLName, name_array, value_array, type_array, value_cnt);
    return NS_OK;
}

NS_IMETHODIMP nsWalletlibService::SI_RestoreSignonData
        (char* URLName, char* name, char** value) {
    ::SINGSIGN_RestoreSignonData(URLName, name, value);
    return NS_OK;
}

NS_IMETHODIMP nsWalletlibService::SI_PromptUsernameAndPassword
        (char *prompt, char **username, char **password, char *URLName, PRBool &status) {
    status = ::SINGSIGN_PromptUsernameAndPassword(prompt, username, password, URLName);
    return NS_OK;
}

NS_IMETHODIMP nsWalletlibService::SI_PromptPassword
        (char *prompt, char **password, char *URLName, PRBool pickFirstUser) {
    *password = ::SINGSIGN_PromptPassword(prompt, URLName, pickFirstUser);
    return NS_OK;
}

NS_IMETHODIMP nsWalletlibService::SI_Prompt 
        (char *prompt, char **username, char *URLName) {
    *username = ::SINGSIGN_Prompt(prompt, *username, URLName);
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
