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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Gaunt (jgaunt@netscape.com)
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

#include "nsBaseWidgetAccessible.h"
#include "nsAccessibilityAtoms.h"
#include "nsIAccessibilityService.h"
#include "nsIAccessibleDocument.h"
#include "nsAccessibleWrap.h"
#include "nsGUIEvent.h"
#include "nsILink.h"
#include "nsINameSpaceManager.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsIServiceManager.h"
#include "nsIURI.h"

// ------------
// nsBlockAccessible
// ------------

nsBlockAccessible::nsBlockAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):nsAccessibleWrap(aNode, aShell)
{
}

NS_IMPL_ISUPPORTS_INHERITED0(nsBlockAccessible, nsAccessible)

/* nsIAccessible accGetAt (in long x, in long y); */
NS_IMETHODIMP nsBlockAccessible::GetChildAtPoint(PRInt32 tx, PRInt32 ty, nsIAccessible **_retval)
{
  // We're going to find the child that contains coordinates (tx,ty)
  PRInt32 x,y,w,h;
  GetBounds(&x,&y,&w,&h);  // Get bounds for this accessible
  if (tx >= x && tx < x + w && ty >= y && ty < y + h)
  {
    // It's within this nsIAccessible, let's drill down
    nsCOMPtr<nsIAccessible> child;
    nsCOMPtr<nsIAccessible> smallestChild;
    PRInt32 smallestArea = -1;
    nsCOMPtr<nsIAccessible> next;
    GetFirstChild(getter_AddRefs(child));
    PRInt32 cx,cy,cw,ch;  // Child bounds

    while(child) {
      child->GetBounds(&cx,&cy,&cw,&ch);
      
      // ok if there are multiple frames the contain the point 
      // and they overlap then pick the smallest. We need to do this
      // for text frames.
      
      // For example, A point that's in block #2 is also in block #1, but we want to return #2:
      //
      // [[block #1 is long wrapped text that continues to
      // another line]]  [[here is a shorter block #2]]

      if (tx >= cx && tx < cx + cw && ty >= cy && ty < cy + ch) 
      {
        if (smallestArea == -1 || cw*ch < smallestArea) {
          smallestArea = cw*ch;
          smallestChild = child;
        }
      }
      child->GetNextSibling(getter_AddRefs(next));
      child = next;
    }

    if (smallestChild != nsnull)
    {
      *_retval = smallestChild;
      NS_ADDREF(*_retval);
      return NS_OK;
    }

    *_retval = this;
    NS_ADDREF(this);
    return NS_OK;
  }

  *_retval = nsnull;
  return NS_OK;
}


//-------------
// nsLeafAccessible
//-------------

nsLeafAccessible::nsLeafAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsAccessibleWrap(aNode, aShell)
{
}

NS_IMPL_ISUPPORTS_INHERITED0(nsLeafAccessible, nsAccessible)

/* nsIAccessible getFirstChild (); */
NS_IMETHODIMP nsLeafAccessible::GetFirstChild(nsIAccessible **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

/* nsIAccessible getLastChild (); */
NS_IMETHODIMP nsLeafAccessible::GetLastChild(nsIAccessible **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

/* long getAccChildCount (); */
NS_IMETHODIMP nsLeafAccessible::GetChildCount(PRInt32 *_retval)
{
  *_retval = 0;
  return NS_OK;
}


//----------------
// nsLinkableAccessible
//----------------

nsLinkableAccessible::nsLinkableAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell) :
  nsAccessibleWrap(aNode, aShell),
  mActionContent(nsnull),
  mIsLink(PR_FALSE),
  mIsLinkVisited(PR_FALSE),
  mIsOnclick(PR_FALSE)
{
  CacheActionContent();
}

NS_IMPL_ISUPPORTS_INHERITED0(nsLinkableAccessible, nsAccessible)

NS_IMETHODIMP nsLinkableAccessible::TakeFocus()
{ 
  if (mActionContent && mActionContent->IsFocusable()) {
    mActionContent->SetFocus(nsCOMPtr<nsPresContext>(GetPresContext()));
  }
  
  return NS_OK;
}

/* long GetState (); */
NS_IMETHODIMP nsLinkableAccessible::GetState(PRUint32 *aState)
{
  nsAccessible::GetState(aState);
  if (mIsLink) {
    *aState |= STATE_LINKED;
    if (mIsLinkVisited)
      *aState |= STATE_TRAVERSED;
    // Make sure we also include all the states of the parent link, such as focusable, focused, etc.
    PRUint32 role;
    GetRole(&role);
    if (role != ROLE_LINK) {
      nsCOMPtr<nsIAccessible> parentAccessible;
      GetParent(getter_AddRefs(parentAccessible));
      if (parentAccessible) {
        PRUint32 orState = 0;
        parentAccessible->GetFinalState(&orState);
        *aState |= orState;
      }
    }
  }
  if (mActionContent && !mActionContent->IsFocusable()) {
    *aState &= ~STATE_FOCUSABLE; // Links must have href or tabindex
  }

  nsCOMPtr<nsIAccessibleDocument> docAccessible(GetDocAccessible());
  if (docAccessible) {
    PRBool isEditable;
    docAccessible->GetIsEditable(&isEditable);
    if (isEditable) {
      *aState &= ~(STATE_FOCUSED | STATE_FOCUSABLE); // Links not focusable in editor
    }
  }
  return NS_OK;
}


NS_IMETHODIMP nsLinkableAccessible::GetValue(nsAString& _retval)
{
  if (mIsLink) {
    nsCOMPtr<nsIDOMNode> linkNode(do_QueryInterface(mActionContent));
    nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mWeakShell));
    if (linkNode && presShell)
      return presShell->GetLinkLocation(linkNode, _retval);
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}


/* PRUint8 getAccNumActions (); */
NS_IMETHODIMP nsLinkableAccessible::GetNumActions(PRUint8 *aNumActions)
{
  *aNumActions = mActionContent ? 1 : 0;
  return NS_OK;
}

/* wstring getAccActionName (in PRUint8 index); */
NS_IMETHODIMP nsLinkableAccessible::GetActionName(PRUint8 index, nsAString& aActionName)
{
  // Action 0 (default action): Jump to link
  aActionName.Truncate();
  if (index == eAction_Jump) {   
    if (mIsLink) {
      return nsAccessible::GetTranslatedString(NS_LITERAL_STRING("jump"), aActionName); 
    }
    else if (mIsOnclick) {
      return nsAccessible::GetTranslatedString(NS_LITERAL_STRING("click"), aActionName); 
    }
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  return NS_ERROR_INVALID_ARG;
}

/* void accDoAction (in PRUint8 index); */
NS_IMETHODIMP nsLinkableAccessible::DoAction(PRUint8 index)
{
  // Action 0 (default action): Jump to link
  if (index == eAction_Jump) {
    if (mActionContent) {
      return DoCommand(mActionContent);
    }
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP nsLinkableAccessible::GetKeyboardShortcut(nsAString& aKeyboardShortcut)
{
  if (mActionContent) {
    nsCOMPtr<nsIDOMNode> actionNode(do_QueryInterface(mActionContent));
    if (actionNode && mDOMNode != actionNode) {
      nsCOMPtr<nsIAccessible> accessible;
      nsCOMPtr<nsIAccessibilityService> accService = 
        do_GetService("@mozilla.org/accessibilityService;1");
      accService->GetAccessibleInWeakShell(actionNode, mWeakShell,
                                           getter_AddRefs(accessible));
      if (accessible) {
        accessible->GetKeyboardShortcut(aKeyboardShortcut);
      }
      return NS_OK;
    }
  }
  return nsAccessible::GetKeyboardShortcut(aKeyboardShortcut);
}

void nsLinkableAccessible::CacheActionContent()
{
  for (nsCOMPtr<nsIContent> walkUpContent(do_QueryInterface(mDOMNode));
       walkUpContent;
       walkUpContent = walkUpContent->GetParent()) {
    nsIAtom *tag = walkUpContent->Tag();
    if ((tag == nsAccessibilityAtoms::a || tag == nsAccessibilityAtoms::area)) {
      // Currently we do not expose <link> tags, because they are not typically
      // in <body> and rendered.
      // We do not yet support xlinks
      nsCOMPtr<nsILink> link = do_QueryInterface(walkUpContent);
      NS_ASSERTION(link, "No nsILink for area or a");
      nsCOMPtr<nsIURI> uri;
      link->GetHrefURI(getter_AddRefs(uri));
      if (uri) {
        mActionContent = walkUpContent;
        mIsLink = PR_TRUE;
        nsLinkState linkState;
        link->GetLinkState(linkState);
        if (linkState == eLinkState_Visited)
          mIsLinkVisited = PR_TRUE;
      }
    }
    if (walkUpContent->HasAttr(kNameSpaceID_None,
                            nsAccessibilityAtoms::onclick)) {
      mActionContent = walkUpContent;
      mIsOnclick = PR_TRUE;
    }
  }
}

NS_IMETHODIMP nsLinkableAccessible::Shutdown()
{
  mActionContent = nsnull;
  return nsAccessibleWrap::Shutdown();
}

