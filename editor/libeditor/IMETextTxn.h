/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IMETextTxn_h__
#define IMETextTxn_h__

#include "EditTxn.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsID.h"
#include "nsIDOMCharacterData.h"
#include "nsString.h"
#include "nscore.h"
#include "mozilla/TextRange.h"

class nsITransaction;

// {D4D25721-2813-11d3-9EA3-0060089FE59B}
#define IME_TEXT_TXN_CID							\
{0xd4d25721, 0x2813, 0x11d3,						\
{0x9e, 0xa3, 0x0, 0x60, 0x8, 0x9f, 0xe5, 0x9b }}


class nsIEditor;


/**
  * A transaction that inserts text into a content node. 
  */
class IMETextTxn : public EditTxn
{
public:
  static const nsIID& GetCID() { static const nsIID iid = IME_TEXT_TXN_CID; return iid; }

  /** initialize the transaction
    * @param aElement the text content node
    * @param aOffset  the location in aElement to do the insertion
    * @param aReplaceLength the length of text to replace (0 == no replacement)
    * @param aTextRangeArray clauses and/or caret information. This may be null.
    * @param aString  the new text to insert
    * @param aSelCon used to get and set the selection
    */
  NS_IMETHOD Init(nsIDOMCharacterData *aElement,
                  uint32_t aOffset,
                  uint32_t aReplaceLength,
                  mozilla::TextRangeArray* aTextRangeArray,
                  const nsAString& aString,
                  nsIEditor* aEditor);

  IMETextTxn();

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(IMETextTxn, EditTxn)

  NS_DECL_EDITTXN

  NS_IMETHOD Merge(nsITransaction *aTransaction, bool *aDidMerge);

  NS_IMETHOD MarkFixed(void);

// nsISupports declarations

  // override QueryInterface to handle IMETextTxn request
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

protected:

  nsresult SetSelectionForRanges();

  /** the text element to operate upon */
  nsCOMPtr<nsIDOMCharacterData> mElement;
  
  /** the offsets into mElement where the insertion should be placed*/
  uint32_t mOffset;

  uint32_t mReplaceLength;

  /** the text to insert into mElement at mOffset */
  nsString mStringToInsert;

  /** the range list **/
  nsRefPtr<mozilla::TextRangeArray> mRanges;

  /** the editor, which is used to get the selection controller */
  nsIEditor *mEditor;

  bool	mFixed;
};

#endif
