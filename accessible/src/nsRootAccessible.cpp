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

#include "nsRootAccessible.h"
#include "nsCOMPtr.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIContent.h"
#include "nsIFrame.h"

//-----------------------------------------------------
// construction 
//-----------------------------------------------------
nsRootAccessible::nsRootAccessible(nsIWeakReference* aShell, nsIFrame* aFrame):nsAccessible(nsnull,nsnull,aShell)
{
 // mFrame = aFrame;
}

//-----------------------------------------------------
// destruction
//-----------------------------------------------------
nsRootAccessible::~nsRootAccessible()
{
}

  /* attribute wstring accName; */
NS_IMETHODIMP nsRootAccessible::GetAccName(PRUnichar * *aAccName) 
{ 
  nsAutoString name;
  name.AssignWithConversion("Mozilla Document");
  *aAccName = name.ToNewUnicode() ;
  return NS_OK;  
}

// helpers
nsIFrame* nsRootAccessible::GetFrame()
{
  //if (!mFrame) {
    nsCOMPtr<nsIPresShell> shell = do_QueryReferent(mPresShell);
    nsIFrame* root = nsnull;
    if (shell) 
      shell->GetRootFrame(&root);
  
    return root;
  //}
 
 // return mFrame;
}

nsIAccessible* nsRootAccessible::CreateNewAccessible(nsIAccessible* aAccessible, nsIContent* aContent, nsIWeakReference* aShell)
{
  return new nsHTMLBlockAccessible(aAccessible, aContent, aShell);
}

  /* readonly attribute nsIAccessible accParent; */
NS_IMETHODIMP nsRootAccessible::GetAccParent(nsIAccessible * *aAccParent) 
{ 
  *aAccParent = nsnull;
  return NS_OK;
}

  /* readonly attribute wstring accRole; */
NS_IMETHODIMP nsRootAccessible::GetAccRole(PRUnichar * *aAccRole) 
{ 
  // failed? Lets do some default behavior
  nsAutoString a;
  a.AssignWithConversion("client");
  *aAccRole = a.ToNewUnicode();
  return NS_OK;  
}
