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
#include "nsSelection.h"
#include "nsISelection.h"

nsresult nsSelection::QueryInterface(const nsIID& aIID,
                                     void** aInstancePtrResult)
{
  static NS_DEFINE_IID(kISelectionIID, NS_ISELECTION_IID);
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kISelectionIID)) {
    *aInstancePtrResult = (void*) ((nsISelection*)this);
    //AddRef();
    return NS_OK;
  }
  return !NS_OK;
}

nsSelection::nsSelection() {
  mRange = new nsSelectionRange();
} 

nsSelection::~nsSelection() {
  delete mRange;
} 

NS_IMPL_ADDREF(nsSelection)
NS_IMPL_RELEASE(nsSelection)

/**
  * Returns whether there is a valid selection
 */ 
PRBool nsSelection::IsValidSelection() {
  return (PRBool)(mRange->GetStartContent() == nsnull ||
                  mRange->GetEndContent() == nsnull);

}


/**
  * Clears the current selection (invalidates the selection)
 */ 
void nsSelection::ClearSelection() {
  mRange->GetStartPoint()->SetPoint(nsnull, -1, PR_TRUE);
  mRange->GetEndPoint()->SetPoint(nsnull, -1, PR_TRUE);
}



/**
  * Copies the data from one nsSelectionPoint in the range to another,
  * it does NOT just assign the pointers
 */
void nsSelection::SetRange(nsSelectionRange * aSelectionRange) {
  mRange->SetRange(aSelectionRange->GetStartPoint(),
                   aSelectionRange->GetEndPoint());
}

/**
  * Copies the data from one nsSelectionPoint in the range to another,
  * it does NOT just assign the pointers
 */
void nsSelection::GetRange(nsSelectionRange * aSelectionRange) {
  mRange->SetRange(aSelectionRange->GetStartPoint(),
                   aSelectionRange->GetEndPoint());
}


/**
  * For debug only, it potentially leaks memory
 */
char * nsSelection::ToString() {
  return mRange->ToString();;
}

