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
#include "editor.h"

// note that aEditor is not refcounted
ChangeAttributeTxn::ChangeAttributeTxn(nsEditor *aEditor,
                                       nsIDOMElement *aElement,
                                       const nsString& aAttribute,
                                       const nsString& aValue)
  : EditTxn(aEditor)
{
  mElement = aElement;
  mAttribute = aAttribute;
  mValue = aValue;
  mAttributeWasSet=PR_FALSE;
}

nsresult ChangeAttributeTxn::Do(void)
{
  // need to get the current value of the attribute and save it, and set mAttributeWasSet
  const int stringlen=100;
  char attributeAsCString[stringlen+1];
  char valueAsCString[stringlen+1];
  mAttribute.ToCString(attributeAsCString, stringlen);
  mAttributeWasSet;
  mEditor->GetAttributeValue(mElement, mAttribute, mUndoValue, mAttributeWasSet);
  
  if (mAttributeWasSet)
    mUndoValue.ToCString(valueAsCString, stringlen);

  // now set the attribute to the new value
  return mEditor->SetAttribute(mElement, mAttribute, mValue);
}

nsresult ChangeAttributeTxn::Undo(void)
{
  nsresult result=NS_OK;
  if (PR_TRUE==mAttributeWasSet)
  {
    result = mEditor->SetAttribute(mElement, mAttribute, mUndoValue);
  }
  else
  {
    result = mEditor->RemoveAttribute(mElement, mAttribute);
  }

  return result;
}

nsresult ChangeAttributeTxn::Redo(void)
{
  return mEditor->SetAttribute(mElement, mAttribute, mValue);
}

nsresult ChangeAttributeTxn::GetIsTransient(PRBool *aIsTransient)
{
  if (nsnull!=aIsTransient)
    *aIsTransient = PR_FALSE;
  return NS_OK;
}

nsresult ChangeAttributeTxn::Merge(PRBool *aDidMerge, nsITransaction *aTransaction)
{
  return NS_OK;
}

nsresult ChangeAttributeTxn::Write(nsIOutputStream *aOutputStream)
{
  return NS_OK;
}

nsresult ChangeAttributeTxn::GetUndoString(nsString **aString)
{
  if (nsnull!=aString)
    *aString=nsnull;
  return NS_OK;
}

nsresult ChangeAttributeTxn::GetRedoString(nsString **aString)
{
  if (nsnull!=aString)
    *aString=nsnull;
  return NS_OK;
}
