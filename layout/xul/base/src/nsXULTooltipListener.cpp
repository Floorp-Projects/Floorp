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

#include "nsXULTooltipListener.h"

#include "nsIDOMMouseEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMXULDocument.h"
#include "nsIDOMXULElement.h"
#include "nsIDocument.h"
#include "nsXULAtoms.h"
#include "nsIPresShell.h"
#include "nsIFrame.h"
#include "nsIPopupBoxObject.h"
#include "nsIPrefBranch.h"
#include "nsIPrefBranchInternal.h"
#include "nsIPrefService.h"
#include "nsIServiceManager.h"
#include "nsITreeView.h"
#include "nsGUIEvent.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIPresContext.h"
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindowInternal.h"

//////////////////////////////////////////////////////////////////////////
//// nsISupports

nsXULTooltipListener::nsXULTooltipListener()
  : mRootBox(nsnull)
  , mSourceNode(nsnull)
  , mTargetNode(nsnull)
  , mCurrentTooltip(nsnull)
  , mTooltipTimer(nsnull)
  , mMouseClientX(0)
  , mMouseClientY(0)
  , mAutoHideTimer(nsnull)
  , mIsSourceTree(PR_FALSE)
  , mNeedTitletip(PR_FALSE)
  , mLastTreeRow(-1)
{
}

nsXULTooltipListener::~nsXULTooltipListener()
{
  HideTooltip();

  nsCOMPtr<nsIPrefBranchInternal> prefInternal(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (prefInternal) {
    // Unregister our pref observer
    prefInternal->RemoveObserver("browser.chrome.toolbar_tips", this);
  }
}

NS_IMPL_ADDREF(nsXULTooltipListener)
NS_IMPL_RELEASE(nsXULTooltipListener)

NS_INTERFACE_MAP_BEGIN(nsXULTooltipListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMouseListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMouseMotionListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMKeyListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMXULListener)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIDOMEventListener, nsIDOMMouseListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMMouseMotionListener)
NS_INTERFACE_MAP_END

//////////////////////////////////////////////////////////////////////////
//// nsIDOMMouseListener

NS_IMETHODIMP
nsXULTooltipListener::MouseDown(nsIDOMEvent* aMouseEvent)
{
  HideTooltip();

  return NS_OK;
}

NS_IMETHODIMP
nsXULTooltipListener::MouseOut(nsIDOMEvent* aMouseEvent)
{
  // if the timer is running and no tooltip is shown, we
  // have to cancel the timer here so that it doesn't 
  // show the tooltip if we move the mouse out of the window
  if (mTooltipTimer && !mCurrentTooltip) {
    mTooltipTimer->Cancel();
    return NS_OK;
  }

  if (mNeedTitletip)
    return NS_OK;

  // check to see if the mouse left the targetNode, and if so,
  // hide the tooltip
  if (mCurrentTooltip) {
    // which node did the mouse leave?
    nsCOMPtr<nsIDOMEventTarget> eventTarget;
    aMouseEvent->GetTarget(getter_AddRefs(eventTarget));
    nsCOMPtr<nsIDOMNode> targetNode(do_QueryInterface(eventTarget));

    // which node is our tooltip on?
    nsCOMPtr<nsIDocument> doc;
    mCurrentTooltip->GetDocument(*getter_AddRefs(doc));
    nsCOMPtr<nsIDOMXULDocument> xulDoc(do_QueryInterface(doc));
    if (!doc)        // remotely possible someone could have 
      return NS_OK;  // removed tooltip from dom while it was open
    nsCOMPtr<nsIDOMNode> tooltipNode;
    xulDoc->GetTooltipNode (getter_AddRefs(tooltipNode));

    // if they're the same, the mouse left the node the tooltip appeared on,
    // close the tooltip.
    if (tooltipNode == targetNode) {
      HideTooltip();

      // reset special tree tracking
      if (mIsSourceTree) {
        mLastTreeRow = -1;
        mLastTreeCol.Truncate();
      }
    }
  }

  return NS_OK;
}

//////////////////////////////////////////////////////////////////////////
//// nsIDOMMouseMotionListener

NS_IMETHODIMP
nsXULTooltipListener::MouseMove(nsIDOMEvent* aMouseEvent)
{
  if (!sShowTooltips)
    return NS_OK;

  // stash the coordinates of the event so that we can still get back to it from within the 
  // timer callback. On win32, we'll get a MouseMove event even when a popup goes away --
  // even when the mouse doesn't change position! To get around this, we make sure the
  // mouse has really moved before proceeding.
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent(do_QueryInterface(aMouseEvent));
  PRInt32 newMouseX, newMouseY;
  mouseEvent->GetClientX(&newMouseX);
  mouseEvent->GetClientY(&newMouseY);
  if (mMouseClientX == newMouseX && mMouseClientY == newMouseY)
    return NS_OK;
  mMouseClientX = newMouseX;
  mMouseClientY = newMouseY;

  if (mIsSourceTree)
    CheckTreeBodyMove(mouseEvent);

  // as the mouse moves, we want to make sure we reset the timer to show it, 
  // so that the delay is from when the mouse stops moving, not when it enters
  // the node.
  KillTooltipTimer();
    
  // If the mouse moves while the tooltip is up, don't do anything. We make it
  // go away only if it times out or leaves the target node. If nothing is
  // showing, though, we have to do the work.
  if (!mCurrentTooltip) {
    if (!mTooltipTimer) // re-use, if available
      mTooltipTimer = do_CreateInstance("@mozilla.org/timer;1");
    if (mTooltipTimer) {
      nsCOMPtr<nsIDOMEventTarget> eventTarget;
      aMouseEvent->GetTarget(getter_AddRefs(eventTarget));
      if (eventTarget) {
        nsCOMPtr<nsIContent> targetContent(do_QueryInterface(eventTarget));
        mTargetNode = targetContent;
      }
      if (mTargetNode) {
        nsresult rv = mTooltipTimer->InitWithFuncCallback(sTooltipCallback, this, 
                                                          kTooltipShowTime, nsITimer::TYPE_ONE_SHOT);
        if (NS_FAILED(rv))
          mTargetNode = nsnull;
      }
    }
  }

  return NS_OK;
}

//////////////////////////////////////////////////////////////////////////
//// nsIDOMKeyListener

NS_IMETHODIMP
nsXULTooltipListener::KeyDown(nsIDOMEvent* aKeyEvent)
{
  HideTooltip();
  return NS_OK;
}

//////////////////////////////////////////////////////////////////////////
//// nsIDOMEventListener


NS_IMETHODIMP
nsXULTooltipListener::HandleEvent(nsIDOMEvent* aEvent)
{
  nsAutoString type;
  aEvent->GetType(type);
  if (type.Equals(NS_LITERAL_STRING("DOMMouseScroll")))
    HideTooltip();
  return NS_OK;
}

//////////////////////////////////////////////////////////////////////////
//// nsIObserver

NS_IMETHODIMP
nsXULTooltipListener::Observe(nsISupports* aSubject, 
                              const char* aTopic,
                              const PRUnichar* aData)
{
  if (nsCRT::strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    NS_ERROR("unknown nsIObserver topic!");
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsIPrefBranch> prefBranch(do_QueryInterface(aSubject));
  NS_ASSERTION(prefBranch, "Pref change topic with no pref subject?");
  prefBranch->GetBoolPref("browser.chrome.toolbar_tips", &sShowTooltips);

  return NS_OK;
}

//////////////////////////////////////////////////////////////////////////
//// nsXULTooltipListener

NS_IMETHODIMP
nsXULTooltipListener::PopupHiding(nsIDOMEvent* aEvent)
{
  DestroyTooltip();
  return NS_OK;
}

//////////////////////////////////////////////////////////////////////////
//// nsXULTooltipListener

PRBool nsXULTooltipListener::sShowTooltips = PR_FALSE;

nsresult
nsXULTooltipListener::Init(nsIContent* aSourceNode, nsIRootBox* aRootBox)
{
  mRootBox = aRootBox;
  mSourceNode = aSourceNode;
  AddTooltipSupport(aSourceNode);
  
  // if the target is an treechildren, we may have some special
  // case handling to do
  nsCOMPtr<nsIAtom> tag;
  mSourceNode->GetTag(*getter_AddRefs(tag));
  mIsSourceTree = tag == nsXULAtoms::treechildren;

  static PRBool prefChangeRegistered = PR_FALSE;

  // Only the first time, register the callback and get the initial value of the pref
  if (!prefChangeRegistered) {
    nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID));  
    if (prefBranch) {
      // get the initial value of the pref
      nsresult rv = prefBranch->GetBoolPref("browser.chrome.toolbar_tips", &sShowTooltips);
      if (NS_SUCCEEDED(rv)) {
        // register the callback so we get notified of updates
        nsCOMPtr<nsIPrefBranchInternal> prefInternal(do_QueryInterface(prefBranch));
        rv = prefInternal->AddObserver("browser.chrome.toolbar_tips", this, PR_FALSE);
        if (NS_SUCCEEDED(rv))
          prefChangeRegistered = PR_TRUE;
      }
    }
  }

  return NS_OK;
}

nsresult
nsXULTooltipListener::AddTooltipSupport(nsIContent* aNode)
{
  if (!aNode)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDOMEventTarget> evtTarget(do_QueryInterface(aNode));
  evtTarget->AddEventListener(NS_LITERAL_STRING("mouseout"), (nsIDOMMouseListener*)this, PR_FALSE);
  evtTarget->AddEventListener(NS_LITERAL_STRING("mousemove"), (nsIDOMMouseListener*)this, PR_FALSE);
  
  return NS_OK;
}

nsresult
nsXULTooltipListener::RemoveTooltipSupport(nsIContent* aNode)
{
  if (!aNode)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDOMEventTarget> evtTarget(do_QueryInterface(aNode));
  evtTarget->RemoveEventListener(NS_LITERAL_STRING("mouseout"), (nsIDOMMouseListener*)this, PR_FALSE);
  evtTarget->RemoveEventListener(NS_LITERAL_STRING("mousemove"), (nsIDOMMouseListener*)this, PR_FALSE);

  return NS_OK;
}

void
nsXULTooltipListener::CheckTreeBodyMove(nsIDOMMouseEvent* aMouseEvent)
{
  nsCOMPtr<nsITreeBoxObject> obx;
  GetSourceTreeBoxObject(getter_AddRefs(obx));
  if (obx) {
    PRInt32 x, y;
    aMouseEvent->GetClientX(&x);
    aMouseEvent->GetClientY(&y);
    PRInt32 row;
    nsXPIDLString colId, obj;

    obx->GetCellAt(x, y, &row, getter_Copies(colId), getter_Copies(obj));
    
    // determine if we are going to need a titletip
    // XXX check the disabletitletips attribute on the tree content
    mNeedTitletip = PR_FALSE;
#ifdef DEBUG_crap
    if (row >= 0 && obj.Equals(NS_LITERAL_STRING("text"))) {
      nsCOMPtr<nsITreeView> view;
      obx->GetView(getter_AddRefs(view));
      PRBool isCropped;
      obx->IsCellCropped(row, colId, &isCropped);
      mNeedTitletip = isCropped;
    }
#endif

    if (mCurrentTooltip && 
        (row != mLastTreeRow || !mLastTreeCol.Equals(colId))) {
      HideTooltip();
    } 

    mLastTreeRow = row;
    mLastTreeCol.Assign(colId);
  }
}

nsresult
nsXULTooltipListener::ShowTooltip()
{
  // get the tooltip content designated for the target node 
  GetTooltipFor(mSourceNode, getter_AddRefs(mCurrentTooltip));
  if (!mCurrentTooltip)
    return NS_ERROR_FAILURE; // the target node doesn't need a tooltip

  // set the node in the document that triggered the tooltip and show it
  nsCOMPtr<nsIDocument> doc;
  mCurrentTooltip->GetDocument(*getter_AddRefs(doc));
  nsCOMPtr<nsIDOMXULDocument> xulDoc(do_QueryInterface(doc));
  if (xulDoc) {
    // Make sure the target node is still attached to some document. 
    // It might have been deleted.
    nsCOMPtr<nsIDocument> targetDoc;
    mSourceNode->GetDocument(*getter_AddRefs(targetDoc));
    if (targetDoc) {
      if (!mIsSourceTree) {
        mLastTreeRow = -1;
        mLastTreeCol.Truncate();
      }

      nsCOMPtr<nsIDOMNode> targetNode(do_QueryInterface(mTargetNode));
      xulDoc->SetTooltipNode(targetNode);
      LaunchTooltip(mSourceNode, mMouseClientX, mMouseClientY);
      mTargetNode = nsnull;

      // at this point, |mCurrentTooltip| holds the content node of
      // the tooltip. If there is an attribute on the popup telling us
      // not to create the auto-hide timer, don't.
      nsCOMPtr<nsIDOMElement> tooltipEl(do_QueryInterface(mCurrentTooltip));
      if (!tooltipEl)
        return NS_ERROR_FAILURE;
      nsAutoString noAutoHide;
      tooltipEl->GetAttribute(NS_LITERAL_STRING("noautohide"), noAutoHide);
      if (noAutoHide != NS_LITERAL_STRING("true"))
        CreateAutoHideTimer();

      // listen for popuphidden on the tooltip node, so that we can
      // be sure DestroyPopup is called even if someone else closes the tooltip
      nsCOMPtr<nsIDOMEventTarget> evtTarget(do_QueryInterface(mCurrentTooltip));
      evtTarget->AddEventListener(NS_LITERAL_STRING("popuphiding"), 
                                  (nsIDOMMouseListener*)this, PR_FALSE);

      // listen for mousedown,keydown, and DOMMouseScroll events at document level
      nsCOMPtr<nsIDocument> doc;
      mSourceNode->GetDocument(*getter_AddRefs(doc));
      if (doc) {
        evtTarget = do_QueryInterface(doc);
        evtTarget->AddEventListener(NS_LITERAL_STRING("DOMMouseScroll"), 
                                    (nsIDOMMouseListener*)this, PR_TRUE);
        evtTarget->AddEventListener(NS_LITERAL_STRING("mousedown"), 
                                    (nsIDOMMouseListener*)this, PR_TRUE);
        evtTarget->AddEventListener(NS_LITERAL_STRING("keydown"), 
                                    (nsIDOMMouseListener*)this, PR_TRUE);
      }
    }
  }

  return NS_OK;
}

static void
GetTreeCellCoords(nsITreeBoxObject* aTreeBox, nsIContent* aSourceNode, 
                  PRInt32 aRow, nsAutoString aCol, PRInt32* aX, PRInt32* aY)
{
  PRInt32 junk;
  const PRUnichar empty[] = {'\0'};
  aTreeBox->GetCoordsForCellItem(aRow, aCol.get(), empty, aX, aY, &junk, &junk);
  nsCOMPtr<nsIDOMXULElement> xulEl(do_QueryInterface(aSourceNode));
  nsCOMPtr<nsIBoxObject> bx;
  xulEl->GetBoxObject(getter_AddRefs(bx));
  PRInt32 myX, myY;
  bx->GetX(&myX);
  bx->GetY(&myY);
  *aX += myX;
  *aY += myY;
}

static void
SetTitletipLabel(nsITreeBoxObject* aTreeBox, nsIContent* aTooltip,
                 PRInt32 aRow, nsAutoString aCol)
{
  nsCOMPtr<nsITreeView> view;
  aTreeBox->GetView(getter_AddRefs(view));

  nsAutoString label;
  view->GetCellText(aRow, aCol.get(), label);
  
  aTooltip->SetAttr(nsnull, nsXULAtoms::label, label, PR_FALSE);
}

nsresult
nsXULTooltipListener::LaunchTooltip(nsIContent* aTarget, PRInt32 aX, PRInt32 aY)
{
  if (!mCurrentTooltip)
    return NS_OK;

  nsCOMPtr<nsIBoxObject> popupBox;
  nsCOMPtr<nsIDOMXULElement> xulTooltipEl(do_QueryInterface(mCurrentTooltip));
  if (!xulTooltipEl) {
    NS_ERROR("tooltip isn't a XUL element!");
    return NS_ERROR_FAILURE;
  }

  xulTooltipEl->GetBoxObject(getter_AddRefs(popupBox));
  nsCOMPtr<nsIPopupBoxObject> popupBoxObject(do_QueryInterface(popupBox));
  if (popupBoxObject) {
    PRInt32 x = aX;
    PRInt32 y = aY;
    if (mNeedTitletip) {
      nsCOMPtr<nsITreeBoxObject> obx;
      GetSourceTreeBoxObject(getter_AddRefs(obx));
      GetTreeCellCoords(obx, mSourceNode,
                            mLastTreeRow, mLastTreeCol, &x, &y);

      SetTitletipLabel(obx, mCurrentTooltip, mLastTreeRow, mLastTreeCol);
      mCurrentTooltip->SetAttr(nsnull, nsXULAtoms::titletip, NS_LITERAL_STRING("true"), PR_FALSE);
    } else
      mCurrentTooltip->UnsetAttr(nsnull, nsXULAtoms::titletip, PR_FALSE);

    nsCOMPtr<nsIDOMElement> targetEl(do_QueryInterface(aTarget));
    popupBoxObject->ShowPopup(targetEl, xulTooltipEl, x, y,
                              NS_LITERAL_STRING("tooltip").get(),
                              NS_LITERAL_STRING("none").get(),
                              NS_LITERAL_STRING("topleft").get());
  }

  return NS_OK;
}

nsresult
nsXULTooltipListener::HideTooltip()
{
  if (mCurrentTooltip) {
    // hide the popup through its box object
    nsCOMPtr<nsIDOMXULElement> tooltipEl(do_QueryInterface(mCurrentTooltip));
    nsCOMPtr<nsIBoxObject> boxObject;
    if (tooltipEl)
      tooltipEl->GetBoxObject(getter_AddRefs(boxObject));
    nsCOMPtr<nsIPopupBoxObject> popupObject(do_QueryInterface(boxObject));
    if (popupObject)
      popupObject->HidePopup();
  }

  DestroyTooltip();
  return NS_OK;
}

static void
GetImmediateChild(nsIContent* aContent, nsIAtom *aTag, nsIContent** aResult) 
{
  *aResult = nsnull;
  PRInt32 childCount;
  aContent->ChildCount(childCount);
  for (PRInt32 i = 0; i < childCount; i++) {
    nsCOMPtr<nsIContent> child;
    aContent->ChildAt(i, *getter_AddRefs(child));
    nsCOMPtr<nsIAtom> tag;
    child->GetTag(*getter_AddRefs(tag));
    if (aTag == tag.get()) {
      *aResult = child;
      return;
    }
  }

  return;
}

nsresult
nsXULTooltipListener::GetTooltipFor(nsIContent* aTarget, nsIContent** aTooltip)
{
  if (!aTarget)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDOMElement> targetEl(do_QueryInterface(aTarget));
  if (!targetEl)
    return NS_ERROR_FAILURE; // could be a text node or something

  // before we go on, make sure that target node still has a window
  nsCOMPtr<nsIDocument> document;
  if (NS_FAILED(aTarget->GetDocument(*getter_AddRefs(document))) || !document) {
    NS_ERROR("Unable to retrieve the tooltip node document.");
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIScriptContext> context;
  nsCOMPtr<nsIScriptGlobalObject> global;
  document->GetScriptGlobalObject(getter_AddRefs(global));
  if (global) {
    if (NS_SUCCEEDED(global->GetContext(getter_AddRefs(context))) && context) {
      nsCOMPtr<nsIDOMWindowInternal> domWindow = do_QueryInterface(global);
      if (!domWindow)
        return NS_ERROR_FAILURE;
      else {
        PRBool needTooltip;
        targetEl->HasAttribute(NS_LITERAL_STRING("tooltiptext"), &needTooltip);
        if (needTooltip) {
          // specifying tooltiptext means we will always use the default tooltip
           mRootBox->GetDefaultTooltip(aTooltip);
           NS_IF_ADDREF(*aTooltip);
           return NS_OK;
        } else {
          nsAutoString tooltipId;
          targetEl->GetAttribute(NS_LITERAL_STRING("tooltip"), tooltipId);

          // if tooltip == _child, look for first <tooltip> child
          if (tooltipId.Equals(NS_LITERAL_STRING("_child"))) {
            GetImmediateChild(aTarget, nsXULAtoms::tooltip, aTooltip); // this addrefs
            NS_IF_ADDREF(*aTooltip);
            return NS_OK;
          } else {
            if (!tooltipId.IsEmpty()) {
              // tooltip must be an id, use getElementById to find it
              nsCOMPtr<nsIDOMXULDocument> xulDocument = do_QueryInterface(document);
              if (!xulDocument) {
                NS_ERROR("tooltip attached to an element that isn't in XUL!");
                return NS_ERROR_FAILURE;
              }

              nsCOMPtr<nsIDOMElement> tooltipEl;
              xulDocument->GetElementById(tooltipId, getter_AddRefs(tooltipEl));

              if (tooltipEl) {
                mNeedTitletip = PR_FALSE;

                nsCOMPtr<nsIContent> tooltipContent(do_QueryInterface(tooltipEl));
                *aTooltip = tooltipContent;
                NS_IF_ADDREF(*aTooltip);

                return NS_OK;
              }
            }
          }
        }

        // titletips should just use the default tooltip
        if (mIsSourceTree && mNeedTitletip) {
          mRootBox->GetDefaultTooltip(aTooltip);
          NS_IF_ADDREF(*aTooltip);
          return NS_OK;
        }
      }
    }
  }

  return NS_OK;
}

nsresult
nsXULTooltipListener::DestroyTooltip()
{
  if (mCurrentTooltip) {
    // clear out the tooltip node on the document
    nsCOMPtr<nsIDocument> doc;
    mCurrentTooltip->GetDocument(*getter_AddRefs(doc));
    if (doc) {
      nsCOMPtr<nsIDOMXULDocument> xulDoc(do_QueryInterface(doc));
      if (xulDoc)
        xulDoc->SetTooltipNode(nsnull);

      // remove the mousedown and keydown listener from document
      nsCOMPtr<nsIDOMEventTarget> evtTarget(do_QueryInterface(doc));
      evtTarget->RemoveEventListener(NS_LITERAL_STRING("DOMMouseScroll"), (nsIDOMMouseListener*)this, PR_TRUE);
      evtTarget->RemoveEventListener(NS_LITERAL_STRING("mousedown"), (nsIDOMMouseListener*)this, PR_TRUE);
      evtTarget->RemoveEventListener(NS_LITERAL_STRING("keydown"), (nsIDOMMouseListener*)this, PR_TRUE);
    }

    // remove the popuphidden listener from tooltip
    nsCOMPtr<nsIDOMEventTarget> evtTarget(do_QueryInterface(mCurrentTooltip));

    // release tooltip before removing listener to prevent our destructor from
    // being called recursively (bug 120863)
    mCurrentTooltip = nsnull;

    evtTarget->RemoveEventListener(NS_LITERAL_STRING("popuphiding"), (nsIDOMMouseListener*)this, PR_FALSE);
  }
  
  // kill any ongoing timers
  KillTooltipTimer();
  if (mAutoHideTimer) {
    mAutoHideTimer->Cancel();
  }

  return NS_OK;
}

void
nsXULTooltipListener::KillTooltipTimer()
{
  if (mTooltipTimer) {
    mTooltipTimer->Cancel();
    mTargetNode = nsnull;
  }
}

void
nsXULTooltipListener::CreateAutoHideTimer()
{
  if (mAutoHideTimer) {
    mAutoHideTimer->Cancel();
  }
  
  if (!mAutoHideTimer) // re-use, if available
    mAutoHideTimer = do_CreateInstance("@mozilla.org/timer;1");
  if (mAutoHideTimer)
    mAutoHideTimer->InitWithFuncCallback(sAutoHideCallback, this, 
                                         kTooltipAutoHideTime, 
                                         nsITimer::TYPE_ONE_SHOT);
}

void
nsXULTooltipListener::sTooltipCallback(nsITimer *aTimer, void *aListener)
{
  nsXULTooltipListener* self = NS_STATIC_CAST(nsXULTooltipListener*, aListener);
  if (self)
    self->ShowTooltip();
}

void
nsXULTooltipListener::sAutoHideCallback(nsITimer *aTimer, void* aListener)
{
  nsXULTooltipListener* self = NS_STATIC_CAST(nsXULTooltipListener*, aListener);
  if (self)
    self->HideTooltip();
}

nsresult
nsXULTooltipListener::GetSourceTreeBoxObject(nsITreeBoxObject** aBoxObject)
{
  *aBoxObject = nsnull;

  if (mIsSourceTree && mSourceNode) {
    nsCOMPtr<nsIContent> treeParent;
    mSourceNode->GetParent(*getter_AddRefs(treeParent));
    nsCOMPtr<nsIDOMXULElement> xulEl(do_QueryInterface(treeParent));
    if (xulEl) {
      nsCOMPtr<nsIBoxObject> bx;
      xulEl->GetBoxObject(getter_AddRefs(bx));
      nsCOMPtr<nsITreeBoxObject> obx(do_QueryInterface(bx));
      if (obx) {
        *aBoxObject = obx;
        NS_ADDREF(*aBoxObject);
        return NS_OK;
      }
    }
  }
  return NS_ERROR_FAILURE;
}
