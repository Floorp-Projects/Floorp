/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   David W. Hyatt (hyatt@netscape.com) (Original Author)
 *   Joe Hewitt (hewitt@netscape.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsCOMPtr.h"
#include "nsPIListBoxObject.h"
#include "nsBoxObject.h"
#include "nsIFrame.h"
#include "nsIDocument.h"
#include "nsBindingManager.h"
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsGkAtoms.h"
#include "nsIScrollableFrame.h"
#include "nsListBoxBodyFrame.h"

class nsListBoxObject : public nsPIListBoxObject, public nsBoxObject
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSILISTBOXOBJECT

  // nsPIListBoxObject
  virtual nsListBoxBodyFrame* GetListBoxBody(bool aFlush);

  nsListBoxObject();

  // nsPIBoxObject
  virtual void Clear();
  virtual void ClearCachedValues();
  
protected:
  nsListBoxBodyFrame *mListBoxBody;
};

NS_IMPL_ISUPPORTS_INHERITED2(nsListBoxObject, nsBoxObject, nsIListBoxObject,
                             nsPIListBoxObject)

nsListBoxObject::nsListBoxObject()
  : mListBoxBody(nsnull)
{
}

//////////////////////////////////////////////////////////////////////////
//// nsIListBoxObject

NS_IMETHODIMP
nsListBoxObject::GetRowCount(PRInt32 *aResult)
{
  nsListBoxBodyFrame* body = GetListBoxBody(PR_TRUE);
  if (body)
    return body->GetRowCount(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsListBoxObject::GetNumberOfVisibleRows(PRInt32 *aResult)
{
  nsListBoxBodyFrame* body = GetListBoxBody(PR_TRUE);
  if (body)
    return body->GetNumberOfVisibleRows(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsListBoxObject::GetIndexOfFirstVisibleRow(PRInt32 *aResult)
{
  nsListBoxBodyFrame* body = GetListBoxBody(PR_TRUE);
  if (body)
    return body->GetIndexOfFirstVisibleRow(aResult);
  return NS_OK;
}

NS_IMETHODIMP nsListBoxObject::EnsureIndexIsVisible(PRInt32 aRowIndex)
{
  nsListBoxBodyFrame* body = GetListBoxBody(PR_TRUE);
  if (body)
    return body->EnsureIndexIsVisible(aRowIndex);
  return NS_OK;
}

NS_IMETHODIMP
nsListBoxObject::ScrollToIndex(PRInt32 aRowIndex)
{
  nsListBoxBodyFrame* body = GetListBoxBody(PR_TRUE);
  if (body)
    return body->ScrollToIndex(aRowIndex);
  return NS_OK;
}

NS_IMETHODIMP
nsListBoxObject::ScrollByLines(PRInt32 aNumLines)
{
  nsListBoxBodyFrame* body = GetListBoxBody(PR_TRUE);
  if (body)
    return body->ScrollByLines(aNumLines);
  return NS_OK;
}

NS_IMETHODIMP
nsListBoxObject::GetItemAtIndex(PRInt32 index, nsIDOMElement **_retval)
{
  nsListBoxBodyFrame* body = GetListBoxBody(PR_TRUE);
  if (body)
    return body->GetItemAtIndex(index, _retval);
  return NS_OK;
}

NS_IMETHODIMP
nsListBoxObject::GetIndexOfItem(nsIDOMElement* aElement, PRInt32 *aResult)
{
  *aResult = 0;

  nsListBoxBodyFrame* body = GetListBoxBody(PR_TRUE);
  if (body)
    return body->GetIndexOfItem(aElement, aResult);
  return NS_OK;
}

//////////////////////

static void
FindBodyContent(nsIContent* aParent, nsIContent** aResult)
{
  if (aParent->Tag() == nsGkAtoms::listboxbody) {
    *aResult = aParent;
    NS_IF_ADDREF(*aResult);
  }
  else {
    nsCOMPtr<nsIDOMNodeList> kids;
    aParent->GetOwnerDoc()->BindingManager()->GetXBLChildNodesFor(aParent, getter_AddRefs(kids));
    if (!kids) return;

    PRUint32 i;
    kids->GetLength(&i);
    // start from the end, cuz we're smart and we know the listboxbody is probably at the end
    while (i > 0) {
      nsCOMPtr<nsIDOMNode> childNode;
      kids->Item(--i, getter_AddRefs(childNode));
      nsCOMPtr<nsIContent> childContent(do_QueryInterface(childNode));
      FindBodyContent(childContent, aResult);
      if (*aResult)
        break;
    }
  }
}

nsListBoxBodyFrame*
nsListBoxObject::GetListBoxBody(bool aFlush)
{
  if (mListBoxBody) {
    return mListBoxBody;
  }

  nsIPresShell* shell = GetPresShell(PR_FALSE);
  if (!shell) {
    return nsnull;
  }

  nsIFrame* frame = aFlush ? 
                      GetFrame(PR_FALSE) /* does Flush_Frames */ :
                      mContent->GetPrimaryFrame();
  if (!frame)
    return nsnull;

  // Iterate over our content model children looking for the body.
  nsCOMPtr<nsIContent> content;
  FindBodyContent(frame->GetContent(), getter_AddRefs(content));

  if (!content)
    return nsnull;

  // this frame will be a nsGFXScrollFrame
  frame = content->GetPrimaryFrame();
  if (!frame)
     return nsnull;
  nsIScrollableFrame* scrollFrame = do_QueryFrame(frame);
  if (!scrollFrame)
    return nsnull;

  // this frame will be the one we want
  nsIFrame* yeahBaby = scrollFrame->GetScrolledFrame();
  if (!yeahBaby)
     return nsnull;

  // It's a frame. Refcounts are irrelevant.
  nsListBoxBodyFrame* listBoxBody = do_QueryFrame(yeahBaby);
  NS_ENSURE_TRUE(listBoxBody &&
                 listBoxBody->SetBoxObject(this),
                 nsnull);
  mListBoxBody = listBoxBody;
  return mListBoxBody;
}

void
nsListBoxObject::Clear()
{
  ClearCachedValues();

  nsBoxObject::Clear();
}

void
nsListBoxObject::ClearCachedValues()
{
  mListBoxBody = nsnull;
}

// Creation Routine ///////////////////////////////////////////////////////////////////////

nsresult
NS_NewListBoxObject(nsIBoxObject** aResult)
{
  *aResult = new nsListBoxObject;
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}
