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
 *   Aaron Leventhal (aaronl@netscape.com)
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

#include "nsAccessible.h"
#include "nsIAccessibleDocument.h"
#include "nsIDocument.h"
#include "nsIImageDocument.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsIScrollableView.h"
#include "nsIViewManager.h"
#include "nsIWidget.h"
#include "nsIDOMDocumentView.h"
#include "nsIDOMAbstractView.h"
#include "nsIDOM3Node.h"
#include "nsIDOMWindowInternal.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMElement.h"
#include "nsHTMLLinkAccessible.h"
#include "nsISelection.h"
#include "nsISelectionController.h"
#include "nsIServiceManager.h"
#include "nsXPIDLString.h"

#include "nsIDOMComment.h"
#include "nsITextContent.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLBRElement.h"
#include "nsIAtom.h"
#include "nsAccessibilityAtoms.h"
#include "nsGUIEvent.h"

#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMXULSelectCntrlItemEl.h"
#include "nsIDOMHTMLObjectElement.h"
#include "nsIDOMXULButtonElement.h"
#include "nsIDOMXULCheckboxElement.h"
#include "nsIDOMDocument.h"
#include "nsIDOMXULElement.h"
#include "nsIDOMXULLabelElement.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIScriptGlobalObject.h"
#include "nsIFocusController.h"
#include "nsAccessibleTreeWalker.h"
#include "nsIURI.h"
#include "nsIImageLoadingContent.h"
#include "nsINameSpaceManager.h"
#include "nsITimer.h"

#ifdef NS_DEBUG
#include "nsIFrameDebug.h"
#include "nsIDOMCharacterData.h"
#endif

/*
 * Class nsAccessible
 */

//-----------------------------------------------------
// construction 
//-----------------------------------------------------
NS_IMPL_ISUPPORTS_INHERITED2(nsAccessible, nsAccessNode, nsIAccessible, nsPIAccessible)

nsAccessible::nsAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell): nsAccessNodeWrap(aNode, aShell), 
  mParent(nsnull), mFirstChild(nsnull), mNextSibling(nsnull), mRoleMapEntry(nsnull)
{
#ifdef NS_DEBUG_X
   {
     nsCOMPtr<nsIPresShell> shell(do_QueryReferent(aShell));
     printf(">>> %p Created Acc - Con: %p  Acc: %p  PS: %p", 
             (nsIAccessible*)this, aContent, aAccessible, shell.get());
     if (shell && aContent != nsnull) {
       nsIFrame* frame;
       shell->GetPrimaryFrameFor(aContent, &frame);
       char * name;
       if (GetNameForFrame(frame, &name)) {
         printf(" Name:[%s]", name);
         nsMemory::Free(name);
       }
     }
     printf("\n");
   }
#endif
}

//-----------------------------------------------------
// destruction
//-----------------------------------------------------
nsAccessible::~nsAccessible()
{
}

NS_IMETHODIMP nsAccessible::GetName(nsAString& aName)
{
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  if (!content) {
    return NS_ERROR_FAILURE;  // Node shut down
  }

  if (mRoleMapEntry) {
    if (mRoleMapEntry->nameRule == eNoName) {
      return NS_OK;
    }
    if (mRoleMapEntry->nameRule == eNameFromSubtree) {
      // DHTML accessible name method for focusable items such as menuitem, treeitem, gridcell, etc.
      nsresult rv = AppendFlatStringFromSubtree(content, &aName);
      if (NS_SUCCEEDED(rv) && !aName.IsEmpty()) {
        return rv;
      }
    }
  }

  if (NS_CONTENT_ATTR_NOT_THERE ==
      content->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::title, aName)) {
    aName.SetIsVoid(PR_TRUE);
  }

  return NS_OK;
}

NS_IMETHODIMP nsAccessible::GetDescription(nsAString& aDescription)
{
  // There are 3 conditions that make an accessible have no accDescription:
  // 1. it's a text node; or
  // 2. it doesn't have an accName; or
  // 3. its title attribute equals to its accName nsAutoString name; 
  nsCOMPtr<nsITextContent> textContent(do_QueryInterface(mDOMNode));
  if (!textContent) {
    nsAutoString name;
    GetName(name);
    if (!name.IsEmpty()) {
      // If there's already a name, we'll expose a description.if it's different than the name
      // If there is no name, then we know the title should really be exposed there
      nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(mDOMNode));
      if (elt)
        elt->GetAttribute(NS_LITERAL_STRING("title"), aDescription);
      if (!elt || aDescription == name)
        aDescription.Truncate();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsAccessible::GetKeyboardShortcut(nsAString& _retval)
{
  static PRInt32 gGeneralAccesskeyModifier = -1;  // magic value of -1 indicates unitialized state

  nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(mDOMNode));
  if (elt) {
    nsAutoString accesskey;
    elt->GetAttribute(NS_LITERAL_STRING("accesskey"), accesskey);
    if (accesskey.IsEmpty()) {
      nsCOMPtr<nsIContent> content = do_QueryInterface(elt);
      nsIContent *labelContent = 
        content->IsContentOfType(nsIContent::eXUL) ? GetXULLabelContent(content) :
                                                     GetHTMLLabelContent(content);

      if (labelContent) {
        labelContent->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::accesskey, accesskey);
      }
      if (accesskey.IsEmpty()) {
        return NS_ERROR_FAILURE;
      }
    }

    if (gGeneralAccesskeyModifier == -1) {
      // Need to initialize cached global accesskey pref
      gGeneralAccesskeyModifier = 0;
      nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID));
      if (prefBranch)
        prefBranch->GetIntPref("ui.key.generalAccessKey", &gGeneralAccesskeyModifier);
    }
    nsAutoString propertyKey;
    switch (gGeneralAccesskeyModifier) {
      case nsIDOMKeyEvent::DOM_VK_CONTROL: propertyKey.AssignLiteral("VK_CONTROL"); break;
      case nsIDOMKeyEvent::DOM_VK_ALT: propertyKey.AssignLiteral("VK_ALT"); break;
      case nsIDOMKeyEvent::DOM_VK_META: propertyKey.AssignLiteral("VK_META"); break;
    }
    if (!propertyKey.IsEmpty())
      nsAccessible::GetFullKeyName(propertyKey, accesskey, _retval);
    else
      _retval= accesskey;
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsAccessible::SetParent(nsIAccessible *aParent)
{
  mParent = aParent;
  return NS_OK;
}

NS_IMETHODIMP nsAccessible::SetFirstChild(nsIAccessible *aFirstChild)
{
  mFirstChild = aFirstChild;
  return NS_OK;
}

NS_IMETHODIMP nsAccessible::SetNextSibling(nsIAccessible *aNextSibling)
{
  mNextSibling = aNextSibling? aNextSibling: DEAD_END_ACCESSIBLE;
  return NS_OK;
}

NS_IMETHODIMP nsAccessible::Init()
{
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  nsAutoString roleString;
  if (content &&
      NS_CONTENT_ATTR_HAS_VALUE == content->GetAttr(kNameSpaceID_XHTML2_Unofficial, 
                                                    nsAccessibilityAtoms::role, 
                                                    roleString)) {
    // QI to nsIDOM3Node causes some overhead. Unfortunately we need to do this each
    // time there is a role attribute, because the prefixe to namespace mappings
    // can change within any subtree via the xmlns attribute
    nsCOMPtr<nsIDOM3Node> dom3Node(do_QueryInterface(content));
    if (dom3Node) {
      nsAutoString prefix;
      NS_NAMED_LITERAL_STRING(kRolesWAI_Namespace, "http://www.w3.org/2005/01/wai-rdf/GUIRoleTaxonomy#");
      dom3Node->LookupPrefix(kRolesWAI_Namespace, prefix);
      prefix += ':';
      if (StringBeginsWith(roleString, prefix)) {
        roleString.Cut(0, prefix.Length());
        nsCString utf8Role = NS_ConvertUCS2toUTF8(roleString); // For easy comparison
        PRUint32 index;
        for (index = 0; gWAIRoleMap[index].roleString; index ++) {
          if (utf8Role.Equals(gWAIRoleMap[index].roleString)) {
            break; // The dynamic role attribute maps to an entry in our table
          }
        }
        // Always use some entry if there is a role string
        // If no match, we use the last entry which maps to ROLE_NOTHING
        mRoleMapEntry = &gWAIRoleMap[index];
      }
    }
  }

  return nsAccessNodeWrap::Init();
}

NS_IMETHODIMP nsAccessible::Shutdown()
{
  mNextSibling = nsnull;
  // Make sure none of it's children point to this parent
  if (mFirstChild) {
    nsCOMPtr<nsIAccessible> current(mFirstChild), next;
    while (current) {
      nsCOMPtr<nsPIAccessible> privateAcc(do_QueryInterface(current));
      current->GetNextSibling(getter_AddRefs(next));
      privateAcc->SetParent(nsnull);
      current = next;
    }
  }
  // Now invalidate the child count and pointers to other accessibles
  InvalidateChildren();
  if (mParent) {
    nsCOMPtr<nsPIAccessible> privateParent(do_QueryInterface(mParent));
    privateParent->InvalidateChildren();
    mParent = nsnull;
  }

  return nsAccessNodeWrap::Shutdown();
}

NS_IMETHODIMP nsAccessible::InvalidateChildren()
{
  // Document has transformed, reset our invalid children and child count
  mAccChildCount = -1;
  mFirstChild = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsAccessible::GetParent(nsIAccessible **  aParent)
{
  if (!mWeakShell) {
    // This node has been shut down
    *aParent = nsnull;
    return NS_ERROR_FAILURE;
  }
  if (mParent) {
    *aParent = mParent;
    NS_ADDREF(*aParent);
    return NS_OK;
  }

  *aParent = nsnull;
  // Last argument of PR_TRUE indicates to walk anonymous content
  nsAccessibleTreeWalker walker(mWeakShell, mDOMNode, PR_TRUE); 
  if (NS_SUCCEEDED(walker.GetParent())) {
    *aParent = walker.mState.accessible;
    SetParent(*aParent); // Cache it, unless perhaps accessible class overrides SetParent
    NS_ADDREF(*aParent);
  }

  return NS_OK;
}

  /* readonly attribute nsIAccessible nextSibling; */
NS_IMETHODIMP nsAccessible::GetNextSibling(nsIAccessible * *aNextSibling) 
{ 
  *aNextSibling = nsnull; 
  if (!mWeakShell) {
    // This node has been shut down
    return NS_ERROR_FAILURE;
  }
  if (!mParent) {
    nsCOMPtr<nsIAccessible> parent;
    GetParent(getter_AddRefs(parent));
    if (parent) {
      PRInt32 numChildren;
      parent->GetChildCount(&numChildren);  // Make sure we cache all of the children
    }
  }

  if (mNextSibling || !mParent) {
    // If no parent, don't try to calculate a new sibling
    // It either means we're at the root or shutting down the parent
    if (mNextSibling != DEAD_END_ACCESSIBLE) {
      NS_IF_ADDREF(*aNextSibling = mNextSibling);
    }
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

  /* readonly attribute nsIAccessible previousSibling; */
NS_IMETHODIMP nsAccessible::GetPreviousSibling(nsIAccessible * *aPreviousSibling) 
{
  *aPreviousSibling = nsnull;

  if (!mWeakShell) {
    // This node has been shut down
    return NS_ERROR_FAILURE;
  }

  if (!mParent) {
    nsCOMPtr<nsIAccessible> parent;
    if (NS_FAILED(GetParent(getter_AddRefs(parent)))) {
      return NS_ERROR_FAILURE;
    }
  }

  nsCOMPtr<nsIAccessible> testAccessible, prevSibling;
  mParent->GetFirstChild(getter_AddRefs(testAccessible));
  while (testAccessible) {
    prevSibling = testAccessible;
    prevSibling->GetNextSibling(getter_AddRefs(testAccessible));
  }

  if (!prevSibling) {
    return NS_ERROR_FAILURE;
  }

  NS_ADDREF(*aPreviousSibling = prevSibling);
  return NS_OK;
}

  /* readonly attribute nsIAccessible firstChild; */
NS_IMETHODIMP nsAccessible::GetFirstChild(nsIAccessible * *aFirstChild) 
{  
  if (gIsCacheDisabled) {
    InvalidateChildren();
  }
  PRInt32 numChildren;
  GetChildCount(&numChildren);  // Make sure we cache all of the children

  NS_IF_ADDREF(*aFirstChild = mFirstChild);

  return NS_OK;  
}

  /* readonly attribute nsIAccessible lastChild; */
NS_IMETHODIMP nsAccessible::GetLastChild(nsIAccessible * *aLastChild)
{  
  GetChildAt(-1, aLastChild);
  return NS_OK;
}

NS_IMETHODIMP nsAccessible::GetChildAt(PRInt32 aChildNum, nsIAccessible **aChild)
{
  // aChildNum is a zero-based index
  // If aChildNum is out of range, last child is returned

  PRInt32 numChildren;
  GetChildCount(&numChildren);

  if (aChildNum >= numChildren || !mWeakShell) {
    *aChild = nsnull;
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIAccessible> current(mFirstChild), nextSibling;
  PRInt32 index = 0;

  while (current) {
    nextSibling = current;
    if (++index > aChildNum) {
      break;
    }
    nextSibling->GetNextSibling(getter_AddRefs(current));
  }

  NS_IF_ADDREF(*aChild = nextSibling);

  return NS_OK;
}

void nsAccessible::CacheChildren(PRBool aWalkAnonContent)
{
  if (!mWeakShell) {
    // This node has been shut down
    mAccChildCount = -1;
    return;
  }

  if (mAccChildCount == eChildCountUninitialized) {
    nsAccessibleTreeWalker walker(mWeakShell, mDOMNode, aWalkAnonContent);
    // Seed the frame hint early while we're still on a container node.
    // This is better than doing the GetPrimaryFrameFor() later on
    // a text node, because text nodes aren't in the frame map.
    walker.mState.frame = GetFrame();

    nsCOMPtr<nsPIAccessible> privatePrevAccessible;
    mAccChildCount = 0;
    walker.GetFirstChild();
    SetFirstChild(walker.mState.accessible);

    while (walker.mState.accessible) {
      ++mAccChildCount;
      privatePrevAccessible = do_QueryInterface(walker.mState.accessible);
      privatePrevAccessible->SetParent(this);
      walker.GetNextSibling();
      privatePrevAccessible->SetNextSibling(walker.mState.accessible);
    }
  }
}

/* readonly attribute long childCount; */
NS_IMETHODIMP nsAccessible::GetChildCount(PRInt32 *aAccChildCount) 
{
  CacheChildren(PR_TRUE);
  *aAccChildCount = mAccChildCount;
  return NS_OK;  
}

/* readonly attribute long indexInParent; */
NS_IMETHODIMP nsAccessible::GetIndexInParent(PRInt32 *aIndexInParent)
{
  *aIndexInParent = -1;
  if (!mWeakShell) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIAccessible> parent;
  GetParent(getter_AddRefs(parent));
  if (!parent) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIAccessible> sibling;
  parent->GetFirstChild(getter_AddRefs(sibling));
  if (!sibling) {
    return NS_ERROR_FAILURE;
  }

  *aIndexInParent = 0;
  while (sibling != this) {
    NS_ASSERTION(sibling, "Never ran into the same child that we started from");

    if (!sibling)
      return NS_ERROR_FAILURE;

    ++*aIndexInParent;
    nsCOMPtr<nsIAccessible> tempAccessible;
    sibling->GetNextSibling(getter_AddRefs(tempAccessible));
    sibling = tempAccessible;
  }

  return NS_OK;
}

nsresult nsAccessible::GetTranslatedString(const nsAString& aKey, nsAString& aStringOut)
{
  nsXPIDLString xsValue;

  if (!gStringBundle || 
    NS_FAILED(gStringBundle->GetStringFromName(PromiseFlatString(aKey).get(), getter_Copies(xsValue)))) 
    return NS_ERROR_FAILURE;

  aStringOut.Assign(xsValue);
  return NS_OK;
}

nsresult nsAccessible::GetFullKeyName(const nsAString& aModifierName, const nsAString& aKeyName, nsAString& aStringOut)
{
  nsXPIDLString modifierName, separator;

  if (!gKeyStringBundle ||
      NS_FAILED(gKeyStringBundle->GetStringFromName(PromiseFlatString(aModifierName).get(), 
                                                    getter_Copies(modifierName))) ||
      NS_FAILED(gKeyStringBundle->GetStringFromName(PromiseFlatString(NS_LITERAL_STRING("MODIFIER_SEPARATOR")).get(), 
                                                    getter_Copies(separator)))) {
    return NS_ERROR_FAILURE;
  }

  aStringOut = modifierName + separator + aKeyName; 
  return NS_OK;
}

PRBool nsAccessible::IsPartiallyVisible(PRBool *aIsOffscreen) 
{
  // We need to know if at least a kMinPixels around the object is visible
  // Otherwise it will be marked STATE_OFFSCREEN and STATE_INVISIBLE
  
  *aIsOffscreen = PR_FALSE;

  const PRUint16 kMinPixels  = 12;
   // Set up the variables we need, return false if we can't get at them all
  nsCOMPtr<nsIPresShell> shell(GetPresShell());
  if (!shell) 
    return PR_FALSE;

  nsIViewManager* viewManager = shell->GetViewManager();
  if (!viewManager)
    return PR_FALSE;

  nsIFrame *frame = GetFrame();
  if (!frame) {
    return PR_FALSE;
  }

  // If visibility:hidden or visibility:collapsed then mark with STATE_INVISIBLE
  if (!frame->GetStyleVisibility()->IsVisible())
  {
      return PR_FALSE;
  }

  nsPresContext *presContext = shell->GetPresContext();
  if (!presContext)
    return PR_FALSE;

  // Get the bounds of the current frame, relative to the current view.
  // We don't use the more accurate GetBoundsRect, because that is more expensive 
  // and the STATE_OFFSCREEN flag that this is used for only needs to be a rough indicator

  nsRect relFrameRect = frame->GetRect();
  nsPoint frameOffset;
  nsIView *containingView = frame->GetViewExternal();
  if (!containingView) {
    frame->GetOffsetFromView(frameOffset, &containingView);
    if (!containingView)
      return PR_FALSE;  // no view -- not visible
    relFrameRect.x = frameOffset.x;
    relFrameRect.y = frameOffset.y;
  }

  float p2t;
  p2t = presContext->PixelsToTwips();
  nsRectVisibility rectVisibility;
  viewManager->GetRectVisibility(containingView, relFrameRect, 
                                 NS_STATIC_CAST(PRUint16, (kMinPixels * p2t)), 
                                 &rectVisibility);

  if (rectVisibility == nsRectVisibility_kVisible) {
    // This view says it is visible, but we need to check the parent view chain :(
    while ((containingView = containingView->GetParent()) != nsnull) {
      if (containingView->GetVisibility() == nsViewVisibility_kHide) {
        return PR_FALSE;
      }
    }
    return PR_TRUE;
  }

  *aIsOffscreen = rectVisibility != nsRectVisibility_kZeroAreaRect;
  return PR_FALSE;
}

/* readonly attribute wstring state; */
NS_IMETHODIMP nsAccessible::GetState(PRUint32 *aState) 
{ 
  *aState = 0;

  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  if (!content) {
    return NS_ERROR_FAILURE;  // Node shut down
  }

  // Set STATE_UNAVAILABLE state based on disabled attribute
  // The disabled attribute is mostly used in XUL elements and HTML forms, but
  // if someone sets it on another attribute, 
  // it seems reasonable to consider it unavailable
  PRBool isDisabled;
  if (content->IsContentOfType(nsIContent::eHTML)) {
    // In HTML, just the presence of the disabled attribute means it is disabled,
    // therefore disabled="false" indicates disabled!
    isDisabled = content->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::disabled);
  }
  else {
    nsAutoString disabled;
    content->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::disabled, disabled);
    isDisabled = disabled.EqualsLiteral("true");
  }
  if (isDisabled) {
    *aState |= STATE_UNAVAILABLE;
  }
  else if (content->IsContentOfType(nsIContent::eELEMENT)) {
    if (!mRoleMapEntry) {
      // Default state for element accessible is focusable unless role is manually set
      // Subclasses of nsAccessible will clear focusable state if necessary
      *aState |= STATE_FOCUSABLE;
    }
    else {
      nsIFrame *frame = GetFrame();
      if (frame && frame->IsFocusable()) {
        *aState |= STATE_FOCUSABLE;
      }
    }

    if (gLastFocusedNode == mDOMNode) {
      *aState |= STATE_FOCUSED;
    }
  }

  // Check if STATE_OFFSCREEN bitflag should be turned on for this object
  PRBool isOffscreen;
  if (!IsPartiallyVisible(&isOffscreen)) {
    *aState |= STATE_INVISIBLE;
    if (isOffscreen)
      *aState |= STATE_OFFSCREEN;
  }

  return NS_OK;
}

  /* readonly attribute boolean focusedChild; */
NS_IMETHODIMP nsAccessible::GetFocusedChild(nsIAccessible **aFocusedChild) 
{ 
  nsCOMPtr<nsIAccessible> focusedChild;
  if (gLastFocusedNode == mDOMNode) {
    focusedChild = this;
  }
  else if (gLastFocusedNode) {
    nsCOMPtr<nsIAccessibilityService> accService =
      do_GetService("@mozilla.org/accessibilityService;1");
    NS_ENSURE_TRUE(accService, NS_ERROR_FAILURE);
    accService->GetAccessibleInWeakShell(gLastFocusedNode, mWeakShell, 
                                         getter_AddRefs(focusedChild));
    if (focusedChild) {
      nsCOMPtr<nsIAccessible> focusedParentAccessible;
      focusedChild->GetParent(getter_AddRefs(focusedParentAccessible));
      if (focusedParentAccessible != this) {
        focusedChild = nsnull;
      }
    }
  }

  NS_IF_ADDREF(*aFocusedChild = focusedChild);
  return NS_OK;
}

  /* nsIAccessible getChildAtPoint (in long x, in long y); */
NS_IMETHODIMP nsAccessible::GetChildAtPoint(PRInt32 tx, PRInt32 ty, nsIAccessible **aAccessible)
{
  *aAccessible = nsnull;
  PRInt32 numChildren; // Make sure all children cached first
  GetChildCount(&numChildren);

  nsCOMPtr<nsIAccessible> child;
  GetFirstChild(getter_AddRefs(child));

  PRInt32 x, y, w, h;
  PRUint32 state;

  while (child) {
    child->GetBounds(&x, &y, &w, &h);
    if (tx >= x && tx < x + w && ty >= y && ty < y + h) {
      child->GetFinalState(&state);
      if ((state & (STATE_OFFSCREEN|STATE_INVISIBLE)) == 0) {   // Don't walk into offscreen items
        NS_ADDREF(*aAccessible = child);
        return NS_OK;
      }
    }
    nsCOMPtr<nsIAccessible> next;
    child->GetNextSibling(getter_AddRefs(next));
    child = next;
  }

  GetState(&state);
  GetBounds(&x, &y, &w, &h);
  if ((state & (STATE_OFFSCREEN|STATE_INVISIBLE)) == 0 &&
      tx >= x && tx < x + w && ty >= y && ty < y + h) {
    *aAccessible = this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

void nsAccessible::GetScreenOrigin(nsPresContext *aPresContext, nsIFrame *aFrame, nsRect *aRect)
{
  aRect->x = aRect->y = 0;

  if (!aPresContext) {
    return;
  }

  nsPoint origin(0,0);
  nsIView *view = aFrame->GetViewExternal();
  if (!view) {
    aFrame->GetOffsetFromView(origin, &view);
    NS_ASSERTION(view, "Frame has no view");
  }

  nsPoint viewOrigin(0,0);
  nsIWidget *widget = view->GetNearestWidget(&viewOrigin);
  origin += viewOrigin;

  // Get the scale from that Presentation Context
  float t2p = aPresContext->TwipsToPixels();

  // Convert to pixels using that scale
  origin.x = NSTwipsToIntPixels(origin.x, t2p);
  origin.y = NSTwipsToIntPixels(origin.y, t2p);
  
  // Add the widget's screen coordinates to the offset we've counted
  NS_ASSERTION(widget, "No widget for top view");
  widget->WidgetToScreen(nsRect(origin.x, origin.y, 1, 1), *aRect);
}

void nsAccessible::GetBoundsRect(nsRect& aTotalBounds, nsIFrame** aBoundingFrame)
{
/*
 * This method is used to determine the bounds of a content node.
 * Because HTML wraps and links are not always rectangular, this
 * method uses the following algorithm:
 *
 * 1) Start with an empty rectangle
 * 2) Add the rect for the primary frame from for the DOM node.
 * 3) For each next frame at the same depth with the same DOM node, add that rect to total
 * 4) If that frame is an inline frame, search deeper at that point in the tree, adding all rects
 */

  // Initialization area
  *aBoundingFrame = nsnull;
  nsIFrame *firstFrame = GetBoundsFrame();
  if (!firstFrame)
    return;

  // Find common relative parent
  // This is an ancestor frame that will incompass all frames for this content node.
  // We need the relative parent so we can get absolute screen coordinates
  nsIFrame *ancestorFrame = firstFrame;

  while (ancestorFrame) {  
    *aBoundingFrame = ancestorFrame;
    // If any other frame type, we only need to deal with the primary frame
    // Otherwise, there may be more frames attached to the same content node
    if (!IsCorrectFrameType(ancestorFrame, nsAccessibilityAtoms::inlineFrame) &&
        !IsCorrectFrameType(ancestorFrame, nsAccessibilityAtoms::textFrame))
      break;
    ancestorFrame = ancestorFrame->GetParent();
  }

  nsIFrame *iterFrame = firstFrame;
  nsCOMPtr<nsIContent> firstContent(do_QueryInterface(mDOMNode));
  nsIContent* iterContent = firstContent;
  PRInt32 depth = 0;

  // Look only at frames below this depth, or at this depth (if we're still on the content node we started with)
  while (iterContent == firstContent || depth > 0) {
    // Coordinates will come back relative to parent frame
    nsRect currFrameBounds = iterFrame->GetRect();
    
    // Make this frame's bounds relative to common parent frame
    currFrameBounds +=
      iterFrame->GetParent()->GetOffsetToExternal(*aBoundingFrame);

    // Add this frame's bounds to total
    aTotalBounds.UnionRect(aTotalBounds, currFrameBounds);

    nsIFrame *iterNextFrame = nsnull;

    if (IsCorrectFrameType(iterFrame, nsAccessibilityAtoms::inlineFrame)) {
      // Only do deeper bounds search if we're on an inline frame
      // Inline frames can contain larger frames inside of them
      iterNextFrame = iterFrame->GetFirstChild(nsnull);
    }

    if (iterNextFrame) 
      ++depth;  // Child was found in code above this: We are going deeper in this iteration of the loop
    else {  
      // Use next sibling if it exists, or go back up the tree to get the first next-in-flow or next-sibling 
      // within our search
      while (iterFrame) {
        iterNextFrame = iterFrame->GetNextInFlow();
        if (!iterNextFrame)
          iterNextFrame = iterFrame->GetNextSibling();
        if (iterNextFrame || --depth < 0) 
          break;
        iterFrame = iterFrame->GetParent();
      }
    }

    // Get ready for the next round of our loop
    iterFrame = iterNextFrame;
    if (iterFrame == nsnull)
      break;
    iterContent = nsnull;
    if (depth == 0)
      iterContent = iterFrame->GetContent();
  }
}


/* void getBounds (out long x, out long y, out long width, out long height); */
NS_IMETHODIMP nsAccessible::GetBounds(PRInt32 *x, PRInt32 *y, PRInt32 *width, PRInt32 *height)
{
  // This routine will get the entire rectange for all the frames in this node
  // -------------------------------------------------------------------------
  //      Primary Frame for node
  //  Another frame, same node                <- Example
  //  Another frame, same node

  nsPresContext *presContext = GetPresContext();
  if (!presContext)
  {
    *x = *y = *width = *height = 0;
    return NS_ERROR_FAILURE;
  }

  float t2p;
  t2p = presContext->TwipsToPixels();   // Get pixels to twips conversion factor

  nsRect unionRectTwips;
  nsIFrame* aBoundingFrame = nsnull;
  GetBoundsRect(unionRectTwips, &aBoundingFrame);   // Unions up all primary frames for this node and all siblings after it
  if (!aBoundingFrame) {
    *x = *y = *width = *height = 0;
    return NS_ERROR_FAILURE;
  }

  *x      = NSTwipsToIntPixels(unionRectTwips.x, t2p); 
  *y      = NSTwipsToIntPixels(unionRectTwips.y, t2p);
  *width  = NSTwipsToIntPixels(unionRectTwips.width, t2p);
  *height = NSTwipsToIntPixels(unionRectTwips.height, t2p);

  // We have the union of the rectangle, now we need to put it in absolute screen coords

  nsRect orgRectPixels, pageRectPixels;
  GetScreenOrigin(presContext, aBoundingFrame, &orgRectPixels);
  *x += orgRectPixels.x;
  *y += orgRectPixels.y;

  return NS_OK;
}

// helpers

/**
  * Static
  * Helper method to help sub classes make sure they have the proper
  *     frame when walking the frame tree to get at children and such
  */
PRBool nsAccessible::IsCorrectFrameType( nsIFrame* aFrame, nsIAtom* aAtom ) 
{
  NS_ASSERTION(aFrame != nsnull, "aFrame is null in call to IsCorrectFrameType!");
  NS_ASSERTION(aAtom != nsnull, "aAtom is null in call to IsCorrectFrameType!");

  return aFrame->GetType() == aAtom;
}


nsIFrame* nsAccessible::GetBoundsFrame()
{
  return GetFrame();
}

/* void removeSelection (); */
NS_IMETHODIMP nsAccessible::RemoveSelection()
{
  nsCOMPtr<nsISelectionController> control(do_QueryReferent(mWeakShell));
  if (!control) {
     return NS_ERROR_FAILURE;  
  }

  nsCOMPtr<nsISelection> selection;
  nsresult rv = control->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(selection));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIDOMNode> parent;
  rv = mDOMNode->GetParentNode(getter_AddRefs(parent));
  if (NS_FAILED(rv))
    return rv;

  rv = selection->Collapse(parent, 0);
  if (NS_FAILED(rv))
    return rv;

  return NS_OK;
}

/* void takeSelection (); */
NS_IMETHODIMP nsAccessible::TakeSelection()
{
  nsCOMPtr<nsISelectionController> control(do_QueryReferent(mWeakShell));
  if (!control)
    return NS_ERROR_FAILURE;  
 
  nsCOMPtr<nsISelection> selection;
  nsresult rv = control->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(selection));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIDOMNode> parent;
  rv = mDOMNode->GetParentNode(getter_AddRefs(parent));
  if (NS_FAILED(rv))
    return rv;

  PRInt32 offsetInParent = 0;
  nsCOMPtr<nsIDOMNode> child;
  rv = parent->GetFirstChild(getter_AddRefs(child));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIDOMNode> next; 

  while(child)
  {
    if (child == mDOMNode) {
      // Collapse selection to just before desired element,
      rv = selection->Collapse(parent, offsetInParent);
      if (NS_FAILED(rv))
        return rv;

      // then extend it to just after
      rv = selection->Extend(parent, offsetInParent+1);
      return rv;
    }

     child->GetNextSibling(getter_AddRefs(next));
     child = next;
     offsetInParent++;
  }

  // didn't find a child
  return NS_ERROR_FAILURE;
}

/* void takeFocus (); */
NS_IMETHODIMP nsAccessible::TakeFocus()
{ 
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  if (!content) {
    return NS_ERROR_FAILURE;
  }
  content->SetFocus(GetPresContext());

  return NS_OK;
}

nsresult nsAccessible::AppendStringWithSpaces(nsAString *aFlatString, const nsAString& textEquivalent)
{
  // Insert spaces to insure that words from controls aren't jammed together
  if (!textEquivalent.IsEmpty()) {
    if (!aFlatString->IsEmpty())
      aFlatString->Append(PRUnichar(' '));
    aFlatString->Append(textEquivalent);
    aFlatString->Append(PRUnichar(' '));
  }
  return NS_OK;
}

nsresult nsAccessible::AppendNameFromAccessibleFor(nsIContent *aContent, nsAString *aFlatString)
{
  nsAutoString textEquivalent;

  nsCOMPtr<nsIDOMNode> domNode(do_QueryInterface(aContent));
  nsCOMPtr<nsIAccessibilityService> accService =
    do_GetService("@mozilla.org/accessibilityService;1");
  NS_ENSURE_TRUE(accService, NS_ERROR_FAILURE);
  nsCOMPtr<nsIAccessible> accessible;
  accService->GetAccessibleInWeakShell(domNode, mWeakShell, getter_AddRefs(accessible));
  if (accessible) {
    accessible->GetName(textEquivalent);
  }

  textEquivalent.CompressWhitespace();
  return AppendStringWithSpaces(aFlatString, textEquivalent);
}

/*
 * AppendFlatStringFromContentNode and AppendFlatStringFromSubtree
 *
 * This method will glean useful text, in whatever form it exists, from any content node given to it.
 * It is used by any decendant of nsAccessible that needs to get text from a single node, as
 * well as by nsAccessible::AppendFlatStringFromSubtree, which gleans and concatenates text from any node and
 * that node's decendants.
 */

nsresult nsAccessible::AppendFlatStringFromContentNode(nsIContent *aContent, nsAString *aFlatString)
{
  if (aContent->IsContentOfType(nsIContent::eTEXT)) {
    nsCOMPtr<nsITextContent> textContent(do_QueryInterface(aContent));
    NS_ASSERTION(textContent, "No text content for text content type");
    // If it's a text node, append the text
    PRBool isHTMLBlock = PR_FALSE;
    nsCOMPtr<nsIPresShell> shell = GetPresShell();
    if (!shell) {
      return NS_ERROR_FAILURE;  
    }

    nsIContent *parentContent = aContent->GetParent();
    nsCOMPtr<nsIContent> appendedSubtreeStart(do_QueryInterface(mDOMNode));
    if (parentContent && parentContent != appendedSubtreeStart) {
      nsIFrame *frame;
      shell->GetPrimaryFrameFor(parentContent, &frame);
      if (frame) {
        // If this text is inside a block level frame (as opposed to span level), we need to add spaces around that 
        // block's text, so we don't get words jammed together in final name
        // Extra spaces will be trimmed out later
        const nsStyleDisplay* display = frame->GetStyleDisplay();
        if (display->IsBlockLevel() ||
          display->mDisplay == NS_STYLE_DISPLAY_TABLE_CELL) {
          isHTMLBlock = PR_TRUE;
          if (!aFlatString->IsEmpty()) {
            aFlatString->Append(PRUnichar(' '));
          }
        }
      }
    }
    if (textContent->TextLength() > 0) {
      nsAutoString text;
      textContent->AppendTextTo(text);
      text.CompressWhitespace();
      if (!text.IsEmpty())
        aFlatString->Append(text);
      if (isHTMLBlock && !aFlatString->IsEmpty())
        aFlatString->Append(PRUnichar(' '));
    }
    return NS_OK;
  }

  nsAutoString textEquivalent;
  if (!aContent->IsContentOfType(nsIContent::eHTML)) {
    if (aContent->IsContentOfType(nsIContent::eXUL)) {
      aContent->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::tooltiptext, textEquivalent);
      textEquivalent.CompressWhitespace();
      return AppendStringWithSpaces(aFlatString, textEquivalent);
    }
    return NS_OK; // Not HTML and not XUL -- we don't handle it yet
  }

  nsCOMPtr<nsIAtom> tag = aContent->Tag();
  if (tag == nsAccessibilityAtoms::img) {
    return AppendNameFromAccessibleFor(aContent, aFlatString);
  }

  if (tag == nsAccessibilityAtoms::input) {
    nsAutoString inputType;
    aContent->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::type, inputType);
    if (inputType.LowerCaseEqualsLiteral("button") ||
        inputType.LowerCaseEqualsLiteral("submit") ||
        inputType.LowerCaseEqualsLiteral("reset") ||
        inputType.LowerCaseEqualsLiteral("image")) {
      return AppendNameFromAccessibleFor(aContent, aFlatString);
    }
  }

  if (tag == nsAccessibilityAtoms::object && !aContent->GetChildCount()) {
    // If object has no alternative content children, try title
    aContent->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::title, textEquivalent);
  }
  else if (tag == nsAccessibilityAtoms::br) {
    // If it's a line break, insert a space so that words aren't jammed together
    aFlatString->AppendLiteral("\r\n");
    return NS_OK;
  }

  textEquivalent.CompressWhitespace();
  return AppendStringWithSpaces(aFlatString, textEquivalent);
}


nsresult nsAccessible::AppendFlatStringFromSubtree(nsIContent *aContent, nsAString *aFlatString)
{
  nsresult rv = AppendFlatStringFromSubtreeRecurse(aContent, aFlatString);
  if (NS_SUCCEEDED(rv) && !aFlatString->IsEmpty()) {
    nsAString::const_iterator start, end;
    aFlatString->BeginReading(start);
    aFlatString->EndReading(end);

    PRInt32 spacesToTruncate = 0;
    while (-- end != start && *end == ' ')
      ++ spacesToTruncate;

    if (spacesToTruncate > 0)
      aFlatString->Truncate(aFlatString->Length() - spacesToTruncate);
  }

  return rv;
}

nsresult nsAccessible::AppendFlatStringFromSubtreeRecurse(nsIContent *aContent, nsAString *aFlatString)
{
  // Depth first search for all text nodes that are decendants of content node.
  // Append all the text into one flat string

  PRUint32 numChildren = aContent->GetChildCount();

  if (numChildren == 0) {
    AppendFlatStringFromContentNode(aContent, aFlatString);
    return NS_OK;
  }

  PRUint32 index;
  for (index = 0; index < numChildren; index++) {
    AppendFlatStringFromSubtree(aContent->GetChildAt(index), aFlatString);
  }
  return NS_OK;
}


nsIContent* nsAccessible::GetXULLabelContent(nsIContent *aForNode)
{
  nsAutoString controlID;
  nsIContent *labelContent = GetLabelForId(aForNode, nsnull, &controlID);
  if (labelContent) {
    return labelContent;
  }

  // If we're in anonymous content, determine whether we should use
  // the binding parent based on where the id for this control is
  aForNode->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::id, controlID);
  if (controlID.IsEmpty()) {
    // If no control ID and we're anonymous content
    // get ID from parent that inserted us.
    aForNode = aForNode->GetBindingParent();
    if (aForNode) {
      aForNode->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::id, controlID);
    }
    if (controlID.IsEmpty()) {
      return nsnull;
    }
  }
  
  // Look for child label of control
  static const PRUint32 kAncestorLevelsToSearch = 3;
  PRUint32 count = 0;
  while (!labelContent && ++count <= kAncestorLevelsToSearch && 
         (aForNode = aForNode->GetParent()) != nsnull) {
    labelContent = GetLabelForId(aForNode, nsAccessibilityAtoms::control, &controlID);
  }

  return labelContent;
}

nsIContent* nsAccessible::GetHTMLLabelContent(nsIContent *aForNode)
{
  nsIContent *walkUpContent = aForNode;

  // go up tree get name of ancestor label if there is one. Don't go up farther than form element
  while ((walkUpContent = walkUpContent->GetParent()) != nsnull) {
    nsIAtom *tag = walkUpContent->Tag();
    if (tag == nsAccessibilityAtoms::label) {
      return walkUpContent;
    }
    if (tag == nsAccessibilityAtoms::form) {
      // Reached top ancestor in form
      // There can be a label targeted at this control using the 
      // for="control_id" attribute. To save computing time, only 
      // look for those inside of a form element
      nsAutoString forId;
      aForNode->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::id, forId);
      // Actually we'll be walking down the content this time, with a depth first search
      if (forId.IsEmpty()) {
        break;
      }
      return GetLabelForId(walkUpContent, nsAccessibilityAtoms::_for, &forId); 
    }
  }

  return nsnull;
}

// Pass in forAttrib == nsnull if any <label> will do
nsIContent *nsAccessible::GetLabelForId(nsIContent *aLookContent,
                                        nsIAtom *forAttrib,
                                        const nsAString *aId)
{
  if (aLookContent->Tag() == nsAccessibilityAtoms::label) {
    if (forAttrib) {
      nsAutoString labelIsFor;
      aLookContent->GetAttr(kNameSpaceID_None, forAttrib, labelIsFor);
      if (labelIsFor.Equals(*aId)) {
        return aLookContent;
      }
    }
    return nsnull;
  }

  // Recursively search descendents for labels
  PRUint32 count  = 0;
  nsIContent *child;

  while ((child = aLookContent->GetChildAt(count++)) != nsnull) {
    nsIContent *labelContent = GetLabelForId(child, forAttrib, aId);
    if (labelContent) {
      return labelContent;
    }
  }
  return nsnull;
}

/**
  * Only called if the element is not a nsIDOMXULControlElement. Initially walks up
  *   the DOM tree to the form, concatonating label elements as it goes. Then checks for
  *   labels with the for="controlID" property.
  */
nsresult nsAccessible::GetHTMLName(nsAString& aLabel, PRBool aCanAggregateSubtree)
{
  if (!mWeakShell || !mDOMNode) {
    return NS_ERROR_FAILURE;   // Node shut down
  }

  nsIContent *labelContent;
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  if ((labelContent = GetHTMLLabelContent(content)) != nsnull) {
    nsAutoString label;
    AppendFlatStringFromSubtree(labelContent, &label);
    label.CompressWhitespace();
    if (!label.IsEmpty()) {
      aLabel = label;
      return NS_OK;
    }
  }

  if (!aCanAggregateSubtree) {
    // Don't use AppendFlatStringFromSubtree for container widgets like menulist
    // Still try the title as as fallback method in that case.
    if (NS_CONTENT_ATTR_NOT_THERE ==
        content->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::title, aLabel)) {
      aLabel.SetIsVoid(PR_TRUE);
    }
    return NS_OK;
  }

  return nsAccessible::GetName(aLabel);
}

/**
  * 3 main cases for XUL Controls to be labeled
  *   1 - control contains label="foo"
  *   2 - control has, as a child, a label element
  *        - label has either value="foo" or children
  *   3 - non-child label contains control="controlID"
  *        - label has either value="foo" or children
  * Once a label is found, the search is discontinued, so a control
  *  that has a label child as well as having a label external to
  *  the control that uses the control="controlID" syntax will use
  *  the child label for its Name.
  */
nsresult nsAccessible::GetXULName(nsAString& aLabel, PRBool aCanAggregateSubtree)
{
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  NS_ASSERTION(content, "No nsIContent for DOM node");

  nsresult rv = NS_OK;
  nsAutoString label;

  // CASE #1 (via label attribute) -- great majority of the cases
  nsCOMPtr<nsIDOMXULLabeledControlElement> labeledEl(do_QueryInterface(mDOMNode));
  if (labeledEl) {
    rv = labeledEl->GetLabel(label);
  }
  else {
    nsCOMPtr<nsIDOMXULSelectControlItemElement> itemEl(do_QueryInterface(mDOMNode));
    if (itemEl) {
      rv = itemEl->GetLabel(label);
    }
    else {
      nsCOMPtr<nsIDOMXULElement> xulEl(do_QueryInterface(mDOMNode));
      if (xulEl) {
        rv = xulEl->GetAttribute(NS_LITERAL_STRING("label"), label);
      }
    }
  }

  // CASES #2 and #3 ------ label as a child or <label control="id" ... > </label>
  if (NS_FAILED(rv) || label.IsEmpty()) {
    label.Truncate();
    nsIContent *labelContent = GetXULLabelContent(content);
    nsCOMPtr<nsIDOMXULLabelElement> xulLabel(do_QueryInterface(labelContent));
    // Check if label's value attribute is used
    if (xulLabel && NS_SUCCEEDED(xulLabel->GetValue(label)) && label.IsEmpty()) {
      // If no value attribute, a non-empty label must contain
      // children that define it's text -- possibly using HTML
      AppendFlatStringFromSubtree(labelContent, &label);
    }
  }

  // XXX If CompressWhiteSpace worked on nsAString we could avoid a copy
  label.CompressWhitespace();
  if (!label.IsEmpty()) {
    aLabel = label;
    return NS_OK;
  }

  content->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::tooltiptext, label);
  label.CompressWhitespace();
  if (!label.IsEmpty()) {
    aLabel = label;
    return NS_OK;
  }

  // Can get text from title of <toolbaritem> if we're a child of a <toolbaritem>
  nsIContent *bindingParent = content->GetBindingParent();
  nsIContent *parent = bindingParent? bindingParent->GetParent() :
                                      content->GetParent();
  if (parent && parent->Tag() == nsAccessibilityAtoms::toolbaritem &&
      NS_CONTENT_ATTR_HAS_VALUE == parent->GetAttr(kNameSpaceID_None,
                                                   nsAccessibilityAtoms::title,
                                                   label)) {
    label.CompressWhitespace();
    aLabel = label;
    return NS_OK;
  }

  // Don't use AppendFlatStringFromSubtree for container widgets like menulist
  return aCanAggregateSubtree? AppendFlatStringFromSubtree(content, &aLabel) : NS_OK;
}

NS_IMETHODIMP nsAccessible::FireToolkitEvent(PRUint32 aEvent, nsIAccessible *aTarget, void * aData)
{
  if (!mWeakShell)
    return NS_ERROR_FAILURE; // Don't fire event for accessible that has been shut down
  nsCOMPtr<nsIAccessibleDocument> docAccessible(GetDocAccessible());
  nsCOMPtr<nsPIAccessible> eventHandlingAccessible(do_QueryInterface(docAccessible));
  if (eventHandlingAccessible) {
    return eventHandlingAccessible->FireToolkitEvent(aEvent, aTarget, aData);
  }

  return NS_ERROR_FAILURE;
}

nsRoleMapEntry nsAccessible::gWAIRoleMap[] = 
{
  // This list of WAI-defined roles are currently hardcoded.
  // Eventually we will most likely be loading an RDF resource that contains this information
  // Using RDF will also allow for role extensibility. See bug 280138.
  // XXX Should we store attribute names in this table as atoms instead of strings?
  // Definition of nsRoleMapEntry and nsStateMapEntry contains comments explaining this table.
  {"alert", ROLE_ALERT, eNameFromSubtree, eNoValue, eNoReqStates, END_ENTRY},
  {"application", ROLE_APPLICATION, eNoName, eNoValue, eNoReqStates, END_ENTRY},
  {"button", ROLE_PUSHBUTTON, eNameFromSubtree, eNoValue, eNoReqStates,
            {"pressed", BOOL_STATE, STATE_PRESSED},
            {"haspopup", BOOL_STATE, STATE_HASPOPUP}, END_ENTRY},
  {"button-submit", ROLE_PUSHBUTTON, eNameFromSubtree, eNoValue, STATE_DEFAULT, END_ENTRY},
  {"checkbox", ROLE_CHECKBUTTON, eNameFromSubtree, eNoValue, eNoReqStates,
            {"checked", BOOL_STATE, STATE_CHECKED},
            {"readonly", BOOL_STATE, STATE_READONLY}, END_ENTRY},
  {"checkbox-tristate", ROLE_CHECKBUTTON, eNameFromSubtree, eNoValue, eNoReqStates, 
            {"checked", BOOL_STATE, STATE_CHECKED},
            {"checked", "mixed", STATE_MIXED},
            {"readonly", BOOL_STATE, STATE_READONLY},
            {"invalid", BOOL_STATE, STATE_INVALID},
            {"required", BOOL_STATE, STATE_REQUIRED}},
  {"columnheader", ROLE_COLUMNHEADER, eNameFromSubtree, eNoValue, STATE_SELECTABLE,
            {"selected", BOOL_STATE, STATE_SELECTED},
            {"readonly", BOOL_STATE, STATE_READONLY}, END_ENTRY},
  {"combobox", ROLE_COMBOBOX, eNameFromTitle, eNoValue, eNoReqStates,
            {"readonly", BOOL_STATE, STATE_READONLY}, 
            {"multiselect", BOOL_STATE, STATE_MULTISELECTABLE | STATE_EXTSELECTABLE}, END_ENTRY},
  {"dialog", ROLE_DIALOG, eNoName, eNoValue, eNoReqStates, END_ENTRY},
  {"document", ROLE_DOCUMENT, eNoName, eNoValue, eNoReqStates, END_ENTRY},
  {"icon", ROLE_ICON, eNameFromSubtree, eNoValue, eNoReqStates, END_ENTRY},
  {"list", ROLE_LIST, eNameFromTitle, eNoValue, eNoReqStates,
            {"readonly", BOOL_STATE, STATE_READONLY},
            {"multiselect", BOOL_STATE, STATE_MULTISELECTABLE | STATE_EXTSELECTABLE}, END_ENTRY},
  {"listitem", ROLE_LISTITEM, eNameFromSubtree, eNoValue, STATE_SELECTABLE,
            {"checked", BOOL_STATE, STATE_CHECKED}, END_ENTRY },
  {"menu", ROLE_MENUPOPUP, eNameFromTitle, eNoValue, eNoReqStates, END_ENTRY},
  {"menubar", ROLE_MENUBAR, eNameFromTitle, eNoValue, eNoReqStates, END_ENTRY},
  {"menuitem", ROLE_MENUITEM, eNameFromSubtree, eNoValue, eNoReqStates, END_ENTRY},
  {"menuitem-radio", ROLE_MENUITEM, eNameFromSubtree, eNoValue, eNoReqStates,
            {"checked", BOOL_STATE, STATE_CHECKED}, END_ENTRY},
  {"menuitem-checkbox", ROLE_MENUITEM, eNameFromSubtree, eNoValue, eNoReqStates,
            {"checked", BOOL_STATE, STATE_CHECKED}, END_ENTRY},
  {"grid", ROLE_TABLE, eNameFromTitle, eNoValue, STATE_FOCUSABLE,
            {"readonly", BOOL_STATE, STATE_READONLY}, END_ENTRY},
  {"gridcell", ROLE_CELL, eNameFromSubtree, eHasValueMinMax, STATE_SELECTABLE,
            {"selected", BOOL_STATE, STATE_SELECTED},
            {"readonly", BOOL_STATE, STATE_READONLY}, 
            {"invalid", BOOL_STATE, STATE_INVALID},
            {"required", BOOL_STATE, STATE_REQUIRED}, END_ENTRY},
  {"group", ROLE_GROUPING, eNameFromTitle, eNoValue, eNoReqStates, END_ENTRY},
  {"link", ROLE_LINK, eNameFromTitle, eNoValue, STATE_LINKED, END_ENTRY},
  {"progressbar", ROLE_PROGRESSBAR, eNameFromTitle, eHasValueMinMax, STATE_READONLY,
            {"valuenow", "unknown", STATE_MIXED}, END_ENTRY},
  {"radio", ROLE_RADIOBUTTON, eNameFromSubtree, eNoValue, eNoReqStates, 
            {"checked", BOOL_STATE, STATE_CHECKED}, END_ENTRY},
  {"rowheader", ROLE_ROWHEADER, eNameFromSubtree, eNoValue, STATE_SELECTABLE,
            {"selected", BOOL_STATE, STATE_SELECTED},
            {"readonly", BOOL_STATE, STATE_READONLY}, END_ENTRY},
  {"secret", ROLE_PASSWORD_TEXT, eNameFromTitle, eNoValue, STATE_PROTECTED,
            {"invalid", BOOL_STATE, STATE_INVALID},
            {"required", BOOL_STATE, STATE_REQUIRED}, END_ENTRY}, // XXX EXT_STATE_SINGLE_LINE
  {"separator", ROLE_SEPARATOR, eNameFromTitle, eNoValue, eNoReqStates, END_ENTRY},
  {"slider", ROLE_SLIDER, eNameFromTitle, eHasValueMinMax, eNoReqStates,
            {"readonly", BOOL_STATE, STATE_READONLY},
            {"invalid", BOOL_STATE, STATE_INVALID},
            {"required", BOOL_STATE, STATE_REQUIRED}, END_ENTRY},
  {"spinbutton", ROLE_SPINBUTTON, eNameFromTitle, eHasValueMinMax, eNoReqStates, 
            {"readonly", BOOL_STATE, STATE_READONLY},
            {"invalid", BOOL_STATE, STATE_INVALID},
            {"required", BOOL_STATE, STATE_REQUIRED}, END_ENTRY},
  {"spreadsheet", ROLE_TABLE, eNameFromTitle, eNoValue, STATE_MULTISELECTABLE | STATE_EXTSELECTABLE | STATE_FOCUSABLE,
            {"readonly", BOOL_STATE, STATE_READONLY}, END_ENTRY},
  {"tab", ROLE_PAGETAB, eNameFromSubtree, eNoValue, eNoReqStates,  END_ENTRY},
  {"tablist", ROLE_PAGETABLIST, eNameFromSubtree, eNoValue, eNoReqStates,  END_ENTRY},
  {"tabpanel", ROLE_PROPERTYPAGE, eNameFromSubtree, eNoValue, eNoReqStates,  END_ENTRY},
  {"textarea", ROLE_TEXT, eNameFromTitle, eHasValueMinMax, eNoReqStates,
            {"readonly", BOOL_STATE, STATE_READONLY},
            {"invalid", BOOL_STATE, STATE_INVALID},
            {"required", BOOL_STATE, STATE_REQUIRED}, END_ENTRY}, // XXX EXT_STATE_MULTI_LINE
  {"textfield", ROLE_TEXT, eNameFromTitle, eHasValueMinMax, eNoReqStates,
            {"readonly", BOOL_STATE, STATE_READONLY},
            {"invalid", BOOL_STATE, STATE_INVALID},
            {"required", BOOL_STATE, STATE_REQUIRED},
            {"haspopup", BOOL_STATE, STATE_HASPOPUP}, END_ENTRY}, // XXX EXT_STATE_SINGLE_LINE
  {"toolbar", ROLE_TOOLBAR, eNoName, eNoValue, eNoReqStates, END_ENTRY},
  {"tree", ROLE_OUTLINE, eNameFromTitle, eNoValue, eNoReqStates,
            {"readonly", BOOL_STATE, STATE_READONLY},
            {"multiselectable", BOOL_STATE, STATE_MULTISELECTABLE | STATE_EXTSELECTABLE}, END_ENTRY},
  {"treeitem", ROLE_OUTLINEITEM, eNameFromSubtree, eNoValue, STATE_SELECTABLE,
            {"selected", BOOL_STATE, STATE_SELECTED},
            {"expanded", BOOL_STATE, STATE_EXPANDED},
            {"expanded", "false", STATE_COLLAPSED},
            {"checked", BOOL_STATE, STATE_CHECKED}, END_ENTRY},
  {nsnull, ROLE_NOTHING, eNoName, eNoValue, eNoReqStates, END_ENTRY} // Last item
};

// XHTML 2 roles
// These don't need a mapping - they are exposed either through DOM or via MSAA role string
// banner, contentinfo, main, navigation, note, search, secondary, seealso

nsStateMapEntry nsAccessible::gDisabledStateMap = {"disabled", BOOL_STATE, STATE_UNAVAILABLE};

NS_IMETHODIMP nsAccessible::GetFinalRole(PRUint32 *aRole)
{
  if (!mDOMNode) {
    return NS_ERROR_FAILURE;  // Node already shut down
  }
  if (mRoleMapEntry) {
    *aRole = mRoleMapEntry->role;
    if (*aRole != ROLE_NOTHING) {
      return NS_OK;
    }
  }
  return GetRole(aRole);
}

PRBool nsAccessible::MappedAttrState(nsIContent *aContent, PRUint32 *aStateInOut,
                                     nsStateMapEntry *aStateMapEntry)
{
  // Return true if we should continue
  if (!aStateMapEntry->attributeName) {
    return PR_FALSE;  // Stop looking -- no more states
  }

  nsAutoString attribValue;
  nsCOMPtr<nsIAtom> attribAtom = do_GetAtom(aStateMapEntry->attributeName); // XXX put atoms directly in entry
  if (NS_CONTENT_ATTR_HAS_VALUE == aContent->GetAttr(kNameSpaceID_StatesWAI_Unofficial,
                                                     attribAtom,
                                                     attribValue)) {
    if (aStateMapEntry->attributeValue == BOOL_STATE) {
      // No attribute value map specified in state map entry indicates state cleared
      if (attribValue.EqualsLiteral("false")) {
        return *aStateInOut &= ~aStateMapEntry->state;
      }
      return *aStateInOut |= aStateMapEntry->state;
    }
    if (NS_ConvertUTF16toUTF8(attribValue).Equals(aStateMapEntry->attributeValue)) {
      return *aStateInOut |= aStateMapEntry->state;
    }
  }

  return PR_TRUE;
}

NS_IMETHODIMP nsAccessible::GetFinalState(PRUint32 *aState)
{
  if (!mDOMNode) {
    return NS_ERROR_FAILURE;  // Node already shut down
  }
  nsresult rv = GetState(aState);
  if (NS_FAILED(rv) || !mRoleMapEntry) {
    return rv;
  }

  PRUint32 finalState = *aState;
  finalState &= ~STATE_READONLY;  // Once DHTML role is used, we're only readonly if DHTML readonly used

  if (gLastFocusedNode == mDOMNode && (mRoleMapEntry->state & STATE_SELECTABLE)) {
    // If we're focused and selectable and not inside a multiselect,
    // then we're also selected
    nsCOMPtr<nsIAccessible> container = this;
    PRUint32 containerState = 0, containerRole;
    while (0 == (containerState & STATE_MULTISELECTABLE)) {
      nsCOMPtr<nsIAccessible> current;
      current.swap(container);
      current->GetParent(getter_AddRefs(container));      
      if (!container || (NS_SUCCEEDED(container->GetFinalRole(&containerRole)) &&
                         containerRole == ROLE_PANE)) {
        finalState |= STATE_SELECTED;
        break;
      }
      container->GetFinalState(&containerState);
    }
  }

  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  if (content) {
    finalState |= mRoleMapEntry->state;
    if (MappedAttrState(content, &finalState, &mRoleMapEntry->attributeMap1) &&
        MappedAttrState(content, &finalState, &mRoleMapEntry->attributeMap2) &&
        MappedAttrState(content, &finalState, &mRoleMapEntry->attributeMap3) &&
        MappedAttrState(content, &finalState, &mRoleMapEntry->attributeMap4)) {
      MappedAttrState(content, &finalState, &mRoleMapEntry->attributeMap5);
    }
    // Anything can be disabled/unavailable
    MappedAttrState(content, &finalState, &gDisabledStateMap);
  }

  *aState = finalState;
  return rv;
}

NS_IMETHODIMP nsAccessible::GetFinalValue(nsAString& aValue)
{
  if (!mDOMNode) {
    return NS_ERROR_FAILURE;  // Node already shut down
  }
  if (mRoleMapEntry) {
    if (mRoleMapEntry->valueRule == eNoValue) {
      return NS_OK;
    }
    nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
    if (content &&
        NS_CONTENT_ATTR_HAS_VALUE == content->GetAttr(kNameSpaceID_StatesWAI_Unofficial,
                                                      nsAccessibilityAtoms::valuenow,
                                                      aValue)) {
      return NS_OK;
    }
  }
  return GetValue(aValue);
}

// Not implemented by this class

/* DOMString getValue (); */
NS_IMETHODIMP nsAccessible::GetValue(nsAString& _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void setName (in DOMString name); */
NS_IMETHODIMP nsAccessible::SetName(const nsAString& name)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* DOMString getKeyBinding (); */
NS_IMETHODIMP nsAccessible::GetKeyBinding(nsAString& _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* unsigned long getRole (); */
NS_IMETHODIMP nsAccessible::GetRole(PRUint32 *aRole)
{
  *aRole = ROLE_NOTHING;
  return NS_OK;
}

/* PRUint8 getAccNumActions (); */
NS_IMETHODIMP nsAccessible::GetNumActions(PRUint8 *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* DOMString getAccActionName (in PRUint8 index); */
NS_IMETHODIMP nsAccessible::GetActionName(PRUint8 index, nsAString& _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void doAction (in PRUint8 index); */
NS_IMETHODIMP nsAccessible::DoAction(PRUint8 index)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* DOMString getHelp (); */
NS_IMETHODIMP nsAccessible::GetHelp(nsAString& _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIAccessible getAccessibleToRight(); */
NS_IMETHODIMP nsAccessible::GetAccessibleToRight(nsIAccessible **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIAccessible getAccessibleToLeft(); */
NS_IMETHODIMP nsAccessible::GetAccessibleToLeft(nsIAccessible **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIAccessible getAccessibleAbove(); */
NS_IMETHODIMP nsAccessible::GetAccessibleAbove(nsIAccessible **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIAccessible getAccessibleBelow(); */
NS_IMETHODIMP nsAccessible::GetAccessibleBelow(nsIAccessible **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void addSelection (); */
NS_IMETHODIMP nsAccessible::AddSelection()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void extendSelection (); */
NS_IMETHODIMP nsAccessible::ExtendSelection()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* unsigned long getExtState (); */
NS_IMETHODIMP nsAccessible::GetExtState(PRUint32 *aExtState)
{
  if (!mDOMNode) {
    return NS_ERROR_FAILURE; // Node shut down
  }
  *aExtState = 0;
  // XXX We can remove this hack once we support RDF-based role & state maps
  if (mRoleMapEntry && mRoleMapEntry->role == ROLE_TEXT) {
    *aExtState = NS_LITERAL_CSTRING("textarea").Equals(mRoleMapEntry->roleString) ? 
       EXT_STATE_MULTI_LINE : EXT_STATE_SINGLE_LINE;
  }
  return NS_OK;
}

/* [noscript] void getNativeInterface(out voidPtr aOutAccessible); */
NS_IMETHODIMP nsAccessible::GetNativeInterface(void **aOutAccessible)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

void nsAccessible::DoCommandCallback(nsITimer *aTimer, void *aClosure)
{
  NS_ASSERTION(gDoCommandTimer, "How did we get here if there was no gDoCommandTimer?");
  NS_RELEASE(gDoCommandTimer);
  gDoCommandTimer = nsnull;

  nsIDOMNode *node = NS_REINTERPRET_CAST(nsIDOMNode*, aClosure);
  nsCOMPtr<nsIDOMXULElement> xulElement(do_QueryInterface(node));
  if (xulElement) {
    xulElement->Click();
  }
  else {
    nsCOMPtr<nsIDOMHTMLInputElement> htmlElement(do_QueryInterface(node));
    if (htmlElement)
      htmlElement->Click();
  }
}

/*
 * Use Timer to execute "Click" command of XUL/HTML element (e.g. menuitem, button...).
 *
 * When "Click" is to open a "modal" dialog/window, it won't return untill the
 * dialog/window is closed. If executing "Click" command directly in
 * nsXXXAccessible::DoAction, it will block AT-Tools(e.g. GOK) that invoke
 * "action" of mozilla accessibles direclty.
 */
nsresult nsAccessible::DoCommand()

{
  if (gDoCommandTimer) {
    // Already have timer going for another command
    NS_WARNING("Doubling up on do command timers doesn't work. This wasn't expected.");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsITimer> timer = do_CreateInstance("@mozilla.org/timer;1");
  if (!timer) {
    return NS_ERROR_OUT_OF_MEMORY;
  } 

  NS_ADDREF(gDoCommandTimer = timer);
  return gDoCommandTimer->InitWithFuncCallback(DoCommandCallback,
                                               (void*)mDOMNode, 0,
                                               nsITimer::TYPE_ONE_SHOT);
}

#ifdef MOZ_ACCESSIBILITY_ATK
// static helper functions
nsresult nsAccessible::GetParentBlockNode(nsIPresShell *aPresShell, nsIDOMNode *aCurrentNode, nsIDOMNode **aBlockNode)
{
  *aBlockNode = nsnull;

  nsCOMPtr<nsIContent> content(do_QueryInterface(aCurrentNode));
  if (! content)
    return NS_ERROR_FAILURE;

  nsIFrame *frame = nsnull;
  aPresShell->GetPrimaryFrameFor(content, &frame);
  if (! frame)
    return NS_ERROR_FAILURE;

  nsIFrame *parentFrame = GetParentBlockFrame(frame);
  if (! parentFrame)
    return NS_ERROR_FAILURE;

  nsPresContext *presContext = aPresShell->GetPresContext();
  nsIAtom* frameType = nsnull;
  while (frame && (frameType = frame->GetType()) != nsAccessibilityAtoms::textFrame) {
    frame = frame->GetFirstChild(nsnull);
  }
  if (! frame || frameType != nsAccessibilityAtoms::textFrame)
    return NS_ERROR_FAILURE;

  PRInt32 index = 0;
  nsIFrame *firstTextFrame = nsnull;
  FindTextFrame(index, presContext, parentFrame->GetFirstChild(nsnull),
                &firstTextFrame, frame);
  if (firstTextFrame) {
    nsIContent *content = firstTextFrame->GetContent();

    if (content) {
      CallQueryInterface(content, aBlockNode);
    }

    return NS_OK;
  }
  else {
    //XXX, why?
  }
  return NS_ERROR_FAILURE;
}

nsIFrame* nsAccessible::GetParentBlockFrame(nsIFrame *aFrame)
{
  if (! aFrame)
    return nsnull;

  nsIFrame* frame = aFrame;
  while (frame && frame->GetType() != nsAccessibilityAtoms::blockFrame) {
    nsIFrame* parentFrame = frame->GetParent();
    frame = parentFrame;
  }
  return frame;
}

PRBool nsAccessible::FindTextFrame(PRInt32 &index, nsPresContext *aPresContext, nsIFrame *aCurFrame, 
                                   nsIFrame **aFirstTextFrame, const nsIFrame *aTextFrame)
// Do a depth-first traversal to find the given text frame(aTextFrame)'s index of the block frame(aCurFrame)
//   it belongs to, also return the first text frame within the same block.
// Parameters:
//   index        [in/out] - the current index - finally, it will be the aTextFrame's index in its block;
//   aCurFrame        [in] - the current frame - its initial value is the first child frame of the block frame;
//   aFirstTextFrame [out] - the first text frame which is within the same block with aTextFrame;
//   aTextFrame       [in] - the text frame we are looking for;
// Return:
//   PR_TRUE  - the aTextFrame was found in its block frame;
//   PR_FALSE - the aTextFrame was NOT found in its block frame;
{
  if (! aCurFrame)
    return PR_FALSE;

  if (aCurFrame == aTextFrame) {
    if (index == 0)
      *aFirstTextFrame = aCurFrame;
    // we got it, stop traversing
    return PR_TRUE;
  }

  nsIAtom* frameType = aCurFrame->GetType();
  if (frameType == nsAccessibilityAtoms::blockFrame) {
    // every block frame will reset the index
    index = 0;
  }
  else {
    if (frameType == nsAccessibilityAtoms::textFrame) {
      nsRect frameRect = aCurFrame->GetRect();
      // skip the empty frame
      if (! frameRect.IsEmpty()) {
        if (index == 0)
          *aFirstTextFrame = aCurFrame;
        index++;
      }
    }

    // we won't expand the tree under a block frame.
    if (FindTextFrame(index, aPresContext, aCurFrame->GetFirstChild(nsnull),
                      aFirstTextFrame, aTextFrame))
      return PR_TRUE;
  }

  nsIFrame* siblingFrame = aCurFrame->GetNextSibling();
  return FindTextFrame(index, aPresContext, siblingFrame, aFirstTextFrame, aTextFrame);
}
#endif //MOZ_ACCESSIBILITY_ATK
