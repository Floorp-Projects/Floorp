/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IMETextTxn_h__
#define IMETextTxn_h__

#include "EditTxn.h"                      // base class
#include "nsCycleCollectionParticipant.h" // various macros
#include "nsString.h"                     // mStringToInsert

class nsEditor;

#define NS_IMETEXTTXN_IID \
  { 0xb391355d, 0x346c, 0x43d1, \
    { 0x85, 0xed, 0x9e, 0x65, 0xbe, 0xe7, 0x7e, 0x48 } }

namespace mozilla {

class TextRangeArray;

namespace dom {

class Text;

/**
  * A transaction that inserts text into a content node.
  */
class IMETextTxn : public EditTxn
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IMETEXTTXN_IID)

  /** @param aTextNode the text content node
    * @param aOffset  the location in aTextNode to do the insertion
    * @param aReplaceLength the length of text to replace (0 == no replacement)
    * @param aTextRangeArray clauses and/or caret information. This may be null.
    * @param aString the new text to insert
    * @param aEditor used to get and set the selection
    */
  IMETextTxn(Text& aTextNode, uint32_t aOffset, uint32_t aReplaceLength,
             TextRangeArray* aTextRangeArray, const nsAString& aString,
             nsEditor& aEditor);

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(IMETextTxn, EditTxn)

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_EDITTXN

  NS_IMETHOD Merge(nsITransaction* aTransaction, bool* aDidMerge) override;

  void MarkFixed();

  static nsresult SetIMESelection(nsEditor& aEditor,
                                  Text* aTextNode,
                                  uint32_t aOffsetInNode,
                                  uint32_t aLengthOfCompositionString,
                                  const TextRangeArray* aRanges);

private:
  ~IMETextTxn();

  nsresult SetSelectionForRanges();

  /** The text element to operate upon */
  RefPtr<Text> mTextNode;

  /** The offsets into mTextNode where the insertion should be placed */
  uint32_t mOffset;

  uint32_t mReplaceLength;

  /** The range list **/
  RefPtr<TextRangeArray> mRanges;

  /** The text to insert into mTextNode at mOffset */
  nsString mStringToInsert;

  /** The editor, which is used to get the selection controller */
  nsEditor& mEditor;

  bool mFixed;
};

NS_DEFINE_STATIC_IID_ACCESSOR(IMETextTxn, NS_IMETEXTTXN_IID)

} // namespace dom
} // namespace mozilla

#endif
