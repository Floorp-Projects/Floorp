/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CreateElementTxn_h__
#define CreateElementTxn_h__

#include "EditTxn.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupportsImpl.h"

class nsEditor;
class nsIAtom;
class nsIContent;
class nsINode;

/**
 * A transaction that creates a new node in the content tree.
 */
namespace mozilla {
namespace dom {

class Element;

class CreateElementTxn : public EditTxn
{
public:
  /** Initialize the transaction.
    * @param aEditor the provider of basic editing functionality
    * @param aTag    the tag (P, HR, TABLE, etc.) for the new element
    * @param aParent the node into which the new element will be inserted
    * @param aOffsetInParent the location in aParent to insert the new element
    *                        if eAppend, the new element is appended as the last child
    */
  CreateElementTxn(nsEditor& aEditor,
                   nsIAtom& aTag,
                   nsINode& aParent,
                   int32_t aOffsetInParent);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(CreateElementTxn, EditTxn)

  NS_DECL_EDITTXN

  NS_IMETHOD RedoTransaction() override;

  already_AddRefed<Element> GetNewNode();

protected:
  virtual ~CreateElementTxn();

  /** the document into which the new node will be inserted */
  nsEditor* mEditor;

  /** the tag (mapping to object type) for the new element */
  nsCOMPtr<nsIAtom> mTag;

  /** the node into which the new node will be inserted */
  nsCOMPtr<nsINode> mParent;

  /** the index in mParent for the new node */
  int32_t mOffsetInParent;

  /** the new node to insert */
  nsCOMPtr<Element> mNewNode;

  /** the node we will insert mNewNode before.  We compute this ourselves. */
  nsCOMPtr<nsIContent> mRefNode;
};

} // namespace dom
} // namespace mozilla

#endif
