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
#include "nsIObserverService.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIContent.h"
#include "nsIDocument.h"

static NS_DEFINE_IID(kIWalletServiceIID, NS_IWALLETSERVICE_IID);
static NS_DEFINE_IID(kIDOMHTMLFormElementIID, NS_IDOMHTMLFORMELEMENT_IID);
static NS_DEFINE_IID(kIFormSubmitObserverIID, NS_IFORMSUBMITOBSERVER_IID);


nsWalletlibService::nsWalletlibService()
{
    NS_INIT_REFCNT();
    Init();
}

nsWalletlibService::~nsWalletlibService()
{
    nsIObserverService *svc = 0;
    nsresult rv = nsServiceManager::GetService( NS_OBSERVERSERVICE_PROGID,
                                                nsIObserverService::GetIID(),
                                                (nsISupports**)&svc );
    if ( NS_SUCCEEDED( rv ) && svc ) {
        nsString  topic(NS_FORMSUBMIT_SUBJECT);
        rv = svc->RemoveObserver( this, topic.GetUnicode());
        nsServiceManager::ReleaseService( NS_OBSERVERSERVICE_PROGID, svc );
    }
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
    if (iid.Equals(kIFormSubmitObserverIID)) {
	*result = NS_STATIC_CAST(nsIFormSubmitObserver*, this);
	AddRef();
	return NS_OK;
    }
    return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsWalletlibService);
NS_IMPL_RELEASE(nsWalletlibService);

NS_IMETHODIMP nsWalletlibService::WALLET_ChangePassword() {
    ::WLLT_ChangePassword();
    return NS_OK;
}

NS_IMETHODIMP nsWalletlibService::WALLET_PreEdit(nsAutoString& walletList) {
    ::WLLT_PreEdit(walletList);
    return NS_OK;
}

NS_IMETHODIMP nsWalletlibService::WALLET_PostEdit(nsAutoString walletList) {
    ::WLLT_PostEdit(walletList);
    return NS_OK;
}

NS_IMETHODIMP nsWalletlibService::WALLET_Prefill
        (nsIPresShell* shell, nsString url, PRBool quick) {
    return ::WLLT_Prefill(shell, url, quick);
}

NS_IMETHODIMP nsWalletlibService::WALLET_OKToCapture
        (PRBool* result, PRInt32 count, char* URLName) {
    ::WLLT_OKToCapture(result, count, URLName);
    return NS_OK;
}

NS_IMETHODIMP nsWalletlibService::WALLET_Capture(
        nsIDocument* doc,
        nsString name,
        nsString value,
        nsString vcard) {
    ::WLLT_Capture(doc, name, value, vcard);
    return NS_OK;
}

NS_IMETHODIMP nsWalletlibService::WALLET_PrefillReturn(nsAutoString results){
    ::WLLT_PrefillReturn(results);
    return NS_OK;
}

NS_IMETHODIMP nsWalletlibService::SI_DisplaySignonInfoAsHTML(){
    ::SINGSIGN_DisplaySignonInfoAsHTML();
    return NS_OK;
}

NS_IMETHODIMP nsWalletlibService::SI_SignonViewerReturn(nsAutoString results){
    ::SINGSIGN_SignonViewerReturn(results);
    return NS_OK;
}

NS_IMETHODIMP nsWalletlibService::SI_GetSignonListForViewer(nsString& aSignonList){
    ::SINGSIGN_GetSignonListForViewer(aSignonList);
    return NS_OK;
}

NS_IMETHODIMP nsWalletlibService::SI_GetRejectListForViewer(nsString& aRejectList){
    ::SINGSIGN_GetRejectListForViewer(aRejectList);
    return NS_OK;
}

NS_IMETHODIMP nsWalletlibService::WALLET_GetNopreviewListForViewer(nsString& aNopreviewList){
    ::WLLT_GetNopreviewListForViewer(aNopreviewList);
    return NS_OK;
}

NS_IMETHODIMP nsWalletlibService::WALLET_GetNocaptureListForViewer(nsString& aNocaptureList){
    ::WLLT_GetNocaptureListForViewer(aNocaptureList);
    return NS_OK;
}

NS_IMETHODIMP nsWalletlibService::WALLET_GetPrefillListForViewer(nsString& aPrefillList){
    ::WLLT_GetPrefillListForViewer(aPrefillList);
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

NS_IMETHODIMP nsWalletlibService::Observe(nsISupports*, const PRUnichar*, const PRUnichar*) 
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

#define CRLF "\015\012"   
NS_IMETHODIMP nsWalletlibService::Notify(nsIContent* formNode) 
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

void nsWalletlibService::Init() 
{
    nsIObserverService *svc = 0;
    nsresult rv = nsServiceManager::GetService( NS_OBSERVERSERVICE_PROGID,
                                                nsIObserverService::GetIID(),
                                                (nsISupports**)&svc );
    if ( NS_SUCCEEDED( rv ) && svc ) {
        nsString  topic(NS_FORMSUBMIT_SUBJECT);
        rv = svc->AddObserver( this, topic.GetUnicode());
        nsServiceManager::ReleaseService( NS_OBSERVERSERVICE_PROGID, svc );
    }
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
