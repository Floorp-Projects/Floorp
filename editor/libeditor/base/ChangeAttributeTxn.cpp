/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "ChangeAttributeTxn.h"
#include "nsIDOMElement.h"
#include "nsEditor.h"

ChangeAttributeTxn::ChangeAttributeTxn()
  : EditTxn()
{
}

ChangeAttributeTxn::~ChangeAttributeTxn()
{
}

NS_IMETHODIMP ChangeAttributeTxn::Init(nsIEditor      *aEditor,
                                       nsIDOMElement  *aElement,
                                       const nsAReadableString& aAttribute,
                                       const nsAReadableString& aValue,
                                       PRBool aRemoveAttribute)
{
	NS_ASSERTION(aEditor && aElement, "bad arg");
  if (!aEditor || !aElement) { return NS_ERROR_NULL_POINTER; }

  mEditor = aEditor;
  mElement = do_QueryInterface(aElement);
  mAttribute = aAttribute;
  mValue = aValue;
  mRemoveAttribute = aRemoveAttribute;
  mAttributeWasSet=PR_FALSE;
  mUndoValue.SetLength(0);
  return NS_OK;
}

NS_IMETHODIMP ChangeAttributeTxn::DoTransaction(void)
{
	NS_ASSERTION(mEditor && mElement, "bad state");
	if (!mEditor || !mElement) { return NS_ERROR_NOT_INITIALIZED; }

  // need to get the current value of the attribute and save it, and set mAttributeWasSet
  nsresult result = mEditor->GetAttributeValue(mElement, mAttribute, mUndoValue, &mAttributeWasSet);
  // XXX: hack until attribute-was-set code is implemented
      if (PR_FALSE==mUndoValue.IsEmpty())
        mAttributeWasSet=PR_TRUE;
  // XXX: end hack
  
  // now set the attribute to the new value
  if (PR_FALSE==mRemoveAttribute)
    result = mElement->SetAttribute(mAttribute, mValue);
  else
   result = mElement->RemoveAttribute(mAttribute);

  return result;
}

NS_IMETHODIMP ChangeAttributeTxn::UndoTransaction(void)
{
	NS_ASSERTION(mEditor && mElement, "bad state");
	if (!mEditor || !mElement) { return NS_ERROR_NOT_INITIALIZED; }

  nsresult result=NS_OK;
  if (PR_TRUE==mAttributeWasSet)
    result = mElement->SetAttribute(mAttribute, mUndoValue);
  else
    result = mElement->RemoveAttribute(mAttribute);

  return result;
}

NS_IMETHODIMP ChangeAttributeTxn::RedoTransaction(void)
{
	NS_ASSERTION(mEditor && mElement, "bad state");
	if (!mEditor || !mElement) { return NS_ERROR_NOT_INITIALIZED; }

  nsresult result;

  if (PR_FALSE==mRemoveAttribute)
   result = mElement->SetAttribute(mAttribute, mValue);
  else
   result = mElement->RemoveAttribute(mAttribute);

  return result;
}

NS_IMETHODIMP ChangeAttributeTxn::Merge(nsITransaction *aTransaction, PRBool *aDidMerge)
{
  if (nsnull!=aDidMerge)
    *aDidMerge=PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP ChangeAttributeTxn::GetTxnDescription(nsAWritableString& aString)
{
  aString.Assign(NS_LITERAL_STRING("ChangeAttributeTxn: "));

  if (PR_FALSE==mRemoveAttribute)
    aString += NS_LITERAL_STRING("[mRemoveAttribute == false] ");
  else
    aString += NS_LITERAL_STRING("[mRemoveAttribute == true] ");
  aString += mAttribute;
  return NS_OK;
}
