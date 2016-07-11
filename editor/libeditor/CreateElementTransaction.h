/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CreateElementTransaction_h
#define CreateElementTransaction_h

#include "mozilla/EditTransactionBase.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupportsImpl.h"

class nsIAtom;
class nsIContent;
class nsINode;

/**
 * A transaction that creates a new node in the content tree.
 */
namespace mozilla {

class EditorBase;
namespace dom {
class Element;
} // namespace dom

class CreateElementTransaction final : public EditTransactionBase
{
public:
  /**
   * Initialize the transaction.
   * @param aEditorBase     The provider of basic editing functionality.
   * @param aTag            The tag (P, HR, TABLE, etc.) for the new element.
   * @param aParent         The node into which the new element will be
   *                        inserted.
   * @param aOffsetInParent The location in aParent to insert the new element.
   *                        If eAppend, the new element is appended as the last
   *                        child.
   */
  CreateElementTransaction(EditorBase& aEditorBase,
                           nsIAtom& aTag,
                           nsINode& aParent,
                           int32_t aOffsetInParent);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(CreateElementTransaction,
                                           EditTransactionBase)

  NS_DECL_EDITTRANSACTIONBASE

  NS_IMETHOD RedoTransaction() override;

  already_AddRefed<dom::Element> GetNewNode();

protected:
  virtual ~CreateElementTransaction();

  // The document into which the new node will be inserted.
  EditorBase* mEditorBase;

  // The tag (mapping to object type) for the new element.
  nsCOMPtr<nsIAtom> mTag;

  // The node into which the new node will be inserted.
  nsCOMPtr<nsINode> mParent;

  // The index in mParent for the new node.
  int32_t mOffsetInParent;

  // The new node to insert.
  nsCOMPtr<dom::Element> mNewNode;

  // The node we will insert mNewNode before.  We compute this ourselves.
  nsCOMPtr<nsIContent> mRefNode;
};

} // namespace mozilla

#endif // #ifndef CreateElementTransaction_h
