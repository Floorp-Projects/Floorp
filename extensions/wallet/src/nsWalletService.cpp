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
#include "nsIWebShell.h"
#include "nsIDocumentViewer.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIFormControl.h"

static NS_DEFINE_IID(kIWalletServiceIID, NS_IWALLETSERVICE_IID);
static NS_DEFINE_IID(kIDOMHTMLFormElementIID, NS_IDOMHTMLFORMELEMENT_IID);
static NS_DEFINE_IID(kIFormSubmitObserverIID, NS_IFORMSUBMITOBSERVER_IID);
static NS_DEFINE_IID(kIDOMHTMLDocumentIID, NS_IDOMHTMLDOCUMENT_IID);
static NS_DEFINE_IID(kDocLoaderServiceCID, NS_DOCUMENTLOADER_SERVICE_CID);
static NS_DEFINE_IID(kIDocumentLoaderIID, NS_IDOCUMENTLOADER_IID);


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

NS_IMETHODIMP
nsWalletlibService::QueryInterface(REFNSIID iid, void** result)
{
  if (! result) {
    return NS_ERROR_NULL_POINTER;
  }
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
  if (iid.Equals(nsIDocumentLoaderObserver::GetIID())) {
    *result = (void*) ((nsIDocumentLoaderObserver*)this);
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsWalletlibService);
NS_IMPL_RELEASE(nsWalletlibService);

NS_IMETHODIMP nsWalletlibService::WALLET_PreEdit(nsAutoString& walletList) {
  ::WLLT_PreEdit(walletList);
  return NS_OK;
}

NS_IMETHODIMP nsWalletlibService::WALLET_PostEdit(nsAutoString walletList) {
  ::WLLT_PostEdit(walletList);
  return NS_OK;
}

NS_IMETHODIMP nsWalletlibService::WALLET_ChangePassword() {
    ::WLLT_ChangePassword();
    return NS_OK;
}

NS_IMETHODIMP nsWalletlibService::WALLET_Prefill(nsIPresShell* shell, nsString url, PRBool quick) {
  return ::WLLT_Prefill(shell, url, quick);
}

NS_IMETHODIMP nsWalletlibService::WALLET_PrefillReturn(nsAutoString results){
  ::WLLT_PrefillReturn(results);
  return NS_OK;
}

NS_IMETHODIMP nsWalletlibService::WALLET_FetchFromNetCenter(){
  ::WLLT_FetchFromNetCenter();
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

NS_IMETHODIMP nsWalletlibService::SI_GetSignonListForViewer(nsString& aSignonList){
  ::SINGSIGN_GetSignonListForViewer(aSignonList);
  return NS_OK;
}

NS_IMETHODIMP nsWalletlibService::SI_GetRejectListForViewer(nsString& aRejectList){
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
NS_IMETHODIMP nsWalletlibService::Notify(nsIContent* formNode) 
{
  if (!formNode) {
    return NS_ERROR_FAILURE;
  }
  ::WLLT_OnSubmit(formNode);
  return NS_OK;
}

void nsWalletlibService::Init() 
{
  nsIObserverService *svc = 0;
  nsIDocumentLoader *docLoaderService;

  nsresult rv = nsServiceManager::GetService
    (NS_OBSERVERSERVICE_PROGID, nsIObserverService::GetIID(), (nsISupports**)&svc );
  if ( NS_SUCCEEDED( rv ) && svc ) {
    nsString  topic(NS_FORMSUBMIT_SUBJECT);
    rv = svc->AddObserver( this, topic.GetUnicode());
    nsServiceManager::ReleaseService( NS_OBSERVERSERVICE_PROGID, svc );
  }

  // Get the global document loader service...  
  rv = nsServiceManager::GetService
    (kDocLoaderServiceCID, kIDocumentLoaderIID, (nsISupports **)&docLoaderService);
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
#ifdef NECKO
nsWalletlibService::OnEndDocumentLoad(nsIDocumentLoader* aLoader, nsIChannel* channel, nsresult aStatus,
									nsIDocumentLoaderObserver * aObserver)
#else
nsWalletlibService::OnEndDocumentLoad(nsIDocumentLoader* aLoader, nsIURI *aUrl, PRInt32 aStatus,
									nsIDocumentLoaderObserver * aObserver)
#endif
{
  nsresult rv = NS_OK;
  nsIContentViewerContainer *cont = nsnull;
  nsIWebShell *ws = nsnull;

  if (aLoader == nsnull) {
    return rv;
  }
  rv = aLoader->GetContainer(&cont);
  if (NS_FAILED(rv) || (cont == nsnull)) {
    return rv;
  }
  rv = cont->QueryInterface(nsIWebShell::GetIID(), (void **)&ws);
  if (NS_FAILED(rv) || (ws == nsnull)) {
    NS_RELEASE(cont);
    return rv;
  }
  nsIContentViewer* cv = nsnull;
  rv = ws->GetContentViewer(&cv);
  if (NS_FAILED(rv) || (cv == nsnull)) {
    NS_RELEASE(ws);
    NS_RELEASE(cont);
    return rv;
  }
  nsIDocumentViewer* docViewer = nsnull;
  rv = cv->QueryInterface(nsIDocumentViewer::GetIID(), (void**) &docViewer);
  if (NS_FAILED(rv) || (docViewer == nsnull)) {
    NS_RELEASE(cv);
    NS_RELEASE(ws);
    NS_RELEASE(cont);
    return rv;
  }
  nsCOMPtr<nsIDocument> doc;
  rv = docViewer->GetDocument(*getter_AddRefs(doc));
  if (NS_FAILED(rv) || (doc == nsnull)) {
    NS_RELEASE(docViewer);
    NS_RELEASE(cv);
    NS_RELEASE(ws);
    NS_RELEASE(cont);
    return rv;
  }
  
  /* get url name as ascii string */
  char *URLName = nsnull;
  nsIURI* docURL = nsnull;
#ifdef NECKO
  char* spec;
#else
  const char* spec;
#endif
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
#ifdef NECKO
  nsCRT::free(spec);
#endif

  nsIDOMHTMLDocument *htmldoc = nsnull;
  rv = doc->QueryInterface(kIDOMHTMLDocumentIID, (void**)&htmldoc);
  if (NS_FAILED(rv) || (htmldoc == nsnull)) {
    NS_RELEASE(docViewer);
    NS_RELEASE(cv);
    NS_RELEASE(ws);
    NS_RELEASE(cont);
    return rv;
  }

  nsIDOMHTMLCollection* forms = nsnull;
  rv = htmldoc->GetForms(&forms);
  if (NS_FAILED(rv) || (forms == nsnull)) {
    NS_RELEASE(htmldoc);
    NS_RELEASE(docViewer);
    NS_RELEASE(cv);
    NS_RELEASE(ws);
    NS_RELEASE(cont);
    return rv;
  }

  PRUint32 numForms;
  forms->GetLength(&numForms);
  for (PRUint32 formX = 0; formX < numForms; formX++) {
    nsIDOMNode* formNode = nsnull;
    forms->Item(formX, &formNode);
    if (nsnull != formNode) {
      nsIDOMHTMLFormElement* formElement = nsnull;
      rv = formNode->QueryInterface(kIDOMHTMLFormElementIID, (void**)&formElement);
      if ((NS_SUCCEEDED(rv)) && (nsnull != formElement)) {
        nsIDOMHTMLCollection* elements = nsnull;
        rv = formElement->GetElements(&elements);
        if ((NS_SUCCEEDED(rv)) && (nsnull != elements)) {
          /* got to the form elements at long last */ 
          PRUint32 numElements;
          elements->GetLength(&numElements);
          for (PRUint32 elementX = 0; elementX < numElements; elementX++) {
            nsIDOMNode* elementNode = nsnull;
            elements->Item(elementX, &elementNode);
            if (nsnull != elementNode) {
              nsIDOMHTMLInputElement *inputElement = nsnull;
              rv = elementNode->QueryInterface
                        (nsIDOMHTMLInputElement::GetIID(), (void**)&inputElement);
              if ((NS_SUCCEEDED(rv)) && (nsnull != inputElement)) {
                nsAutoString type("");
                rv = inputElement->GetType(type);
                if (NS_SUCCEEDED(rv)) {
                  if ((type == "") || (type.Compare("text", PR_TRUE) == 0) ||
                    (type.Compare("password", PR_TRUE) == 0)) {
                    nsAutoString field;
                    rv = inputElement->GetName(field);
                    if (NS_SUCCEEDED(rv)) {
                      char* nameString = field.ToNewCString();
                      if (nameString) {
                        char* valueString = NULL;
                        SINGSIGN_RestoreSignonData(URLName, nameString, &valueString);
                        nsAutoString value(valueString);                                    
                        rv = inputElement->SetValue(value);
                        delete[] nameString;
                      }
                    }
                  }
                }
                NS_RELEASE(inputElement);
              }
              NS_RELEASE(elementNode);
            }
          }
          NS_RELEASE(elements);
        }
        NS_RELEASE(formElement);
      }
      NS_RELEASE(formNode);
    }
  }

  NS_RELEASE(forms);
  NS_RELEASE(htmldoc);
  NS_RELEASE(docViewer);
  NS_RELEASE(cv);
  NS_RELEASE(ws);
  NS_RELEASE(cont);
  return rv;
}

NS_IMETHODIMP
#ifdef NECKO
nsWalletlibService::OnStartURLLoad
  (nsIDocumentLoader* loader, nsIChannel* channel, nsIContentViewer* aViewer)
#else
nsWalletlibService::OnStartURLLoad
  (nsIDocumentLoader* loader, nsIURI* aURL, const char* aContentType, nsIContentViewer* aViewer)
#endif
{
 return NS_OK;
}

NS_IMETHODIMP
#ifdef NECKO
nsWalletlibService::OnProgressURLLoad
  (nsIDocumentLoader* loader, nsIChannel* channel, PRUint32 aProgress, PRUint32 aProgressMax)
#else
nsWalletlibService::OnProgressURLLoad
  (nsIDocumentLoader* loader, nsIURI* aURL, PRUint32 aProgress, PRUint32 aProgressMax)
#endif
{
  return NS_OK;
}

NS_IMETHODIMP
#ifdef NECKO
nsWalletlibService::OnStatusURLLoad
  (nsIDocumentLoader* loader, nsIChannel* channel, nsString& aMsg)
#else
nsWalletlibService::OnStatusURLLoad(nsIDocumentLoader* loader, nsIURI* aURL, nsString& aMsg)
#endif
{
  return NS_OK;
}


NS_IMETHODIMP
#ifdef NECKO
nsWalletlibService::OnEndURLLoad
  (nsIDocumentLoader* loader, nsIChannel* channel, nsresult aStatus)
#else
nsWalletlibService::OnEndURLLoad
  (nsIDocumentLoader* loader, nsIURI* aURL, PRInt32 aStatus)
#endif
{
  return NS_OK;
}

NS_IMETHODIMP
#ifdef NECKO
nsWalletlibService::HandleUnknownContentType
  (nsIDocumentLoader* loader, nsIChannel* channel, const char *aContentType, const char *aCommand )
#else
nsWalletlibService::HandleUnknownContentType
  (nsIDocumentLoader* loader, nsIURI *aURL, const char *aContentType, const char *aCommand )
#endif
{
  return NS_OK;
}

/* call to create the wallet object */

nsresult
NS_NewWalletService(nsIWalletService** result)
{
  nsIWalletService* wallet = new nsWalletlibService();
  if (! wallet) {
    return NS_ERROR_NULL_POINTER;
  }
  *result = wallet;
  NS_ADDREF(*result);
  return NS_OK;
}
