/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
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
                                       const nsString& aAttribute,
                                       const nsString& aValue,
                                       PRBool aRemoveAttribute)
{
  if (nsnull!=aEditor && nsnull!=aElement)
  {
    mEditor = do_QueryInterface(aEditor);
    mElement = do_QueryInterface(aElement);
    mAttribute = aAttribute;
    mValue = aValue;
    mRemoveAttribute = aRemoveAttribute;
    mAttributeWasSet=PR_FALSE;
    mUndoValue="";
    return NS_OK;
  }
  else
    return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP ChangeAttributeTxn::Do(void)
{
  // need to get the current value of the attribute and save it, and set mAttributeWasSet
  const int stringlen=100;
  char attributeAsCString[stringlen+1];
  char valueAsCString[stringlen+1];
  mAttribute.ToCString(attributeAsCString, stringlen);
  mAttributeWasSet;
  nsresult result = mEditor->GetAttributeValue(mElement, mAttribute, mUndoValue, mAttributeWasSet);
  // XXX: hack until attribute-was-set code is implemented
      if (PR_FALSE==mUndoValue.Equals(""))
        mAttributeWasSet=PR_TRUE;
  // XXX: end hack
  
  if (mAttributeWasSet)
    mUndoValue.ToCString(valueAsCString, stringlen);

  // now set the attribute to the new value
  if (PR_FALSE==mRemoveAttribute)
    result = mElement->SetAttribute(mAttribute, mValue);
  else
   result = mElement->RemoveAttribute(mAttribute);

  return result;
}

NS_IMETHODIMP ChangeAttributeTxn::Undo(void)
{
  nsresult result=NS_OK;
  if (PR_TRUE==mAttributeWasSet)
    result = mElement->SetAttribute(mAttribute, mUndoValue);
  else
    result = mElement->RemoveAttribute(mAttribute);

  return result;
}

NS_IMETHODIMP ChangeAttributeTxn::Redo(void)
{
  nsresult result;

  if (PR_FALSE==mRemoveAttribute)
   result = mElement->SetAttribute(mAttribute, mValue);
  else
   result = mElement->RemoveAttribute(mAttribute);

  return result;
}

NS_IMETHODIMP ChangeAttributeTxn::Merge(PRBool *aDidMerge, nsITransaction *aTransaction)
{
  if (nsnull!=aDidMerge)
    *aDidMerge=PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP ChangeAttributeTxn::Write(nsIOutputStream *aOutputStream)
{
  return NS_OK;
}

NS_IMETHODIMP ChangeAttributeTxn::GetUndoString(nsString **aString)
{
  if (nsnull!=aString)
  {
    if (PR_FALSE==mRemoveAttribute)
      **aString="Change Attribute: ";
    else
      **aString="Remove Attribute: ";
    **aString += mAttribute;
  }
  return NS_OK;
}

NS_IMETHODIMP ChangeAttributeTxn::GetRedoString(nsString **aString)
{
  if (nsnull!=aString)
  {
    if (PR_FALSE==mRemoveAttribute)
      **aString="Change Attribute: ";
    else
      **aString="Add Attribute: ";
    **aString += mAttribute;
  }
  return NS_OK;
}
