/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ChangeAttributeTxn_h__
#define ChangeAttributeTxn_h__

#include "EditTxn.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIDOMElement.h"
#include "nsISupportsImpl.h"
#include "nsString.h"
#include "nscore.h"

class nsIEditor;

/**
 * A transaction that changes an attribute of a content node. 
 * This transaction covers add, remove, and change attribute.
 */
class ChangeAttributeTxn : public EditTxn
{
public:
  /** Initialize the transaction.
    * @param aEditor the object providing core editing operations
    * @param aNode   the node whose attribute will be changed
    * @param aAttribute the name of the attribute to change
    * @param aValue     the new value for aAttribute, if aRemoveAttribute is false
    * @param aRemoveAttribute if true, remove aAttribute from aNode
    */
  NS_IMETHOD Init(nsIEditor      *aEditor,
                  nsIDOMElement  *aNode,
                  const nsAString& aAttribute,
                  const nsAString& aValue,
                  bool aRemoveAttribute);

  ChangeAttributeTxn();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ChangeAttributeTxn, EditTxn)

  NS_DECL_EDITTXN

  NS_IMETHOD RedoTransaction();

protected:

  /** the editor that created this transaction */
  nsIEditor*  mEditor;
  
  /** the element to operate upon */
  nsCOMPtr<nsIDOMElement> mElement;
  
  /** the attribute to change */
  nsString mAttribute;

  /** the value to set the attribute to (ignored if mRemoveAttribute==true) */
  nsString mValue;

  /** the value to set the attribute to for undo */
  nsString mUndoValue;

  /** true if the mAttribute was set on mElement at the time of execution */
  bool     mAttributeWasSet;

  /** true if the operation is to remove mAttribute from mElement */
  bool     mRemoveAttribute;
};

#endif
