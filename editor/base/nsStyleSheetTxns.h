/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsStylesheetTxns_h__
#define nsStylesheetTxns_h__

#include "EditTxn.h"
#include "nsCOMPtr.h"
#include "nsIEditor.h"
#include "nsICSSStyleSheet.h"

#define ADD_STYLESHEET_TXN_CID \
{/* d05e2980-2fbe-11d3-9ce4-e8393835307c */ \
0xd05e2980, 0x2fbe, 0x11d3, { 0x9c, 0xe4, 0xe8, 0x39, 0x38, 0x35, 0x30, 0x7c } }

#define REMOVE_STYLESHEET_TXN_CID \
{/* d05e2981-2fbe-11d3-9ce4-e8393835307c */ \
0xd05e2981, 0x2fbe, 0x11d3, { 0x9c, 0xe4, 0xe8, 0x39, 0x38, 0x35, 0x30, 0x7c } }


class AddStyleSheetTxn : public EditTxn
{
  friend class TransactionFactory;

public:

  static const nsIID& GetCID() { static nsIID iid = ADD_STYLESHEET_TXN_CID; return iid; }

  virtual ~AddStyleSheetTxn();

  /** Initialize the transaction.
    * @param aEditor the object providing core editing operations
    * @param aSheet   the stylesheet to add
    */
  NS_IMETHOD Init(nsIEditor         *aEditor,
                  nsICSSStyleSheet  *aSheet);

private:
  AddStyleSheetTxn();

public:

  NS_IMETHOD DoTransaction(void);

  NS_IMETHOD UndoTransaction(void);

  NS_IMETHOD RedoTransaction(void);

  NS_IMETHOD Merge(nsITransaction *aTransaction, PRBool *aDidMerge);

  NS_IMETHOD GetTxnDescription(nsAWritableString& aTxnDescription);

protected:

  nsIEditor*  mEditor;									// the editor that created this transaction
  nsCOMPtr<nsICSSStyleSheet>	mSheet;		// the style sheet to add
  
};


class RemoveStyleSheetTxn : public EditTxn
{
  friend class TransactionFactory;

public:

  static const nsIID& GetCID() { static nsIID iid = REMOVE_STYLESHEET_TXN_CID; return iid; }

	virtual ~RemoveStyleSheetTxn();
	
  /** Initialize the transaction.
    * @param aEditor the object providing core editing operations
    * @param aSheet   the stylesheet to remove
    */
  NS_IMETHOD Init(nsIEditor         *aEditor,
                  nsICSSStyleSheet  *aSheet);
	
private:
  RemoveStyleSheetTxn();

public:

  NS_IMETHOD DoTransaction(void);

  NS_IMETHOD UndoTransaction(void);

  NS_IMETHOD RedoTransaction(void);

  NS_IMETHOD Merge(nsITransaction *aTransaction, PRBool *aDidMerge);

  NS_IMETHOD GetTxnDescription(nsAWritableString& aTxnDescription);

protected:

  nsIEditor*  mEditor;									// the editor that created this transaction
  nsCOMPtr<nsICSSStyleSheet>	mSheet;		// the style sheet to remove
  
};


#endif /* nsStylesheetTxns_h__ */
