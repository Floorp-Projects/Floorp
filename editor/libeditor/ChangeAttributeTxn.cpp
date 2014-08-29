/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Element.h"        // for Element

#include "ChangeAttributeTxn.h"
#include "nsAString.h"
#include "nsError.h"                    // for NS_ERROR_NOT_INITIALIZED, etc

using namespace mozilla;
using namespace mozilla::dom;

ChangeAttributeTxn::ChangeAttributeTxn(Element& aElement, nsIAtom& aAttribute,
                                       const nsAString* aValue)
  : EditTxn()
  , mElement(&aElement)
  , mAttribute(&aAttribute)
  , mValue(aValue ? *aValue : EmptyString())
  , mRemoveAttribute(!aValue)
  , mAttributeWasSet(false)
  , mUndoValue()
{
}

ChangeAttributeTxn::~ChangeAttributeTxn()
{
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(ChangeAttributeTxn, EditTxn,
                                   mElement)

NS_IMPL_ADDREF_INHERITED(ChangeAttributeTxn, EditTxn)
NS_IMPL_RELEASE_INHERITED(ChangeAttributeTxn, EditTxn)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ChangeAttributeTxn)
NS_INTERFACE_MAP_END_INHERITING(EditTxn)

NS_IMETHODIMP
ChangeAttributeTxn::DoTransaction()
{
  // Need to get the current value of the attribute and save it, and set
  // mAttributeWasSet
  mAttributeWasSet = mElement->GetAttr(kNameSpaceID_None, mAttribute,
                                       mUndoValue);

  // XXX: hack until attribute-was-set code is implemented
  if (!mUndoValue.IsEmpty()) {
    mAttributeWasSet = true;
  }
  // XXX: end hack

  // Now set the attribute to the new value
  if (mRemoveAttribute) {
    return mElement->UnsetAttr(kNameSpaceID_None, mAttribute, true);
  }

  return mElement->SetAttr(kNameSpaceID_None, mAttribute, mValue, true);
}

NS_IMETHODIMP
ChangeAttributeTxn::UndoTransaction()
{
  if (mAttributeWasSet) {
    return mElement->SetAttr(kNameSpaceID_None, mAttribute, mUndoValue, true);
  }
  return mElement->UnsetAttr(kNameSpaceID_None, mAttribute, true);
}

NS_IMETHODIMP
ChangeAttributeTxn::RedoTransaction()
{
  if (mRemoveAttribute) {
    return mElement->UnsetAttr(kNameSpaceID_None, mAttribute, true);
  }

  return mElement->SetAttr(kNameSpaceID_None, mAttribute, mValue, true);
}

NS_IMETHODIMP
ChangeAttributeTxn::GetTxnDescription(nsAString& aString)
{
  aString.AssignLiteral("ChangeAttributeTxn: [mRemoveAttribute == ");

  if (mRemoveAttribute) {
    aString.AppendLiteral("true] ");
  } else {
    aString.AppendLiteral("false] ");
  }
  aString += nsDependentAtomString(mAttribute);
  return NS_OK;
}
