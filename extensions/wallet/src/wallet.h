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

#include "xp_core.h"
#include "nsIPresShell.h"
#include "nsString.h"
#include "nsFileSpec.h"
#include "nsIPrompt.h"

class nsIDOMWindowInternal;
class nsIDOMNode;

XP_BEGIN_PROTOS

#define HEADER_VERSION "#2c"

#define YES_BUTTON 0
#define NO_BUTTON 1
#define NEVER_BUTTON 2

static const char *pref_Crypto = "wallet.crypto";

extern void
WLLT_ChangePassword(PRBool* status);

extern void
WLLT_DeleteAll();

extern void
WLLT_ClearUserData();

extern void
WLLT_DeletePersistentUserData();

extern void
WLLT_PreEdit(nsString& walletList);

extern void
WLLT_PostEdit(const nsString& walletList);

extern void
WLLT_PrefillReturn(const nsString& results);

extern void
WLLT_RequestToCapture(nsIPresShell* shell, nsIDOMWindowInternal * win, PRUint32* status);

extern nsresult
WLLT_PrefillOneElement
  (nsIDOMWindowInternal* win, nsIDOMNode* elementNode, nsString& compositeValue);

extern nsresult
WLLT_Prefill(nsIPresShell* shell, PRBool quick, nsIDOMWindowInternal* win);

extern void
WLLT_GetNopreviewListForViewer(nsString& aNopreviewList);

extern void
WLLT_GetNocaptureListForViewer(nsString& aNocaptureList);

extern void
WLLT_GetPrefillListForViewer(nsString& aPrefillList);

extern void
WLLT_OnSubmit(nsIContent* formNode, nsIDOMWindowInternal* window);

extern void
WLLT_FetchFromNetCenter();

extern void
WLLT_ExpirePassword(PRBool* status);

extern void
WLLT_InitReencryptCallback(nsIDOMWindowInternal* window);

extern nsresult
Wallet_Encrypt(const nsString& text, nsString& crypt);

extern nsresult
Wallet_Decrypt(const nsString& crypt, nsString& text);

extern nsresult Wallet_ProfileDirectory(nsFileSpec& dirSpec);

extern PRUnichar * Wallet_Localize(char * genericString);

extern char* Wallet_RandomName(char* suffix);

extern PRBool Wallet_ConfirmYN(PRUnichar * szMessage, nsIDOMWindowInternal* window);

extern PRInt32 Wallet_3ButtonConfirm(PRUnichar * szMessage, nsIDOMWindowInternal* window);

extern void Wallet_GiveCaveat(nsIDOMWindowInternal* window, nsIPrompt* dialog);

extern nsresult
Wallet_Encrypt2(const nsString& text, nsString& crypt);

extern nsresult
Wallet_Decrypt2(const nsString& crypt, nsString& text);

extern void
Wallet_UTF8Put(nsOutputFileStream& strm, PRUnichar c);

extern PRUnichar
Wallet_UTF8Get(nsInputFileStream& strm);

extern void
Wallet_SignonViewerReturn(const nsString& results);

extern void
Wallet_ReleaseAllLists();

XP_END_PROTOS

#endif /* !_WALLET_H */
