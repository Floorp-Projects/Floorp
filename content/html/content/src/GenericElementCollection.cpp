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

#include "GenericElementCollection.h"
#include "nsIDOMElement.h"
#include "nsGenericHTMLElement.h"

GenericElementCollection::GenericElementCollection(nsIContent *aParent, 
                                                   nsIAtom *aTag)
  : nsGenericDOMHTMLCollection()
{
  mParent = aParent;
  mTag = aTag;
  NS_IF_ADDREF(aTag);
}

GenericElementCollection::~GenericElementCollection()
{
  // we do NOT have a ref-counted reference to mParent, so do NOT
  // release it!  this is to avoid circular references.  The
  // instantiator who provided mParent is responsible for managing our
  // reference for us.

  // Release reference on the tag
  NS_IF_RELEASE(mTag);
}

// we re-count every call.  A better implementation would be to set ourselves up as
// an observer of contentAppended, contentInserted, and contentDeleted
NS_IMETHODIMP 
GenericElementCollection::GetLength(PRUint32* aLength)
{
  if (nsnull==aLength)
    return NS_ERROR_NULL_POINTER;
  *aLength=0;
  nsresult result = NS_OK;
  if (nsnull!=mParent)
  {
    nsIContent *child=nsnull;
    PRUint32 childIndex=0;
    mParent->ChildAt(childIndex, child);
    while (nsnull!=child)
    {
      nsIAtom *childTag;
      child->GetTag(childTag);
      if (mTag==childTag)
      {
        (*aLength)++;
      }
      NS_RELEASE(childTag);
      NS_RELEASE(child);
      childIndex++;
      mParent->ChildAt(childIndex, child);
    }
  }
  return result;
}

NS_IMETHODIMP 
GenericElementCollection::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  *aReturn=nsnull;
  PRUint32 theIndex = 0;
  nsresult rv = NS_OK;
  if (nsnull!=mParent)
  {
    nsIContent *child=nsnull;
    PRUint32 childIndex=0;
    mParent->ChildAt(childIndex, child);
    while (nsnull!=child)
    {
      nsIAtom *childTag;
      child->GetTag(childTag);
      if (mTag==childTag)
      {
        if (aIndex==theIndex)
        {
          child->QueryInterface(NS_GET_IID(nsIDOMNode), (void**)aReturn);   // out-param addref
          NS_ASSERTION(nsnull!=aReturn, "content element must be an nsIDOMNode");
          NS_RELEASE(childTag);
          NS_RELEASE(child);
          break;
        }
        theIndex++;
      }
      NS_RELEASE(childTag);
      NS_RELEASE(child);
      childIndex++;
      mParent->ChildAt(childIndex, child);
    }
  }
  return rv;
}

NS_IMETHODIMP 
GenericElementCollection::NamedItem(const nsAReadableString& aName, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;
  nsresult rv = NS_OK;
  if (nsnull!=mParent)
  {
  }
  return rv;
}

NS_IMETHODIMP
GenericElementCollection::ParentDestroyed()
{
  // see comment in destructor, do NOT release mParent!
  mParent = nsnull;
  return NS_OK;
}

#ifdef DEBUG
nsresult
GenericElementCollection::SizeOf(nsISizeOfHandler* aSizer,
                                 PRUint32* aResult) const
{
  if (!aResult) return NS_ERROR_NULL_POINTER;
  *aResult = sizeof(*this);
  return NS_OK;
}
#endif
