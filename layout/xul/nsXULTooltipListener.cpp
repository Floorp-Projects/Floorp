/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXULTooltipListener.h"

#include "XULButtonElement.h"
#include "XULTreeElement.h"
#include "nsXULElement.h"
#include "mozilla/dom/Document.h"
#include "nsGkAtoms.h"
#include "nsMenuPopupFrame.h"
#include "nsIDragService.h"
#include "nsIDragSession.h"
#include "nsITreeView.h"
#include "nsIScriptContext.h"
#include "nsPIDOMWindow.h"
#include "nsXULPopupManager.h"
#include "nsIPopupContainer.h"
#include "nsServiceManagerUtils.h"
#include "nsTreeColumns.h"
#include "nsContentUtils.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/PresShell.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"  // for Event
#include "mozilla/dom/MouseEvent.h"
#include "mozilla/dom/TreeColumnBinding.h"
#include "mozilla/dom/XULTreeElementBinding.h"
#include "mozilla/TextEvents.h"

using namespace mozilla;
using namespace mozilla::dom;

nsXULTooltipListener* nsXULTooltipListener::sInstance = nullptr;

//////////////////////////////////////////////////////////////////////////
//// nsISupports

nsXULTooltipListener::nsXULTooltipListener()
    : mTooltipShownOnce(false),
      mIsSourceTree(false),
      mNeedTitletip(false),
      mLastTreeRow(-1) {}

nsXULTooltipListener::~nsXULTooltipListener() {
  MOZ_ASSERT(sInstance == this);
  sInstance = nullptr;

  HideTooltip();
}

NS_IMPL_ISUPPORTS(nsXULTooltipListener, nsIDOMEventListener)

void nsXULTooltipListener::MouseOut(Event* aEvent) {
  // reset flag so that tooltip will display on the next MouseMove
  mTooltipShownOnce = false;
  mPreviousMouseMoveTarget = nullptr;

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
  if (mNeedTitletip) return;
#endif

  // check to see if the mouse left the targetNode, and if so,
  // hide the tooltip
  if (currentTooltip) {
    // which node did the mouse leave?
    EventTarget* eventTarget = aEvent->GetComposedTarget();
    nsCOMPtr<nsINode> targetNode = nsINode::FromEventTargetOrNull(eventTarget);
    if (targetNode && targetNode->IsContent() &&
        !targetNode->AsContent()->GetContainingShadow()) {
      eventTarget = aEvent->GetTarget();
    }

    nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
    if (pm) {
      nsCOMPtr<nsINode> tooltipNode =
          pm->GetLastTriggerTooltipNode(currentTooltip->GetComposedDoc());

      // If the target node is the current tooltip target node, the mouse
      // left the node the tooltip appeared on, so close the tooltip. However,
      // don't do this if the mouse moved onto the tooltip in case the
      // tooltip appears positioned near the mouse.
      nsCOMPtr<EventTarget> relatedTarget =
          aEvent->AsMouseEvent()->GetRelatedTarget();
      nsIContent* relatedContent =
          nsIContent::FromEventTargetOrNull(relatedTarget);
      if (tooltipNode == targetNode && relatedContent != currentTooltip) {
        HideTooltip();
        // reset special tree tracking
        if (mIsSourceTree) {
          mLastTreeRow = -1;
          mLastTreeCol = nullptr;
        }
      }
    }
  }
}

void nsXULTooltipListener::MouseMove(Event* aEvent) {
  if (!ShowTooltips()) {
    return;
  }

  // stash the coordinates of the event so that we can still get back to it from
  // within the timer callback. On win32, we'll get a MouseMove event even when
  // a popup goes away -- even when the mouse doesn't change position! To get
  // around this, we make sure the mouse has really moved before proceeding.
  MouseEvent* mouseEvent = aEvent->AsMouseEvent();
  if (!mouseEvent) {
    return;
  }
  auto newMouseScreenPoint = mouseEvent->ScreenPointLayoutDevicePix();

  // filter out false win32 MouseMove event
  if (mMouseScreenPoint == newMouseScreenPoint) {
    return;
  }

  nsCOMPtr<nsIContent> currentTooltip = do_QueryReferent(mCurrentTooltip);
  nsCOMPtr<EventTarget> eventTarget = aEvent->GetComposedTarget();
  nsIContent* content = nsIContent::FromEventTargetOrNull(eventTarget);

  bool isSameTarget = true;
  nsCOMPtr<nsIContent> tempContent = do_QueryReferent(mPreviousMouseMoveTarget);
  if (tempContent && tempContent != content) {
    isSameTarget = false;
  }

  // filter out minor movements due to crappy optical mice and shaky hands
  // to prevent tooltips from hiding prematurely. Do not filter out movements
  // if we are changing targets, as they may register new tooltips.
  if (currentTooltip && isSameTarget &&
      abs(mMouseScreenPoint.x - newMouseScreenPoint.x) <=
          kTooltipMouseMoveTolerance &&
      abs(mMouseScreenPoint.y - newMouseScreenPoint.y) <=
          kTooltipMouseMoveTolerance) {
    return;
  }
  mMouseScreenPoint = newMouseScreenPoint;
  mPreviousMouseMoveTarget = do_GetWeakReference(content);

  nsCOMPtr<nsIContent> sourceContent =
      do_QueryInterface(aEvent->GetCurrentTarget());
  mSourceNode = do_GetWeakReference(sourceContent);
  mIsSourceTree = sourceContent->IsXULElement(nsGkAtoms::treechildren);
  if (mIsSourceTree) CheckTreeBodyMove(mouseEvent);

  // as the mouse moves, we want to make sure we reset the timer to show it,
  // so that the delay is from when the mouse stops moving, not when it enters
  // the node.
  KillTooltipTimer();

  // Hide the current tooltip if we change target nodes. If the new target
  // has the same tooltip, we will open it again. We cannot compare
  // the targets' tooltips because popupshowing events can set the tooltip.
  if (!isSameTarget) {
    HideTooltip();
    mTooltipShownOnce = false;
  }

  // If the mouse moves while the tooltip is up, hide it. If nothing is
  // showing and the tooltip hasn't been displayed since the mouse entered
  // the node, then start the timer to show the tooltip.
  // If we have moved to a different target, we need to display the new tooltip,
  // as the previous target's tooltip will have just been hidden.
  if ((!currentTooltip && !mTooltipShownOnce) || !isSameTarget) {
    // don't show tooltips attached to elements outside of a menu popup
    // when hovering over an element inside it. The popupsinherittooltip
    // attribute may be used to disable this behaviour, which is useful for
    // large menu hierarchies such as bookmarks.
    if (!sourceContent->IsElement() ||
        !sourceContent->AsElement()->AttrValueIs(
            kNameSpaceID_None, nsGkAtoms::popupsinherittooltip,
            nsGkAtoms::_true, eCaseMatters)) {
      for (nsIContent* targetContent =
               nsIContent::FromEventTargetOrNull(eventTarget);
           targetContent && targetContent != sourceContent;
           targetContent = targetContent->GetParent()) {
        if (targetContent->IsAnyOfXULElements(
                nsGkAtoms::menupopup, nsGkAtoms::panel, nsGkAtoms::tooltip)) {
          mSourceNode = nullptr;
          return;
        }
      }
    }

    mTargetNode = do_GetWeakReference(eventTarget);
    if (mTargetNode) {
      nsresult rv = NS_NewTimerWithFuncCallback(
          getter_AddRefs(mTooltipTimer), sTooltipCallback, this,
          LookAndFeel::GetInt(LookAndFeel::IntID::TooltipDelay, 500),
          nsITimer::TYPE_ONE_SHOT, "sTooltipCallback",
          GetMainThreadSerialEventTarget());
      if (NS_FAILED(rv)) {
        mTargetNode = nullptr;
        mSourceNode = nullptr;
      }
    }
    return;
  }

  if (mIsSourceTree) return;
  // Hide the tooltip if it is currently showing.
  if (currentTooltip) {
    HideTooltip();
    // set a flag so that the tooltip is only displayed once until the mouse
    // leaves the node
    mTooltipShownOnce = true;
  }
}

NS_IMETHODIMP
nsXULTooltipListener::HandleEvent(Event* aEvent) {
  nsAutoString type;
  aEvent->GetType(type);
  if (type.EqualsLiteral("wheel") || type.EqualsLiteral("mousedown") ||
      type.EqualsLiteral("mouseup") || type.EqualsLiteral("dragstart")) {
    HideTooltip();
    return NS_OK;
  }

  if (type.EqualsLiteral("keydown")) {
    // Hide the tooltip if a non-modifier key is pressed.
    WidgetKeyboardEvent* keyEvent = aEvent->WidgetEventPtr()->AsKeyboardEvent();
    if (KeyEventHidesTooltip(*keyEvent)) {
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

bool nsXULTooltipListener::ShowTooltips() {
  return StaticPrefs::browser_chrome_toolbar_tips();
}

bool nsXULTooltipListener::KeyEventHidesTooltip(
    const WidgetKeyboardEvent& aEvent) {
  switch (StaticPrefs::browser_chrome_toolbar_tips_hide_on_keydown()) {
    case 0:
      return false;
    case 1:
      return true;
    default:
      return !aEvent.IsModifierKeyEvent();
  }
}

void nsXULTooltipListener::AddTooltipSupport(nsIContent* aNode) {
  MOZ_ASSERT(aNode);
  MOZ_ASSERT(this == sInstance);

  aNode->AddSystemEventListener(u"mouseout"_ns, this, false, false);
  aNode->AddSystemEventListener(u"mousemove"_ns, this, false, false);
  aNode->AddSystemEventListener(u"mousedown"_ns, this, false, false);
  aNode->AddSystemEventListener(u"mouseup"_ns, this, false, false);
  aNode->AddSystemEventListener(u"dragstart"_ns, this, true, false);
}

void nsXULTooltipListener::RemoveTooltipSupport(nsIContent* aNode) {
  MOZ_ASSERT(aNode);
  MOZ_ASSERT(this == sInstance);

  // The last reference to us can go after some of these calls.
  RefPtr<nsXULTooltipListener> instance = this;

  aNode->RemoveSystemEventListener(u"mouseout"_ns, this, false);
  aNode->RemoveSystemEventListener(u"mousemove"_ns, this, false);
  aNode->RemoveSystemEventListener(u"mousedown"_ns, this, false);
  aNode->RemoveSystemEventListener(u"mouseup"_ns, this, false);
  aNode->RemoveSystemEventListener(u"dragstart"_ns, this, true);
}

void nsXULTooltipListener::CheckTreeBodyMove(MouseEvent* aMouseEvent) {
  nsCOMPtr<nsIContent> sourceNode = do_QueryReferent(mSourceNode);
  if (!sourceNode) return;

  // get the documentElement of the document the tree is in
  Document* doc = sourceNode->GetComposedDoc();

  RefPtr<XULTreeElement> tree = GetSourceTree();
  Element* root = doc ? doc->GetRootElement() : nullptr;
  if (root && root->GetPrimaryFrame() && tree) {
    CSSIntPoint pos = aMouseEvent->ScreenPoint(CallerType::System);

    // subtract off the documentElement's position
    // XXX Isn't this just converting to client points?
    CSSIntRect rect = root->GetPrimaryFrame()->GetScreenRect();
    pos -= rect.TopLeft();

    ErrorResult rv;
    TreeCellInfo cellInfo;
    tree->GetCellAt(pos.x, pos.y, cellInfo, rv);

    int32_t row = cellInfo.mRow;
    RefPtr<nsTreeColumn> col = cellInfo.mCol;

    // determine if we are going to need a titletip
    // XXX check the disabletitletips attribute on the tree content
    mNeedTitletip = false;
    if (row >= 0 && cellInfo.mChildElt.EqualsLiteral("text")) {
      mNeedTitletip = tree->IsCellCropped(row, col, rv);
    }

    nsCOMPtr<nsIContent> currentTooltip = do_QueryReferent(mCurrentTooltip);
    if (currentTooltip && (row != mLastTreeRow || col != mLastTreeCol)) {
      HideTooltip();
    }

    mLastTreeRow = row;
    mLastTreeCol = col;
  }
}

nsresult nsXULTooltipListener::ShowTooltip() {
  nsCOMPtr<nsIContent> sourceNode = do_QueryReferent(mSourceNode);

  // get the tooltip content designated for the target node
  nsCOMPtr<nsIContent> tooltipNode;
  GetTooltipFor(sourceNode, getter_AddRefs(tooltipNode));
  if (!tooltipNode || sourceNode == tooltipNode) {
    return NS_ERROR_FAILURE;  // the target node doesn't need a tooltip
  }

  // set the node in the document that triggered the tooltip and show it
  // Make sure the document still has focus.
  auto* doc = tooltipNode->GetComposedDoc();
  if (!doc || !nsContentUtils::IsChromeDoc(doc) ||
      doc->IsTopLevelWindowInactive()) {
    return NS_OK;
  }

  // Make sure the target node is still attached to some document.
  // It might have been deleted.
  if (!sourceNode->IsInComposedDoc()) {
    return NS_OK;
  }

  if (!mIsSourceTree) {
    mLastTreeRow = -1;
    mLastTreeCol = nullptr;
  }

  mCurrentTooltip = do_GetWeakReference(tooltipNode);
  LaunchTooltip();
  mTargetNode = nullptr;

  nsCOMPtr<nsIContent> currentTooltip = do_QueryReferent(mCurrentTooltip);
  if (!currentTooltip) {
    return NS_OK;
  }

  // Listen for popuphidden on the tooltip node so that we can be sure
  // DestroyPopup is called even if someone else closes the tooltip.
  currentTooltip->AddSystemEventListener(u"popuphiding"_ns, this, false, false);

  // Listen for mousedown, mouseup, keydown, and mouse events at document level.
  if (Document* doc = sourceNode->GetComposedDoc()) {
    // Probably, we should listen to untrusted events for hiding tooltips on
    // content since tooltips might disturb something of web applications. If we
    // don't specify the aWantsUntrusted of AddSystemEventListener(), the event
    // target sets it to TRUE if the target is in content.
    doc->AddSystemEventListener(u"wheel"_ns, this, true);
    doc->AddSystemEventListener(u"mousedown"_ns, this, true);
    doc->AddSystemEventListener(u"mouseup"_ns, this, true);
    doc->AddSystemEventListener(u"keydown"_ns, this, true);
  }
  mSourceNode = nullptr;

  return NS_OK;
}

static void SetTitletipLabel(XULTreeElement* aTree, Element* aTooltip,
                             int32_t aRow, nsTreeColumn* aCol) {
  nsCOMPtr<nsITreeView> view = aTree->GetView();
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

void nsXULTooltipListener::LaunchTooltip() {
  RefPtr<Element> currentTooltip = do_QueryReferent(mCurrentTooltip);
  if (!currentTooltip) {
    return;
  }

  if (mIsSourceTree && mNeedTitletip) {
    RefPtr<XULTreeElement> tree = GetSourceTree();

    SetTitletipLabel(tree, currentTooltip, mLastTreeRow, mLastTreeCol);
    if (!(currentTooltip = do_QueryReferent(mCurrentTooltip))) {
      // Because of mutation events, currentTooltip can be null.
      return;
    }
    currentTooltip->SetAttr(kNameSpaceID_None, nsGkAtoms::titletip, u"true"_ns,
                            true);
  } else {
    currentTooltip->UnsetAttr(kNameSpaceID_None, nsGkAtoms::titletip, true);
  }

  if (!(currentTooltip = do_QueryReferent(mCurrentTooltip))) {
    // Because of mutation events, currentTooltip can be null.
    return;
  }

  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (!pm) {
    return;
  }

  auto cleanup = MakeScopeExit([&] {
    // Clear the current tooltip if the popup was not opened successfully.
    if (!pm->IsPopupOpen(currentTooltip)) {
      mCurrentTooltip = nullptr;
    }
  });

  RefPtr<Element> target = do_QueryReferent(mTargetNode);
  if (!target) {
    return;
  }

  pm->ShowTooltipAtScreen(currentTooltip, target, mMouseScreenPoint);
}

nsresult nsXULTooltipListener::HideTooltip() {
  if (nsCOMPtr<Element> currentTooltip = do_QueryReferent(mCurrentTooltip)) {
    if (nsXULPopupManager* pm = nsXULPopupManager::GetInstance()) {
      pm->HidePopup(currentTooltip, {});
    }
  }

  DestroyTooltip();
  return NS_OK;
}

static void GetImmediateChild(nsIContent* aContent, nsAtom* aTag,
                              nsIContent** aResult) {
  *aResult = nullptr;
  for (nsCOMPtr<nsIContent> childContent = aContent->GetFirstChild();
       childContent; childContent = childContent->GetNextSibling()) {
    if (childContent->IsXULElement(aTag)) {
      childContent.forget(aResult);
      return;
    }
  }
}

nsresult nsXULTooltipListener::FindTooltip(nsIContent* aTarget,
                                           nsIContent** aTooltip) {
  if (!aTarget) return NS_ERROR_NULL_POINTER;

  // before we go on, make sure that target node still has a window
  Document* document = aTarget->GetComposedDoc();
  if (!document) {
    NS_WARNING("Unable to retrieve the tooltip node document.");
    return NS_ERROR_FAILURE;
  }
  nsPIDOMWindowOuter* window = document->GetWindow();
  if (!window) {
    return NS_OK;
  }

  if (window->Closed()) {
    return NS_OK;
  }

  // non-XUL elements should just use the default tooltip
  if (!aTarget->IsXULElement()) {
    nsIPopupContainer* popupContainer =
        nsIPopupContainer::GetPopupContainer(document->GetPresShell());
    NS_ENSURE_STATE(popupContainer);
    if (RefPtr<Element> tooltip = popupContainer->GetDefaultTooltip()) {
      tooltip.forget(aTooltip);
      return NS_OK;
    }
    return NS_ERROR_FAILURE;
  }

  // On Windows, the OS shows the tooltip, so we don't want Gecko to do it
#ifdef XP_WIN
  if (nsIFrame* f = aTarget->GetPrimaryFrame()) {
    if (f->StyleDisplay()->GetWindowButtonType()) {
      return NS_OK;
    }
  }
#endif

  nsAutoString tooltipText;
  aTarget->AsElement()->GetAttr(nsGkAtoms::tooltiptext, tooltipText);

  if (!tooltipText.IsEmpty()) {
    // specifying tooltiptext means we will always use the default tooltip
    nsIPopupContainer* popupContainer =
        nsIPopupContainer::GetPopupContainer(document->GetPresShell());
    NS_ENSURE_STATE(popupContainer);
    if (RefPtr<Element> tooltip = popupContainer->GetDefaultTooltip()) {
      tooltip->SetAttr(kNameSpaceID_None, nsGkAtoms::label, tooltipText, true);
      tooltip.forget(aTooltip);
    }
    return NS_OK;
  }

  nsAutoString tooltipId;
  aTarget->AsElement()->GetAttr(nsGkAtoms::tooltip, tooltipId);

  // if tooltip == _child, look for first <tooltip> child
  if (tooltipId.EqualsLiteral("_child")) {
    GetImmediateChild(aTarget, nsGkAtoms::tooltip, aTooltip);
    return NS_OK;
  }

  if (!tooltipId.IsEmpty()) {
    DocumentOrShadowRoot* documentOrShadowRoot =
        aTarget->GetUncomposedDocOrConnectedShadowRoot();
    // tooltip must be an id, use getElementById to find it
    if (documentOrShadowRoot) {
      nsCOMPtr<nsIContent> tooltipEl =
          documentOrShadowRoot->GetElementById(tooltipId);

      if (tooltipEl) {
        mNeedTitletip = false;
        tooltipEl.forget(aTooltip);
        return NS_OK;
      }
    }
  }

  // titletips should just use the default tooltip
  if (mIsSourceTree && mNeedTitletip) {
    nsIPopupContainer* popupContainer =
        nsIPopupContainer::GetPopupContainer(document->GetPresShell());
    NS_ENSURE_STATE(popupContainer);
    NS_IF_ADDREF(*aTooltip = popupContainer->GetDefaultTooltip());
  }

  return NS_OK;
}

nsresult nsXULTooltipListener::GetTooltipFor(nsIContent* aTarget,
                                             nsIContent** aTooltip) {
  *aTooltip = nullptr;
  nsCOMPtr<nsIContent> tooltip;
  nsresult rv = FindTooltip(aTarget, getter_AddRefs(tooltip));
  if (NS_FAILED(rv) || !tooltip) {
    return rv;
  }

  // Submenus can't be used as tooltips, see bug 288763.
  if (nsIContent* parent = tooltip->GetParent()) {
    if (auto* button = XULButtonElement::FromNode(parent)) {
      if (button->IsMenu()) {
        NS_WARNING("Menu cannot be used as a tooltip");
        return NS_ERROR_FAILURE;
      }
    }
  }

  tooltip.swap(*aTooltip);
  return rv;
}

nsresult nsXULTooltipListener::DestroyTooltip() {
  nsCOMPtr<nsIDOMEventListener> kungFuDeathGrip(this);
  nsCOMPtr<nsIContent> currentTooltip = do_QueryReferent(mCurrentTooltip);
  if (currentTooltip) {
    // release tooltip before removing listener to prevent our destructor from
    // being called recursively (bug 120863)
    mCurrentTooltip = nullptr;

    // clear out the tooltip node on the document
    if (nsCOMPtr<Document> doc = currentTooltip->GetComposedDoc()) {
      // remove the mousedown and keydown listener from document
      doc->RemoveSystemEventListener(u"wheel"_ns, this, true);
      doc->RemoveSystemEventListener(u"mousedown"_ns, this, true);
      doc->RemoveSystemEventListener(u"mouseup"_ns, this, true);
      doc->RemoveSystemEventListener(u"keydown"_ns, this, true);
    }

    // remove the popuphidden listener from tooltip
    currentTooltip->RemoveSystemEventListener(u"popuphiding"_ns, this, false);
  }

  // kill any ongoing timers
  KillTooltipTimer();
  mSourceNode = nullptr;
  mLastTreeCol = nullptr;

  return NS_OK;
}

void nsXULTooltipListener::KillTooltipTimer() {
  if (mTooltipTimer) {
    mTooltipTimer->Cancel();
    mTooltipTimer = nullptr;
    mTargetNode = nullptr;
  }
}

void nsXULTooltipListener::sTooltipCallback(nsITimer* aTimer, void* aListener) {
  RefPtr<nsXULTooltipListener> instance = sInstance;
  if (instance) instance->ShowTooltip();
}

XULTreeElement* nsXULTooltipListener::GetSourceTree() {
  nsCOMPtr<nsIContent> sourceNode = do_QueryReferent(mSourceNode);
  if (mIsSourceTree && sourceNode) {
    RefPtr<XULTreeElement> xulEl =
        XULTreeElement::FromNodeOrNull(sourceNode->GetParent());
    return xulEl;
  }

  return nullptr;
}
