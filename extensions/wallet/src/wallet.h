/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


/*
   wallet.h --- prototypes for wallet functions.
*/


#ifndef _WALLET_H
#define _WALLET_H

#include "ntypes.h"
#include "nsIPresShell.h"
#include "nsString.h"
#include "nsFileSpec.h"

XP_BEGIN_PROTOS

#define HEADER_VERSION "#2c"

#define YES_BUTTON 0
#define NO_BUTTON 1
#define NEVER_BUTTON 2

static const char *pref_Crypto = "wallet.crypto";

extern void
WLLT_ChangePassword();

extern void
WLLT_ReencryptAll();

extern void
WLLT_PreEdit(nsAutoString& walletList);

extern void
WLLT_PostEdit(nsAutoString walletList);

extern void
WLLT_PrefillReturn(nsAutoString results);

extern void
WLLT_RequestToCapture(nsIPresShell* shell);

extern nsresult
WLLT_Prefill(nsIPresShell* shell, PRBool quick);

extern void
WLLT_GetNopreviewListForViewer (nsAutoString& aNopreviewList);

extern void
WLLT_GetNocaptureListForViewer (nsAutoString& aNocaptureList);

extern void
WLLT_GetPrefillListForViewer (nsAutoString& aPrefillList);

extern void
WLLT_OnSubmit (nsIContent* formNode);

extern void
WLLT_FetchFromNetCenter();

extern void
WLLT_ExpirePassword();

extern void
WLLT_InitReencryptCallback();

extern nsresult
Wallet_Encrypt (nsAutoString text, nsAutoString& crypt);

extern nsresult
Wallet_Decrypt (nsAutoString crypt, nsAutoString& text);

extern nsresult Wallet_ProfileDirectory(nsFileSpec& dirSpec);

extern PRUnichar * Wallet_Localize(char * genericString);

extern char* Wallet_RandomName(char* suffix);

extern PRBool Wallet_ConfirmYN(PRUnichar * szMessage);

extern PRInt32 Wallet_3ButtonConfirm(PRUnichar * szMessage);

extern nsresult
Wallet_Encrypt2 (nsAutoString text, nsAutoString& crypt);

extern nsresult
Wallet_Decrypt2 (nsAutoString crypt, nsAutoString& text);

extern void
Wallet_UTF8Put(nsOutputFileStream strm, PRUnichar c);

extern PRUnichar
Wallet_UTF8Get(nsInputFileStream strm);

extern void
Wallet_SignonViewerReturn (nsAutoString results);

XP_END_PROTOS

#endif /* !_WALLET_H */
