/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsCOMPtr.h"
#include "nsWalletService.h"
#include "nsIServiceManager.h"
#include "wallet.h"
#include "singsign.h"
#include "nsIObserverService.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDocumentLoader.h"
#include "nsCURILoader.h"
#include "nsIWebShell.h"
#include "nsIDocumentViewer.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIFormControl.h"
#include "nsIDocShell.h"
#include "nsIDOMWindowInternal.h"
#include "nsINetSupportDialogService.h"
#include "nsIInterfaceRequestor.h"
#include "nsIPrompt.h"

static NS_DEFINE_IID(kDocLoaderServiceCID, NS_DOCUMENTLOADER_SERVICE_CID);
static NS_DEFINE_CID(kNetSupportDialogCID, NS_NETSUPPORTDIALOG_CID);

nsWalletlibService::nsWalletlibService()
{
  NS_INIT_REFCNT();
  ++mRefCnt; // Stabilization that can't accidentally |Release()| me
    Init();
  --mRefCnt;
}

nsWalletlibService::~nsWalletlibService()
{
#ifdef DEBUG_dp
  printf("Wallet Service destroyed successfully.\n");
#endif /* DEBUG_dp */
}

NS_IMPL_THREADSAFE_ISUPPORTS5(nsWalletlibService,
                              nsIWalletService,
                              nsIObserver,
                              nsIFormSubmitObserver,
                              nsIDocumentLoaderObserver,
                              nsISupportsWeakReference)

NS_IMETHODIMP nsWalletlibService::WALLET_PreEdit(nsAutoString& walletList) {
  ::WLLT_PreEdit(walletList);
  return NS_OK;
}

NS_IMETHODIMP nsWalletlibService::WALLET_PostEdit(nsAutoString walletList) {
  ::WLLT_PostEdit(walletList);
  return NS_OK;
}

NS_IMETHODIMP nsWalletlibService::WALLET_ChangePassword(PRBool* status) {
  ::WLLT_ChangePassword(status);
  return NS_OK;
}

NS_IMETHODIMP nsWalletlibService::WALLET_DeleteAll() {
  ::WLLT_DeleteAll();
  return NS_OK;
}

NS_IMETHODIMP nsWalletlibService::WALLET_RequestToCapture(nsIPresShell* shell, nsIDOMWindowInternal* win, PRUint32* status) {
  ::WLLT_RequestToCapture(shell, win, status);
  return NS_OK;
}

NS_IMETHODIMP nsWalletlibService::WALLET_Prefill(nsIPresShell* shell, PRBool quick, nsIDOMWindowInternal* win, PRBool* status) {
  return ::WLLT_Prefill(shell, quick, win);
}

NS_IMETHODIMP nsWalletlibService::WALLET_PrefillReturn(nsAutoString results){
  ::WLLT_PrefillReturn(results);
  return NS_OK;
}

NS_IMETHODIMP nsWalletlibService::WALLET_FetchFromNetCenter(){
  ::WLLT_FetchFromNetCenter();
  return NS_OK;
}

NS_IMETHODIMP nsWalletlibService::WALLET_ExpirePassword(PRBool* status){
  ::WLLT_ExpirePassword(status);
  return NS_OK;
}

NS_IMETHODIMP nsWalletlibService::WALLET_InitReencryptCallback(nsIDOMWindowInternal* window){
  /* register callback to be used when encryption pref changes */
  ::WLLT_InitReencryptCallback(window);
  return NS_OK;
}

NS_IMETHODIMP nsWalletlibService::SI_RemoveUser(const char *key, const PRUnichar *userName) {
  ::SINGSIGN_RemoveUser(key, userName);
  return NS_OK;
}

NS_IMETHODIMP nsWalletlibService::SI_StorePassword(const char *key, const PRUnichar *userName, const PRUnichar *password) {
  ::SINGSIGN_StorePassword(key, userName, password);
  return NS_OK;
}


NS_IMETHODIMP nsWalletlibService::WALLET_GetNopreviewListForViewer(nsAutoString& aNopreviewList){
  ::WLLT_GetNopreviewListForViewer(aNopreviewList);
  return NS_OK;
}

NS_IMETHODIMP nsWalletlibService::WALLET_GetNocaptureListForViewer(nsAutoString& aNocaptureList){
  ::WLLT_GetNocaptureListForViewer(aNocaptureList);
  return NS_OK;
}

NS_IMETHODIMP nsWalletlibService::WALLET_GetPrefillListForViewer(nsAutoString& aPrefillList){
  ::WLLT_GetPrefillListForViewer(aPrefillList);
  return NS_OK;
}

NS_IMETHODIMP nsWalletlibService::SI_GetSignonListForViewer(nsAutoString& aSignonList){
  ::SINGSIGN_GetSignonListForViewer(aSignonList);
  return NS_OK;
}

NS_IMETHODIMP nsWalletlibService::SI_GetRejectListForViewer(nsAutoString& aRejectList){
  ::SINGSIGN_GetRejectListForViewer(aRejectList);
  return NS_OK;
}

NS_IMETHODIMP nsWalletlibService::SI_SignonViewerReturn(nsAutoString results){
  ::SINGSIGN_SignonViewerReturn(results);
  return NS_OK;
}

NS_IMETHODIMP nsWalletlibService::Observe(nsISupports*, const PRUnichar*, const PRUnichar*) 
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

#define CRLF "\015\012"   
NS_IMETHODIMP nsWalletlibService::Notify(nsIContent* formNode, nsIDOMWindowInternal* window, nsIURI* actionURL)
{
  if (!formNode) {
    return NS_ERROR_FAILURE;
  }
  ::WLLT_OnSubmit(formNode, window);
  return NS_OK;
}

void nsWalletlibService::Init() 
{
  nsIObserverService *svc = 0;
  nsIDocumentLoader *docLoaderService;

  nsresult rv = nsServiceManager::GetService
    (NS_OBSERVERSERVICE_CONTRACTID, NS_GET_IID(nsIObserverService), (nsISupports**)&svc );
  if ( NS_SUCCEEDED( rv ) && svc ) {
    nsString  topic; topic.AssignWithConversion(NS_FORMSUBMIT_SUBJECT);
    rv = svc->AddObserver( this, topic.GetUnicode());
    nsServiceManager::ReleaseService( NS_OBSERVERSERVICE_CONTRACTID, svc );
  }

  // Get the global document loader service...  
  rv = nsServiceManager::GetService
    (kDocLoaderServiceCID, NS_GET_IID(nsIDocumentLoader), (nsISupports **)&docLoaderService);
  if (NS_SUCCEEDED(rv) && docLoaderService) {
    //Register ourselves as an observer for the new doc loader
    docLoaderService->AddObserver((nsIDocumentLoaderObserver*)this);
    nsServiceManager::ReleaseService(kDocLoaderServiceCID, docLoaderService );
  }

}

NS_IMETHODIMP
nsWalletlibService::OnStartDocumentLoad(nsIDocumentLoader* aLoader, nsIURI* aURL, const char* aCommand)
{
  return NS_OK;
}

#include "prmem.h"

NS_IMETHODIMP
nsWalletlibService::OnEndDocumentLoad(nsIDocumentLoader* aLoader, nsIChannel* channel, nsresult aStatus)
{
  nsresult rv = NS_OK;

  if (aLoader == nsnull) {
    return rv;
  }
  nsCOMPtr<nsISupports> cont;
  rv = aLoader->GetContainer(getter_AddRefs(cont));
  if (NS_FAILED(rv) || (cont == nsnull)) {
    return rv;
  }
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(cont));
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

  nsCOMPtr<nsIContentViewer> cv;
  rv = docShell->GetContentViewer(getter_AddRefs(cv));
  if (NS_FAILED(rv) || (cv == nsnull)) {
    return rv;
  }
  nsCOMPtr<nsIDocumentViewer> docViewer(do_QueryInterface(cv));
  NS_ENSURE_TRUE(docViewer, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocument> doc;
  rv = docViewer->GetDocument(*getter_AddRefs(doc));
  if (NS_FAILED(rv) || (doc == nsnull)) {
    return rv;
  }
  
  /* get url name as ascii string */
  char *URLName = nsnull;
  nsIURI* docURL = nsnull;
  char* spec;
  if (!doc) {
    return NS_OK;
  }
  docURL = doc->GetDocumentURL();
  if (!docURL) {
    return NS_OK;
  }
  (void)docURL->GetSpec(&spec);
  URLName = (char*)PR_Malloc(PL_strlen(spec)+1);
  PL_strcpy(URLName, spec);
  NS_IF_RELEASE(docURL);
  nsCRT::free(spec);

  nsCOMPtr<nsIDOMHTMLDocument> htmldoc(do_QueryInterface(doc));
  if (htmldoc == nsnull) {
    PR_Free(URLName);
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDOMHTMLCollection> forms;
  rv = htmldoc->GetForms(getter_AddRefs(forms));
  if (NS_FAILED(rv) || (forms == nsnull)) {
    PR_Free(URLName);
    return rv;
  }

  PRUint32 elementNumber = 0;
  PRUint32 numForms;
  forms->GetLength(&numForms);
  for (PRUint32 formX = 0; formX < numForms; formX++) {
    nsCOMPtr<nsIDOMNode> formNode;
    forms->Item(formX, getter_AddRefs(formNode));
    if (nsnull != formNode) {
      nsCOMPtr<nsIDOMHTMLFormElement> formElement(do_QueryInterface(formNode));
      if ((nsnull != formElement)) {
        nsCOMPtr<nsIDOMHTMLCollection> elements;
        rv = formElement->GetElements(getter_AddRefs(elements));
        if ((NS_SUCCEEDED(rv)) && (nsnull != elements)) {
          /* got to the form elements at long last */ 
          PRUint32 numElements;
          elements->GetLength(&numElements);
          /* get number of passwords on form */
          PRInt32 passwordCount = 0;
          for (PRUint32 elementXX = 0; elementXX < numElements; elementXX++) {
            nsCOMPtr<nsIDOMNode> elementNode;
            elements->Item(elementXX, getter_AddRefs(elementNode));
            if (nsnull != elementNode) {
              nsCOMPtr<nsIDOMHTMLInputElement> inputElement(do_QueryInterface(elementNode));
              if ((NS_SUCCEEDED(rv)) && (nsnull != inputElement)) {
                nsAutoString type;
                rv = inputElement->GetType(type);
                if (NS_SUCCEEDED(rv)) {
                  if (type.CompareWithConversion("password", PR_TRUE) == 0) {
                    passwordCount++;
                  }
                }
              }
            }
          }
          /* don't prefill if there were no passwords on the form */
          if (passwordCount == 0) {
            continue;
          }
          for (PRUint32 elementX = 0; elementX < numElements; elementX++) {
            nsCOMPtr<nsIDOMNode> elementNode;
            elements->Item(elementX, getter_AddRefs(elementNode));
            if (nsnull != elementNode) {
              nsCOMPtr<nsIDOMHTMLInputElement> inputElement(do_QueryInterface(elementNode));
              if ((NS_SUCCEEDED(rv)) && (nsnull != inputElement)) {
                nsAutoString type;
                rv = inputElement->GetType(type);
                if (NS_SUCCEEDED(rv)) {
                  if ((type.IsEmpty()) || (type.CompareWithConversion("text", PR_TRUE) == 0) ||
                    (type.CompareWithConversion("password", PR_TRUE) == 0)) {
                    nsAutoString field;
                    rv = inputElement->GetName(field);
                    if (NS_SUCCEEDED(rv)) {
                      PRUnichar* nameString = field.ToNewUnicode();
                      if (nameString) {
                        /* note: we do not want to prefill if there is a default value */
                        nsAutoString value;
                        rv = inputElement->GetValue(value);
                        if (NS_FAILED(rv) || value.Length() == 0) {
                          PRUnichar* valueString = NULL;
                          nsCOMPtr<nsIInterfaceRequestor> interfaces;
                          nsCOMPtr<nsIPrompt> prompter;

                          if (channel)
                            channel->GetNotificationCallbacks(getter_AddRefs(interfaces));
                          if (interfaces)
                            interfaces->GetInterface(NS_GET_IID(nsIPrompt), getter_AddRefs(prompter));
                          if (!prompter)
                            prompter = do_GetService(kNetSupportDialogCID);
                          if (prompter) {
                            SINGSIGN_RestoreSignonData(prompter, URLName, nameString, &valueString, elementNumber++);
                          }
                          if (valueString) {
                            nsAutoString value(valueString);                                    
                            rv = inputElement->SetValue(value);
                            // warning! don't delete valueString
                          }
                        }
                        Recycle(nameString);
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  PR_Free(URLName);
  return rv;
}

NS_IMETHODIMP
nsWalletlibService::OnStartURLLoad
  (nsIDocumentLoader* loader, nsIChannel* channel)
{
 return NS_OK;
}

NS_IMETHODIMP
nsWalletlibService::OnProgressURLLoad
  (nsIDocumentLoader* loader, nsIChannel* channel, PRUint32 aProgress, PRUint32 aProgressMax)
{
  return NS_OK;
}

NS_IMETHODIMP
nsWalletlibService::OnStatusURLLoad
  (nsIDocumentLoader* loader, nsIChannel* channel, nsString& aMsg)
{
  return NS_OK;
}


NS_IMETHODIMP
nsWalletlibService::OnEndURLLoad
  (nsIDocumentLoader* loader, nsIChannel* channel, nsresult aStatus)
{
  return NS_OK;
}

NS_IMETHODIMP
nsWalletlibService::GetPassword(PRUnichar **password)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWalletlibService::HaveData(nsIPrompt* dialog, const char *key, const PRUnichar *userName, PRBool *_retval)
{
  return ::SINGSIGN_HaveData(dialog, key, userName, _retval);
}

NS_IMETHODIMP
nsWalletlibService::WALLET_Encrypt (const PRUnichar *text, char **crypt) {
  nsAutoString textAutoString( text );
  nsAutoString cryptAutoString;
  PRBool rv = ::Wallet_Encrypt(textAutoString, cryptAutoString);
  *crypt = cryptAutoString.ToNewCString();
  return rv;
}

NS_IMETHODIMP
nsWalletlibService::WALLET_Decrypt (const char *crypt, PRUnichar **text) {
  nsAutoString cryptAutoString; cryptAutoString.AssignWithConversion(crypt);
  nsAutoString textAutoString;
  PRBool rv = ::Wallet_Decrypt(cryptAutoString, textAutoString);
  *text = textAutoString.ToNewUnicode();
  return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsSingleSignOnPrompt

NS_IMPL_THREADSAFE_ISUPPORTS2(nsSingleSignOnPrompt, nsISingleSignOnPrompt, nsIPrompt)

NS_IMETHODIMP
nsSingleSignOnPrompt::Alert(const PRUnichar *dialogTitle, const PRUnichar *text)
{
  return mPrompt->Alert(dialogTitle, text);
}

NS_IMETHODIMP
nsSingleSignOnPrompt::Confirm(const PRUnichar *dialogTitle, const PRUnichar *text, PRBool *_retval)
{
  return mPrompt->Confirm(dialogTitle, text, _retval);
}

NS_IMETHODIMP
nsSingleSignOnPrompt::ConfirmCheck(const PRUnichar *dialogTitle, const PRUnichar *text, 
                                   const PRUnichar *checkMsg, PRBool *checkValue, PRBool *_retval)
{
  return mPrompt->ConfirmCheck(dialogTitle, text, checkMsg, checkValue, _retval);
}

NS_IMETHODIMP
nsSingleSignOnPrompt::Prompt(const PRUnichar *dialogTitle, const PRUnichar *text, 
                             const PRUnichar *passwordRealm, PRUint32 savePassword,
                             const PRUnichar *defaultText, PRUnichar **result, PRBool *_retval)
{
  nsresult rv;
  nsCAutoString realm;
  realm.AssignWithConversion(passwordRealm);     // XXX should be PRUnichar*
  rv = SINGSIGN_Prompt(dialogTitle, text, defaultText, result, realm.GetBuffer(), mPrompt, _retval, savePassword);
  return rv;
}

NS_IMETHODIMP
nsSingleSignOnPrompt::PromptUsernameAndPassword(const PRUnichar *dialogTitle, const PRUnichar *text, 
                                                const PRUnichar *passwordRealm, PRUint32 savePassword, 
                                                PRUnichar **user, PRUnichar **pwd, PRBool *_retval)
{
  nsresult rv;
  nsCAutoString realm;
  realm.AssignWithConversion(passwordRealm);     // XXX should be PRUnichar*
  rv = SINGSIGN_PromptUsernameAndPassword(dialogTitle, text, user, pwd,
                                          realm.GetBuffer(), mPrompt, _retval, savePassword);
  return rv;
}

NS_IMETHODIMP
nsSingleSignOnPrompt::PromptPassword(const PRUnichar *dialogTitle, const PRUnichar *text, 
                                     const PRUnichar *passwordRealm, PRUint32 savePassword, 
                                     PRUnichar **pwd, PRBool *_retval)
{
  nsresult rv;
  nsCAutoString realm;
  realm.AssignWithConversion(passwordRealm);     // XXX should be PRUnichar*
  rv = SINGSIGN_PromptPassword(dialogTitle, text, pwd,
                               realm.GetBuffer(), mPrompt, _retval, savePassword);
  return rv;
}

NS_IMETHODIMP
nsSingleSignOnPrompt::Select(const PRUnichar *dialogTitle, const PRUnichar *text, PRUint32 count,
                    const PRUnichar **selectList, PRInt32 *outSelection, PRBool *_retval)
{
  return mPrompt->Select(dialogTitle, text, count, selectList, outSelection, _retval);
}

NS_IMETHODIMP
nsSingleSignOnPrompt::UniversalDialog(const PRUnichar *titleMessage, const PRUnichar *dialogTitle, 
                             const PRUnichar *text, const PRUnichar *checkboxMsg, const PRUnichar *button0Text,
                             const PRUnichar *button1Text, const PRUnichar *button2Text, 
                             const PRUnichar *button3Text, const PRUnichar *editfield1Msg,
                             const PRUnichar *editfield2Msg, PRUnichar **editfield1Value,
                             PRUnichar **editfield2Value, const PRUnichar *iconURL,
                             PRBool *checkboxState, PRInt32 numberButtons, PRInt32 numberEditfields,
                             PRInt32 editField1Password, PRInt32 *buttonPressed)
{
  return mPrompt->UniversalDialog(titleMessage, dialogTitle, text, checkboxMsg, button0Text, button1Text,
                                  button2Text, button3Text, editfield1Msg, editfield2Msg, editfield1Value,
                                  editfield2Value, iconURL, checkboxState, numberButtons, numberEditfields,
                                  editField1Password, buttonPressed);
}
  
// nsISingleSignOnPrompt methods:

NS_IMETHODIMP
nsSingleSignOnPrompt::Init(nsIPrompt* dialogs)
{
  mPrompt = dialogs;
  return NS_OK;
}

NS_METHOD
nsSingleSignOnPrompt::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  if (aOuter)
    return NS_ERROR_NO_AGGREGATION;

  nsSingleSignOnPrompt* prompt = new nsSingleSignOnPrompt();
  if (prompt == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(prompt);
  nsresult rv = prompt->QueryInterface(aIID, aResult);
  NS_RELEASE(prompt);
  return rv;
}

////////////////////////////////////////////////////////////////////////////////
