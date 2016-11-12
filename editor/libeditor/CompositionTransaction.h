/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CompositionTransaction_h
#define CompositionTransaction_h

#include "mozilla/EditTransactionBase.h"  // base class
#include "nsCycleCollectionParticipant.h" // various macros
#include "nsString.h"                     // mStringToInsert

#define NS_IMETEXTTXN_IID \
  { 0xb391355d, 0x346c, 0x43d1, \
    { 0x85, 0xed, 0x9e, 0x65, 0xbe, 0xe7, 0x7e, 0x48 } }

namespace mozilla {

class EditorBase;
class RangeUpdater;
class TextRangeArray;

namespace dom {
class Text;
} // namespace dom

/**
 * CompositionTransaction stores all edit for a composition, i.e.,
 * from compositionstart event to compositionend event.  E.g., inserting a
 * composition string, modifying the composition string or its IME selection
 * ranges and commit or cancel the composition.
 */
class CompositionTransaction final : public EditTransactionBase
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IMETEXTTXN_IID)

  /**
   * @param aTextNode           The start node of text content.
   * @param aOffset             The location in aTextNode to do the insertion.
   * @param aReplaceLength      The length of text to replace. 0 means not
   *                            replacing existing text.
   * @param aTextRangeArray     Clauses and/or caret information. This may be
   *                            null.
   * @param aString             The new text to insert.
   * @param aEditorBase         Used to get and set the selection.
   * @param aRangeUpdater       The range updater
   */
  CompositionTransaction(dom::Text& aTextNode,
                         uint32_t aOffset, uint32_t aReplaceLength,
                         TextRangeArray* aTextRangeArray,
                         const nsAString& aString,
                         EditorBase& aEditorBase,
                         RangeUpdater* aRangeUpdater);

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(CompositionTransaction,
                                           EditTransactionBase)

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_EDITTRANSACTIONBASE

  NS_IMETHOD Merge(nsITransaction* aTransaction, bool* aDidMerge) override;

  void MarkFixed();

  static nsresult SetIMESelection(EditorBase& aEditorBase,
                                  dom::Text* aTextNode,
                                  uint32_t aOffsetInNode,
                                  uint32_t aLengthOfCompositionString,
                                  const TextRangeArray* aRanges);

private:
  ~CompositionTransaction();

  nsresult SetSelectionForRanges();

  // The text element to operate upon.
  RefPtr<dom::Text> mTextNode;

  // The offsets into mTextNode where the insertion should be placed.
  uint32_t mOffset;

  uint32_t mReplaceLength;

  // The range list.
  RefPtr<TextRangeArray> mRanges;

  // The text to insert into mTextNode at mOffset.
  nsString mStringToInsert;

  // The editor, which is used to get the selection controller.
  EditorBase& mEditorBase;

  RangeUpdater* mRangeUpdater;

  bool mFixed;
};

NS_DEFINE_STATIC_IID_ACCESSOR(CompositionTransaction, NS_IMETEXTTXN_IID)

} // namespace mozilla

#endif // #ifndef CompositionTransaction_h
