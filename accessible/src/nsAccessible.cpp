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

#include "nsAccessible.h"
#include "nsCOMPtr.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsIScrollableView.h"
#include "nsRootAccessible.h"
#include "nsIScriptGlobalObject.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMElement.h"
#include "nsIEventStateManager.h"
#include "nsHTMLFormControlAccessible.h"
#include "nsILink.h"
#include "nsHTMLLinkAccessible.h"
#include "nsIDOMHTMLAreaElement.h"
#include "nsIServiceManager.h"
#include "nsIStringBundle.h"
#include "nsXPIDLString.h"

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

class nsFrameTreeWalker {
public:
  nsFrameTreeWalker(nsIPresContext* aPresContext, nsIDOMNode* aNode);
  PRBool GetNextSibling();
  PRBool GetParent();
  PRBool GetFirstChild();
  void GetBounds(nsRect& aBounds);

  PRBool IsSameContent();

  void SetNode(nsIDOMNode* node);
  void InitDepth();
  nsIFrame* mFrame;

private:
  nsCOMPtr<nsIDOMNode> mDOMNode;
  nsCOMPtr<nsIDOMNode> mNodeToFind;
  nsCOMPtr<nsIPresContext> mPresContext;
  PRInt32 mDepth;
  PRInt32 mDepthToFind;
};

class nsDOMNodeBoundsFinder
{
public:
  nsDOMNodeBoundsFinder(nsIPresContext* aPresContext, nsIDOMNode* aNode);
  virtual void GetBounds(nsRect& aBounds);

private:
  nsCOMPtr<nsIDOMNode> mDOMNode;
  nsFrameTreeWalker mWalker;
};

//------ nsAccessibleBoundsFinder -----

nsDOMNodeBoundsFinder::nsDOMNodeBoundsFinder(nsIPresContext* aPresContext, nsIDOMNode* aNode)
:mWalker(aPresContext, aNode), mDOMNode(aNode)
{
};

void nsDOMNodeBoundsFinder::GetBounds(nsRect& aBounds)
{
  // sum up all the child rectangles
  nsCOMPtr<nsIDOMNode> node(mDOMNode);
 
  mWalker.SetNode(mDOMNode);
  mWalker.GetBounds(aBounds);

  while(mWalker.GetNextSibling()) 
  {
     nsRect rect;
     mWalker.GetBounds(rect);
     aBounds.UnionRect(aBounds,rect);
  }
}

nsFrameTreeWalker::nsFrameTreeWalker(nsIPresContext* aPresContext, nsIDOMNode* aNode)
{
  mPresContext = aPresContext;
  mDOMNode = aNode;

  nsCOMPtr<nsIPresShell> shell;
  mPresContext->GetShell(getter_AddRefs(shell));
  nsIFrame* frame = nsnull;
  nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));
  shell->GetPrimaryFrameFor(content, &mFrame);

  InitDepth();
}

void nsFrameTreeWalker::SetNode(nsIDOMNode* aNode)
{
  mDOMNode = aNode;
  mNodeToFind = aNode;
  mDepthToFind = mDepth;
}

void nsFrameTreeWalker::InitDepth()
{
  if (!mFrame)
    return;

  nsIFrame* parent = nsnull;
  mFrame->GetParent(&parent);
  mDepth = 0;

  while(parent)
  {
    parent->GetParent(&parent);
    mDepth++;
  }
}

void nsFrameTreeWalker::GetBounds(nsRect& aBounds)
{
  mFrame->GetRect(aBounds);
}

PRBool nsFrameTreeWalker::GetParent()
{
  //printf("Get parent\n");

  if (!mFrame)
    return PR_FALSE;

  nsIFrame* parent = nsnull;
  mFrame->GetParent(&parent);

  // if no parent then we hit the root
  // just return that top frame
  if (!parent) {
    mFrame = nsnull;
    return PR_FALSE;
  }

  mFrame = parent;
  mDepth--;
  
  return GetParent();
}


PRBool nsFrameTreeWalker::GetNextSibling()
{
  //printf("Get next\n");

  if (!mFrame)
    return PR_FALSE;

  // get next sibling
  nsIFrame* next = nsnull;
  mFrame->GetNextSibling(&next);
  
  // if failed
  if (!next)
  {
    // if parent has content
    nsIFrame* parent = nsnull;
    mFrame->GetParent(&parent);
    
    // if no parent fail
    if (!parent) {
      mFrame = nsnull;
      return PR_FALSE;
    }

    // fail if we reach a parent that is accessible
    mFrame = parent;
    mDepth--;

    if (mDepth == mDepthToFind)
    {
      if (IsSameContent())
        return PR_TRUE;
      else {
        mFrame = nsnull;
        return PR_FALSE;
      }
    } else {
      // next on parent
      mFrame = parent;
      return GetNextSibling();
    }
  }

  mFrame = next;

  // if next has content
  if (mDepth == mDepthToFind)
  {
    if (IsSameContent())
      return PR_TRUE;
    else {
      mFrame = nsnull;
      return PR_FALSE;
    }
  }
  
  // if next doesn't have node

  // call first on next
  mFrame = next;
  if (GetFirstChild())
     return PR_TRUE;

  // call next on next
  mFrame = next;
  return GetNextSibling();
}

PRBool nsFrameTreeWalker::GetFirstChild()
{
  //printf("Get first\n");
  if (!mFrame)
    return PR_FALSE;

  // get first child
  nsIFrame* child = nsnull;
  mFrame->FirstChild(mPresContext, nsnull, &child);

  while(child)
  { 
    mFrame = child;

    mDepth++;

    // if the child has a content node done
    if (mDepth == mDepthToFind)
    {
      if (IsSameContent())
        return PR_TRUE;
      else {
        mFrame = nsnull;
        return PR_FALSE;
      }
    } else if (GetFirstChild()) // otherwise try first child
      return PR_TRUE;

    mDepth--;

    // get next sibling
    nsIFrame* next = nsnull;
    child->GetNextSibling(&next);

    child = next;
  }

  // fail
  mFrame = nsnull;
  mDepth = -1;
  return PR_FALSE;
}

PRBool nsFrameTreeWalker::IsSameContent()
{
  nsCOMPtr<nsIContent> c;
  mFrame->GetContent(getter_AddRefs(c));
  nsCOMPtr<nsIDOMNode> node(do_QueryInterface(c));
  if (node == mNodeToFind)
    return PR_TRUE;
  else
    return PR_FALSE;

}

//--------------

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
  nsRect GetBounds();
  nsIFrame* GetPrimaryFrame();

  PRBool GetAccessible();

  nsCOMPtr<nsIWeakReference> mPresShell;
  nsCOMPtr<nsIAccessible> mAccessible;
  nsCOMPtr<nsIDOMNode> mDOMNode;
};

nsDOMTreeWalker::nsDOMTreeWalker(nsIWeakReference* aPresShell, nsIDOMNode* aNode)
{
  mDOMNode = aNode;
  mAccessible = nsnull;
  mPresShell = aPresShell;
}

nsRect nsDOMTreeWalker::GetBounds()
{
  nsRect r(0,0,0,0);
  nsIFrame* frame = GetPrimaryFrame();
  if (frame)
    frame->GetRect(r);

  return r;
}

nsIFrame* nsDOMTreeWalker::GetPrimaryFrame()
{
  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mPresShell));
  nsIFrame* frame = nsnull;
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  shell->GetPrimaryFrameFor(content, &frame);
  return frame;
}

PRBool nsDOMTreeWalker::GetParent()
{
  //printf("Get parent\n");

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
    if (NS_SUCCEEDED(rv)) {
      mAccessible = acc;
      mDOMNode = do_QueryInterface(acc);
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
        nsCOMPtr<nsIAccessible> accessible(do_QueryInterface(frame));
        if (!accessible)
          accessible = do_QueryInterface(content);
        if (accessible) {
          nsCOMPtr<nsIWeakReference> wr = getter_AddRefs(NS_GetWeakReference(parentPresShell));
          nsCOMPtr<nsIDOMNode> node(do_QueryInterface(accessible));
          mAccessible = accessible;
          mDOMNode = node;
          mPresShell = wr;
          return PR_TRUE;
        }
      }

      mAccessible = new nsRootAccessible(mPresShell);
      mDOMNode = do_QueryInterface(mAccessible);
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
    if (NS_SUCCEEDED(rv)) {
      mAccessible = acc;
      mDOMNode = do_QueryInterface(acc);
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
    if (NS_SUCCEEDED(rv)) {
      mAccessible = acc;
      mDOMNode = do_QueryInterface(acc);
      return PR_TRUE;
    }
  }

  //printf("Get first\n");

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
  //printf("Get previous\n");

  nsCOMPtr<nsIDOMNode> child(mDOMNode);
  GetParent();
  
  return GetChildBefore(mDOMNode, child);
}

PRBool nsDOMTreeWalker::GetLastChild()
{
  //printf("Get last\n");

  return GetChildBefore(mDOMNode, nsnull);
}

PRInt32 nsDOMTreeWalker::GetCount()
{

  //printf("Get count\n");

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

PRBool nsDOMTreeWalker::GetAccessible()
{
  mAccessible = nsnull;

  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));

  if (!content)
    return PR_FALSE;

  nsCOMPtr<nsIDOMHTMLAreaElement> areaContent(do_QueryInterface(mDOMNode));
  if (areaContent)   // Area elements are implemented in nsHTMLImageAccessible as children of the image
    return PR_FALSE; // Return, otherwise the image frame looks like an accessible object in the wrong place

  nsIFrame* frame = GetPrimaryFrame();
  if (!frame)
    return PR_FALSE;

  mAccessible = do_QueryInterface(frame);
  if (!mAccessible)
    mAccessible = do_QueryInterface(mDOMNode);

  if (!mAccessible) {
    // is it a link?
    nsCOMPtr<nsILink> link(do_QueryInterface(mDOMNode));
    if (link) {
       nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mPresShell));
       mAccessible = new nsHTMLLinkAccessible(shell, mDOMNode);
    }
  }

  if (mAccessible)
    return PR_TRUE;
  else
    return PR_FALSE;
}


/*
 * Class nsAccessible
 */

//-----------------------------------------------------
// construction 
//-----------------------------------------------------
nsAccessible::nsAccessible(nsIAccessible* aAccessible, nsIDOMNode* aNode, nsIWeakReference* aShell)
{
  NS_INIT_REFCNT();

  // get frame and node
  mDOMNode = aNode;
  mAccessible = aAccessible;
  mPresShell = aShell;
     
  nsCOMPtr<nsIDocument> document;
  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mPresShell));
  if (shell)
    shell->GetDocument(getter_AddRefs(document));
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

NS_IMPL_ADDREF(nsAccessible);
NS_IMPL_RELEASE(nsAccessible);

//NS_IMPL_ISUPPORTS1(nsAccessible, nsIAccessible);
NS_INTERFACE_MAP_BEGIN(nsAccessible)
  NS_INTERFACE_MAP_ENTRY(nsIAccessible)
  if (aIID.Equals(NS_GET_IID(nsIDOMNode))) {
    nsIDOMNode* node = mDOMNode;
    *aInstancePtr = (void*) node;
    NS_ADDREF(node);
    return NS_OK;
  } else
NS_INTERFACE_MAP_END


nsresult nsAccessible::GetAccParent(nsIAccessible **  aAccParent)
{

  // delegate
  if (mAccessible) {
    nsresult rv = mAccessible->GetAccParent(aAccParent);
    if (NS_SUCCEEDED(rv))
      return rv;
  }

  nsCOMPtr<nsIPresContext> context;
  GetPresContext(context);

  nsDOMTreeWalker walker(mPresShell, mDOMNode); 
  if (walker.GetParent()) {
    *aAccParent = CreateNewParentAccessible(walker.mAccessible, walker.mDOMNode, walker.mPresShell);
    NS_ADDREF(*aAccParent);
    return NS_OK;
  }

  *aAccParent = nsnull;
  return NS_OK;
}

  /* readonly attribute nsIAccessible accNextSibling; */
NS_IMETHODIMP nsAccessible::GetAccNextSibling(nsIAccessible * *aAccNextSibling) 
{ 
  // delegate
 
  if (mAccessible) {
    nsresult rv = mAccessible->GetAccNextSibling(aAccNextSibling);
    if (NS_SUCCEEDED(rv))
      return rv;
  }

  // failed? Lets do some default behavior

  nsDOMTreeWalker walker(mPresShell, mDOMNode);

  walker.GetNextSibling();

  if (walker.mAccessible && walker.mDOMNode) 
  {
    *aAccNextSibling = CreateNewNextAccessible(walker.mAccessible, walker.mDOMNode, walker.mPresShell);
    NS_ADDREF(*aAccNextSibling);
    return NS_OK;
  }

  *aAccNextSibling = nsnull;

  return NS_OK;  
}

  /* readonly attribute nsIAccessible accPreviousSibling; */
NS_IMETHODIMP nsAccessible::GetAccPreviousSibling(nsIAccessible * *aAccPreviousSibling) 
{ 
// delegate
 
  if (mAccessible) {
    nsresult rv = mAccessible->GetAccPreviousSibling(aAccPreviousSibling);
    if (NS_SUCCEEDED(rv))
      return rv;
  }
 
  // failed? Lets do some default behavior
  nsDOMTreeWalker walker(mPresShell, mDOMNode); 
  walker.GetPreviousSibling();

  if (walker.mAccessible && walker.mDOMNode) 
  {
    *aAccPreviousSibling = CreateNewPreviousAccessible(walker.mAccessible, walker.mDOMNode, walker.mPresShell);
    NS_ADDREF(*aAccPreviousSibling);
    return NS_OK;
  }

  *aAccPreviousSibling = nsnull;

  return NS_OK;  
}

  /* readonly attribute nsIAccessible accFirstChild; */
NS_IMETHODIMP nsAccessible::GetAccFirstChild(nsIAccessible * *aAccFirstChild) 
{  
  // delegate
  if (mAccessible) {
    nsresult rv = mAccessible->GetAccFirstChild(aAccFirstChild);
    if (NS_SUCCEEDED(rv))
      return rv;
  }

  nsDOMTreeWalker walker(mPresShell, mDOMNode); 
  walker.GetFirstChild();

  if (walker.mAccessible && walker.mDOMNode) 
  {
    *aAccFirstChild = CreateNewFirstAccessible(walker.mAccessible, walker.mDOMNode, walker.mPresShell);
    NS_ADDREF(*aAccFirstChild);
    return NS_OK;
  }

  *aAccFirstChild = nsnull;

  return NS_OK;  
}

  /* readonly attribute nsIAccessible accFirstChild; */
NS_IMETHODIMP nsAccessible::GetAccLastChild(nsIAccessible * *aAccLastChild)
{  
  // delegate
  if (mAccessible) {
    nsresult rv = mAccessible->GetAccLastChild(aAccLastChild);
    if (NS_SUCCEEDED(rv))
      return rv;
  }

  nsDOMTreeWalker walker(mPresShell, mDOMNode); 
  walker.GetLastChild();

  if (walker.mAccessible && walker.mDOMNode) 
  {
    *aAccLastChild = CreateNewLastAccessible(walker.mAccessible, walker.mDOMNode, walker.mPresShell);
    NS_ADDREF(*aAccLastChild);
    return NS_OK;
  }

  *aAccLastChild = nsnull;

  return NS_OK;
}

/* readonly attribute long accChildCount; */
NS_IMETHODIMP nsAccessible::GetAccChildCount(PRInt32 *aAccChildCount) 
{
  
  // delegate
  if (mAccessible) {
    nsresult rv = mAccessible->GetAccChildCount(aAccChildCount);
    if (NS_SUCCEEDED(rv))
      return rv;
  }
  

  // failed? Lets do some default behavior

  nsDOMTreeWalker walker(mPresShell, mDOMNode); 
  *aAccChildCount = walker.GetCount();

  return NS_OK;  
}


  /* attribute wstring accName; */
NS_IMETHODIMP nsAccessible::GetAccName(PRUnichar * *aAccName) 
{ 
  // delegate
  if (mAccessible) {
    nsresult rv = mAccessible->GetAccName(aAccName);
    if (NS_SUCCEEDED(rv) && *aAccName != nsnull)
      return rv;
  }

  *aAccName = 0;
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRUint8 getAccNumActions (); */
NS_IMETHODIMP nsAccessible::GetAccNumActions(PRUint8 *_retval) 
{
  *_retval = 0;
  if (mAccessible)
    mAccessible->GetAccNumActions(_retval);
  return NS_OK;
}


nsresult nsAccessible::GetTranslatedString(PRUnichar *aKey, nsAWritableString *aStringOut)
{
  static nsCOMPtr<nsIStringBundle> stringBundle;
  static PRBool firstTime = PR_TRUE;

  if (firstTime) {
    firstTime = PR_FALSE;
    nsresult rv;
    NS_WITH_SERVICE(nsIStringBundleService, stringBundleService, kStringBundleServiceCID, &rv);
    // nsCOMPtr<nsIStringBundleService> stringBundleService(do_GetService(kStringBundleServiceCID, &rv));
    if (!stringBundleService) { 
      NS_WARNING("ERROR: Failed to get StringBundle Service instance.\n");
      return NS_ERROR_FAILURE;;
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


/* wstring getAccActionName (in PRUint8 index); */
NS_IMETHODIMP nsAccessible::GetAccActionName(PRUint8 index, PRUnichar **_retval)
{
  PRUint8 numActions;
  *_retval = 0;
  if (mAccessible && NS_SUCCEEDED(GetAccNumActions(&numActions)) && index<numActions) {
    nsresult rv = mAccessible->GetAccActionName(index, _retval);
    if (**_retval && NS_SUCCEEDED(rv)) {
      nsAutoString newString;
      rv = GetTranslatedString(*_retval, &newString);
      if (NS_SUCCEEDED(rv)) {
        delete *_retval;
        *_retval = newString.ToNewUnicode();
      }
    }
    return NS_OK;  // keep key for name if can't get translated version
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void accDoAction (in PRUint8 index); */
NS_IMETHODIMP nsAccessible::AccDoAction(PRUint8 index) 
{
  PRUint8 numActions;
  return (mAccessible && NS_SUCCEEDED(GetAccNumActions(&numActions)) && index<numActions)?
    mAccessible->AccDoAction(index): NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP nsAccessible::SetAccName(const PRUnichar * aAccName) 
{ 
  // delegate
  if (mAccessible) {
    nsresult rv = mAccessible->SetAccName(aAccName);
    if (NS_SUCCEEDED(rv))
      return rv;
  }

  return NS_ERROR_NOT_IMPLEMENTED;  
}

  /* attribute wstring accValue; */
NS_IMETHODIMP nsAccessible::GetAccValue(PRUnichar * *aAccValue) 
{ 
  // delegate
  if (mAccessible) {
    nsresult rv = mAccessible->GetAccValue(aAccValue);

    if (NS_SUCCEEDED(rv) && *aAccValue != nsnull)
      return rv;
  }

  *aAccValue = 0;
  return NS_ERROR_NOT_IMPLEMENTED;  
}

NS_IMETHODIMP nsAccessible::SetAccValue(const PRUnichar * aAccValue) { return NS_ERROR_NOT_IMPLEMENTED;  }

  /* readonly attribute wstring accDescription; */
NS_IMETHODIMP nsAccessible::GetAccDescription(PRUnichar * *aAccDescription) 
{ 
  // delegate
  if (mAccessible) {
    nsresult rv = mAccessible->GetAccDescription(aAccDescription);
    if (NS_SUCCEEDED(rv) && *aAccDescription != nsnull)
      return rv;
  }

  return NS_ERROR_NOT_IMPLEMENTED;  
}

  /* readonly attribute unsigned long accRole; */
NS_IMETHODIMP nsAccessible::GetAccRole(PRUint32  *aAccRole) 
{ 
  // delegate
  if (mAccessible) {
    nsresult rv = mAccessible->GetAccRole(aAccRole);
    if (NS_SUCCEEDED(rv))
      return rv;
  }

  return NS_ERROR_NOT_IMPLEMENTED;  
}

  /* readonly attribute wstring accState; */
NS_IMETHODIMP nsAccessible::GetAccState(PRUint32 *aAccState) 
{ 
  nsresult rv = NS_OK; 
  *aAccState = 0;

  // delegate
  if (mAccessible) 
    rv = mAccessible->GetAccState(aAccState);

  if (NS_SUCCEEDED(rv) && mFocusController) {
    nsCOMPtr<nsIDOMElement> focusedElement, currElement(do_QueryInterface(mDOMNode));
    mFocusController->GetFocusedElement(getter_AddRefs(focusedElement));
    if (focusedElement == currElement)
      *aAccState |= STATE_FOCUSED;
  }

  return rv;
}

NS_IMETHODIMP nsAccessible::GetAccExtState(PRUint32 *aAccExtState) 
{ 
  // delegate
  if (mAccessible) 
    return mAccessible->GetAccExtState(aAccExtState);

  return NS_ERROR_NOT_IMPLEMENTED;  
}

  /* readonly attribute wstring accHelp; */
NS_IMETHODIMP nsAccessible::GetAccHelp(PRUnichar * *aAccHelp) 
{ 
  // delegate
  if (mAccessible) {
    nsresult rv = mAccessible->GetAccHelp(aAccHelp);
    if (NS_SUCCEEDED(rv) && *aAccHelp != nsnull)
      return rv;
  }

  // failed? Lets do some default behavior
  return NS_ERROR_NOT_IMPLEMENTED;  
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

  /* void accNavigateRight (); */
NS_IMETHODIMP nsAccessible::AccNavigateRight(nsIAccessible **_retval) { return NS_OK;  }

  /* void navigateLeft (); */
NS_IMETHODIMP nsAccessible::AccNavigateLeft(nsIAccessible **_retval) { return NS_OK;  }

  /* void navigateUp (); */

NS_IMETHODIMP nsAccessible::AccNavigateUp(nsIAccessible **_retval) { return NS_OK;  }

  /* void navigateDown (); */
NS_IMETHODIMP nsAccessible::AccNavigateDown(nsIAccessible **_retval) { return NS_OK;  }



  /* void addSelection (); */
NS_IMETHODIMP nsAccessible::AccAddSelection(void) 
{ 
  // delegate
  if (mAccessible) {
    nsresult rv = mAccessible->AccAddSelection();
    if (NS_SUCCEEDED(rv))
      return rv;
  }

  return NS_ERROR_FAILURE;  
}

  /* void removeSelection (); */
NS_IMETHODIMP nsAccessible::AccRemoveSelection(void) 
{ 
  // delegate
  if (mAccessible) {
    nsresult rv = mAccessible->AccRemoveSelection();
    if (NS_SUCCEEDED(rv))
      return rv;
  }

  return NS_ERROR_FAILURE;  
}

  /* void extendSelection (); */
NS_IMETHODIMP nsAccessible::AccExtendSelection(void) 
{ 
  // delegate
  if (mAccessible) {
    nsresult rv = mAccessible->AccExtendSelection();
    if (NS_SUCCEEDED(rv))
      return rv;
  }

  return NS_ERROR_FAILURE;  
}

  /* void takeSelection (); */
NS_IMETHODIMP nsAccessible::AccTakeSelection(void) 
{ 
  // delegate
  if (mAccessible) {
    nsresult rv = mAccessible->AccTakeSelection();
    if (NS_SUCCEEDED(rv))
      return rv;
  }

  return NS_ERROR_FAILURE;  
}

  /* void takeFocus (); */
NS_IMETHODIMP nsAccessible::AccTakeFocus(void) 
{ 
  // delegate
  if (mAccessible) {
    nsresult rv = mAccessible->AccTakeFocus();
    if (NS_SUCCEEDED(rv))
      return rv;
  }

  return NS_ERROR_FAILURE;  
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

  *aContent = doc->GetRootContent(); // this addrefs
  return NS_OK;
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


void nsAccessible::GetBounds(nsRect& aBounds)
{
  nsCOMPtr<nsIPresContext> presContext;
  GetPresContext(presContext);

  nsDOMNodeBoundsFinder finder(presContext, mDOMNode);
  finder.GetBounds(aBounds);
}


/* void accGetBounds (out long x, out long y, out long width, out long height); */
NS_IMETHODIMP nsAccessible::AccGetBounds(PRInt32 *x, PRInt32 *y, PRInt32 *width, PRInt32 *height)
{
  // delegate
  if (mAccessible) {
    nsresult rv = mAccessible->AccGetBounds(x,y,width,height);
    if (NS_SUCCEEDED(rv))
      return rv;
  }

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
  GetBounds(unionRectTwips);   // Unions up all primary frames for this node and all siblings after it

  *x      = NSTwipsToIntPixels(unionRectTwips.x, t2p); 
  *y      = NSTwipsToIntPixels(unionRectTwips.y, t2p);
  *width  = NSTwipsToIntPixels(unionRectTwips.width, t2p);
  *height = NSTwipsToIntPixels(unionRectTwips.height, t2p);

  // We have the union of the rectangle, now we need to put it in absolute screen coords

  if (presContext) {
    nsIFrame* frame = GetBoundsFrame();   // Get our primary frame
    nsRect orgRectTwips, frameRectTwips, orgRectPixels;

    // Get the offset of this frame in screen coordinates
    if (frame && NS_SUCCEEDED(GetAbsoluteFramePosition(presContext, frame, orgRectTwips, orgRectPixels))) {
      frame->GetRect(frameRectTwips);   // Usually just the primary frame, but can be the choice list frame for an nsSelectAccessible
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

nsIFrame* nsAccessible::GetBoundsFrame()
{
  return GetFrame();
}

nsIFrame* nsAccessible::GetFrame()
{
  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mPresShell));
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

nsIAccessible* nsAccessible::CreateNewNextAccessible(nsIAccessible* aAccessible, nsIDOMNode* aNode, nsIWeakReference* aShell)
{
  return CreateNewAccessible(aAccessible, aNode, aShell);
}

nsIAccessible* nsAccessible::CreateNewPreviousAccessible(nsIAccessible* aAccessible, nsIDOMNode* aNode, nsIWeakReference* aShell)
{
  return CreateNewAccessible(aAccessible, aNode, aShell);
}

nsIAccessible* nsAccessible::CreateNewParentAccessible(nsIAccessible* aAccessible, nsIDOMNode* aNode, nsIWeakReference* aShell)
{
  return CreateNewAccessible(aAccessible, aNode, aShell);
}

nsIAccessible* nsAccessible::CreateNewFirstAccessible(nsIAccessible* aAccessible, nsIDOMNode* aNode, nsIWeakReference* aShell)
{
  return CreateNewAccessible(aAccessible, aNode, aShell);
}

nsIAccessible* nsAccessible::CreateNewLastAccessible(nsIAccessible* aAccessible, nsIDOMNode* aNode, nsIWeakReference* aShell)
{
  return CreateNewAccessible(aAccessible, aNode, aShell);
}

nsIAccessible* nsAccessible::CreateNewAccessible(nsIAccessible* aAccessible, nsIDOMNode* aNode, nsIWeakReference* aShell)
{
  NS_ASSERTION(aAccessible && aNode,"Error not accessible or content");
  return new nsAccessible(aAccessible, aNode, aShell);
}

// ------- nsHTMLBlockAccessible ------

nsHTMLBlockAccessible::nsHTMLBlockAccessible(nsIAccessible* aAccessible, nsIDOMNode* aNode, nsIWeakReference* aShell):nsAccessible(aAccessible, aNode, aShell)
{
}

nsIAccessible* nsHTMLBlockAccessible::CreateNewAccessible(nsIAccessible* aAccessible, nsIDOMNode* aNode, nsIWeakReference* aShell)
{
  NS_ASSERTION(aAccessible && aNode,"Error not accessible or content");
  return new nsHTMLBlockAccessible(aAccessible, aNode, aShell);
}

/* nsIAccessible accGetAt (in long x, in long y); */
NS_IMETHODIMP nsHTMLBlockAccessible::AccGetAt(PRInt32 tx, PRInt32 ty, nsIAccessible **_retval)
{
  // We're going to find the child that contains coordinates (tx,ty)
  PRInt32 x,y,w,h;
  AccGetBounds(&x,&y,&w,&h);  // Get bounds for this accessible
  if (tx > x && tx < x + w && ty > y && ty < y + h)
  {
    // It's within this nsIAccessible, let's drill down
    nsCOMPtr<nsIAccessible> child;
    nsCOMPtr<nsIAccessible> smallestChild;
    PRInt32 smallestArea = -1;
    nsCOMPtr<nsIAccessible> next;
    GetAccFirstChild(getter_AddRefs(child));
    PRInt32 cx,cy,cw,ch;  // Child bounds

    while(child) {
      child->AccGetBounds(&cx,&cy,&cw,&ch);
      
      // ok if there are multiple frames the contain the point 
      // and they overlap then pick the smallest. We need to do this
      // for text frames.
      
      // For example, A point that's in block #2 is also in block #1, but we want to return #2:
      //
      // [[block #1 is long wrapped text that continues to
      // another line]]  [[here is a shorter block #2]]

      if (tx > cx && tx < cx + cw && ty > cy && ty < cy + ch) 
      {
        if (smallestArea == -1 || cw*ch < smallestArea) {
          smallestArea = cw*ch;
          smallestChild = child;
        }
      }
      child->GetAccNextSibling(getter_AddRefs(next));
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





