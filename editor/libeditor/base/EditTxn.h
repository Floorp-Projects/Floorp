/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#ifndef EditTxn_h__
#define EditTxn_h__

#include "nsITransaction.h"
#include "nsIOutputStream.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsPIEditorTransaction.h"

#define EDIT_TXN_CID \
{/* c5ea31b0-ac48-11d2-86d8-000064657374 */ \
0xc5ea31b0, 0xac48, 0x11d2, \
{0x86, 0xd8, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74} }

/**
 * base class for all document editing transactions.
 * provides default concrete behavior for all nsITransaction methods.
 * EditTxns optionally have a name.  This name is for internal purposes only, 
 * it is never seen by the user or by any external entity.
 */
class EditTxn : public nsITransaction
              , public nsPIEditorTransaction
{
public:

  static const nsIID& GetCID() { static nsIID iid = EDIT_TXN_CID; return iid; }

  NS_DECL_ISUPPORTS

  EditTxn();
  virtual ~EditTxn();


  NS_IMETHOD DoTransaction(void);

  NS_IMETHOD UndoTransaction(void);

  NS_IMETHOD RedoTransaction(void);

  NS_IMETHOD GetIsTransient(PRBool *aIsTransient);

  NS_IMETHOD Merge(nsITransaction *aTransaction, PRBool *aDidMerge);

  NS_IMETHOD GetTxnDescription(nsAWritableString& aTxnDescription);
};

#endif
