/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
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
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef InsertTextTxn_h__
#define InsertTextTxn_h__

#include "EditTxn.h"
#include "nsIDOMCharacterData.h"
#include "nsIEditor.h"
#include "nsCOMPtr.h"

#define INSERT_TEXT_TXN_CID \
{/* 93276f00-ab2c-11d2-8f4b-006008159b0c*/ \
0x93276f00, 0xab2c, 0x11d2, \
{0x8f, 0xb4, 0x0, 0x60, 0x8, 0x15, 0x9b, 0xc} }

/**
  * A transaction that inserts text into a content node. 
  */
class InsertTextTxn : public EditTxn
{
public:

  static const nsIID& GetCID() { static const nsIID iid = INSERT_TEXT_TXN_CID; return iid; }

  /** initialize the transaction
    * @param aElement the text content node
    * @param aOffset  the location in aElement to do the insertion
    * @param aString  the new text to insert
    * @param aPresShell used to get and set the selection
    */
  NS_IMETHOD Init(nsIDOMCharacterData *aElement,
                  PRUint32 aOffset,
                  const nsAString& aString,
                  nsIEditor *aEditor);

  InsertTextTxn();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(InsertTextTxn, EditTxn)
	
  NS_DECL_EDITTXN

  NS_IMETHOD Merge(nsITransaction *aTransaction, bool *aDidMerge);

  /** return the string data associated with this transaction */
  NS_IMETHOD GetData(nsString& aResult);

protected:

  /** return PR_TRUE if aOtherTxn immediately follows this txn */
  virtual bool IsSequentialInsert(InsertTextTxn *aOtherTxn);
  
  /** the text element to operate upon */
  nsCOMPtr<nsIDOMCharacterData> mElement;
  
  /** the offset into mElement where the insertion is to take place */
  PRUint32 mOffset;

  /** the text to insert into mElement at mOffset */
  nsString mStringToInsert;

  /** the editor, which we'll need to get the selection */
  nsIEditor *mEditor;   
};

#endif
