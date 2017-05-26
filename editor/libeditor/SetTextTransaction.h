/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SetTextTransaction_h
#define mozilla_SetTextTransaction_h

#include "mozilla/EditTransactionBase.h"  // base class
#include "nsCycleCollectionParticipant.h" // various macros
#include "nsID.h"                       // NS_DECLARE_STATIC_IID_ACCESSOR
#include "nsISupportsImpl.h"            // NS_DECL_ISUPPORTS_INHERITED
#include "nsString.h"                   // nsString members
#include "nscore.h"                     // NS_IMETHOD, nsAString

class nsITransaction;

#define NS_SETTEXTTXN_IID \
{ 0x568bac0b, 0xa42a, 0x4150, \
  { 0xbd, 0x90, 0x34, 0xd0, 0x2f, 0x32, 0x74, 0x2e } }

namespace mozilla {

class EditorBase;
class RangeUpdater;

namespace dom {
class Text;
} // namespace dom

/**
 * A transaction that inserts text into a content node.
 */
class SetTextTransaction final : public EditTransactionBase
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_SETTEXTTXN_IID)

  /**
   * @param aTextNode       The text content node.
   * @param aString         The new text to insert.
   * @param aEditorBase     Used to get and set the selection.
   * @param aRangeUpdater   The range updater
   */
  SetTextTransaction(dom::Text& aTextNode,
                     const nsAString& aString, EditorBase& aEditorBase,
                     RangeUpdater* aRangeUpdater);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(SetTextTransaction,
                                           EditTransactionBase)

  NS_DECL_EDITTRANSACTIONBASE

  NS_IMETHOD Merge(nsITransaction* aTransaction, bool* aDidMerge) override;

private:
  virtual ~SetTextTransaction();

  // The Text node to operate upon.
  RefPtr<dom::Text> mTextNode;

  // The text to insert into mTextNode at mOffset.
  nsString mStringToSet;

  // The previous text for undo
  nsString mPreviousData;

  // The editor, which we'll need to get the selection.
  RefPtr<EditorBase> mEditorBase;

  RangeUpdater* mRangeUpdater;
};

NS_DEFINE_STATIC_IID_ACCESSOR(SetTextTransaction, NS_SETTEXTTXN_IID)

} // namespace mozilla

#endif // #ifndef mozilla_SetTextTransaction_h
