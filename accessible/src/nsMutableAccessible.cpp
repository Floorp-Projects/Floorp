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

#include "nsMutableAccessible.h"
#include "nsINameSpaceManager.h"

NS_IMPL_ISUPPORTS1(nsMutableAccessible, nsIAccessible)

nsMutableAccessible::nsMutableAccessible(nsISupports* aNode)
{
  NS_INIT_ISUPPORTS();
  NS_ASSERTION(aNode,"We must have a valid node!!");
  mNode = aNode;
  mNameNodeValue = PR_FALSE;
  mNameStringSet = PR_FALSE;
  mRole = NS_LITERAL_STRING("unknown");
  mIsLeaf = PR_FALSE;
}

nsMutableAccessible::~nsMutableAccessible()
{
}

NS_IMETHODIMP nsMutableAccessible::SetIsLeaf(PRBool aLeaf)
{
    mIsLeaf = aLeaf;
    return NS_OK;
}

/* void SetNodeAsNodeValue (); */
NS_IMETHODIMP nsMutableAccessible::SetNameAsNodeValue()
{
    mNameNodeValue = PR_TRUE;
    return NS_OK;
}

/* void SetName (in wstring name); */
NS_IMETHODIMP nsMutableAccessible::SetName(const PRUnichar *aName)
{
    mName = aName; 
    mNameStringSet = PR_TRUE;
    return NS_OK;
}

/* void SetNameAsAttribute (in nsIAtom atom); */
NS_IMETHODIMP nsMutableAccessible::SetNameAsAttribute(nsIAtom *aAttribute)
{
   mNameAttribute = aAttribute;
   return NS_OK;
}

/* void SetNameAsAttribute (in nsIAtom atom); */
NS_IMETHODIMP nsMutableAccessible::SetRole(const PRUnichar *aRole)
{
   mRole = aRole;
   return NS_OK;
}

NS_IMETHODIMP nsMutableAccessible::GetAccParent(nsIAccessible **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMutableAccessible::GetAccNextSibling(nsIAccessible **_retval)
{ 
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMutableAccessible::GetAccPreviousSibling(nsIAccessible **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMutableAccessible::GetAccFirstChild(nsIAccessible **_retval)
{
  if (mIsLeaf) {
    *_retval = nsnull;
    return NS_OK;
  }

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMutableAccessible::GetAccLastChild(nsIAccessible **_retval)
{
  if (mIsLeaf) {
    *_retval = nsnull;
    return NS_OK;
  }

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMutableAccessible::GetAccChildCount(PRInt32 *_retval)
{
  if (mIsLeaf) {
    *_retval = 0;
    return NS_OK;
  }

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMutableAccessible::GetAccName(PRUnichar **_retval)
{
  nsAutoString value;

  if (mNameNodeValue) {
    // see if we can get nodevalue
    nsCOMPtr<nsIDOMNode> node = do_QueryInterface(mNode);
    if (node) {
      node->GetNodeValue(value);
      value.CompressWhitespace();
    } else {
      *_retval = nsnull;
      return NS_ERROR_NOT_IMPLEMENTED;
    }
  } else if (mNameStringSet) {
    value = mName;
  } else if (mNameAttribute) {
    nsCOMPtr<nsIContent> content = do_QueryInterface(mNode);
    content->GetAttribute(kNameSpaceID_None, mNameAttribute, value);
    value.CompressWhitespace();
  } else {
     *_retval = nsnull;
     return NS_ERROR_NOT_IMPLEMENTED;
  }
    
  *_retval = value.ToNewUnicode();

  return NS_OK;
}

NS_IMETHODIMP nsMutableAccessible::GetAccValue(PRUnichar **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMutableAccessible::SetAccName(const PRUnichar *name)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMutableAccessible::SetAccValue(const PRUnichar *value)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMutableAccessible::GetAccDescription(PRUnichar **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMutableAccessible::GetAccRole(PRUnichar **_retval)
{
  *_retval = mRole.ToNewUnicode();
  return NS_OK;
}

NS_IMETHODIMP nsMutableAccessible::GetAccState(PRUint32 *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMutableAccessible::GetAccExtState(PRUint32 *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMutableAccessible::GetAccDefaultAction(PRUnichar **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMutableAccessible::GetAccHelp(PRUnichar **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMutableAccessible::GetAccFocused(PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMutableAccessible::AccGetAt(PRInt32 x, PRInt32 y, nsIAccessible **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMutableAccessible::AccNavigateRight(nsIAccessible **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMutableAccessible::AccNavigateLeft(nsIAccessible **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMutableAccessible::AccNavigateUp(nsIAccessible **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMutableAccessible::AccNavigateDown(nsIAccessible **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMutableAccessible::AccGetBounds(PRInt32 *x, PRInt32 *y, PRInt32 *width, PRInt32 *height)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMutableAccessible::AccAddSelection()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMutableAccessible::AccRemoveSelection()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMutableAccessible::AccExtendSelection()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMutableAccessible::AccTakeSelection()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMutableAccessible::AccTakeFocus()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMutableAccessible::AccDoDefaultAction()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

