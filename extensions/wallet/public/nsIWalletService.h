/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.	Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsIWalletService_h___
#define nsIWalletService_h___

//#include "nscore.h"
#include "nsISupports.h"
#include "nsString.h"
#include "nsIPresShell.h"
#include "nsIURL.h"

// {738CFD51-ABCF-11d2-AB4B-0080C787AD96}
#define NS_IWALLETSERVICE_IID	 \
{ 0x738cfd51, 0xabcf, 0x11d2, { 0xab, 0x4b, 0x0, 0x80, 0xc7, 0x87, 0xad, 0x96 } }


// {738CFD52-ABCF-11d2-AB4B-0080C787AD96}
#define NS_WALLETSERVICE_CID \
{ 0x738cfd52, 0xabcf, 0x11d2, { 0xab, 0x4b, 0x0, 0x80, 0xc7, 0x87, 0xad, 0x96 } }

#define NS_WALLETSERVICE_PROGID		"component://netscape/wallet"
#define NS_WALLETSERVICE_CLASSNAME	"Auto Form Fill and Wallet"

/**
 * The nsIWalletService interface provides an API to the wallet service.
 * This is a preliminary interface which <B>will</B> change over time!
 *
 */
struct nsIWalletService : public nsISupports
{
    NS_IMETHOD WALLET_PreEdit(nsIURL* url) = 0;
    NS_IMETHOD WALLET_Prefill(nsIPresShell* shell, PRBool quick) = 0;
    NS_IMETHOD WALLET_Capture(nsIDocument* doc, nsString name, nsString value) = 0;
    NS_IMETHOD WALLET_OKToCapture(PRBool* result, PRInt32 count, char* URLName) = 0;

    NS_IMETHOD SI_DisplaySignonInfoAsHTML()=0;
    NS_IMETHOD SI_RememberSignonData
        (char* URLName, char** name_array, char** value_array, char** type_array, PRInt32 value_cnt) = 0;
    NS_IMETHOD SI_RestoreSignonData
        (char* URLNAME, char* name, char** value)=0;

    NS_IMETHOD SI_PromptUsernameAndPassword
        (char *prompt, char **username, char **password, char *URLName, PRBool &status)=0;
    NS_IMETHOD SI_PromptPassword
        (char *prompt, char **password, char *URLName, PRBool pickFirstUser)=0;
    NS_IMETHOD SI_Prompt
        (char *prompt, char **username, char *URLName)=0;
};

#endif /* nsIWalletService_h___ */
