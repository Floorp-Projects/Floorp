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

#ifndef SetDocTitleTxn_h__
#define SetDocTitleTxn_h__

#include "EditTxn.h"
#include "nsIEditor.h"
#include "nsIHTMLEditor.h"
#include "nsITransaction.h"
#include "nsCOMPtr.h"

#define SET_DOC_TITLE_TXN_CID \
{ /*7FC508B5-ED8F-11d4-AF02-0050040AE132 */ \
0x7fc508b5, 0xed8f, 0x11d4, \
{ 0xaf, 0x2, 0x0, 0x50, 0x4, 0xa, 0xe1, 0x32 } }

/**
 * A transaction that changes the document's title,
 *  which is a text node under the <title> tag in a page's <head> section
 * provides default concrete behavior for all nsITransaction methods.
 */
class SetDocTitleTxn : public EditTxn
{
public:

  static const nsIID& GetCID() { static nsIID iid = SET_DOC_TITLE_TXN_CID; return iid; }

  virtual ~SetDocTitleTxn();


  /** Initialize the transaction.
    * @param aEditor the object providing core editing operations
    * @param aValue  the new value for document title
    */
  NS_IMETHOD Init(nsIHTMLEditor  *aEditor,
                  const nsAReadableString *aValue);
private:
  SetDocTitleTxn();
  nsresult SetDocTitle(const nsAReadableString& aTitle);
  nsresult SetDomTitle(const nsAReadableString& aTitle);

public:
  NS_IMETHOD DoTransaction(void);

  NS_IMETHOD UndoTransaction(void);

  NS_IMETHOD RedoTransaction(void);

  NS_IMETHOD Merge(nsITransaction *aTransaction, PRBool *aDidMerge);

  NS_IMETHOD GetIsTransient(PRBool *aIsTransient);

  NS_IMETHOD GetTxnDescription(nsAWritableString& aTxnDescription);

protected:

  /** the editor that created this transaction */
  nsIHTMLEditor*  mEditor;
  
  /** The new title string */
  nsString    mValue;

  /** The previous title string to use for undo */
  nsString    mUndoValue;

  /** Set true if we dont' really change the title during Do() */
  PRPackedBool mIsTransient;

  friend class TransactionFactory;
};

#endif
