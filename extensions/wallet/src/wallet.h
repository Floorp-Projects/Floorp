/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


/*
   wallet.h --- prototypes for wallet functions.
*/


#ifndef _WALLET_H
#define _WALLET_H

#include "ntypes.h"
#include "nsIPresShell.h"
#include "nsString.h"
#include "nsIURL.h"

XP_BEGIN_PROTOS

extern void
WLLT_ChangePassword();

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
WLLT_GetNopreviewListForViewer (nsString& aNopreviewList);

extern void
WLLT_GetNocaptureListForViewer (nsString& aNocaptureList);

extern void
WLLT_GetPrefillListForViewer (nsString& aPrefillList);

extern void
WLLT_OnSubmit (nsIContent* formNode);

extern void
WLLT_FetchFromNetCenter();

XP_END_PROTOS

#endif /* !_WALLET_H */
