/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsInsertHTMLTxn_h__
#define nsInsertHTMLTxn_h__

#include "EditTxn.h"
#include "nsIEditor.h"
#include "nsIDOMRange.h"
#include "nsCOMPtr.h"

#define NS_INSERT_HTML_TXN_CID \
{/* a6cf90fd-15b3-11d2-932e-00805f8add3 */ \
0xa6cf90fc, 0x15b3, 0x11d2, \
{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

/**
 * A transaction that inserts a string of html source
 */
class nsInsertHTMLTxn : public EditTxn
{
public:

  static const nsIID& GetCID() { static nsIID iid = NS_INSERT_HTML_TXN_CID; return iid; }

  /** initialize the transaction.
    * @param aSrc     the source for the HTML to insert
    * @param aEditor  the editor in which to do the work
    */
  NS_IMETHOD Init(const nsString& aSrc,
                  nsIEditor *aEditor);

private:
  nsInsertHTMLTxn();

public:

  virtual ~nsInsertHTMLTxn();

  NS_IMETHOD Do(void);

  NS_IMETHOD Undo(void);

  NS_IMETHOD Merge(PRBool *aDidMerge, nsITransaction *aTransaction);

  NS_IMETHOD Write(nsIOutputStream *aOutputStream);

  NS_IMETHOD GetUndoString(nsString *aString);

  NS_IMETHOD GetRedoString(nsString *aString);

protected:
  
  /** the html to insert */
  nsString mSrc;

  /** the range representing the inserted fragment */
  nsCOMPtr<nsIDOMRange> mRange;

  /** the editor for this transaction */
  nsCOMPtr<nsIEditor> mEditor;

  friend class TransactionFactory;

};

#endif
