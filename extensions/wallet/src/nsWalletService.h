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
 */

#ifndef nsWalletService_h___
#define nsWalletService_h___

#include "nsIWalletService.h"
#include "nsIObserver.h"
#include "nsIFormSubmitObserver.h"
#include "nsIDocumentLoaderObserver.h"
#include "nsWeakReference.h"
#include "nsIPasswordSink.h"
#include "nsIDOMWindow.h"
#include "nsIURI.h"

class nsWalletlibService : public nsIWalletService,
                           public nsIFormSubmitObserver,
                           public nsIDocumentLoaderObserver,
                           public nsIPasswordSink,
                           public nsSupportsWeakReference {

public:
  NS_DECL_ISUPPORTS
  nsWalletlibService();

  /* Implementation of the nsIWalletService interface */
 
  NS_IMETHOD WALLET_PreEdit(nsAutoString& walletList);
  NS_IMETHOD WALLET_PostEdit(nsAutoString walletList);
  NS_IMETHOD WALLET_ChangePassword();
  NS_IMETHOD WALLET_RequestToCapture(nsIPresShell* shell);
  NS_IMETHOD WALLET_Prefill(nsIPresShell* shell, PRBool quick);
  NS_IMETHOD WALLET_PrefillReturn(nsAutoString results);
  NS_IMETHOD WALLET_FetchFromNetCenter();
  NS_IMETHOD WALLET_ExpirePassword();

  NS_IMETHOD PromptUsernameAndPasswordURL
    (const PRUnichar *text, PRUnichar **user, PRUnichar **pwd,
     const char *urlname, PRBool stripUrl, nsIPrompt* dialog, PRBool *_retval);
  NS_IMETHOD PromptPasswordURL
    (const PRUnichar *text, PRUnichar **pwd, const char *urlname, PRBool stripUrl, nsIPrompt* dialog, PRBool *_retval);
  NS_IMETHOD PromptURL
    (const PRUnichar *text, const PRUnichar *defaultText, PRUnichar **result,
     const char *urlname, PRBool stripUrl, nsIPrompt* dialog, PRBool *_retval);

  NS_IMETHOD SI_RemoveUser(const char *URLName, PRBool stripUrl, const PRUnichar *userName);
  NS_IMETHOD SI_StorePassword(const char *URLName, PRBool stripUrl, const PRUnichar *userName, const PRUnichar *password);

  NS_IMETHOD HaveData(const char *url, const PRUnichar *userName, PRBool stripUrl, PRBool *_retval);

  NS_IMETHOD WALLET_GetNopreviewListForViewer(nsAutoString& aNopreviewList);
  NS_IMETHOD WALLET_GetNocaptureListForViewer(nsAutoString& aNocaptureList);
  NS_IMETHOD WALLET_GetPrefillListForViewer(nsAutoString& aPrefillList);
  NS_IMETHOD SI_GetSignonListForViewer(nsAutoString& aSignonList);
  NS_IMETHOD SI_GetRejectListForViewer(nsAutoString& aRejectList);
  NS_IMETHOD SI_SignonViewerReturn(nsAutoString results);

  // nsIObserver
  NS_DECL_NSIOBSERVER
  NS_IMETHOD Notify(nsIContent* formNode, nsIDOMWindow* window, nsIURI* actionURL);

  // nsIDocumentLoaderObserver
  NS_DECL_NSIDOCUMENTLOADEROBSERVER

  // nsIPasswordSink
  NS_DECL_NSIPASSWORDSINK

protected:
  virtual ~nsWalletlibService();

private:
  void    Init();
};


#endif /* nsWalletService_h___ */
