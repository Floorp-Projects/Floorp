/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsCOMPtr.h"
#include "nsXBLInsertionPoint.h"
#include "nsIContent.h"
#include "nsISupportsArray.h"

nsXBLInsertionPoint::nsXBLInsertionPoint(nsIContent* aParentElement, PRUint32 aIndex, nsIContent* aDefaultContent)
:mElements(nsnull)
{
  NS_INIT_REFCNT();
  mParentElement = aParentElement;
  mIndex = aIndex;
  mDefaultContentTemplate = aDefaultContent;
}

nsXBLInsertionPoint::~nsXBLInsertionPoint()
{
}

NS_IMPL_ISUPPORTS1(nsXBLInsertionPoint, nsIXBLInsertionPoint)

NS_IMETHODIMP
nsXBLInsertionPoint::GetInsertionParent(nsIContent** aParentElement)
{
  *aParentElement = mParentElement;
  NS_IF_ADDREF(mParentElement);
  return NS_OK;
}

NS_IMETHODIMP
nsXBLInsertionPoint::GetInsertionIndex(PRInt32 *aResult)
{
  *aResult = mIndex;
  return NS_OK;
}

NS_IMETHODIMP
nsXBLInsertionPoint::SetDefaultContent(nsIContent* aDefaultContent)
{
  mDefaultContent = aDefaultContent;
  return NS_OK;
}

NS_IMETHODIMP
nsXBLInsertionPoint::GetDefaultContent(nsIContent** aDefaultContent)
{
  *aDefaultContent = mDefaultContent;
  NS_IF_ADDREF(*aDefaultContent);
  return NS_OK;
}

NS_IMETHODIMP
nsXBLInsertionPoint::SetDefaultContentTemplate(nsIContent* aDefaultContent)
{
  mDefaultContentTemplate = aDefaultContent;
  return NS_OK;
}

NS_IMETHODIMP
nsXBLInsertionPoint::GetDefaultContentTemplate(nsIContent** aDefaultContent)
{
  *aDefaultContent = mDefaultContentTemplate;
  NS_IF_ADDREF(*aDefaultContent);
  return NS_OK;
}


NS_IMETHODIMP
nsXBLInsertionPoint::AddChild(nsIContent* aChildElement)
{
  if (!mElements)
    NS_NewISupportsArray(getter_AddRefs(mElements));

  mElements->AppendElement(aChildElement);

  return NS_OK;
}

NS_IMETHODIMP
nsXBLInsertionPoint::InsertChildAt(PRInt32 aIndex, nsIContent* aChildElement)
{
  if (!mElements)
    NS_NewISupportsArray(getter_AddRefs(mElements));

  mElements->InsertElementAt(aChildElement, aIndex);
  
  return NS_OK;
}

NS_IMETHODIMP
nsXBLInsertionPoint::RemoveChild(nsIContent* aChildElement)
{
  if (mElements)
    mElements->RemoveElement(aChildElement);
  return NS_OK;
}

NS_IMETHODIMP
nsXBLInsertionPoint::ChildCount(PRUint32* aResult)
{
  *aResult = 0;
  if (mElements)
    mElements->Count(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsXBLInsertionPoint::ChildAt(PRUint32 aIndex, nsIContent** aResult)
{
  if (!mElements) {
    *aResult = nsnull;
    return NS_ERROR_FAILURE;
  }

  *aResult = (nsIContent*)(mElements->ElementAt(aIndex)); // Addref happens in return.
  return NS_OK;
}

NS_IMETHODIMP
nsXBLInsertionPoint::Matches(nsIContent* aContent, PRUint32 aIndex, PRBool* aResult)
{
  *aResult = (aContent == mParentElement && mIndex != -1 && ((PRInt32)aIndex) == mIndex);
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////////

nsresult
NS_NewXBLInsertionPoint(nsIContent* aParentElement, PRUint32 aIndex, nsIContent* aDefContent,
                        nsIXBLInsertionPoint** aResult)
{
  *aResult = new nsXBLInsertionPoint(aParentElement, aIndex, aDefContent);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}
