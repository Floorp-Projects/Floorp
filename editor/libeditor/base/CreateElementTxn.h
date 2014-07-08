/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CreateElementTxn_h__
#define CreateElementTxn_h__

#include "EditTxn.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIDOMNode.h"
#include "nsISupportsImpl.h"
#include "nsString.h"
#include "nscore.h"

class nsEditor;

/**
 * A transaction that creates a new node in the content tree.
 */
class CreateElementTxn : public EditTxn
{
public:
  enum { eAppend=-1 };

  /** Initialize the transaction.
    * @param aEditor the provider of basic editing functionality
    * @param aTag    the tag (P, HR, TABLE, etc.) for the new element
    * @param aParent the node into which the new element will be inserted
    * @param aOffsetInParent the location in aParent to insert the new element
    *                        if eAppend, the new element is appended as the last child
    */
  NS_IMETHOD Init(nsEditor *aEditor,
                  const nsAString& aTag,
                  nsIDOMNode *aParent,
                  uint32_t aOffsetInParent);

  CreateElementTxn();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(CreateElementTxn, EditTxn)

  NS_DECL_EDITTXN

  NS_IMETHOD RedoTransaction();

  NS_IMETHOD GetNewNode(nsIDOMNode **aNewNode);

protected:
  virtual ~CreateElementTxn();

  /** the document into which the new node will be inserted */
  nsEditor* mEditor;
  
  /** the tag (mapping to object type) for the new element */
  nsString mTag;

  /** the node into which the new node will be inserted */
  nsCOMPtr<nsIDOMNode> mParent;

  /** the index in mParent for the new node */
  uint32_t mOffsetInParent;

  /** the new node to insert */
  nsCOMPtr<nsIDOMNode> mNewNode;  

  /** the node we will insert mNewNode before.  We compute this ourselves. */
  nsCOMPtr<nsIDOMNode> mRefNode;
};

#endif
