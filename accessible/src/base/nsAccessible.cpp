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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *       John Gaunt (jgaunt@netscape.com)
 *       Aaron Leventhal (aaronl@netscape.com)
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

#include "nsAccessible.h"
#include "nsIAccessibleDocument.h"
#include "nsIDocument.h"
#include "nsIImageDocument.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsIScrollableView.h"
#include "nsIViewManager.h"
#include "nsIWidget.h"
#include "nsIDOMDocumentView.h"
#include "nsIDOMAbstractView.h"
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

#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLLabelElement.h"
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
  mParent(nsnull), mFirstChild(nsnull), mNextSibling(nsnull)
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

NS_IMETHODIMP nsAccessible::GetAccName(nsAString& _retval)
{
  nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(mDOMNode));
  if (elt) 
    return elt->GetAttribute(NS_LITERAL_STRING("title"), _retval);
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsAccessible::GetAccDescription(nsAString& aDescription)
{
  // There are 3 conditions that make an accessible have no accDescription:
  // 1. it's a text node; or
  // 2. it doesn't have an accName; or
  // 3. its title attribute equals to its accName nsAutoString name; 
  nsCOMPtr<nsITextContent> textContent(do_QueryInterface(mDOMNode));
  if (!textContent) {
    nsAutoString name;
    GetAccName(name);
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

NS_IMETHODIMP nsAccessible::GetAccKeyboardShortcut(nsAString& _retval)
{
  static PRInt32 gGeneralAccesskeyModifier = -1;  // magic value of -1 indicates unitialized state

  nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(mDOMNode));
  if (elt) {
    nsAutoString accesskey;
    elt->GetAttribute(NS_LITERAL_STRING("accesskey"), accesskey);
    if (accesskey.IsEmpty())
      return NS_OK;

    if (gGeneralAccesskeyModifier == -1) {
      // Need to initialize cached global accesskey pref
      gGeneralAccesskeyModifier = 0;
      nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID));
      if (prefBranch)
        prefBranch->GetIntPref("ui.key.generalAccessKey", &gGeneralAccesskeyModifier);
    }
    nsAutoString propertyKey;
    switch (gGeneralAccesskeyModifier) {
      case nsIDOMKeyEvent::DOM_VK_CONTROL: propertyKey = NS_LITERAL_STRING("VK_CONTROL"); break;
      case nsIDOMKeyEvent::DOM_VK_ALT: propertyKey = NS_LITERAL_STRING("VK_ALT"); break;
      case nsIDOMKeyEvent::DOM_VK_META: propertyKey = NS_LITERAL_STRING("VK_META"); break;
    }
    if (!propertyKey.IsEmpty())
      nsAccessible::GetFullKeyName(propertyKey, accesskey, _retval);
    else
      _retval= accesskey;
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsAccessible::SetAccParent(nsIAccessible *aParent)
{
  mParent = aParent;
  return NS_OK;
}

NS_IMETHODIMP nsAccessible::SetAccFirstChild(nsIAccessible *aFirstChild)
{
  mFirstChild = aFirstChild;
  return NS_OK;
}

NS_IMETHODIMP nsAccessible::SetAccNextSibling(nsIAccessible *aNextSibling)
{
  mNextSibling = aNextSibling? aNextSibling: DEAD_END_ACCESSIBLE;
  return NS_OK;
}

NS_IMETHODIMP nsAccessible::Shutdown()
{
  mNextSibling = nsnull;
  // Make sure none of it's children point to this parent
  if (mFirstChild) {
    nsCOMPtr<nsIAccessible> current(mFirstChild), next;
    while (current) {
      nsCOMPtr<nsPIAccessible> privateAcc(do_QueryInterface(current));
      privateAcc->SetAccParent(nsnull);
      current->GetAccNextSibling(getter_AddRefs(next));
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

NS_IMETHODIMP nsAccessible::GetAccParent(nsIAccessible **  aAccParent)
{
  if (!mWeakShell) {
    // This node has been shut down
    *aAccParent = nsnull;
    return NS_ERROR_FAILURE;
  }
  if (mParent) {
    *aAccParent = mParent;
    NS_ADDREF(*aAccParent);
    return NS_OK;
  }

  *aAccParent = nsnull;
  // Last argument of PR_TRUE indicates to walk anonymous content
  nsAccessibleTreeWalker walker(mWeakShell, mDOMNode, PR_TRUE); 
  if (NS_SUCCEEDED(walker.GetParent())) {
    *aAccParent = mParent = walker.mState.accessible;
    NS_ADDREF(*aAccParent);
  }

  return NS_OK;
}

  /* readonly attribute nsIAccessible accNextSibling; */
NS_IMETHODIMP nsAccessible::GetAccNextSibling(nsIAccessible * *aAccNextSibling) 
{ 
  *aAccNextSibling = nsnull; 
  if (!mWeakShell) {
    // This node has been shut down
    return NS_ERROR_FAILURE;
  }
  if (mNextSibling || !mParent) {
    // If no parent, don't try to calculate a new sibling
    // It either means we're at the root or shutting down the parent
    if (mNextSibling != DEAD_END_ACCESSIBLE) {
      NS_IF_ADDREF(*aAccNextSibling = mNextSibling);
    }
    return NS_OK;
  }

  // Last argument of PR_TRUE indicates to walk anonymous content
  nsAccessibleTreeWalker walker(mWeakShell, mDOMNode, PR_TRUE);

  if (NS_SUCCEEDED(walker.GetNextSibling())) {
    *aAccNextSibling = walker.mState.accessible;
    NS_ADDREF(*aAccNextSibling);
    nsCOMPtr<nsPIAccessible> privateAcc(do_QueryInterface(*aAccNextSibling));
    privateAcc->SetAccParent(mParent);

    mNextSibling = *aAccNextSibling;
  }

  if (!mNextSibling)
    mNextSibling = DEAD_END_ACCESSIBLE;

  return NS_OK;  
}

  /* readonly attribute nsIAccessible accPreviousSibling; */
NS_IMETHODIMP nsAccessible::GetAccPreviousSibling(nsIAccessible * *aAccPreviousSibling) 
{
  *aAccPreviousSibling = nsnull;

  if (!mWeakShell) {
    // This node has been shut down
    return NS_ERROR_FAILURE;
  }

  // Last argument of PR_TRUE indicates to walk anonymous content
  nsAccessibleTreeWalker walker(mWeakShell, mDOMNode, PR_TRUE);
  if (NS_SUCCEEDED(walker.GetPreviousSibling())) {
    *aAccPreviousSibling = walker.mState.accessible;
    NS_ADDREF(*aAccPreviousSibling);
    // Use last walker state to cache data on prev accessible
    nsCOMPtr<nsPIAccessible> privateAcc(do_QueryInterface(*aAccPreviousSibling));
    privateAcc->SetAccParent(mParent);
  }

  return NS_OK;  
}

  /* readonly attribute nsIAccessible accFirstChild; */
NS_IMETHODIMP nsAccessible::GetAccFirstChild(nsIAccessible * *aAccFirstChild) 
{  
  PRInt32 numChildren;
  GetAccChildCount(&numChildren);  // Make sure we cache all of the children

  NS_IF_ADDREF(*aAccFirstChild = mFirstChild);

  return NS_OK;  
}

  /* readonly attribute nsIAccessible accFirstChild; */
NS_IMETHODIMP nsAccessible::GetAccLastChild(nsIAccessible * *aAccLastChild)
{  
  GetChildAt(-1, aAccLastChild);
  return NS_OK;
}

NS_IMETHODIMP nsAccessible::GetChildAt(PRInt32 aChildNum, nsIAccessible **aChild)
{
  // aChildNum is a zero-based index
  // If aChildNum is out of range, last child is returned

  PRInt32 numChildren;
  GetAccChildCount(&numChildren);

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
    nextSibling->GetAccNextSibling(getter_AddRefs(current));
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
    nsCOMPtr<nsPIAccessible> privatePrevAccessible;
    mAccChildCount = 0;
    walker.GetFirstChild();
    SetAccFirstChild(walker.mState.accessible);
    while (walker.mState.accessible) {
      ++mAccChildCount;
      privatePrevAccessible = do_QueryInterface(walker.mState.accessible);
      privatePrevAccessible->SetAccParent(this);
      walker.GetNextSibling();
      privatePrevAccessible->SetAccNextSibling(walker.mState.accessible);
    }
  }
}

/* readonly attribute long accChildCount; */
NS_IMETHODIMP nsAccessible::GetAccChildCount(PRInt32 *aAccChildCount) 
{
  CacheChildren(PR_TRUE);
  *aAccChildCount = mAccChildCount;
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

  nsCOMPtr<nsIViewManager> viewManager;
  shell->GetViewManager(getter_AddRefs(viewManager));
  if (!viewManager)
    return PR_FALSE;

  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  if (!content)   // Null means we are on the document node
    return PR_TRUE;   // Document itself is visible

  nsIFrame *frame = nsnull;
  shell->GetPrimaryFrameFor(content, &frame);
  if (!frame) 
    return PR_FALSE;

  // If visibility:hidden or visibility:collapsed then mark with STATE_INVISIBLE
  if (!frame->GetStyleVisibility()->IsVisible())
  {
      return PR_FALSE;
  }

  nsCOMPtr<nsIPresContext> presContext;
  shell->GetPresContext(getter_AddRefs(presContext));
  if (!presContext)
    return PR_FALSE;

  // Get the bounds of the current frame, relative to the current view.
  // We don't use the more accurate AccGetBounds, because that is more expensive 
  // and the STATE_OFFSCREEN flag that this is used for only needs to be a rough indicator

  nsRect relFrameRect;
  nsPoint frameOffset;
  frame->GetRect(relFrameRect);
  nsIView *containingView = frame->GetViewExternal(presContext);
  if (!containingView) {
    frame->GetOffsetFromView(presContext, frameOffset, &containingView);
    if (!containingView)
      return PR_FALSE;  // no view -- not visible
    relFrameRect.x = frameOffset.x;
    relFrameRect.y = frameOffset.y;
  }

  float p2t;
  presContext->GetPixelsToTwips(&p2t);
  nsRectVisibility rectVisibility;
  viewManager->GetRectVisibility(containingView, relFrameRect, 
                                 NS_STATIC_CAST(PRUint16, (kMinPixels * p2t)), 
                                 &rectVisibility);

  if (rectVisibility == nsRectVisibility_kVisible)
    return PR_TRUE;

  *aIsOffscreen = PR_TRUE;
  return PR_FALSE;
}

nsresult nsAccessible::GetFocusedNode(nsIDOMNode *aCurrentNode, nsIDOMNode **aFocusedNode)
{
  nsCOMPtr<nsIFocusController> focusController;
  nsCOMPtr<nsIDocument> document;
  nsCOMPtr<nsIContent> content(do_QueryInterface(aCurrentNode));
  if (content)
    content->GetDocument(getter_AddRefs(document));

  if (!document)
    document = do_QueryInterface(aCurrentNode);
  if (document) {
    nsCOMPtr<nsIScriptGlobalObject> ourGlobal;
    document->GetScriptGlobalObject(getter_AddRefs(ourGlobal));
    nsCOMPtr<nsPIDOMWindow> ourWindow(do_QueryInterface(ourGlobal));
    if (ourWindow) 
      ourWindow->GetRootFocusController(getter_AddRefs(focusController));
  }

  if (focusController) {
    nsCOMPtr<nsIDOMNode> focusedNode;
    nsCOMPtr<nsIDOMElement> focusedElement; 
    focusController->GetFocusedElement(getter_AddRefs(focusedElement));
    if (!focusedElement) {
      nsCOMPtr<nsIDOMWindowInternal> windowInternal;
      focusController->GetFocusedWindow(getter_AddRefs(windowInternal));
      nsCOMPtr<nsIDOMWindow> window(do_QueryInterface(windowInternal));
      if (window) {
        nsCOMPtr<nsIDOMDocument> domDoc;
        window->GetDocument(getter_AddRefs(domDoc));
        focusedNode = do_QueryInterface(domDoc);
      }
    }
    else
      focusedNode = do_QueryInterface(focusedElement);
    if (focusedNode) {
      *aFocusedNode = focusedNode;
      NS_ADDREF(*aFocusedNode);
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

/* readonly attribute wstring accState; */
NS_IMETHODIMP nsAccessible::GetAccState(PRUint32 *aAccState) 
{ 
  nsresult rv = NS_OK; 
  *aAccState = 0;

  nsCOMPtr<nsIDOMElement> currElement(do_QueryInterface(mDOMNode));
  if (currElement) {
    // Set STATE_UNAVAILABLE state based on disabled attribute
    // The disabled attribute is mostly used in XUL elements and HTML forms, but
    // if someone sets it on another attribute, 
    // it seems reasonable to consider it unavailable
    PRBool isDisabled = PR_FALSE;
    currElement->HasAttribute(NS_LITERAL_STRING("disabled"), &isDisabled);
    if (isDisabled)  
      *aAccState |= STATE_UNAVAILABLE;
    else { 
      *aAccState |= STATE_FOCUSABLE;
      nsCOMPtr<nsIDOMNode> focusedNode;
      if (NS_SUCCEEDED(GetFocusedNode(mDOMNode, getter_AddRefs(focusedNode))) && focusedNode == mDOMNode)
        *aAccState |= STATE_FOCUSED;
    }
  }

  // Check if STATE_OFFSCREEN bitflag should be turned on for this object
  PRBool isOffscreen;
  if (!IsPartiallyVisible(&isOffscreen)) {
    *aAccState |= STATE_INVISIBLE;
    if (isOffscreen)
      *aAccState |= STATE_OFFSCREEN;
  }

  return rv;
}

  /* readonly attribute boolean accFocused; */
NS_IMETHODIMP nsAccessible::GetAccFocused(nsIAccessible **aAccFocused) 
{ 
  *aAccFocused = nsnull;

  nsCOMPtr<nsIAccessibilityService> accService(do_GetService("@mozilla.org/accessibilityService;1"));

  nsCOMPtr<nsIDOMNode> focusedNode;
  if (accService &&
       NS_SUCCEEDED(GetFocusedNode(mDOMNode, getter_AddRefs(focusedNode)))) {
    nsCOMPtr<nsIAccessible> accessible;
    if (NS_SUCCEEDED(accService->GetAccessibleInWeakShell(focusedNode, 
                                                          mWeakShell,
                                                          getter_AddRefs(accessible)))) {
      *aAccFocused = accessible;
      NS_ADDREF(*aAccFocused);
      return NS_OK;
    }
  }
 
  return NS_ERROR_FAILURE;  
}

  /* nsIAccessible accGetChildAt (in long x, in long y); */
NS_IMETHODIMP nsAccessible::AccGetAt(PRInt32 tx, PRInt32 ty, nsIAccessible **_retval)
{  
  PRInt32 x, y, w, h;
  AccGetBounds(&x,&y,&w,&h);

  if (tx >= x && tx < x + w && ty >= y && ty < y + h)
  {
    nsCOMPtr<nsIAccessible> child;
    nsCOMPtr<nsIAccessible> next;

    PRInt32 numChildren; // Make sure all children cached first
    GetAccChildCount(&numChildren);

    GetAccFirstChild(getter_AddRefs(child));

    PRInt32 cx,cy,cw,ch;

    while (child) {
      // First test if offscreen bit is set for menus
      // We don't want to walk into offscreen menus or menu items
      PRUint32 role = ROLE_NOTHING, state = 0;
      child->GetAccRole(&role);

      if (role == ROLE_MENUPOPUP || role == ROLE_MENUITEM || role == ROLE_SEPARATOR) {
        child->GetAccState(&state);
        if (role == ROLE_MENUPOPUP && (state&STATE_OFFSCREEN) == 0) {
          // Skip menupopup layer and go straight to menuitem's
          return child->AccGetAt(tx, ty, _retval);
        }
      }

      if ((state & STATE_OFFSCREEN) == 0) {   // Don't walk into offscreen menu items
        child->AccGetBounds(&cx,&cy,&cw,&ch);
        if (tx >= cx && tx < cx + cw && ty >= cy && ty < cy + ch) 
        {
          *_retval = child;
          NS_ADDREF(*_retval);
          return NS_OK;
        }
      }
      child->GetAccNextSibling(getter_AddRefs(next));
      child = next;
    }
  }

  *_retval = this;
  NS_ADDREF(this);
  return NS_OK;
}

NS_IMETHODIMP nsAccessible::AccGetDOMNode(nsIDOMNode **_retval)
{
    *_retval = mDOMNode;
    NS_IF_ADDREF(*_retval);
    return NS_OK;
}

void nsAccessible::GetScreenOrigin(nsIPresContext *aPresContext, nsIFrame *aFrame, nsRect *aRect)
{
  aRect->x = aRect->y = 0;

  if (aPresContext) {
    PRInt32 offsetX = 0;
    PRInt32 offsetY = 0;
    nsCOMPtr<nsIWidget> widget;
    
    while (aFrame) {
      // Look for a widget so we can get screen coordinates
      nsIView* view = aFrame->GetViewExternal(aPresContext);
      nsPoint origin;
      if (view) {
        view->GetWidget(*getter_AddRefs(widget));
        if (widget)
          break;
      }
      // No widget yet, so count up the coordinates of the frame 
      aFrame->GetOrigin(origin);
      offsetX += origin.x;
      offsetY += origin.y;
  
      aFrame->GetParent(&aFrame);
    }
    
    if (widget) {
      // Get the scale from that Presentation Context
      float t2p;
      aPresContext->GetTwipsToPixels(&t2p);
    
      // Convert to pixels using that scale
      offsetX = NSTwipsToIntPixels(offsetX, t2p);
      offsetY = NSTwipsToIntPixels(offsetY, t2p);
      
      // Add the widget's screen coordinates to the offset we've counted
      nsRect oldBox(0,0,0,0);
      widget->WidgetToScreen(oldBox, *aRect);
      aRect->x += offsetX;
      aRect->y += offsetY;
    }
  }
}

void nsAccessible::GetScrollOffset(nsRect *aRect)
{
  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mWeakShell));
  if (shell) {
    nsCOMPtr<nsIDocument> doc;
    shell->GetDocument(getter_AddRefs(doc));
    nsCOMPtr<nsIDOMDocumentView> docView(do_QueryInterface(doc));
    if (!docView) 
      return;

    nsCOMPtr<nsIDOMAbstractView> abstractView;
    docView->GetDefaultView(getter_AddRefs(abstractView));
    if (!abstractView) 
      return;

    nsCOMPtr<nsIDOMWindowInternal> window(do_QueryInterface(abstractView));
    window->GetPageXOffset(&aRect->x);
    window->GetPageYOffset(&aRect->y);
  }
}


void nsAccessible::GetBounds(nsRect& aTotalBounds, nsIFrame** aBoundingFrame)
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
    ancestorFrame->GetParent(&ancestorFrame); 
  }

  nsIFrame *iterFrame = firstFrame;
  nsCOMPtr<nsIContent> firstContent(do_QueryInterface(mDOMNode));
  nsCOMPtr<nsIContent> iterContent(firstContent);
  PRInt32 depth = 0;

  // Look only at frames below this depth, or at this depth (if we're still on the content node we started with)
  while (iterContent == firstContent || depth > 0) {
    // Coordinates will come back relative to parent frame
    nsIFrame *parentFrame = iterFrame;
    nsRect currFrameBounds;
    iterFrame->GetRect(currFrameBounds);
   
    // We just want the width and height - only get relative coordinates if we're not already
    // at the bounding frame
    currFrameBounds.x = currFrameBounds.y = 0;

    // Make this frame's bounds relative to common parent frame
    while (parentFrame && parentFrame != *aBoundingFrame) {
      nsRect parentFrameBounds;
      parentFrame->GetRect(parentFrameBounds);
      // Add this frame's bounds to our total rectangle
      currFrameBounds.x += parentFrameBounds.x;
      currFrameBounds.y += parentFrameBounds.y;

      parentFrame->GetParent(&parentFrame);
    }

    // Add this frame's bounds to total
    aTotalBounds.UnionRect(aTotalBounds, currFrameBounds);

    nsIFrame *iterNextFrame = nsnull;

    if (IsCorrectFrameType(iterFrame, nsAccessibilityAtoms::inlineFrame)) {
      // Only do deeper bounds search if we're on an inline frame
      // Inline frames can contain larger frames inside of them
      iterFrame->FirstChild(nsCOMPtr<nsIPresContext>(GetPresContext()), 
                            nsnull, &iterNextFrame);
    }

    if (iterNextFrame) 
      ++depth;  // Child was found in code above this: We are going deeper in this iteration of the loop
    else {  
      // Use next sibling if it exists, or go back up the tree to get the first next-in-flow or next-sibling 
      // within our search
      while (iterFrame) {
        iterFrame->GetNextInFlow(&iterNextFrame);
        if (!iterNextFrame)
          iterFrame->GetNextSibling(&iterNextFrame);
        if (iterNextFrame || --depth < 0) 
          break;
        iterFrame->GetParent(&iterFrame);
      }
    }

    // Get ready for the next round of our loop
    iterFrame = iterNextFrame;
    if (iterFrame == nsnull)
      break;
    iterContent = nsnull;
    if (depth == 0)
      iterFrame->GetContent(getter_AddRefs(iterContent));
  }
}


/* void accGetBounds (out long x, out long y, out long width, out long height); */
NS_IMETHODIMP nsAccessible::AccGetBounds(PRInt32 *x, PRInt32 *y, PRInt32 *width, PRInt32 *height)
{
  // This routine will get the entire rectange for all the frames in this node
  // -------------------------------------------------------------------------
  //      Primary Frame for node
  //  Another frame, same node                <- Example
  //  Another frame, same node

  nsCOMPtr<nsIPresContext> presContext(GetPresContext());
  if (!presContext)
  {
    *x = *y = *width = *height = 0;
    return NS_ERROR_FAILURE;
  }

  float t2p;
  presContext->GetTwipsToPixels(&t2p);   // Get pixels to twips conversion factor

  nsRect unionRectTwips;
  nsIFrame* aBoundingFrame = nsnull;
  GetBounds(unionRectTwips, &aBoundingFrame);   // Unions up all primary frames for this node and all siblings after it
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
  PRUint32 role;
  GetAccRole(&role);
  if (role != ROLE_PANE)
    GetScrollOffset(&pageRectPixels);  // Add scroll offsets if not the document itself
  *x += orgRectPixels.x - pageRectPixels.x;
  *y += orgRectPixels.y - pageRectPixels.y;

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

  nsCOMPtr<nsIAtom> frameType;
  aFrame->GetFrameType(getter_AddRefs(frameType));

  return (frameType.get() == aAtom);
}


nsIFrame* nsAccessible::GetBoundsFrame()
{
  return GetFrame();
}

/* void accRemoveSelection (); */
NS_IMETHODIMP nsAccessible::AccRemoveSelection()
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

/* void accTakeSelection (); */
NS_IMETHODIMP nsAccessible::AccTakeSelection()
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

/* void accTakeFocus (); */
NS_IMETHODIMP nsAccessible::AccTakeFocus()
{ 
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  if (!content) {
    return NS_ERROR_FAILURE;
  }
  return content->SetFocus(nsCOMPtr<nsIPresContext>(GetPresContext()));
}

NS_IMETHODIMP nsAccessible::AppendStringWithSpaces(nsAString *aFlatString, const nsAString& textEquivalent)
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

/*
 * AppendFlatStringFromContentNode and AppendFlatStringFromSubtree
 *
 * This method will glean useful text, in whatever form it exists, from any content node given to it.
 * It is used by any decendant of nsAccessible that needs to get text from a single node, as
 * well as by nsAccessible::AppendFlatStringFromSubtree, which gleans and concatenates text from any node and
 * that node's decendants.
 */

NS_IMETHODIMP nsAccessible::AppendFlatStringFromContentNode(nsIContent *aContent, nsAString *aFlatString)
{
  nsAutoString textEquivalent;
  nsCOMPtr<nsIDOMXULElement> xulElement(do_QueryInterface(aContent));
  if (xulElement) {
    nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(aContent));
    NS_ASSERTION(elt, "No DOM element for content node!");
    elt->GetAttribute(NS_LITERAL_STRING("value"), textEquivalent); // Prefer value over tooltiptext
    if (textEquivalent.IsEmpty())
      elt->GetAttribute(NS_LITERAL_STRING("tooltiptext"), textEquivalent);
    textEquivalent.CompressWhitespace();
    return AppendStringWithSpaces(aFlatString, textEquivalent);
  }

  nsCOMPtr<nsITextContent> textContent(do_QueryInterface(aContent));
  if (textContent) {     
    // If it's a text node, but node a comment node, append the text
    nsCOMPtr<nsIDOMComment> commentNode(do_QueryInterface(aContent));
    if (!commentNode) {
      PRBool isHTMLBlock = PR_FALSE;
      nsIFrame *frame;
      nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mWeakShell));
      if (!shell) {
         return NS_ERROR_FAILURE;  
      }

      nsCOMPtr<nsIContent> parentContent;
      aContent->GetParent(getter_AddRefs(parentContent));
      if (parentContent) {
        nsresult rv = shell->GetPrimaryFrameFor(parentContent, &frame);
        if (NS_SUCCEEDED(rv)) {
          // If this text is inside a block level frame (as opposed to span level), we need to add spaces around that 
          // block's text, so we don't get words jammed together in final name
          // Extra spaces will be trimmed out later
          const nsStyleDisplay* display = frame->GetStyleDisplay();
          if (display->IsBlockLevel() ||
              display->mDisplay == NS_STYLE_DISPLAY_TABLE_CELL)
          {
              isHTMLBlock = PR_TRUE;
              if (!aFlatString->IsEmpty())
                aFlatString->Append(PRUnichar(' '));
          }
        }
      }

      PRInt32 origLength;
      textContent->GetTextLength(&origLength);
      if (origLength > 0) {
        nsAutoString text;
        textContent->CopyText(text);
        text.CompressWhitespace();
        if (!text.IsEmpty())
          aFlatString->Append(text);
        if (isHTMLBlock && !aFlatString->IsEmpty())
          aFlatString->Append(PRUnichar(' '));
      }
    }
    return NS_OK;
  }

  nsCOMPtr<nsIDOMHTMLBRElement> brElement(do_QueryInterface(aContent));
  if (brElement) {   // If it's a line break, insert a space so that words aren't jammed together
    aFlatString->Append(NS_LITERAL_STRING("\r\n"));
    return NS_OK;
  }

  nsCOMPtr<nsIDOMHTMLInputElement> inputContent;
  nsCOMPtr<nsIDOMHTMLObjectElement> objectContent;
  nsCOMPtr<nsIDOMHTMLImageElement> imageContent(do_QueryInterface(aContent));
  if (!imageContent) {
    inputContent = do_QueryInterface(aContent);
    if (!inputContent)
      objectContent = do_QueryInterface(aContent);
  }
  if (imageContent || inputContent || objectContent) {
    nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(aContent));
    NS_ASSERTION(elt, "No DOM element for content node!");
    elt->GetAttribute(NS_LITERAL_STRING("alt"), textEquivalent);
    if (textEquivalent.IsEmpty())
      elt->GetAttribute(NS_LITERAL_STRING("title"), textEquivalent);
    else {
      // If we're in an image document (an image viewed by itself)
      // then the image's alt text is generated text,
      // so that an error shows when the image doesn't load.
      // We don't want that text.

      nsCOMPtr<nsIDocument> doc;
      aContent->GetDocument(getter_AddRefs(doc));
      nsCOMPtr<nsIImageDocument> imageDoc(do_QueryInterface(doc));
      if (imageDoc)  // We don't want this faux error text
        textEquivalent.Truncate();
    }
    if (textEquivalent.IsEmpty() && imageContent) {
      nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mWeakShell));
      nsCOMPtr<nsIDOMNode> imageNode(do_QueryInterface(aContent));
      if (imageNode && presShell)
        presShell->GetImageLocation(imageNode, textEquivalent);
    }
    if (textEquivalent.IsEmpty())
      elt->GetAttribute(NS_LITERAL_STRING("src"), textEquivalent);

    if (textEquivalent.IsEmpty())
      elt->GetAttribute(NS_LITERAL_STRING("data"), textEquivalent); // for <object>s with images
    textEquivalent.CompressWhitespace();
    return AppendStringWithSpaces(aFlatString, textEquivalent);
  }

  return NS_OK;
}


NS_IMETHODIMP nsAccessible::AppendFlatStringFromSubtree(nsIContent *aContent, nsAString *aFlatString)
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

  PRInt32 numChildren = 0;

  aContent->ChildCount(numChildren);
  if (numChildren == 0) {
    AppendFlatStringFromContentNode(aContent, aFlatString);
    return NS_OK;
  }

  nsCOMPtr<nsIContent> contentWalker;
  PRInt32 index;
  for (index = 0; index < numChildren; index++) {
    aContent->ChildAt(index, getter_AddRefs(contentWalker));
    AppendFlatStringFromSubtree(contentWalker, aFlatString);
  }
  return NS_OK;
}

/**
  * Called for XUL work only
  * Checks the label's value first then makes a call to get the 
  *  text from the children if the value is not set.
  */
NS_IMETHODIMP nsAccessible::AppendLabelText(nsIDOMNode *aLabelNode, nsAString& _retval)
{
  NS_ASSERTION(aLabelNode, "Label Node passed in is null");
  nsCOMPtr<nsIDOMXULLabelElement> labelNode(do_QueryInterface(aLabelNode));
  // label's value="foo" is set
  if ( labelNode && NS_SUCCEEDED(labelNode->GetValue(_retval))) {
    if (_retval.IsEmpty()) {
      // label contains children who define it's text -- possibly HTML
      nsCOMPtr<nsIContent> content(do_QueryInterface(labelNode));
      if (content)
        return AppendFlatStringFromSubtree(content, &_retval);
    }
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

/**
  * Called for HTML work only
  */
NS_IMETHODIMP nsAccessible::AppendLabelFor(nsIContent *aLookNode, const nsAString *aId, nsAString *aLabel)
{
  PRInt32 numChildren = 0;

  nsCOMPtr<nsIDOMHTMLLabelElement> labelElement(do_QueryInterface(aLookNode));
  if (labelElement) {
    nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(aLookNode));
    nsresult rv = NS_OK;

    if (elt) {
      nsAutoString labelIsFor;
      elt->GetAttribute(NS_LITERAL_STRING("for"),labelIsFor);
      if (labelIsFor.Equals(*aId))
        rv = AppendFlatStringFromSubtree(aLookNode, aLabel);
    }
    return rv;
  }

  aLookNode->ChildCount(numChildren);
  nsCOMPtr<nsIContent> contentWalker;
  PRInt32 index;
  for (index = 0; index < numChildren; index++) {
    aLookNode->ChildAt(index, getter_AddRefs(contentWalker));
    if (contentWalker)
      AppendLabelFor(contentWalker, aId, aLabel);
  }
  return NS_OK;
}

/**
  * Only called if the element is not a nsIDOMXULControlElement. Initially walks up
  *   the DOM tree to the form, concatonating label elements as it goes. Then checks for
  *   labels with the for="controlID" property.
  */
NS_IMETHODIMP nsAccessible::GetHTMLAccName(nsAString& _retval)
{
  if (!mWeakShell || !mDOMNode) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIContent> walkUpContent(do_QueryInterface(mDOMNode));
  nsCOMPtr<nsIDOMHTMLLabelElement> labelElement;
  nsCOMPtr<nsIDOMHTMLFormElement> formElement;
  nsresult rv = NS_OK;

  nsAutoString label;
  // go up tree get name of ancestor label if there is one. Don't go up farther than form element
  while (label.IsEmpty() && !formElement) {
    labelElement = do_QueryInterface(walkUpContent);
    if (labelElement) 
      rv = AppendFlatStringFromSubtree(walkUpContent, &label);
    formElement = do_QueryInterface(walkUpContent); // reached top ancestor in form
    if (formElement) {
      break;
    }
    nsCOMPtr<nsIContent> nextParent;
    walkUpContent->GetParent(getter_AddRefs(nextParent));
    if (!nextParent) {
      break;
    }
    walkUpContent = nextParent;
  }
  

  // There can be a label targeted at this control using the for="control_id" attribute
  // To save computing time, only look for those inside of a form element
  
  if (walkUpContent) {
    nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(mDOMNode));
    nsAutoString forId;
    elt->GetAttribute(NS_LITERAL_STRING("id"), forId);
    // Actually we'll be walking down the content this time, with a depth first search
    if (!forId.IsEmpty())
      AppendLabelFor(walkUpContent,&forId,&label); 
  } 
  
  label.CompressWhitespace();
  if (label.IsEmpty())
    return nsAccessible::GetAccName(_retval);

  _retval.Assign(label);
  
  return NS_OK;
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
/* wstring getAccName (); */
NS_IMETHODIMP nsAccessible::GetXULAccName(nsAString& _retval)
{
  nsresult rv;
  nsAutoString label;

  // CASE #1 -- great majority of the cases
  nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(mDOMNode));
  NS_ASSERTION(domElement, "No domElement for accessible DOM node!");
  rv = domElement->GetAttribute(NS_LITERAL_STRING("label"), label) ;

  if (NS_FAILED(rv) || label.IsEmpty() ) {

    // CASE #2 ------ label as a child
    nsCOMPtr<nsIDOMNodeList>labelChildren;
    NS_ASSERTION(domElement, "No domElement for accessible DOM node!");
    if (NS_SUCCEEDED(rv = domElement->GetElementsByTagName(NS_LITERAL_STRING("label"), getter_AddRefs(labelChildren)))) {
      PRUint32 length = 0;
      if (NS_SUCCEEDED(rv = labelChildren->GetLength(&length)) && length > 0) {
        for (PRUint32 i = 0; i < length; ++i) {
          nsCOMPtr<nsIDOMNode> child;
          if (NS_SUCCEEDED(rv = labelChildren->Item(i, getter_AddRefs(child) ))) {
            rv = AppendLabelText(child, label);
          }
        }
      }
    }
    
    if (NS_FAILED(rv) || label.IsEmpty()) {

      // CASE #3 ----- non-child label pointing to control
      //  XXX jgaunt
      //   decided to search the parent's children for labels linked to
      //   this control via the control="controlID" syntax, instead of searching
      //   the entire document with:
      //
      //      nsCOMPtr<nsIDocument> doc;
      //      nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
      //      content->GetDocument(getter_AddRefs(doc));
      //      nsCOMPtr<nsIDOMXULDocument> xulDoc(do_QueryInterface(doc));
      //      if (xulDoc) {
      //        nsCOMPtr<nsIDOMNodeList>labelList;
      //        if (NS_SUCCEEDED(rv = xulDoc->GetElementsByAttribute(NS_LITERAL_STRING("control"), controlID, getter_AddRefs(labelList))))
      //
      //   This should keep search times down and still get the relevant
      //   labels.
      nsAutoString controlID;
      domElement->GetAttribute(NS_LITERAL_STRING("id"), controlID);
      nsCOMPtr<nsIDOMNode> parent;
      if (!controlID.IsEmpty() && NS_SUCCEEDED(rv = mDOMNode->GetParentNode(getter_AddRefs(parent)))) {
        nsCOMPtr<nsIDOMXULElement> xulElement(do_QueryInterface(parent));
        NS_ASSERTION(xulElement, "No xulElement for parent DOM Node!");
        if (xulElement) {
          nsCOMPtr<nsIDOMNodeList> labelList;
          if (NS_SUCCEEDED(rv = xulElement->GetElementsByAttribute(NS_LITERAL_STRING("control"), controlID, getter_AddRefs(labelList))))
          {
            PRUint32 length = 0;
            if (NS_SUCCEEDED(rv = labelList->GetLength(&length)) && length > 0) {
              for (PRUint32 index = 0; index < length; ++index) {
                nsCOMPtr<nsIDOMNode> child;
                if (NS_SUCCEEDED(rv = labelList->Item(index, getter_AddRefs(child) ))) {
                  rv = AppendLabelText(child, label);
                }
              }
            }
          }
        }
      }  // End of CASE #3
    }  // END of CASE #2
  }  // END of CASE #1

  label.CompressWhitespace();
  if (label.IsEmpty()) {
    nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
    return nsAccessible::AppendFlatStringFromSubtree(content, &_retval);
  }
  
  _retval.Assign(label);
  return NS_OK;
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

// Not implemented by this class

/* DOMString getAccValue (); */
NS_IMETHODIMP nsAccessible::GetAccValue(nsAString& _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void setAccName (in DOMString name); */
NS_IMETHODIMP nsAccessible::SetAccName(const nsAString& name)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* DOMString getKeybinding (); */
NS_IMETHODIMP nsAccessible::GetAccKeybinding(nsAString& _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* unsigned long getAccRole (); */
NS_IMETHODIMP nsAccessible::GetAccRole(PRUint32 *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRUint8 getAccNumActions (); */
NS_IMETHODIMP nsAccessible::GetAccNumActions(PRUint8 *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* DOMString getAccActionName (in PRUint8 index); */
NS_IMETHODIMP nsAccessible::GetAccActionName(PRUint8 index, nsAString& _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void accDoAction (in PRUint8 index); */
NS_IMETHODIMP nsAccessible::AccDoAction(PRUint8 index)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* DOMString getAccHelp (); */
NS_IMETHODIMP nsAccessible::GetAccHelp(nsAString& _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIAccessible accGetFromRight(); */
NS_IMETHODIMP nsAccessible::AccGetFromRight(nsIAccessible **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIAccessible accGetFromLeft(); */
NS_IMETHODIMP nsAccessible::AccGetFromLeft(nsIAccessible **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIAccessible accGetFromAbove(); */
NS_IMETHODIMP nsAccessible::AccGetFromAbove(nsIAccessible **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIAccessible accGetFromBelow(); */
NS_IMETHODIMP nsAccessible::AccGetFromBelow(nsIAccessible **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void accAddSelection (); */
NS_IMETHODIMP nsAccessible::AccAddSelection()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void accExtendSelection (); */
NS_IMETHODIMP nsAccessible::AccExtendSelection()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* unsigned long getAccExtState (); */
NS_IMETHODIMP nsAccessible::GetAccExtState(PRUint32 *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] void getNativeInterface(out voidPtr aOutAccessible); */
NS_IMETHODIMP nsAccessible::GetNativeInterface(void **aOutAccessible)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

#ifdef MOZ_ACCESSIBILITY_ATK
// static helper function
nsresult nsAccessible::GetParentBlockNode(nsIDOMNode *aCurrentNode, nsIDOMNode **aBlockNode)
{
  *aBlockNode = nsnull;

  nsCOMPtr<nsIContent> content(do_QueryInterface(aCurrentNode));
  if (!content)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDocument> doc;
  content->GetDocument(getter_AddRefs(doc));
  if (!doc)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPresShell> presShell;
  doc->GetShellAt(0, getter_AddRefs(presShell));

  nsIFrame* frame = nsnull;
  nsCOMPtr<nsIAtom> frameType;
  presShell->GetPrimaryFrameFor(content, &frame);
  if (frame)
    frame->GetFrameType(getter_AddRefs(frameType));
  while (frame && frameType != nsAccessibilityAtoms::blockFrame) {
    nsIFrame* parentFrame = nsnull;
    frame->GetParent(&parentFrame);
    if (parentFrame)
      parentFrame->GetFrameType(getter_AddRefs(frameType));
    frame = parentFrame;
  }

  if (! frame)
    return NS_ERROR_FAILURE;

  frame->GetContent(getter_AddRefs(content));
  nsCOMPtr<nsIDOMNode> domNode(do_QueryInterface(content));
  *aBlockNode = domNode;
  NS_IF_ADDREF(*aBlockNode);
  return NS_OK;
}
#endif  //MOZ_ACCESSIBILITY_ATK



