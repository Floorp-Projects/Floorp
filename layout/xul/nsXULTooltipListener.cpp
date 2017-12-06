/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXULTooltipListener.h"

#include "nsIDOMMouseEvent.h"
#include "nsIDOMXULDocument.h"
#include "nsXULElement.h"
#include "nsIDocument.h"
#include "nsGkAtoms.h"
#include "nsMenuPopupFrame.h"
#include "nsIServiceManager.h"
#include "nsIDragService.h"
#include "nsIDragSession.h"
#ifdef MOZ_XUL
#include "nsITreeView.h"
#endif
#include "nsIScriptContext.h"
#include "nsPIDOMWindow.h"
#ifdef MOZ_XUL
#include "nsXULPopupManager.h"
#endif
#include "nsIRootBox.h"
#include "nsIBoxObject.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/Preferences.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h" // for nsIDOMEvent::InternalDOMEvent()
#include "mozilla/dom/BoxObject.h"
#include "mozilla/TextEvents.h"

using namespace mozilla;
using namespace mozilla::dom;

nsXULTooltipListener* nsXULTooltipListener::mInstance = nullptr;

//////////////////////////////////////////////////////////////////////////
//// nsISupports

nsXULTooltipListener::nsXULTooltipListener()
  : mMouseScreenX(0)
  , mMouseScreenY(0)
  , mTooltipShownOnce(false)
#ifdef MOZ_XUL
  , mIsSourceTree(false)
  , mNeedTitletip(false)
  , mLastTreeRow(-1)
#endif
{
  if (sTooltipListenerCount++ == 0) {
    // register the callback so we get notified of updates
    Preferences::RegisterCallback(ToolbarTipsPrefChanged,
                                  "browser.chrome.toolbar_tips");

    // Call the pref callback to initialize our state.
    ToolbarTipsPrefChanged("browser.chrome.toolbar_tips", nullptr);
  }
}

nsXULTooltipListener::~nsXULTooltipListener()
{
  if (nsXULTooltipListener::mInstance == this) {
    ClearTooltipCache();
  }
  HideTooltip();

  if (--sTooltipListenerCount == 0) {
    // Unregister our pref observer
    Preferences::UnregisterCallback(ToolbarTipsPrefChanged,
                                    "browser.chrome.toolbar_tips");
  }
}

NS_IMPL_ISUPPORTS(nsXULTooltipListener, nsIDOMEventListener)

void
nsXULTooltipListener::MouseOut(nsIDOMEvent* aEvent)
{
  // reset flag so that tooltip will display on the next MouseMove
  mTooltipShownOnce = false;

  // if the timer is running and no tooltip is shown, we
  // have to cancel the timer here so that it doesn't
  // show the tooltip if we move the mouse out of the window
  nsCOMPtr<nsIContent> currentTooltip = do_QueryReferent(mCurrentTooltip);
  if (mTooltipTimer && !currentTooltip) {
    mTooltipTimer->Cancel();
    mTooltipTimer = nullptr;
    return;
  }

#ifdef DEBUG_crap
  if (mNeedTitletip)
    return;
#endif

#ifdef MOZ_XUL
  // check to see if the mouse left the targetNode, and if so,
  // hide the tooltip
  if (currentTooltip) {
    // which node did the mouse leave?
    nsCOMPtr<nsIDOMNode> targetNode = do_QueryInterface(
      aEvent->InternalDOMEvent()->GetTarget());

    nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
    if (pm) {
      nsCOMPtr<nsIDOMNode> tooltipNode =
        pm->GetLastTriggerTooltipNode(currentTooltip->GetUncomposedDoc());
      if (tooltipNode == targetNode) {
        // if the target node is the current tooltip target node, the mouse
        // left the node the tooltip appeared on, so close the tooltip.
        HideTooltip();
        // reset special tree tracking
        if (mIsSourceTree) {
          mLastTreeRow = -1;
          mLastTreeCol = nullptr;
        }
      }
    }
  }
#endif
}

void
nsXULTooltipListener::MouseMove(nsIDOMEvent* aEvent)
{
  if (!sShowTooltips)
    return;

  // stash the coordinates of the event so that we can still get back to it from within the
  // timer callback. On win32, we'll get a MouseMove event even when a popup goes away --
  // even when the mouse doesn't change position! To get around this, we make sure the
  // mouse has really moved before proceeding.
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent(do_QueryInterface(aEvent));
  if (!mouseEvent)
    return;
  int32_t newMouseX, newMouseY;
  mouseEvent->GetScreenX(&newMouseX);
  mouseEvent->GetScreenY(&newMouseY);

  // filter out false win32 MouseMove event
  if (mMouseScreenX == newMouseX && mMouseScreenY == newMouseY)
    return;

  // filter out minor movements due to crappy optical mice and shaky hands
  // to prevent tooltips from hiding prematurely.
  nsCOMPtr<nsIContent> currentTooltip = do_QueryReferent(mCurrentTooltip);

  if ((currentTooltip) &&
      (abs(mMouseScreenX - newMouseX) <= kTooltipMouseMoveTolerance) &&
      (abs(mMouseScreenY - newMouseY) <= kTooltipMouseMoveTolerance))
    return;
  mMouseScreenX = newMouseX;
  mMouseScreenY = newMouseY;

  nsCOMPtr<nsIContent> sourceContent = do_QueryInterface(
    aEvent->InternalDOMEvent()->GetCurrentTarget());
  mSourceNode = do_GetWeakReference(sourceContent);
#ifdef MOZ_XUL
  mIsSourceTree = sourceContent->IsXULElement(nsGkAtoms::treechildren);
  if (mIsSourceTree)
    CheckTreeBodyMove(mouseEvent);
#endif

  // as the mouse moves, we want to make sure we reset the timer to show it,
  // so that the delay is from when the mouse stops moving, not when it enters
  // the node.
  KillTooltipTimer();

  // If the mouse moves while the tooltip is up, hide it. If nothing is
  // showing and the tooltip hasn't been displayed since the mouse entered
  // the node, then start the timer to show the tooltip.
  if (!currentTooltip && !mTooltipShownOnce) {
    nsCOMPtr<EventTarget> eventTarget = aEvent->InternalDOMEvent()->GetTarget();

    // don't show tooltips attached to elements outside of a menu popup
    // when hovering over an element inside it. The popupsinherittooltip
    // attribute may be used to disable this behaviour, which is useful for
    // large menu hierarchies such as bookmarks.
    if (!sourceContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::popupsinherittooltip,
                                    nsGkAtoms::_true, eCaseMatters)) {
      nsCOMPtr<nsIContent> targetContent = do_QueryInterface(eventTarget);
      while (targetContent && targetContent != sourceContent) {
        if (targetContent->IsAnyOfXULElements(nsGkAtoms::menupopup,
                                              nsGkAtoms::panel,
                                              nsGkAtoms::tooltip)) {
          mSourceNode = nullptr;
          return;
        }

        targetContent = targetContent->GetParent();
      }
    }

    mTargetNode = do_GetWeakReference(eventTarget);
    if (mTargetNode) {
      nsresult rv = NS_NewTimerWithFuncCallback(
        getter_AddRefs(mTooltipTimer),
        sTooltipCallback, this,
        LookAndFeel::GetInt(LookAndFeel::eIntID_TooltipDelay, 500),
        nsITimer::TYPE_ONE_SHOT,
        "sTooltipCallback",
        sourceContent->OwnerDoc()->EventTargetFor(TaskCategory::Other));
      if (NS_FAILED(rv)) {
        mTargetNode = nullptr;
        mSourceNode = nullptr;
      }
    }
    return;
  }

#ifdef MOZ_XUL
  if (mIsSourceTree)
    return;
#endif

  HideTooltip();
  // set a flag so that the tooltip is only displayed once until the mouse
  // leaves the node
  mTooltipShownOnce = true;
}

NS_IMETHODIMP
nsXULTooltipListener::HandleEvent(nsIDOMEvent* aEvent)
{
  nsAutoString type;
  aEvent->GetType(type);
  if (type.EqualsLiteral("DOMMouseScroll") ||
      type.EqualsLiteral("mousedown") ||
      type.EqualsLiteral("mouseup") ||
      type.EqualsLiteral("dragstart")) {
    HideTooltip();
    return NS_OK;
  }

  if (type.EqualsLiteral("keydown")) {
    // Hide the tooltip if a non-modifier key is pressed.
    WidgetKeyboardEvent* keyEvent = aEvent->WidgetEventPtr()->AsKeyboardEvent();
    if (!keyEvent->IsModifierKeyEvent()) {
      HideTooltip();
    }

    return NS_OK;
  }

  if (type.EqualsLiteral("popuphiding")) {
    DestroyTooltip();
    return NS_OK;
  }

  // Note that mousemove, mouseover and mouseout might be
  // fired even during dragging due to widget's bug.
  nsCOMPtr<nsIDragService> dragService =
    do_GetService("@mozilla.org/widget/dragservice;1");
  NS_ENSURE_TRUE(dragService, NS_OK);
  nsCOMPtr<nsIDragSession> dragSession;
  dragService->GetCurrentSession(getter_AddRefs(dragSession));
  if (dragSession) {
    return NS_OK;
  }

  // Not dragging.

  if (type.EqualsLiteral("mousemove")) {
    MouseMove(aEvent);
    return NS_OK;
  }

  if (type.EqualsLiteral("mouseout")) {
    MouseOut(aEvent);
    return NS_OK;
  }

  return NS_OK;
}

//////////////////////////////////////////////////////////////////////////
//// nsXULTooltipListener

// static
void
nsXULTooltipListener::ToolbarTipsPrefChanged(const char *aPref,
                                             void *aClosure)
{
  sShowTooltips =
    Preferences::GetBool("browser.chrome.toolbar_tips", sShowTooltips);
}

//////////////////////////////////////////////////////////////////////////
//// nsXULTooltipListener

bool nsXULTooltipListener::sShowTooltips = false;
uint32_t nsXULTooltipListener::sTooltipListenerCount = 0;

nsresult
nsXULTooltipListener::AddTooltipSupport(nsIContent* aNode)
{
  if (!aNode)
    return NS_ERROR_NULL_POINTER;

  aNode->AddSystemEventListener(NS_LITERAL_STRING("mouseout"), this,
                                false, false);
  aNode->AddSystemEventListener(NS_LITERAL_STRING("mousemove"), this,
                                false, false);
  aNode->AddSystemEventListener(NS_LITERAL_STRING("mousedown"), this,
                                false, false);
  aNode->AddSystemEventListener(NS_LITERAL_STRING("mouseup"), this,
                                false, false);
  aNode->AddSystemEventListener(NS_LITERAL_STRING("dragstart"), this,
                                true, false);

  return NS_OK;
}

nsresult
nsXULTooltipListener::RemoveTooltipSupport(nsIContent* aNode)
{
  if (!aNode)
    return NS_ERROR_NULL_POINTER;

  aNode->RemoveSystemEventListener(NS_LITERAL_STRING("mouseout"), this, false);
  aNode->RemoveSystemEventListener(NS_LITERAL_STRING("mousemove"), this, false);
  aNode->RemoveSystemEventListener(NS_LITERAL_STRING("mousedown"), this, false);
  aNode->RemoveSystemEventListener(NS_LITERAL_STRING("mouseup"), this, false);
  aNode->RemoveSystemEventListener(NS_LITERAL_STRING("dragstart"), this, true);

  return NS_OK;
}

#ifdef MOZ_XUL
void
nsXULTooltipListener::CheckTreeBodyMove(nsIDOMMouseEvent* aMouseEvent)
{
  nsCOMPtr<nsIContent> sourceNode = do_QueryReferent(mSourceNode);
  if (!sourceNode)
    return;

  // get the boxObject of the documentElement of the document the tree is in
  nsCOMPtr<nsIBoxObject> bx;
  nsIDocument* doc = sourceNode->GetComposedDoc();
  if (doc) {
    ErrorResult ignored;
    bx = doc->GetBoxObjectFor(doc->GetRootElement(), ignored);
  }

  nsCOMPtr<nsITreeBoxObject> obx;
  GetSourceTreeBoxObject(getter_AddRefs(obx));
  if (bx && obx) {
    int32_t x, y;
    aMouseEvent->GetScreenX(&x);
    aMouseEvent->GetScreenY(&y);

    int32_t row;
    nsCOMPtr<nsITreeColumn> col;
    nsAutoString obj;

    // subtract off the documentElement's boxObject
    int32_t boxX, boxY;
    bx->GetScreenX(&boxX);
    bx->GetScreenY(&boxY);
    x -= boxX;
    y -= boxY;

    obx->GetCellAt(x, y, &row, getter_AddRefs(col), obj);

    // determine if we are going to need a titletip
    // XXX check the disabletitletips attribute on the tree content
    mNeedTitletip = false;
    int16_t colType = -1;
    if (col) {
      col->GetType(&colType);
    }
    if (row >= 0 && obj.EqualsLiteral("text") &&
        colType != nsITreeColumn::TYPE_PASSWORD) {
      obx->IsCellCropped(row, col, &mNeedTitletip);
    }

    nsCOMPtr<nsIContent> currentTooltip = do_QueryReferent(mCurrentTooltip);
    if (currentTooltip && (row != mLastTreeRow || col != mLastTreeCol)) {
      HideTooltip();
    }

    mLastTreeRow = row;
    mLastTreeCol = col;
  }
}
#endif

nsresult
nsXULTooltipListener::ShowTooltip()
{
  nsCOMPtr<nsIContent> sourceNode = do_QueryReferent(mSourceNode);

  // get the tooltip content designated for the target node
  nsCOMPtr<nsIContent> tooltipNode;
  GetTooltipFor(sourceNode, getter_AddRefs(tooltipNode));
  if (!tooltipNode || sourceNode == tooltipNode)
    return NS_ERROR_FAILURE; // the target node doesn't need a tooltip

  // set the node in the document that triggered the tooltip and show it
  nsCOMPtr<nsIDOMXULDocument> xulDoc =
    do_QueryInterface(tooltipNode->GetComposedDoc());
  if (xulDoc) {
    // Make sure the target node is still attached to some document.
    // It might have been deleted.
    if (sourceNode->IsInComposedDoc()) {
#ifdef MOZ_XUL
      if (!mIsSourceTree) {
        mLastTreeRow = -1;
        mLastTreeCol = nullptr;
      }
#endif

      mCurrentTooltip = do_GetWeakReference(tooltipNode);
      LaunchTooltip();
      mTargetNode = nullptr;

      nsCOMPtr<nsIContent> currentTooltip = do_QueryReferent(mCurrentTooltip);
      if (!currentTooltip)
        return NS_OK;

      // listen for popuphidden on the tooltip node, so that we can
      // be sure DestroyPopup is called even if someone else closes the tooltip
      currentTooltip->AddSystemEventListener(NS_LITERAL_STRING("popuphiding"),
                                             this, false, false);

      // listen for mousedown, mouseup, keydown, and DOMMouseScroll events at document level
      nsIDocument* doc = sourceNode->GetComposedDoc();
      if (doc) {
        // Probably, we should listen to untrusted events for hiding tooltips
        // on content since tooltips might disturb something of web
        // applications.  If we don't specify the aWantsUntrusted of
        // AddSystemEventListener(), the event target sets it to TRUE if the
        // target is in content.
        doc->AddSystemEventListener(NS_LITERAL_STRING("DOMMouseScroll"),
                                    this, true);
        doc->AddSystemEventListener(NS_LITERAL_STRING("mousedown"),
                                    this, true);
        doc->AddSystemEventListener(NS_LITERAL_STRING("mouseup"),
                                    this, true);
#ifndef XP_WIN
        // On Windows, key events don't close tooltips.
        doc->AddSystemEventListener(NS_LITERAL_STRING("keydown"),
                                    this, true);
#endif
      }
      mSourceNode = nullptr;
    }
  }

  return NS_OK;
}

#ifdef MOZ_XUL
// XXX: "This stuff inside DEBUG_crap could be used to make tree tooltips work
//       in the future."
#ifdef DEBUG_crap
static void
GetTreeCellCoords(nsITreeBoxObject* aTreeBox, nsIContent* aSourceNode,
                  int32_t aRow, nsITreeColumn* aCol, int32_t* aX, int32_t* aY)
{
  int32_t junk;
  aTreeBox->GetCoordsForCellItem(aRow, aCol, EmptyCString(), aX, aY, &junk, &junk);
  RefPtr<nsXULElement> xulEl = nsXULElement::FromContent(aSourceNode);
  IgnoredErrorResult ignored;
  nsCOMPtr<nsIBoxObject> bx = xulEl->GetBoxObject(ignored);
  int32_t myX, myY;
  bx->GetX(&myX);
  bx->GetY(&myY);
  *aX += myX;
  *aY += myY;
}
#endif

static void
SetTitletipLabel(nsITreeBoxObject* aTreeBox, Element* aTooltip,
                 int32_t aRow, nsITreeColumn* aCol)
{
  nsCOMPtr<nsITreeView> view;
  aTreeBox->GetView(getter_AddRefs(view));
  if (view) {
    nsAutoString label;
#ifdef DEBUG
    nsresult rv =
#endif
      view->GetCellText(aRow, aCol, label);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Couldn't get the cell text!");
    aTooltip->SetAttr(kNameSpaceID_None, nsGkAtoms::label, label, true);
  }
}
#endif

void
nsXULTooltipListener::LaunchTooltip()
{
  nsCOMPtr<Element> currentTooltip = do_QueryReferent(mCurrentTooltip);
  if (!currentTooltip)
    return;

#ifdef MOZ_XUL
  if (mIsSourceTree && mNeedTitletip) {
    nsCOMPtr<nsITreeBoxObject> obx;
    GetSourceTreeBoxObject(getter_AddRefs(obx));

    SetTitletipLabel(obx, currentTooltip, mLastTreeRow, mLastTreeCol);
    if (!(currentTooltip = do_QueryReferent(mCurrentTooltip))) {
      // Because of mutation events, currentTooltip can be null.
      return;
    }
    currentTooltip->SetAttr(kNameSpaceID_None, nsGkAtoms::titletip, NS_LITERAL_STRING("true"), true);
  } else {
    currentTooltip->UnsetAttr(kNameSpaceID_None, nsGkAtoms::titletip, true);
  }
  if (!(currentTooltip = do_QueryReferent(mCurrentTooltip))) {
    // Because of mutation events, currentTooltip can be null.
    return;
  }

  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm) {
    nsCOMPtr<nsIContent> target = do_QueryReferent(mTargetNode);
    pm->ShowTooltipAtScreen(currentTooltip, target, mMouseScreenX, mMouseScreenY);

    // Clear the current tooltip if the popup was not opened successfully.
    if (!pm->IsPopupOpen(currentTooltip))
      mCurrentTooltip = nullptr;
  }
#endif

}

nsresult
nsXULTooltipListener::HideTooltip()
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIContent> currentTooltip = do_QueryReferent(mCurrentTooltip);
  if (currentTooltip) {
    nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
    if (pm)
      pm->HidePopup(currentTooltip, false, false, false, false);
  }
#endif

  DestroyTooltip();
  return NS_OK;
}

static void
GetImmediateChild(nsIContent* aContent, nsAtom *aTag, nsIContent** aResult)
{
  *aResult = nullptr;
  uint32_t childCount = aContent->GetChildCount();
  for (uint32_t i = 0; i < childCount; i++) {
    nsIContent *child = aContent->GetChildAt(i);

    if (child->IsXULElement(aTag)) {
      *aResult = child;
      NS_ADDREF(*aResult);
      return;
    }
  }
}

nsresult
nsXULTooltipListener::FindTooltip(nsIContent* aTarget, nsIContent** aTooltip)
{
  if (!aTarget)
    return NS_ERROR_NULL_POINTER;

  // before we go on, make sure that target node still has a window
  nsIDocument *document = aTarget->GetComposedDoc();
  if (!document) {
    NS_WARNING("Unable to retrieve the tooltip node document.");
    return NS_ERROR_FAILURE;
  }
  nsPIDOMWindowOuter *window = document->GetWindow();
  if (!window) {
    return NS_OK;
  }

  if (window->Closed()) {
    return NS_OK;
  }

  nsAutoString tooltipText;
  aTarget->GetAttr(kNameSpaceID_None, nsGkAtoms::tooltiptext, tooltipText);
  if (!tooltipText.IsEmpty()) {
    // specifying tooltiptext means we will always use the default tooltip
    nsIRootBox* rootBox = nsIRootBox::GetRootBox(document->GetShell());
    NS_ENSURE_STATE(rootBox);
    if (RefPtr<Element> tooltip = rootBox->GetDefaultTooltip()) {
      tooltip->SetAttr(kNameSpaceID_None, nsGkAtoms::label, tooltipText, true);
      tooltip.forget(aTooltip);
    }
    return NS_OK;
  }

  nsAutoString tooltipId;
  aTarget->GetAttr(kNameSpaceID_None, nsGkAtoms::tooltip, tooltipId);

  // if tooltip == _child, look for first <tooltip> child
  if (tooltipId.EqualsLiteral("_child")) {
    GetImmediateChild(aTarget, nsGkAtoms::tooltip, aTooltip);
    return NS_OK;
  }

  if (!tooltipId.IsEmpty() && aTarget->IsInUncomposedDoc()) {
    // tooltip must be an id, use getElementById to find it
    //XXXsmaug If aTarget is in shadow dom, should we use
    //         ShadowRoot::GetElementById()?
    nsCOMPtr<nsIContent> tooltipEl = document->GetElementById(tooltipId);

    if (tooltipEl) {
#ifdef MOZ_XUL
      mNeedTitletip = false;
#endif
      tooltipEl.forget(aTooltip);
      return NS_OK;
    }
  }

#ifdef MOZ_XUL
  // titletips should just use the default tooltip
  if (mIsSourceTree && mNeedTitletip) {
    nsIRootBox* rootBox = nsIRootBox::GetRootBox(document->GetShell());
    NS_ENSURE_STATE(rootBox);
    NS_IF_ADDREF(*aTooltip = rootBox->GetDefaultTooltip());
  }
#endif

  return NS_OK;
}


nsresult
nsXULTooltipListener::GetTooltipFor(nsIContent* aTarget, nsIContent** aTooltip)
{
  *aTooltip = nullptr;
  nsCOMPtr<nsIContent> tooltip;
  nsresult rv = FindTooltip(aTarget, getter_AddRefs(tooltip));
  if (NS_FAILED(rv) || !tooltip) {
    return rv;
  }

#ifdef MOZ_XUL
  // Submenus can't be used as tooltips, see bug 288763.
  nsIContent* parent = tooltip->GetParent();
  if (parent) {
    nsMenuFrame* menu = do_QueryFrame(parent->GetPrimaryFrame());
    if (menu) {
      NS_WARNING("Menu cannot be used as a tooltip");
      return NS_ERROR_FAILURE;
    }
  }
#endif

  tooltip.swap(*aTooltip);
  return rv;
}

nsresult
nsXULTooltipListener::DestroyTooltip()
{
  nsCOMPtr<nsIDOMEventListener> kungFuDeathGrip(this);
  nsCOMPtr<nsIContent> currentTooltip = do_QueryReferent(mCurrentTooltip);
  if (currentTooltip) {
    // release tooltip before removing listener to prevent our destructor from
    // being called recursively (bug 120863)
    mCurrentTooltip = nullptr;

    // clear out the tooltip node on the document
    nsCOMPtr<nsIDocument> doc = currentTooltip->GetComposedDoc();
    if (doc) {
      // remove the mousedown and keydown listener from document
      doc->RemoveSystemEventListener(NS_LITERAL_STRING("DOMMouseScroll"), this,
                                     true);
      doc->RemoveSystemEventListener(NS_LITERAL_STRING("mousedown"), this,
                                     true);
      doc->RemoveSystemEventListener(NS_LITERAL_STRING("mouseup"), this, true);
#ifndef XP_WIN
      doc->RemoveSystemEventListener(NS_LITERAL_STRING("keydown"), this, true);
#endif
    }

    // remove the popuphidden listener from tooltip
    currentTooltip->RemoveSystemEventListener(NS_LITERAL_STRING("popuphiding"), this, false);
  }

  // kill any ongoing timers
  KillTooltipTimer();
  mSourceNode = nullptr;
#ifdef MOZ_XUL
  mLastTreeCol = nullptr;
#endif

  return NS_OK;
}

void
nsXULTooltipListener::KillTooltipTimer()
{
  if (mTooltipTimer) {
    mTooltipTimer->Cancel();
    mTooltipTimer = nullptr;
    mTargetNode = nullptr;
  }
}

void
nsXULTooltipListener::sTooltipCallback(nsITimer *aTimer, void *aListener)
{
  RefPtr<nsXULTooltipListener> instance = mInstance;
  if (instance)
    instance->ShowTooltip();
}

#ifdef MOZ_XUL
nsresult
nsXULTooltipListener::GetSourceTreeBoxObject(nsITreeBoxObject** aBoxObject)
{
  *aBoxObject = nullptr;

  nsCOMPtr<nsIContent> sourceNode = do_QueryReferent(mSourceNode);
  if (mIsSourceTree && sourceNode) {
    RefPtr<nsXULElement> xulEl =
      nsXULElement::FromContentOrNull(sourceNode->GetParent());
    if (xulEl) {
      IgnoredErrorResult ignored;
      nsCOMPtr<nsIBoxObject> bx = xulEl->GetBoxObject(ignored);
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
#endif
