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

#ifndef nsWalletService_h___
#define nsWalletService_h___

#include "nsIWalletService.h"
#include "nsIObserver.h"
#include "nsIFormSubmitObserver.h"
#include "nsIDocumentLoaderObserver.h"

class nsWalletlibService : public nsIWalletService,
                           public nsIObserver,
                           public nsIFormSubmitObserver,
                           public nsIDocumentLoaderObserver {

public:
  NS_DECL_ISUPPORTS
  nsWalletlibService();

  /* Implementation of the nsIWalletService interface */
  NS_IMETHOD WALLET_PreEdit(nsAutoString& walletList);
  NS_IMETHOD WALLET_PostEdit(nsAutoString walletList);
  NS_IMETHOD WALLET_ChangePassword();
  NS_IMETHOD WALLET_Prefill(nsIPresShell* shell, nsString url, PRBool quick);
  NS_IMETHOD WALLET_PrefillReturn(nsAutoString results);
  NS_IMETHOD WALLET_FetchFromNetCenter();

  NS_IMETHOD SI_PromptUsernameAndPassword
      (char *prompt, char **username, char **password, char *URLName, PRBool &status);
  NS_IMETHOD SI_PromptPassword
      (char *prompt, char **password, char *URLName, PRBool pickFirstUser);
  NS_IMETHOD SI_Prompt
      (char *prompt, char **username, char *URLName);

  NS_IMETHOD WALLET_GetNopreviewListForViewer(nsString& aNopreviewList);
  NS_IMETHOD WALLET_GetNocaptureListForViewer(nsString& aNocaptureList);
  NS_IMETHOD WALLET_GetPrefillListForViewer(nsString& aPrefillList);
  NS_IMETHOD SI_GetSignonListForViewer(nsString& aSignonList);
  NS_IMETHOD SI_GetRejectListForViewer(nsString& aRejectList);
  NS_IMETHOD SI_SignonViewerReturn(nsAutoString results);

  // nsIObserver
  NS_DECL_NSIOBSERVER
  NS_IMETHOD Notify(nsIContent* formNode);

  // nsIDocumentLoaderObserver
#ifdef NECKO
  NS_IMETHOD OnStartDocumentLoad
    (nsIDocumentLoader* loader, nsIURI* aURL, const char* aCommand);
  NS_IMETHOD OnEndDocumentLoad
    (nsIDocumentLoader* loader, nsIChannel* channel, nsresult aStatus,
     nsIDocumentLoaderObserver* aObserver);
  NS_IMETHOD OnStartURLLoad
    (nsIDocumentLoader* loader, nsIChannel* channel, nsIContentViewer* aViewer);
  NS_IMETHOD OnProgressURLLoad
    (nsIDocumentLoader* loader, nsIChannel* channel, PRUint32 aProgress,
     PRUint32 aProgressMax);
  NS_IMETHOD OnStatusURLLoad
    (nsIDocumentLoader* loader, nsIChannel* channel, nsString& aMsg);
  NS_IMETHOD OnEndURLLoad
    (nsIDocumentLoader* loader, nsIChannel* channel, nsresult aStatus);
  NS_IMETHOD HandleUnknownContentType
    (nsIDocumentLoader* loader, nsIChannel* channel, const char *aContentType,
     const char *aCommand );		
#else
  NS_IMETHOD OnStartDocumentLoad
    (nsIDocumentLoader* loader, nsIURI* aURL, const char* aCommand);
  NS_IMETHOD OnEndDocumentLoad
    (nsIDocumentLoader* loader, nsIURI *aUrl, PRInt32 aStatus,
     nsIDocumentLoaderObserver * aObserver);
  NS_IMETHOD OnStartURLLoad
    (nsIDocumentLoader* loader, nsIURI* aURL, const char* aContentType,
     nsIContentViewer* aViewer);
  NS_IMETHOD OnProgressURLLoad
    (nsIDocumentLoader* loader, nsIURI* aURL, PRUint32 aProgress, PRUint32 aProgressMax);
  NS_IMETHOD OnStatusURLLoad(nsIDocumentLoader* loader, nsIURI* aURL, nsString& aMsg);
  NS_IMETHOD OnEndURLLoad(nsIDocumentLoader* loader, nsIURI* aURL, PRInt32 aStatus);
  NS_IMETHOD HandleUnknownContentType(nsIDocumentLoader* loader,
                                        nsIURI *aURL,
                                        const char *aContentType,
                                        const char *aCommand );
#endif

protected:
  virtual ~nsWalletlibService();

private:
  void    Init();
};


#endif /* nsWalletService_h___ */
