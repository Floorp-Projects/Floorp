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
#include "nsIAccessibilityService.h"
#include "nsCOMPtr.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsIScrollableView.h"
#include "nsIViewManager.h"
#include "nsIWidget.h"
#include "nsRootAccessible.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMElement.h"
#include "nsIEventStateManager.h"
#include "nsHTMLFormControlAccessible.h"
#include "nsHTMLLinkAccessible.h"
#include "nsIDOMHTMLAreaElement.h"
#include "nsISelection.h"
#include "nsISelectionController.h"
#include "nsIServiceManager.h"
#include "nsIStringBundle.h"
#include "nsXPIDLString.h"

#include "nsIDOMComment.h"
#include "nsITextContent.h"
#include "nsIEventStateManager.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLBRElement.h"
#include "nsIDOMXULElement.h"
#include "nsIAtom.h"
#include "nsHTMLAtoms.h"
#include "nsLayoutAtoms.h"
#include "nsINameSpaceManager.h"
#include "nsGUIEvent.h"

#include "nsILink.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsReadableUtils.h"

// IFrame Helpers
#include "nsIDocShell.h"
#include "nsIWebShell.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMHTMLIFrameElement.h"
#include "nsIDOMHTMLFrameElement.h"
#include "nsIDocShellTreeItem.h"

#ifdef NS_DEBUG
#include "nsIFrameDebug.h"
#include "nsIDOMCharacterData.h"
#endif

//#define DEBUG_LEAKS

#ifdef DEBUG_LEAKS
static gnsAccessibles = 0;
#endif

static NS_DEFINE_CID(kStringBundleServiceCID,  NS_STRINGBUNDLESERVICE_CID);

/** This class is used to walk the DOM tree. It skips
  * everything but nodes that either implement nsIAccessible
  * or have primary frames that implement "GetAccessible"
  */
class nsDOMTreeWalker {
public:
  nsDOMTreeWalker(nsIWeakReference* aShell, nsIDOMNode* aContent);
  PRBool GetNextSibling();
  PRBool GetPreviousSibling();
  PRBool GetParent();
  PRBool GetFirstChild();
  PRBool GetLastChild();
  PRBool GetChildBefore(nsIDOMNode* aParent, nsIDOMNode* aChild);
  PRInt32 GetCount();

  PRBool GetAccessible();

  nsCOMPtr<nsIWeakReference> mPresShell;
  nsCOMPtr<nsIAccessible> mAccessible;
  nsCOMPtr<nsIDOMNode> mDOMNode;
  nsCOMPtr<nsIAccessibilityService> mAccService;
};


nsDOMTreeWalker::nsDOMTreeWalker(nsIWeakReference* aPresShell, nsIDOMNode* aNode)
{
  mDOMNode = aNode;
  mAccessible = nsnull;
  mPresShell = aPresShell;
  mAccService = do_GetService("@mozilla.org/accessibilityService;1");
}


PRBool nsDOMTreeWalker::GetParent()
{
  if (!mDOMNode)
  {
    mAccessible = nsnull;
    return PR_FALSE;
  }

  // do we already have an accessible? Ask it.
  if (mAccessible)
  {
    nsCOMPtr<nsIAccessible> acc;
    nsresult rv = mAccessible->GetAccParent(getter_AddRefs(acc));
    if (NS_SUCCEEDED(rv) && acc) {
      mAccessible = acc;
      acc->AccGetDOMNode(getter_AddRefs(mDOMNode));
      return PR_TRUE;
    }
  }

  nsCOMPtr<nsIDOMNode> parent;
  mDOMNode->GetParentNode(getter_AddRefs(parent));

  // if no parent then we hit the root
  // we want to return the accessible outside of us. So walk out of 
  // the document if we can
  if (!parent) {
    nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mPresShell));
    if (presShell) {
      nsCOMPtr<nsIPresShell> parentPresShell;
      nsCOMPtr<nsIContent> content;
      if (NS_SUCCEEDED(nsAccessible::GetParentPresShellAndContent(presShell,
                                                    getter_AddRefs(parentPresShell),
                                                    getter_AddRefs(content)))) {
        nsIFrame* frame;
        parentPresShell->GetPrimaryFrameFor(content, &frame);
        if (frame) {
          nsCOMPtr<nsIAccessible> accessible;

          frame->GetAccessible(getter_AddRefs(accessible));

          if (!accessible)
            accessible = do_QueryInterface(content);
          if (accessible) {
            nsCOMPtr<nsIWeakReference> wr(do_GetWeakReference(parentPresShell));
            accessible->AccGetDOMNode(getter_AddRefs(mDOMNode));
            mAccessible = accessible;
            mPresShell = wr;
            return PR_TRUE;
          }
        }
      }
      mAccessible = new nsRootAccessible(mPresShell);
      mAccessible->AccGetDOMNode(getter_AddRefs(mDOMNode));
      return PR_TRUE;
    }    

    mAccessible = nsnull;
    mDOMNode = nsnull;
    return PR_FALSE;
  }

  mDOMNode = parent;
  if (GetAccessible()) 
    return PR_TRUE;
  
  return GetParent();
}


PRBool nsDOMTreeWalker::GetNextSibling()
{
  //printf("Get next\n");

  if (!mDOMNode)
  {
    mAccessible = nsnull;
    return PR_FALSE;
  }

  // do we already have an accessible? Ask it.
  if (mAccessible)
  {
    nsCOMPtr<nsIAccessible> acc;
    nsresult rv = mAccessible->GetAccNextSibling(getter_AddRefs(acc));
    if (NS_SUCCEEDED(rv) && acc) {
      mAccessible = acc;
      acc->AccGetDOMNode(getter_AddRefs(mDOMNode));
      return PR_TRUE;
    }
  }

  // get next sibling
  nsCOMPtr<nsIDOMNode> next;
  mDOMNode->GetNextSibling(getter_AddRefs(next));
  
  // if failed
  if (!next)
  {
      // do we already have an accessible? Ask it.
    if (mAccessible)
    {
      nsCOMPtr<nsIAccessible> acc;
      nsresult rv = mAccessible->GetAccParent(getter_AddRefs(acc));
      // if there is a parent then this is the last child.
      if (NS_SUCCEEDED(rv)) {
          mAccessible = nsnull;
          mDOMNode = nsnull;
          return PR_FALSE;
      }
    } 

    // if parent has content
    nsCOMPtr<nsIDOMNode> parent;
    mDOMNode->GetParentNode(getter_AddRefs(parent));
  
    // if no parent fail
    if (!parent) {
      mAccessible = nsnull;
      mDOMNode = nsnull;
      return PR_FALSE;
    }

    // fail if we reach a parent that is accessible
    mDOMNode = parent;
    if (GetAccessible())
    {
      // fail
      mAccessible = nsnull;
      mDOMNode = nsnull;
      return PR_FALSE;
    } else {
      // next on parent
      mDOMNode = parent;
      return GetNextSibling();
    }
  }

  // if next has content
  mDOMNode = next;
  if (GetAccessible())
    return PR_TRUE;

  // if next doesn't have node

  // call first on next
  mDOMNode = next;
  if (GetFirstChild())
     return PR_TRUE;

  // call next on next
  mDOMNode = next;
  return GetNextSibling();
}


PRBool nsDOMTreeWalker::GetFirstChild()
{

  if (!mDOMNode)
  {
    mAccessible = nsnull;
    return PR_FALSE;
  }

  // do we already have an accessible? Ask it.
  if (mAccessible)
  {
    nsCOMPtr<nsIAccessible> acc;
    nsresult rv = mAccessible->GetAccFirstChild(getter_AddRefs(acc));
    if (NS_SUCCEEDED(rv) && acc) {
      mAccessible = acc;
      acc->AccGetDOMNode(getter_AddRefs(mDOMNode));
      return PR_TRUE;
    }
  }

  // get first child
  nsCOMPtr<nsIDOMNode> child;
  mDOMNode->GetFirstChild(getter_AddRefs(child));

  while(child)
  { 
    mDOMNode = child;
    // if the child has a content node done
    if (GetAccessible())
      return PR_TRUE;
    else if (GetFirstChild()) // otherwise try first child
      return PR_TRUE;

    // get next sibling
    nsCOMPtr<nsIDOMNode> next;
    child->GetNextSibling(getter_AddRefs(next));

    child = next;
  }

  // fail
  mAccessible = nsnull;
  mDOMNode = nsnull;
  return PR_FALSE;
}


PRBool nsDOMTreeWalker::GetChildBefore(nsIDOMNode* aParent, nsIDOMNode* aChild)
{

  mDOMNode = aParent;

  if (!mDOMNode)
  {
    mAccessible = nsnull;
    return PR_FALSE;
  }

  GetFirstChild();

  // if the child is not us
  if (mDOMNode == aChild) {
    mAccessible = nsnull;
    mDOMNode = nsnull;
    return PR_FALSE;
  }

  nsCOMPtr<nsIDOMNode> prev(mDOMNode);
  nsCOMPtr<nsIAccessible> prevAccessible(mAccessible);

  while(mDOMNode) 
  {
    GetNextSibling();

    if (mDOMNode == aChild)
      break;

    prev = mDOMNode;
    prevAccessible = mAccessible;
  }

  mAccessible = prevAccessible;
  mDOMNode = prev;

  return PR_TRUE;
}

PRBool nsDOMTreeWalker::GetPreviousSibling()
{
  nsCOMPtr<nsIDOMNode> child(mDOMNode);
  GetParent();
  
  return GetChildBefore(mDOMNode, child);
}

PRBool nsDOMTreeWalker::GetLastChild()
{
  return GetChildBefore(mDOMNode, nsnull);
}


PRInt32 nsDOMTreeWalker::GetCount()
{

  nsCOMPtr<nsIDOMNode> node(mDOMNode);
  nsCOMPtr<nsIAccessible> a(mAccessible);

  GetFirstChild();

  PRInt32 count = 0;
  while(mDOMNode) 
  {
    count++;
    GetNextSibling();
  }

  mDOMNode = node;
  mAccessible = a;

  return count;
}

/**
 * If the DOM node's frame has and accessible or the DOMNode
 * itself implements nsIAccessible return it.
 */

PRBool nsDOMTreeWalker::GetAccessible()
{
  mAccessible = nsnull;

  return (mAccService &&
          NS_SUCCEEDED(mAccService->GetAccessibleFor(mDOMNode, getter_AddRefs(mAccessible))) &&
          mAccessible);
}


/*
 * Class nsAccessible
 */

//-------------------------`----------------------------
// construction 
//-----------------------------------------------------
nsAccessible::nsAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell)
{
  NS_INIT_REFCNT();

  // get frame and node
  mDOMNode = aNode;
  mPresShell = aShell;
     
  nsCOMPtr<nsIDocument> document;
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  if (content)
    content->GetDocument(*getter_AddRefs(document));
  if (document) {
    nsCOMPtr<nsIScriptGlobalObject> ourGlobal;
    document->GetScriptGlobalObject(getter_AddRefs(ourGlobal));
    nsCOMPtr<nsPIDOMWindow> ourWindow(do_QueryInterface(ourGlobal));
    if(ourWindow) 
      ourWindow->GetRootFocusController(getter_AddRefs(mFocusController));
  }
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

#ifdef DEBUG_LEAKS
  printf("nsAccessibles=%d\n", ++gnsAccessibles);
#endif
}

//-----------------------------------------------------
// destruction
//-----------------------------------------------------
nsAccessible::~nsAccessible()
{
#ifdef DEBUG_LEAKS
  printf("nsAccessibles=%d\n", --gnsAccessibles);
#endif
}


NS_IMETHODIMP nsAccessible::GetAccName(nsAWritableString& _retval)
{
  nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(mDOMNode));
  if (elt) 
    return elt->GetAttribute(NS_LITERAL_STRING("title"), _retval);
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsAccessible::GetAccParent(nsIAccessible **  aAccParent)
{
  nsCOMPtr<nsIPresContext> context;
  GetPresContext(context);

  nsDOMTreeWalker walker(mPresShell, mDOMNode); 
  if (walker.GetParent()) {
    *aAccParent = walker.mAccessible;
    NS_ADDREF(*aAccParent);
    return NS_OK;
  }

  *aAccParent = nsnull;
  return NS_OK;
}

  /* readonly attribute nsIAccessible accNextSibling; */
NS_IMETHODIMP nsAccessible::GetAccNextSibling(nsIAccessible * *aAccNextSibling) 
{ 
  nsDOMTreeWalker walker(mPresShell, mDOMNode);

  walker.GetNextSibling();

  if (walker.mAccessible && walker.mDOMNode) 
  {
    *aAccNextSibling = walker.mAccessible;
    NS_ADDREF(*aAccNextSibling);
    return NS_OK;
  }

  *aAccNextSibling = nsnull;

  return NS_OK;  
}

  /* readonly attribute nsIAccessible accPreviousSibling; */
NS_IMETHODIMP nsAccessible::GetAccPreviousSibling(nsIAccessible * *aAccPreviousSibling) 
{  
  // failed? Lets do some default behavior
  nsDOMTreeWalker walker(mPresShell, mDOMNode); 
  walker.GetPreviousSibling();

  if (walker.mAccessible && walker.mDOMNode) 
  {
    *aAccPreviousSibling = walker.mAccessible;
    NS_ADDREF(*aAccPreviousSibling);
    return NS_OK;
  }

  *aAccPreviousSibling = nsnull;

  return NS_OK;  
}

  /* readonly attribute nsIAccessible accFirstChild; */
NS_IMETHODIMP nsAccessible::GetAccFirstChild(nsIAccessible * *aAccFirstChild) 
{  
  nsDOMTreeWalker walker(mPresShell, mDOMNode); 
  walker.GetFirstChild();

  if (walker.mAccessible && walker.mDOMNode) 
  {
    *aAccFirstChild = walker.mAccessible;
    NS_ADDREF(*aAccFirstChild);
    return NS_OK;
  }

  *aAccFirstChild = nsnull;

  return NS_OK;  
}

  /* readonly attribute nsIAccessible accFirstChild; */
NS_IMETHODIMP nsAccessible::GetAccLastChild(nsIAccessible * *aAccLastChild)
{  
  nsDOMTreeWalker walker(mPresShell, mDOMNode); 
  walker.GetLastChild();

  if (walker.mAccessible && walker.mDOMNode) 
  {
    *aAccLastChild = walker.mAccessible;
    NS_ADDREF(*aAccLastChild);
    return NS_OK;
  }

  *aAccLastChild = nsnull;

  return NS_OK;
}

/* readonly attribute long accChildCount; */
NS_IMETHODIMP nsAccessible::GetAccChildCount(PRInt32 *aAccChildCount) 
{
  nsDOMTreeWalker walker(mPresShell, mDOMNode); 
  *aAccChildCount = walker.GetCount();

  return NS_OK;  
}

nsresult nsAccessible::GetTranslatedString(PRUnichar *aKey, nsAWritableString *aStringOut)
{
  static nsCOMPtr<nsIStringBundle> stringBundle;
  static PRBool firstTime = PR_TRUE;

  if (firstTime) {
    firstTime = PR_FALSE;
    nsresult rv;
    nsCOMPtr<nsIStringBundleService> stringBundleService = 
             do_GetService(kStringBundleServiceCID, &rv);
    // nsCOMPtr<nsIStringBundleService> stringBundleService(do_GetService(kStringBundleServiceCID, &rv));
    if (!stringBundleService) { 
      NS_WARNING("ERROR: Failed to get StringBundle Service instance.\n");
      return NS_ERROR_FAILURE;
    }
    stringBundleService->CreateBundle(ACCESSIBLE_BUNDLE_URL, getter_AddRefs(stringBundle));
  }

  nsXPIDLString xsValue;
  if (!stringBundle || 
    NS_FAILED(stringBundle->GetStringFromName(aKey, getter_Copies(xsValue)))) 
    return NS_ERROR_FAILURE;

  aStringOut->Assign(xsValue);
  return NS_OK;
}

PRBool nsAccessible::IsEntirelyVisible() 
{
  // Set up the variables we need, return false if we can't get at them all
  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mPresShell));
  if (!shell) 
    return PR_FALSE;

  nsCOMPtr<nsIViewManager> viewManager;
  shell->GetViewManager(getter_AddRefs(viewManager));
  if (!viewManager)
    return PR_FALSE;

  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  if (!content) 
    return PR_FALSE;

  nsIFrame *frame = nsnull;
  shell->GetPrimaryFrameFor(content, &frame);
  if (!frame) 
    return PR_FALSE;

  nsCOMPtr<nsIPresContext> presContext;
  shell->GetPresContext(getter_AddRefs(presContext));
  if (!presContext)
    return PR_FALSE;

  // Get the bounds of the current frame, relative to the current view.
  // We don't use the more accurate GetAccBounds, because that is more expensive 
  // and the STATE_OFFSCREEN flag that this is used for only needs to be a rough indicator
  nsRect relFrameRect;
  nsIView *containingView = nsnull;
  nsPoint frameOffset;
  frame->GetRect(relFrameRect);
  frame->GetView(presContext, &containingView);
  if (!containingView) {
    frame->GetOffsetFromView(presContext, frameOffset, &containingView);
    if (!containingView)
      return PR_FALSE;  // no view -- not visible
    relFrameRect.x = frameOffset.x;
    relFrameRect.y = frameOffset.y;
  }

  PRBool isVisible = PR_FALSE;
  viewManager->IsRectVisible(containingView, relFrameRect, PR_TRUE, &isVisible);

  return isVisible;
}

/* readonly attribute wstring accState; */
NS_IMETHODIMP nsAccessible::GetAccState(PRUint32 *aAccState) 
{ 
  nsresult rv = NS_OK; 
  *aAccState = 0;

  nsCOMPtr<nsIDOMElement> currElement(do_QueryInterface(mDOMNode));
  if (currElement) {
    *aAccState |= STATE_FOCUSABLE;
    if (mFocusController) {
      nsCOMPtr<nsIDOMElement> focusedElement; 
      rv = mFocusController->GetFocusedElement(getter_AddRefs(focusedElement));
      if (NS_SUCCEEDED(rv) && focusedElement == currElement)
        *aAccState |= STATE_FOCUSED;
    }
  }

  // Check if STATE_OFFSCREEN bitflag should be turned on for this object
  if (!IsEntirelyVisible())
    *aAccState |= STATE_OFFSCREEN;

  return rv;
}

  /* readonly attribute boolean accFocused; */
NS_IMETHODIMP nsAccessible::GetAccFocused(nsIAccessible * *aAccFocused) 
{ 
  nsCOMPtr<nsIDOMElement> focusedElement;
  if (mFocusController) {
    mFocusController->GetFocusedElement(getter_AddRefs(focusedElement));
  }

  nsIFrame *frame = nsnull;
  if (focusedElement) {
    nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mPresShell));
    if (!shell) {
       *aAccFocused = nsnull;
       return NS_OK;  
    }

    nsCOMPtr<nsIContent> content(do_QueryInterface(focusedElement));
    if (shell && content)
      shell->GetPrimaryFrameFor(content, &frame);
  }

  if (frame) {
    nsCOMPtr<nsIAccessible> acc(do_QueryInterface(frame));
    if (acc) { 
      *aAccFocused = acc;
      NS_ADDREF(*aAccFocused);
      return NS_OK;
    }
  }
 
  *aAccFocused = nsnull;

  return NS_OK;  
}

  /* nsIAccessible accGetChildAt (in long x, in long y); */
NS_IMETHODIMP nsAccessible::AccGetAt(PRInt32 tx, PRInt32 ty, nsIAccessible **_retval)
{
  PRInt32 x,y,w,h;
  AccGetBounds(&x,&y,&w,&h);
  if (tx > x && tx < x + w && ty > y && ty < y + h)
  {
    nsCOMPtr<nsIAccessible> child;
    nsCOMPtr<nsIAccessible> next;
    GetAccFirstChild(getter_AddRefs(child));
    PRInt32 cx,cy,cw,ch;

    while(child) {
      child->AccGetBounds(&cx,&cy,&cw,&ch);
      if (tx > cx && tx < cx + cw && ty > cy && ty < cy + ch) 
      {
        *_retval = child;
        NS_ADDREF(*_retval);
        return NS_OK;
      }
      child->GetAccNextSibling(getter_AddRefs(next));
      child = next;
    }


    *_retval = this;
    NS_ADDREF(this);
    return NS_OK;
  }

  *_retval = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsAccessible::AccGetDOMNode(nsIDOMNode **_retval)
{
    *_retval = mDOMNode;
    NS_IF_ADDREF(*_retval);
    return NS_OK;
}

nsresult nsAccessible::GetDocShellFromPS(nsIPresShell* aPresShell, nsIDocShell** aDocShell)
{
  *aDocShell = nsnull;
  if (aPresShell) {
    nsCOMPtr<nsIDocument> doc;
    aPresShell->GetDocument(getter_AddRefs(doc));
    if (doc) {
      nsCOMPtr<nsIScriptGlobalObject> scriptObj;
      doc->GetScriptGlobalObject(getter_AddRefs(scriptObj));
      if (scriptObj) {
        scriptObj->GetDocShell(aDocShell);
        return *aDocShell != nsnull?NS_OK:NS_ERROR_FAILURE;
      }
    }
  }
  return NS_ERROR_FAILURE;
}
  
//-------------------------------------------------------
// This gets ref counted copies of the PresShell, PresContext, 
// and Root Content for a given nsIDocShell
nsresult 
nsAccessible::GetDocShellObjects(nsIDocShell*     aDocShell,
                                 nsIPresShell**   aPresShell, 
                                 nsIPresContext** aPresContext, 
                                 nsIContent**     aContent)
{
  NS_ENSURE_ARG_POINTER(aDocShell);
  NS_ENSURE_ARG_POINTER(aPresShell);
  NS_ENSURE_ARG_POINTER(aPresContext);
  NS_ENSURE_ARG_POINTER(aContent);


  aDocShell->GetPresShell(aPresShell); // this addrefs
  if (*aPresShell == nsnull) return NS_ERROR_FAILURE;

  aDocShell->GetPresContext(aPresContext); // this addrefs
  if (*aPresContext == nsnull) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDocument> doc;
  (*aPresShell)->GetDocument(getter_AddRefs(doc));
  if (!doc) return NS_ERROR_FAILURE;

  return doc->GetRootContent(aContent); // this addrefs
}

//-------------------------------------------------------
// 
nsresult 
nsAccessible::GetDocShells(nsIPresShell* aPresShell,
                           nsIDocShell** aDocShell,
                           nsIDocShell** aParentDocShell)
{
  NS_ENSURE_ARG_POINTER(aPresShell);
  NS_ENSURE_ARG_POINTER(aDocShell);
  NS_ENSURE_ARG_POINTER(aParentDocShell);

  *aDocShell = nsnull;

  // Start by finding our PresShell and from that
  // we get our nsIDocShell in order to walk the DocShell tree
  if (NS_SUCCEEDED(GetDocShellFromPS(aPresShell, aDocShell))) {
    // Now that we have the DocShell QI 
    // it to a tree item to find it's parent
    nsCOMPtr<nsIDocShell> docShell = *aDocShell;
    nsCOMPtr<nsIDocShellTreeItem> item(do_QueryInterface(docShell));
    if (item) {
      nsCOMPtr<nsIDocShellTreeItem> itemParent;
      item->GetParent(getter_AddRefs(itemParent));
      // QI to get the WebShell for the parent document
      nsCOMPtr<nsIDocShell> pDocShell(do_QueryInterface(itemParent));
      if (pDocShell) {
        *aParentDocShell = pDocShell.get();
        NS_ADDREF(*aParentDocShell);
        return NS_OK;
      }
    }
  }

  NS_IF_RELEASE(*aDocShell);
  return NS_ERROR_FAILURE;
}

//-------------------------------------------------------
// 
nsresult 
nsAccessible::GetParentPresShellAndContent(nsIPresShell*  aPresShell,
                                           nsIPresShell** aParentPresShell,
                                           nsIContent**   aSubShellContent)
{
  NS_ENSURE_ARG_POINTER(aPresShell);
  NS_ENSURE_ARG_POINTER(aParentPresShell);
  NS_ENSURE_ARG_POINTER(aSubShellContent);

  *aParentPresShell = nsnull;
  *aSubShellContent = nsnull;

  nsCOMPtr<nsIDocShell> docShell;
  nsCOMPtr<nsIDocShell> parentDocShell;
  if (NS_FAILED(GetDocShells(aPresShell, 
                             getter_AddRefs(docShell), 
                             getter_AddRefs(parentDocShell)))) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIPresContext> parentPresContext;
  nsCOMPtr<nsIContent> parentRootContent;
  if (NS_FAILED(GetDocShellObjects(parentDocShell, 
                                   aParentPresShell,
                                   getter_AddRefs(parentPresContext),
                                   getter_AddRefs(parentRootContent)))) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(docShell));
  if (FindContentForWebShell(*aParentPresShell, parentRootContent, 
                              webShell, aSubShellContent)) {
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}
 
PRBool 
nsAccessible::FindContentForWebShell(nsIPresShell* aParentPresShell,
                                     nsIContent*   aParentContent,
                                     nsIWebShell*  aWebShell,
                                     nsIContent**  aFoundContent)
{
  NS_ASSERTION(aWebShell, "Pointer is null!");
  NS_ASSERTION(aParentPresShell, "Pointer is null!");
  NS_ASSERTION(aParentContent, "Pointer is null!");
  NS_ASSERTION(aFoundContent, "Pointer is null!");

  nsCOMPtr<nsIDOMHTMLIFrameElement> iFrame(do_QueryInterface(aParentContent));
  nsCOMPtr<nsIDOMHTMLFrameElement> frame(do_QueryInterface(aParentContent));
#ifdef NS_DEBUG_X
  {
    printf("** FindContent - Content %p",aParentContent);
    nsIFrame* frame;
    aParentPresShell->GetPrimaryFrameFor(aParentContent, &frame);
    if (frame) {
      char * name;
      GetNameForFrame(frame, &name);
      printf("  [%s]", name?name:"<no name>");
      if (name) nsMemory::Free(name);
    }
    printf("\n");
  }
#endif

  if (iFrame || frame) {
    //printf("********* Found IFrame %p\n", aParentContent);
    nsCOMPtr<nsISupports> supps;
    aParentPresShell->GetSubShellFor(aParentContent, getter_AddRefs(supps));
    if (supps) {
      nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(supps));
      //printf("********* Checking %p == %p (parent)\n", webShell.get(), aWebShell);
      if (webShell.get() == aWebShell) {
        //printf("********* Found WebShell %p \n", aWebShell);
        *aFoundContent = aParentContent;
        NS_ADDREF(aParentContent);
        return PR_TRUE;
      }
    }
  }

  // walk children content
  PRInt32 count;
  aParentContent->ChildCount(count);
  for (PRInt32 i=0;i<count;i++) {
    nsIContent* child;
    aParentContent->ChildAt(i, child);
    if (FindContentForWebShell(aParentPresShell, child, aWebShell, aFoundContent)) {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

// Calculate a frame's position in screen coordinates
nsresult
nsAccessible::GetAbsoluteFramePosition(nsIPresContext* aPresContext,
                                       nsIFrame *aFrame, 
                                       nsRect& aAbsoluteTwipsRect, 
                                       nsRect& aAbsolutePixelRect)
{
  //XXX: This code needs to take the view's offset into account when calculating
  //the absolute coordinate of the frame.
  nsresult rv = NS_OK;
 
  aFrame->GetRect(aAbsoluteTwipsRect);
  // zero these out, 
  // because the GetOffsetFromView figures them out
  // We're only keeping the height and width in aAbsoluteTwipsRect
  aAbsoluteTwipsRect.x = 0;
  aAbsoluteTwipsRect.y = 0;

    // Get conversions between twips and pixels
  float t2p;
  float p2t;
  aPresContext->GetTwipsToPixels(&t2p);
  aPresContext->GetPixelsToTwips(&p2t);
  
   // Add in frame's offset from it it's containing view
  nsIView *containingView = nsnull;
  nsPoint offset(0,0);
  rv = aFrame->GetOffsetFromView(aPresContext, offset, &containingView);
  if (containingView == nsnull) {
    aFrame->GetView(aPresContext, &containingView);
    nsRect r;
    aFrame->GetRect(r);
    offset.x = r.x;
    offset.y = r.y;
  }
  if (NS_SUCCEEDED(rv) && (nsnull != containingView)) {
    aAbsoluteTwipsRect.x += offset.x;
    aAbsoluteTwipsRect.y += offset.y;

    nsPoint viewOffset;
    containingView->GetPosition(&viewOffset.x, &viewOffset.y);
    nsIView * parent;
    containingView->GetParent(parent);

    // if we don't have a parent view then 
    // check to see if we have a widget and adjust our offset for the widget
    if (parent == nsnull) {
      nsIWidget * widget;
      containingView->GetWidget(widget);
      if (nsnull != widget) {
        // Add in the absolute offset of the widget.
        nsRect absBounds;
        nsRect lc;
        widget->WidgetToScreen(lc, absBounds);
        // Convert widget coordinates to twips   
        aAbsoluteTwipsRect.x += NSIntPixelsToTwips(absBounds.x, p2t);
        aAbsoluteTwipsRect.y += NSIntPixelsToTwips(absBounds.y, p2t);   
        NS_RELEASE(widget);
      }
      rv = NS_OK;
    } else {

      while (nsnull != parent) {
        nsPoint po;
        parent->GetPosition(&po.x, &po.y);
        viewOffset.x += po.x;
        viewOffset.y += po.y;
        nsIScrollableView * scrollView;
        if (NS_OK == containingView->QueryInterface(NS_GET_IID(nsIScrollableView), (void **)&scrollView)) {
          nscoord x;
          nscoord y;
          scrollView->GetScrollPosition(x, y);
          viewOffset.x -= x;
          viewOffset.y -= y;
        }
        nsIWidget * widget;
        parent->GetWidget(widget);
        if (nsnull != widget) {
          // Add in the absolute offset of the widget.
          nsRect absBounds;
          nsRect lc;
          widget->WidgetToScreen(lc, absBounds);
          // Convert widget coordinates to twips   
          aAbsoluteTwipsRect.x += NSIntPixelsToTwips(absBounds.x, p2t);
          aAbsoluteTwipsRect.y += NSIntPixelsToTwips(absBounds.y, p2t);   
          NS_RELEASE(widget);
          break;
        }
        parent->GetParent(parent);
      }
      aAbsoluteTwipsRect.x += viewOffset.x;
      aAbsoluteTwipsRect.y += viewOffset.y;
    }
  }

   // convert to pixel coordinates
  if (NS_SUCCEEDED(rv)) {
   aAbsolutePixelRect.x = NSTwipsToIntPixels(aAbsoluteTwipsRect.x, t2p);
   aAbsolutePixelRect.y = NSTwipsToIntPixels(aAbsoluteTwipsRect.y, t2p);
   aAbsolutePixelRect.width = NSTwipsToIntPixels(aAbsoluteTwipsRect.width, t2p);
   aAbsolutePixelRect.height = NSTwipsToIntPixels(aAbsoluteTwipsRect.height, t2p);
  }

  return rv;
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
    if (IsCorrectFrameType(ancestorFrame, nsLayoutAtoms::blockFrame) || 
        IsCorrectFrameType(ancestorFrame, nsLayoutAtoms::areaFrame)) {
      break;
    }
    ancestorFrame->GetParent(&ancestorFrame); 
  }

  // Get ready for loop
  nsCOMPtr<nsIPresContext> presContext;
  GetPresContext(presContext);

  nsIFrame *iterFrame = firstFrame;
  nsCOMPtr<nsIContent> firstContent(do_QueryInterface(mDOMNode));
  nsCOMPtr<nsIContent> iterContent(firstContent);
  nsRect currFrameBounds;
  PRInt32 depth = 0;

  // Look only at frames below this depth, or at this depth (if we're still on the content node we started with)
  while (iterContent == firstContent || depth > 0) {
    // Coordinates will come back relative to parent frame
    nsIFrame *parentFrame = iterFrame;
    iterFrame->GetRect(currFrameBounds);

    // Make this frame's bounds relative to common parent frame
    while (parentFrame != *aBoundingFrame) {  
      parentFrame->GetParent(&parentFrame);
      if (!parentFrame) 
        break;
      nsRect parentFrameBounds;
      parentFrame->GetRect(parentFrameBounds);
      // Add this frame's bounds to our total rectangle
      currFrameBounds.x += parentFrameBounds.x;
      currFrameBounds.y += parentFrameBounds.y;
    }

    // Add this frame's bounds to total
    aTotalBounds.UnionRect(aTotalBounds, currFrameBounds);

    nsIFrame *iterNextFrame = nsnull;

    if (IsCorrectFrameType(iterFrame, nsLayoutAtoms::inlineFrame)) {
      // Only do deeper bounds search if we're on an inline frame
      // Inline frames can contain larger frames inside of them
      iterFrame->FirstChild(presContext, nsnull, &iterNextFrame);
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

  float t2p;
  nsCOMPtr<nsIPresContext> presContext;
  GetPresContext(presContext);
  if (!presContext)
  {
    *x = *y = *width = *height = 0;
    return NS_ERROR_FAILURE;
  }

  presContext->GetTwipsToPixels(&t2p);   // Get pixels to twips conversion factor

  nsRect unionRectTwips;
  nsIFrame* aRelativeFrame = nsnull;
  GetBounds(unionRectTwips, &aRelativeFrame);   // Unions up all primary frames for this node and all siblings after it

  *x      = NSTwipsToIntPixels(unionRectTwips.x, t2p); 
  *y      = NSTwipsToIntPixels(unionRectTwips.y, t2p);
  *width  = NSTwipsToIntPixels(unionRectTwips.width, t2p);
  *height = NSTwipsToIntPixels(unionRectTwips.height, t2p);

  // We have the union of the rectangle, now we need to put it in absolute screen coords

  if (presContext) {
    nsRect orgRectTwips, frameRectTwips, orgRectPixels;

    // Get the offset of this frame in screen coordinates
    if (NS_SUCCEEDED(GetAbsoluteFramePosition(presContext, aRelativeFrame, orgRectTwips, orgRectPixels))) {
      aRelativeFrame->GetRect(frameRectTwips);   // Usually just the primary frame, but can be the choice list frame for an nsSelectAccessible
      // Add in the absolute coorinates.
      // Since these absolute coordinates are for the primary frame, 
      // also subtract the difference between our primary frame and our bounds frame
      *x += orgRectPixels.x - NSTwipsToIntPixels(frameRectTwips.x, t2p);
      *y += orgRectPixels.y - NSTwipsToIntPixels(frameRectTwips.y, t2p);
    }
  }

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

nsIFrame* nsAccessible::GetFrame()
{
  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mPresShell));
  if (!shell) 
    return nsnull;  

  nsIFrame* frame = nsnull;
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  shell->GetPrimaryFrameFor(content, &frame);
  return frame;
}

void nsAccessible::GetPresContext(nsCOMPtr<nsIPresContext>& aContext)
{
  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mPresShell));

  if (shell) {
    shell->GetPresContext(getter_AddRefs(aContext));
  } else
    aContext = nsnull;
}

/* void accRemoveSelection (); */
NS_IMETHODIMP nsAccessible::AccRemoveSelection()
{
  nsCOMPtr<nsISelectionController> control(do_QueryReferent(mPresShell));
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
  nsCOMPtr<nsISelectionController> control(do_QueryReferent(mPresShell));
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
  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mPresShell));
  if (!shell) {
     return NS_ERROR_FAILURE;  
  }

  nsCOMPtr<nsIPresContext> context;
  shell->GetPresContext(getter_AddRefs(context));
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  content->SetFocus(context);
  
  return NS_OK;
}

NS_IMETHODIMP nsAccessible::AppendStringWithSpaces(nsAWritableString *aFlatString, nsAReadableString& textEquivalent)
{
  // Insert spaces to insure that words from controls aren't jammed together
  if (!textEquivalent.IsEmpty()) {
    aFlatString->Append(NS_LITERAL_STRING(" "));
    aFlatString->Append(textEquivalent);
    aFlatString->Append(NS_LITERAL_STRING(" "));
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

NS_IMETHODIMP nsAccessible::AppendFlatStringFromContentNode(nsIContent *aContent, nsAWritableString *aFlatString)
{
  nsAutoString textEquivalent;
  nsCOMPtr<nsIDOMXULElement> xulElement(do_QueryInterface(aContent));
  if (xulElement) {
    nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(aContent));
    NS_ASSERTION(elt, "No DOM element for content node!");
    elt->GetAttribute(NS_LITERAL_STRING("value"), textEquivalent); // Prefer value over tooltiptext
    if (textEquivalent.IsEmpty())
      elt->GetAttribute(NS_LITERAL_STRING("tooltiptext"), textEquivalent);
    return AppendStringWithSpaces(aFlatString, textEquivalent);
  }

  nsCOMPtr<nsITextContent> textContent(do_QueryInterface(aContent));
  if (textContent) {     
    // If it's a text node, but node a comment node, append the text
    nsCOMPtr<nsIDOMComment> commentNode(do_QueryInterface(aContent));
    if (!commentNode) {
      PRBool isHTMLBlock = PR_FALSE;
      nsIFrame *frame;
      nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mPresShell));
      if (!shell) {
         return NS_ERROR_FAILURE;  
      }

      nsCOMPtr<nsIContent> parentContent;
      aContent->GetParent(*getter_AddRefs(parentContent));
      if (parentContent) {
        nsresult rv = shell->GetPrimaryFrameFor(parentContent, &frame);
        if (NS_SUCCEEDED(rv)) {
          // If this text is inside a block level frame (as opposed to span level), we need to add spaces around that 
          // block's text, so we don't get words jammed together in final name
          // Extra spaces will be trimmed out later
          nsCOMPtr<nsIStyleContext> styleContext;
          frame->GetStyleContext(getter_AddRefs(styleContext));
          if (styleContext) {
            const nsStyleDisplay* display = (const nsStyleDisplay*)styleContext->GetStyleData(eStyleStruct_Display);
            if (display->IsBlockLevel() || display->mDisplay == NS_STYLE_DISPLAY_TABLE_CELL) {
              isHTMLBlock = PR_TRUE;
              aFlatString->Append(NS_LITERAL_STRING(" "));
            }
          }
        }
      }
      nsAutoString text;
      textContent->CopyText(text);
      if (text.Length()>0)
        aFlatString->Append(text);
      if (isHTMLBlock)
        aFlatString->Append(NS_LITERAL_STRING(" "));
    }
    return NS_OK;
  }

  nsCOMPtr<nsIDOMHTMLBRElement> brElement(do_QueryInterface(aContent));
  if (brElement) {   // If it's a line break, insert a space so that words aren't jammed together
    aFlatString->Append(NS_LITERAL_STRING(" "));
    return NS_OK;
  }

  nsCOMPtr<nsIDOMHTMLImageElement> imageContent(do_QueryInterface(aContent));
  nsCOMPtr<nsIDOMHTMLInputElement> inputContent;
  if (!imageContent)
    inputContent = do_QueryInterface(aContent);
  if (imageContent || inputContent) {
    nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(aContent));
    NS_ASSERTION(elt, "No DOM element for content node!");
    elt->GetAttribute(NS_LITERAL_STRING("alt"), textEquivalent);
    if (textEquivalent.IsEmpty())
      elt->GetAttribute(NS_LITERAL_STRING("title"), textEquivalent);
    if (textEquivalent.IsEmpty())
      elt->GetAttribute(NS_LITERAL_STRING("name"), textEquivalent);
    if (textEquivalent.IsEmpty())
      elt->GetAttribute(NS_LITERAL_STRING("src"), textEquivalent);
    return AppendStringWithSpaces(aFlatString, textEquivalent);
  }

  return NS_OK;
}


NS_IMETHODIMP nsAccessible::AppendFlatStringFromSubtree(nsIContent *aContent, nsAWritableString *aFlatString)
{
  // Depth first search for all text nodes that are decendants of content node.
  // Append all the text into one flat string

  PRInt32 numChildren = 0;

  aContent->ChildCount(numChildren);
  if (numChildren == 0) {
    AppendFlatStringFromContentNode(aContent, aFlatString);
    return NS_OK;
  }
    
  nsIContent *contentWalker;
  PRInt32 index;
  for (index = 0; index < numChildren; index++) {
    aContent->ChildAt(index, contentWalker);
    AppendFlatStringFromSubtree(contentWalker, aFlatString);
  }
  return NS_OK;
}
