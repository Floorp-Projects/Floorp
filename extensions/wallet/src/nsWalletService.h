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

class nsWalletlibService : public nsIWalletService,
                           public nsIObserver,
                           public nsIFormSubmitObserver {

public:
    NS_DECL_ISUPPORTS
    nsWalletlibService();

    /* Implementation of the nsIWalletService interface */
    NS_IMETHOD WALLET_ChangePassword();
    NS_IMETHOD WALLET_PreEdit(nsAutoString& walletList);
    NS_IMETHOD WALLET_PostEdit(nsAutoString walletList);
    NS_IMETHOD WALLET_Prefill(nsIPresShell* shell, nsString url, PRBool quick);
    NS_IMETHOD WALLET_Capture
      (nsIDocument* doc, nsString name, nsString value, nsString vcard);
    NS_IMETHOD WALLET_OKToCapture(PRBool* result, PRInt32 count, char* URLName);
    NS_IMETHOD WALLET_PrefillReturn(nsAutoString results);

    NS_IMETHOD SI_DisplaySignonInfoAsHTML();
    NS_IMETHOD SI_SignonViewerReturn(nsAutoString results);
    NS_IMETHOD SI_GetSignonListForViewer(nsString& aSignonList);
    NS_IMETHOD SI_GetRejectListForViewer(nsString& aRejectList);
    NS_IMETHOD WALLET_GetNopreviewListForViewer(nsString& aNopreviewList);
    NS_IMETHOD WALLET_GetNocaptureListForViewer(nsString& aNocaptureList);
    NS_IMETHOD WALLET_GetPrefillListForViewer(nsString& aPrefillList);

    NS_IMETHOD SI_RememberSignonData
        (char* URLName, char** name_array, char** value_array, char** type_array, PRInt32 value_cnt);
    NS_IMETHOD SI_RestoreSignonData
        (char* URLNAME, char* name, char** value);
    NS_IMETHOD SI_PromptUsernameAndPassword
        (char *prompt, char **username, char **password, char *URLName, PRBool &status);
    NS_IMETHOD SI_PromptPassword
        (char *prompt, char **password, char *URLName, PRBool pickFirstUser);
    NS_IMETHOD SI_Prompt
        (char *prompt, char **username, char *URLName);

    // nsIObserver
    NS_DECL_IOBSERVER

    NS_IMETHOD Notify(nsIContent* formNode);


protected:
    virtual ~nsWalletlibService();

private:
    void    Init();
};


#endif /* nsWalletService_h___ */
