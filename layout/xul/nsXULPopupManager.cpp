/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGkAtoms.h"
#include "nsXULPopupManager.h"
#include "nsMenuFrame.h"
#include "nsMenuPopupFrame.h"
#include "nsMenuBarFrame.h"
#include "nsIPopupBoxObject.h"
#include "nsMenuBarListener.h"
#include "nsContentUtils.h"
#include "nsIDOMDocument.h"
#include "nsDOMEvent.h"
#include "nsIDOMEvent.h"
#include "nsIDOMXULElement.h"
#include "nsIXULDocument.h"
#include "nsIXULTemplateBuilder.h"
#include "nsEventDispatcher.h"
#include "nsEventStateManager.h"
#include "nsCSSFrameConstructor.h"
#include "nsLayoutUtils.h"
#include "nsViewManager.h"
#include "nsIComponentManager.h"
#include "nsITimer.h"
#include "nsFocusManager.h"
#include "nsIDocShell.h"
#include "nsPIDOMWindow.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIBaseWindow.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMMouseEvent.h"
#include "nsCaret.h"
#include "nsIDocument.h"
#include "nsPIWindowRoot.h"
#include "nsFrameManager.h"
#include "nsIObserverService.h"
#include "mozilla/dom/Element.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/Services.h"

using namespace mozilla;
using namespace mozilla::dom;

static_assert(nsIDOMKeyEvent::DOM_VK_HOME  == nsIDOMKeyEvent::DOM_VK_END + 1 &&
              nsIDOMKeyEvent::DOM_VK_LEFT  == nsIDOMKeyEvent::DOM_VK_END + 2 &&
              nsIDOMKeyEvent::DOM_VK_UP    == nsIDOMKeyEvent::DOM_VK_END + 3 &&
              nsIDOMKeyEvent::DOM_VK_RIGHT == nsIDOMKeyEvent::DOM_VK_END + 4 &&
              nsIDOMKeyEvent::DOM_VK_DOWN  == nsIDOMKeyEvent::DOM_VK_END + 5,
              "nsXULPopupManager assumes some keyCode values are consecutive");

const nsNavigationDirection DirectionFromKeyCodeTable[2][6] = {
  {
    eNavigationDirection_Last,   // nsIDOMKeyEvent::DOM_VK_END
    eNavigationDirection_First,  // nsIDOMKeyEvent::DOM_VK_HOME
    eNavigationDirection_Start,  // nsIDOMKeyEvent::DOM_VK_LEFT
    eNavigationDirection_Before, // nsIDOMKeyEvent::DOM_VK_UP
    eNavigationDirection_End,    // nsIDOMKeyEvent::DOM_VK_RIGHT
    eNavigationDirection_After   // nsIDOMKeyEvent::DOM_VK_DOWN
  },
  {
    eNavigationDirection_Last,   // nsIDOMKeyEvent::DOM_VK_END
    eNavigationDirection_First,  // nsIDOMKeyEvent::DOM_VK_HOME
    eNavigationDirection_End,    // nsIDOMKeyEvent::DOM_VK_LEFT
    eNavigationDirection_Before, // nsIDOMKeyEvent::DOM_VK_UP
    eNavigationDirection_Start,  // nsIDOMKeyEvent::DOM_VK_RIGHT
    eNavigationDirection_After   // nsIDOMKeyEvent::DOM_VK_DOWN
  }
};

nsXULPopupManager* nsXULPopupManager::sInstance = nullptr;

nsIContent* nsMenuChainItem::Content()
{
  return mFrame->GetContent();
}

void nsMenuChainItem::SetParent(nsMenuChainItem* aParent)
{
  if (mParent) {
    NS_ASSERTION(mParent->mChild == this, "Unexpected - parent's child not set to this");
    mParent->mChild = nullptr;
  }
  mParent = aParent;
  if (mParent) {
    if (mParent->mChild)
      mParent->mChild->mParent = nullptr;
    mParent->mChild = this;
  }
}

void nsMenuChainItem::Detach(nsMenuChainItem** aRoot)
{
  // If the item has a child, set the child's parent to this item's parent,
  // effectively removing the item from the chain. If the item has no child,
  // just set the parent to null.
  if (mChild) {
    NS_ASSERTION(this != *aRoot, "Unexpected - popup with child at end of chain");
    mChild->SetParent(mParent);
  }
  else {
    // An item without a child should be the first item in the chain, so set
    // the first item pointer, pointed to by aRoot, to the parent.
    NS_ASSERTION(this == *aRoot, "Unexpected - popup with no child not at end of chain");
    *aRoot = mParent;
    SetParent(nullptr);
  }
}

NS_IMPL_ISUPPORTS3(nsXULPopupManager,
                   nsIDOMEventListener,
                   nsITimerCallback,
                   nsIObserver)

nsXULPopupManager::nsXULPopupManager() :
  mRangeOffset(0),
  mCachedMousePoint(0, 0),
  mCachedModifiers(0),
  mActiveMenuBar(nullptr),
  mPopups(nullptr),
  mNoHidePanels(nullptr),
  mTimerMenu(nullptr)
{
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->AddObserver(this, "xpcom-shutdown", false);
  }
}

nsXULPopupManager::~nsXULPopupManager() 
{
  NS_ASSERTION(!mPopups && !mNoHidePanels, "XUL popups still open");
}

nsresult
nsXULPopupManager::Init()
{
  sInstance = new nsXULPopupManager();
  NS_ENSURE_TRUE(sInstance, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(sInstance);
  return NS_OK;
}

void
nsXULPopupManager::Shutdown()
{
  NS_IF_RELEASE(sInstance);
}

NS_IMETHODIMP
nsXULPopupManager::Observe(nsISupports *aSubject,
                           const char *aTopic,
                           const PRUnichar *aData)
{
  if (!nsCRT::strcmp(aTopic, "xpcom-shutdown")) {
    if (mKeyListener) {
      mKeyListener->RemoveEventListener(NS_LITERAL_STRING("keypress"), this, true);
      mKeyListener->RemoveEventListener(NS_LITERAL_STRING("keydown"), this, true);
      mKeyListener->RemoveEventListener(NS_LITERAL_STRING("keyup"), this, true);
      mKeyListener = nullptr;
    }
    mRangeParent = nullptr;
    // mOpeningPopup is cleared explicitly soon after using it.
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->RemoveObserver(this, "xpcom-shutdown");
    }
  }

  return NS_OK;
}

nsXULPopupManager*
nsXULPopupManager::GetInstance()
{
  MOZ_ASSERT(sInstance);
  return sInstance;
}

bool
nsXULPopupManager::Rollup(uint32_t aCount, const nsIntPoint* pos, nsIContent** aLastRolledUp)
{
  bool consume = false;

  nsMenuChainItem* item = GetTopVisibleMenu();
  if (item) {
    if (aLastRolledUp) {
      // we need to get the popup that will be closed last, so that
      // widget can keep track of it so it doesn't reopen if a mouse
      // down event is going to processed.
      // Keep going up the menu chain to get the first level menu. This will
      // be the one that closes up last. It's possible that this menu doesn't
      // end up closing because the popuphiding event was cancelled, but in
      // that case we don't need to deal with the menu reopening as it will
      // already still be open.
      nsMenuChainItem* first = item;
      while (first->GetParent())
        first = first->GetParent();
      *aLastRolledUp = first->Content();
    }

    consume = item->Frame()->ConsumeOutsideClicks();
    // If the click was over the anchor, always consume the click. This way,
    // clicking on a menu doesn't reopen the menu.
    if (!consume && pos) {
      nsCOMPtr<nsIContent> anchor = item->Frame()->GetAnchor();
      if (anchor && anchor->GetPrimaryFrame()) {
        // It's possible that some other element is above the anchor at the same
        // position, but the only thing that would happen is that the mouse
        // event will get consumed, so here only a quick coordinates check is
        // done rather than a slower complete check of what is at that location.
        if (anchor->GetPrimaryFrame()->GetScreenRect().Contains(*pos)) {
          consume = true;
        }
      }
    }

    // if a number of popups to close has been specified, determine the last
    // popup to close
    nsIContent* lastPopup = nullptr;
    if (aCount != UINT32_MAX) {
      nsMenuChainItem* last = item;
      while (--aCount && last->GetParent()) {
        last = last->GetParent();
      }
      if (last) {
        lastPopup = last->Content();
      }
    }

    HidePopup(item->Content(), true, true, false, lastPopup);
  }

  return consume;
}

////////////////////////////////////////////////////////////////////////
bool nsXULPopupManager::ShouldRollupOnMouseWheelEvent()
{
  // should rollup only for autocomplete widgets
  // XXXndeakin this should really be something the popup has more control over

  nsMenuChainItem* item = GetTopVisibleMenu();
  if (!item)
    return false;

  nsIContent* content = item->Frame()->GetContent();
  if (!content)
    return false;

  if (content->AttrValueIs(kNameSpaceID_None, nsGkAtoms::rolluponmousewheel,
                           nsGkAtoms::_true, eCaseMatters))
    return true;

  if (content->AttrValueIs(kNameSpaceID_None, nsGkAtoms::rolluponmousewheel,
                           nsGkAtoms::_false, eCaseMatters))
    return false;

  nsAutoString value;
  content->GetAttr(kNameSpaceID_None, nsGkAtoms::type, value);
  return StringBeginsWith(value, NS_LITERAL_STRING("autocomplete"));
}

bool nsXULPopupManager::ShouldConsumeOnMouseWheelEvent()
{
  nsMenuChainItem* item = GetTopVisibleMenu();
  if (!item)
    return false;

  nsMenuPopupFrame* frame = item->Frame();
  if (frame->PopupType() != ePopupTypePanel)
    return true;

  nsIContent* content = frame->GetContent();
  return !(content && content->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                                           nsGkAtoms::arrow, eCaseMatters));
}

// a menu should not roll up if activated by a mouse activate message (eg. X-mouse)
bool nsXULPopupManager::ShouldRollupOnMouseActivate()
{
  return false;
}

uint32_t
nsXULPopupManager::GetSubmenuWidgetChain(nsTArray<nsIWidget*> *aWidgetChain)
{
  // this method is used by the widget code to determine the list of popups
  // that are open. If a mouse click occurs outside one of these popups, the
  // panels will roll up. If the click is inside a popup, they will not roll up
  uint32_t count = 0, sameTypeCount = 0;

  NS_ASSERTION(aWidgetChain, "null parameter");
  nsMenuChainItem* item = GetTopVisibleMenu();
  while (item) {
    nsCOMPtr<nsIWidget> widget = item->Frame()->GetWidget();
    NS_ASSERTION(widget, "open popup has no widget");
    aWidgetChain->AppendElement(widget.get());
    // In the case when a menulist inside a panel is open, clicking in the
    // panel should still roll up the menu, so if a different type is found,
    // stop scanning.
    nsMenuChainItem* parent = item->GetParent();
    if (!sameTypeCount) {
      count++;
      if (!parent || item->Frame()->PopupType() != parent->Frame()->PopupType() ||
                     item->IsContextMenu() != parent->IsContextMenu()) {
        sameTypeCount = count;
      }
    }
    item = parent;
  }

  return sameTypeCount;
}

nsIWidget*
nsXULPopupManager::GetRollupWidget()
{
  nsMenuChainItem* item = GetTopVisibleMenu();
  return item ? item->Frame()->GetWidget() : nullptr;
}

void
nsXULPopupManager::AdjustPopupsOnWindowChange(nsPIDOMWindow* aWindow)
{
  // When the parent window is moved, adjust any child popups. Dismissable
  // menus and panels are expected to roll up when a window is moved, so there
  // is no need to check these popups, only the noautohide popups.
  nsMenuChainItem* item = mNoHidePanels;
  while (item) {
    // only move popups that are within the same window and where auto
    // positioning has not been disabled
    nsMenuPopupFrame* frame= item->Frame();
    if (frame->GetAutoPosition()) {
      nsIContent* popup = frame->GetContent();
      if (popup) {
        nsIDocument* document = popup->GetCurrentDoc();
        if (document) {
          nsPIDOMWindow* window = document->GetWindow();
          if (window) {
            window = window->GetPrivateRoot();
            if (window == aWindow) {
              frame->SetPopupPosition(nullptr, true);
            }
          }
        }
      }
    }

    item = item->GetParent();
  }
}

static
nsMenuPopupFrame* GetPopupToMoveOrResize(nsIFrame* aFrame)
{
  nsMenuPopupFrame* menuPopupFrame = do_QueryFrame(aFrame);
  if (!menuPopupFrame)
    return nullptr;

  // no point moving or resizing hidden popups
  if (menuPopupFrame->PopupState() != ePopupOpenAndVisible)
    return nullptr;

  nsIWidget* widget = menuPopupFrame->GetWidget();
  if (widget && !widget->IsVisible())
    return nullptr;

  return menuPopupFrame;
}

void
nsXULPopupManager::PopupMoved(nsIFrame* aFrame, nsIntPoint aPnt)
{
  nsMenuPopupFrame* menuPopupFrame = GetPopupToMoveOrResize(aFrame);
  if (!menuPopupFrame)
    return;

  // Convert desired point to CSS pixels for comparison
  nsPresContext* presContext = menuPopupFrame->PresContext();
  aPnt.x = presContext->DevPixelsToIntCSSPixels(aPnt.x);
  aPnt.y = presContext->DevPixelsToIntCSSPixels(aPnt.y);

  // Don't do anything if the popup is already at the specified location. This
  // prevents recursive calls when a popup is positioned.
  nsIntPoint currentPnt = menuPopupFrame->ScreenPosition();
  nsIWidget* widget = menuPopupFrame->GetWidget();
  if ((aPnt.x != currentPnt.x || aPnt.y != currentPnt.y) || (widget &&
      widget->GetClientOffset() != menuPopupFrame->GetLastClientOffset())) {
    // Update the popup's position using SetPopupPosition if the popup is
    // anchored and at the parent level as these maintain their position
    // relative to the parent window. Otherwise, just update the popup to
    // the specified screen coordinates.
    if (menuPopupFrame->IsAnchored() &&
        menuPopupFrame->PopupLevel() == ePopupLevelParent) {
      menuPopupFrame->SetPopupPosition(nullptr, true);
    }
    else {
      menuPopupFrame->MoveTo(aPnt.x, aPnt.y, false);
    }
  }
}

void
nsXULPopupManager::PopupResized(nsIFrame* aFrame, nsIntSize aSize)
{
  nsMenuPopupFrame* menuPopupFrame = GetPopupToMoveOrResize(aFrame);
  if (!menuPopupFrame)
    return;

  nsView* view = menuPopupFrame->GetView();
  if (!view)
    return;

  nsIntRect curDevSize = view->CalcWidgetBounds(eWindowType_popup);
  // If the size is what we think it is, we have nothing to do.
  if (curDevSize.width == aSize.width && curDevSize.height == aSize.height)
    return;

  // The size is different. Convert the actual size to css pixels and store it
  // as 'width' and 'height' attributes on the popup.
  nsPresContext* presContext = menuPopupFrame->PresContext();

  nsIntSize newCSS(presContext->DevPixelsToIntCSSPixels(aSize.width),
                   presContext->DevPixelsToIntCSSPixels(aSize.height));

  nsIContent* popup = menuPopupFrame->GetContent();
  nsAutoString width, height;
  width.AppendInt(newCSS.width);
  height.AppendInt(newCSS.height);
  popup->SetAttr(kNameSpaceID_None, nsGkAtoms::width, width, false);
  popup->SetAttr(kNameSpaceID_None, nsGkAtoms::height, height, true);
}

nsMenuPopupFrame*
nsXULPopupManager::GetPopupFrameForContent(nsIContent* aContent, bool aShouldFlush)
{
  if (aShouldFlush) {
    nsIDocument *document = aContent->GetCurrentDoc();
    if (document) {
      nsCOMPtr<nsIPresShell> presShell = document->GetShell();
      if (presShell)
        presShell->FlushPendingNotifications(Flush_Layout);
    }
  }

  return do_QueryFrame(aContent->GetPrimaryFrame());
}

nsMenuChainItem*
nsXULPopupManager::GetTopVisibleMenu()
{
  nsMenuChainItem* item = mPopups;
  while (item && item->Frame()->PopupState() == ePopupInvisible)
    item = item->GetParent();
  return item;
}

void
nsXULPopupManager::GetMouseLocation(nsIDOMNode** aNode, int32_t* aOffset)
{
  *aNode = mRangeParent;
  NS_IF_ADDREF(*aNode);
  *aOffset = mRangeOffset;
}

void
nsXULPopupManager::InitTriggerEvent(nsIDOMEvent* aEvent, nsIContent* aPopup,
                                    nsIContent** aTriggerContent)
{
  mCachedMousePoint = nsIntPoint(0, 0);

  if (aTriggerContent) {
    *aTriggerContent = nullptr;
    if (aEvent) {
      // get the trigger content from the event
      nsCOMPtr<nsIContent> target = do_QueryInterface(
        aEvent->InternalDOMEvent()->GetTarget());
      target.forget(aTriggerContent);
    }
  }

  mCachedModifiers = 0;

  nsCOMPtr<nsIDOMUIEvent> uiEvent = do_QueryInterface(aEvent);
  if (uiEvent) {
    uiEvent->GetRangeParent(getter_AddRefs(mRangeParent));
    uiEvent->GetRangeOffset(&mRangeOffset);

    // get the event coordinates relative to the root frame of the document
    // containing the popup.
    NS_ASSERTION(aPopup, "Expected a popup node");
    WidgetEvent* event = aEvent->GetInternalNSEvent();
    if (event) {
      WidgetInputEvent* inputEvent = event->AsInputEvent();
      if (inputEvent) {
        mCachedModifiers = inputEvent->modifiers;
      }
      nsIDocument* doc = aPopup->GetCurrentDoc();
      if (doc) {
        nsIPresShell* presShell = doc->GetShell();
        nsPresContext* presContext;
        if (presShell && (presContext = presShell->GetPresContext())) {
          nsPresContext* rootDocPresContext =
            presContext->GetRootPresContext();
          if (!rootDocPresContext)
            return;
          nsIFrame* rootDocumentRootFrame = rootDocPresContext->
              PresShell()->FrameManager()->GetRootFrame();
          if ((event->eventStructType == NS_MOUSE_EVENT || 
               event->eventStructType == NS_MOUSE_SCROLL_EVENT ||
               event->eventStructType == NS_WHEEL_EVENT) &&
               !event->AsGUIEvent()->widget) {
            // no widget, so just use the client point if available
            nsCOMPtr<nsIDOMMouseEvent> mouseEvent = do_QueryInterface(aEvent);
            nsIntPoint clientPt;
            mouseEvent->GetClientX(&clientPt.x);
            mouseEvent->GetClientY(&clientPt.y);

            // XXX this doesn't handle IFRAMEs in transforms
            nsPoint thisDocToRootDocOffset = presShell->FrameManager()->
              GetRootFrame()->GetOffsetToCrossDoc(rootDocumentRootFrame);
            // convert to device pixels
            mCachedMousePoint.x = presContext->AppUnitsToDevPixels(
                nsPresContext::CSSPixelsToAppUnits(clientPt.x) + thisDocToRootDocOffset.x);
            mCachedMousePoint.y = presContext->AppUnitsToDevPixels(
                nsPresContext::CSSPixelsToAppUnits(clientPt.y) + thisDocToRootDocOffset.y);
          }
          else if (rootDocumentRootFrame) {
            nsPoint pnt =
              nsLayoutUtils::GetEventCoordinatesRelativeTo(event, rootDocumentRootFrame);
            mCachedMousePoint = nsIntPoint(rootDocPresContext->AppUnitsToDevPixels(pnt.x),
                                           rootDocPresContext->AppUnitsToDevPixels(pnt.y));
          }
        }
      }
    }
  }
  else {
    mRangeParent = nullptr;
    mRangeOffset = 0;
  }
}

void
nsXULPopupManager::SetActiveMenuBar(nsMenuBarFrame* aMenuBar, bool aActivate)
{
  if (aActivate)
    mActiveMenuBar = aMenuBar;
  else if (mActiveMenuBar == aMenuBar)
    mActiveMenuBar = nullptr;

  UpdateKeyboardListeners();
}

void
nsXULPopupManager::ShowMenu(nsIContent *aMenu,
                            bool aSelectFirstItem,
                            bool aAsynchronous)
{
  // generate any template content first. Otherwise, the menupopup may not
  // have been created yet.
  if (aMenu) {
    nsIContent* element = aMenu;
    do {
      nsCOMPtr<nsIDOMXULElement> xulelem = do_QueryInterface(element);
      if (xulelem) {
        nsCOMPtr<nsIXULTemplateBuilder> builder;
        xulelem->GetBuilder(getter_AddRefs(builder));
        if (builder) {
          builder->CreateContents(aMenu, true);
          break;
        }
      }
      element = element->GetParent();
    } while (element);
  }

  nsMenuFrame* menuFrame = do_QueryFrame(aMenu->GetPrimaryFrame());
  if (!menuFrame || !menuFrame->IsMenu())
    return;

  nsMenuPopupFrame* popupFrame =  menuFrame->GetPopup();
  if (!popupFrame || !MayShowPopup(popupFrame))
    return;

  // inherit whether or not we're a context menu from the parent
  bool parentIsContextMenu = false;
  bool onMenuBar = false;
  bool onmenu = menuFrame->IsOnMenu();

  nsMenuParent* parent = menuFrame->GetMenuParent();
  if (parent && onmenu) {
    parentIsContextMenu = parent->IsContextMenu();
    onMenuBar = parent->IsMenuBar();
  }

  nsAutoString position;
  if (onMenuBar || !onmenu)
    position.AssignLiteral("after_start");
  else
    position.AssignLiteral("end_before");

  // there is no trigger event for menus
  InitTriggerEvent(nullptr, nullptr, nullptr);
  popupFrame->InitializePopup(aMenu, nullptr, position, 0, 0, true);

  if (aAsynchronous) {
    nsCOMPtr<nsIRunnable> event =
      new nsXULPopupShowingEvent(popupFrame->GetContent(),
                                 parentIsContextMenu, aSelectFirstItem);
    NS_DispatchToCurrentThread(event);
  }
  else {
    nsCOMPtr<nsIContent> popupContent = popupFrame->GetContent();
    FirePopupShowingEvent(popupContent, parentIsContextMenu, aSelectFirstItem);
  }
}

void
nsXULPopupManager::ShowPopup(nsIContent* aPopup,
                             nsIContent* aAnchorContent,
                             const nsAString& aPosition,
                             int32_t aXPos, int32_t aYPos,
                             bool aIsContextMenu,
                             bool aAttributesOverride,
                             bool aSelectFirstItem,
                             nsIDOMEvent* aTriggerEvent)
{
  nsMenuPopupFrame* popupFrame = GetPopupFrameForContent(aPopup, true);
  if (!popupFrame || !MayShowPopup(popupFrame))
    return;

  nsCOMPtr<nsIContent> triggerContent;
  InitTriggerEvent(aTriggerEvent, aPopup, getter_AddRefs(triggerContent));

  popupFrame->InitializePopup(aAnchorContent, triggerContent, aPosition,
                              aXPos, aYPos, aAttributesOverride);

  FirePopupShowingEvent(aPopup, aIsContextMenu, aSelectFirstItem);
}

void
nsXULPopupManager::ShowPopupAtScreen(nsIContent* aPopup,
                                     int32_t aXPos, int32_t aYPos,
                                     bool aIsContextMenu,
                                     nsIDOMEvent* aTriggerEvent)
{
  nsMenuPopupFrame* popupFrame = GetPopupFrameForContent(aPopup, true);
  if (!popupFrame || !MayShowPopup(popupFrame))
    return;

  nsCOMPtr<nsIContent> triggerContent;
  InitTriggerEvent(aTriggerEvent, aPopup, getter_AddRefs(triggerContent));

  popupFrame->InitializePopupAtScreen(triggerContent, aXPos, aYPos, aIsContextMenu);
  FirePopupShowingEvent(aPopup, aIsContextMenu, false);
}

void
nsXULPopupManager::ShowTooltipAtScreen(nsIContent* aPopup,
                                       nsIContent* aTriggerContent,
                                       int32_t aXPos, int32_t aYPos)
{
  nsMenuPopupFrame* popupFrame = GetPopupFrameForContent(aPopup, true);
  if (!popupFrame || !MayShowPopup(popupFrame))
    return;

  InitTriggerEvent(nullptr, nullptr, nullptr);

  nsPresContext* pc = popupFrame->PresContext();
  mCachedMousePoint = nsIntPoint(pc->CSSPixelsToDevPixels(aXPos),
                                 pc->CSSPixelsToDevPixels(aYPos));

  // coordinates are relative to the root widget
  nsPresContext* rootPresContext = pc->GetRootPresContext();
  if (rootPresContext) {
    nsIWidget *rootWidget = rootPresContext->GetRootWidget();
    if (rootWidget) {
      mCachedMousePoint -= rootWidget->WidgetToScreenOffset();
    }
  }

  popupFrame->InitializePopupAtScreen(aTriggerContent, aXPos, aYPos, false);

  FirePopupShowingEvent(aPopup, false, false);
}

void
nsXULPopupManager::ShowPopupWithAnchorAlign(nsIContent* aPopup,
                                            nsIContent* aAnchorContent,
                                            nsAString& aAnchor,
                                            nsAString& aAlign,
                                            int32_t aXPos, int32_t aYPos,
                                            bool aIsContextMenu)
{
  nsMenuPopupFrame* popupFrame = GetPopupFrameForContent(aPopup, true);
  if (!popupFrame || !MayShowPopup(popupFrame))
    return;

  InitTriggerEvent(nullptr, nullptr, nullptr);

  popupFrame->InitializePopupWithAnchorAlign(aAnchorContent, aAnchor,
                                             aAlign, aXPos, aYPos);
  FirePopupShowingEvent(aPopup, aIsContextMenu, false);
}

static void
CheckCaretDrawingState() {

  // There is 1 caret per document, we need to find the focused
  // document and erase its caret.
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm) {
    nsCOMPtr<nsIDOMWindow> window;
    fm->GetFocusedWindow(getter_AddRefs(window));
    if (!window)
      return;

    nsCOMPtr<nsIDOMDocument> domDoc;
    nsCOMPtr<nsIDocument> focusedDoc;
    window->GetDocument(getter_AddRefs(domDoc));
    focusedDoc = do_QueryInterface(domDoc);
    if (!focusedDoc)
      return;

    nsIPresShell* presShell = focusedDoc->GetShell();
    if (!presShell)
      return;

    nsRefPtr<nsCaret> caret = presShell->GetCaret();
    if (!caret)
      return;
    caret->CheckCaretDrawingState();
  }
}

void
nsXULPopupManager::ShowPopupCallback(nsIContent* aPopup,
                                     nsMenuPopupFrame* aPopupFrame,
                                     bool aIsContextMenu,
                                     bool aSelectFirstItem)
{
  nsPopupType popupType = aPopupFrame->PopupType();
  bool ismenu = (popupType == ePopupTypeMenu);

  nsMenuChainItem* item =
    new nsMenuChainItem(aPopupFrame, aIsContextMenu, popupType);
  if (!item)
    return;

  // install keyboard event listeners for navigating menus. For panels, the
  // escape key may be used to close the panel. However, the ignorekeys
  // attribute may be used to disable adding these event listeners for popups
  // that want to handle their own keyboard events.
  if (aPopup->AttrValueIs(kNameSpaceID_None, nsGkAtoms::ignorekeys,
                           nsGkAtoms::_true, eCaseMatters))
    item->SetIgnoreKeys(true);

  if (ismenu) {
    // if the menu is on a menubar, use the menubar's listener instead
    nsMenuFrame* menuFrame = do_QueryFrame(aPopupFrame->GetParent());
    if (menuFrame) {
      item->SetOnMenuBar(menuFrame->IsOnMenuBar());
    }
  }

  // use a weak frame as the popup will set an open attribute if it is a menu
  nsWeakFrame weakFrame(aPopupFrame);
  aPopupFrame->ShowPopup(aIsContextMenu, aSelectFirstItem);
  ENSURE_TRUE(weakFrame.IsAlive());

  // popups normally hide when an outside click occurs. Panels may use
  // the noautohide attribute to disable this behaviour. It is expected
  // that the application will hide these popups manually. The tooltip
  // listener will handle closing the tooltip also.
  if (aPopupFrame->IsNoAutoHide() || popupType == ePopupTypeTooltip) {
    item->SetParent(mNoHidePanels);
    mNoHidePanels = item;
  }
  else {
    nsIContent* oldmenu = nullptr;
    if (mPopups)
      oldmenu = mPopups->Content();
    item->SetParent(mPopups);
    mPopups = item;
    SetCaptureState(oldmenu);
  }

  if (aSelectFirstItem) {
    nsMenuFrame* next = GetNextMenuItem(aPopupFrame, nullptr, true);
    aPopupFrame->SetCurrentMenuItem(next);
  }

  if (ismenu)
    UpdateMenuItems(aPopup);

  // Caret visibility may have been affected, ensure that
  // the caret isn't now drawn when it shouldn't be.
  CheckCaretDrawingState();
}

void
nsXULPopupManager::HidePopup(nsIContent* aPopup,
                             bool aHideChain,
                             bool aDeselectMenu,
                             bool aAsynchronous,
                             nsIContent* aLastPopup)
{
  // if the popup is on the nohide panels list, remove it but don't close any
  // other panels
  nsMenuPopupFrame* popupFrame = nullptr;
  bool foundPanel = false;
  nsMenuChainItem* item = mNoHidePanels;
  while (item) {
    if (item->Content() == aPopup) {
      foundPanel = true;
      popupFrame = item->Frame();
      break;
    }
    item = item->GetParent();
  }

  // when removing a menu, all of the child popups must be closed
  nsMenuChainItem* foundMenu = nullptr;
  item = mPopups;
  while (item) {
    if (item->Content() == aPopup) {
      foundMenu = item;
      break;
    }
    item = item->GetParent();
  }

  nsPopupType type = ePopupTypePanel;
  bool deselectMenu = false;
  nsCOMPtr<nsIContent> popupToHide, nextPopup, lastPopup;
  if (foundMenu) {
    // at this point, foundMenu will be set to the found item in the list. If
    // foundMenu is the topmost menu, the one to remove, then there are no other
    // popups to hide. If foundMenu is not the topmost menu, then there may be
    // open submenus below it. In this case, we need to make sure that those
    // submenus are closed up first. To do this, we scan up the menu list to
    // find the topmost popup with only menus between it and foundMenu and
    // close that menu first. In synchronous mode, the FirePopupHidingEvent
    // method will be called which in turn calls HidePopupCallback to close up
    // the next popup in the chain. These two methods will be called in
    // sequence recursively to close up all the necessary popups. In
    // asynchronous mode, a similar process occurs except that the
    // FirePopupHidingEvent method is called asynchronously. In either case,
    // nextPopup is set to the content node of the next popup to close, and
    // lastPopup is set to the last popup in the chain to close, which will be
    // aPopup, or null to close up all menus.

    nsMenuChainItem* topMenu = foundMenu;
    // Use IsMenu to ensure that foundMenu is a menu and scan down the child
    // list until a non-menu is found. If foundMenu isn't a menu at all, don't
    // scan and just close up this menu.
    if (foundMenu->IsMenu()) {
      item = topMenu->GetChild();
      while (item && item->IsMenu()) {
        topMenu = item;
        item = item->GetChild();
      }
    }
    
    deselectMenu = aDeselectMenu;
    popupToHide = topMenu->Content();
    popupFrame = topMenu->Frame();
    type = popupFrame->PopupType();

    nsMenuChainItem* parent = topMenu->GetParent();

    // close up another popup if there is one, and we are either hiding the
    // entire chain or the item to hide isn't the topmost popup.
    if (parent && (aHideChain || topMenu != foundMenu))
      nextPopup = parent->Content();

    lastPopup = aLastPopup ? aLastPopup : (aHideChain ? nullptr : aPopup);
  }
  else if (foundPanel) {
    popupToHide = aPopup;
  }

  if (popupFrame) {
    nsPopupState state = popupFrame->PopupState();
    // if the popup is already being hidden, don't attempt to hide it again
    if (state == ePopupHiding)
      return;
    // change the popup state to hiding. Don't set the hiding state if the
    // popup is invisible, otherwise nsMenuPopupFrame::HidePopup will
    // run again. In the invisible state, we just want the events to fire.
    if (state != ePopupInvisible)
      popupFrame->SetPopupState(ePopupHiding);

    // for menus, popupToHide is always the frontmost item in the list to hide.
    if (aAsynchronous) {
      nsCOMPtr<nsIRunnable> event =
        new nsXULPopupHidingEvent(popupToHide, nextPopup, lastPopup,
                                  type, deselectMenu);
        NS_DispatchToCurrentThread(event);
    }
    else {
      FirePopupHidingEvent(popupToHide, nextPopup, lastPopup,
                           popupFrame->PresContext(), type, deselectMenu);
    }
  }
}

void
nsXULPopupManager::HidePopupCallback(nsIContent* aPopup,
                                     nsMenuPopupFrame* aPopupFrame,
                                     nsIContent* aNextPopup,
                                     nsIContent* aLastPopup,
                                     nsPopupType aPopupType,
                                     bool aDeselectMenu)
{
  if (mCloseTimer && mTimerMenu == aPopupFrame) {
    mCloseTimer->Cancel();
    mCloseTimer = nullptr;
    mTimerMenu = nullptr;
  }

  // The popup to hide is aPopup. Search the list again to find the item that
  // corresponds to the popup to hide aPopup. This is done because it's
  // possible someone added another item (attempted to open another popup)
  // or removed a popup frame during the event processing so the item isn't at
  // the front anymore.
  nsMenuChainItem* item = mNoHidePanels;
  while (item) {
    if (item->Content() == aPopup) {
      item->Detach(&mNoHidePanels);
      break;
    }
    item = item->GetParent();
  }

  if (!item) {
    item = mPopups;
    while (item) {
      if (item->Content() == aPopup) {
        item->Detach(&mPopups);
        SetCaptureState(aPopup);
        break;
      }
      item = item->GetParent();
    }
  }

  delete item;

  nsWeakFrame weakFrame(aPopupFrame);
  aPopupFrame->HidePopup(aDeselectMenu, ePopupClosed);
  ENSURE_TRUE(weakFrame.IsAlive());

  // send the popuphidden event synchronously. This event has no default
  // behaviour.
  nsEventStatus status = nsEventStatus_eIgnore;
  WidgetMouseEvent event(true, NS_XUL_POPUP_HIDDEN, nullptr,
                         WidgetMouseEvent::eReal);
  nsEventDispatcher::Dispatch(aPopup, aPopupFrame->PresContext(),
                              &event, nullptr, &status);
  ENSURE_TRUE(weakFrame.IsAlive());

  // if there are more popups to close, look for the next one
  if (aNextPopup && aPopup != aLastPopup) {
    nsMenuChainItem* foundMenu = nullptr;
    nsMenuChainItem* item = mPopups;
    while (item) {
      if (item->Content() == aNextPopup) {
        foundMenu = item;
        break;
      }
      item = item->GetParent();
    }

    // continue hiding the chain of popups until the last popup aLastPopup
    // is reached, or until a popup of a different type is reached. This
    // last check is needed so that a menulist inside a non-menu panel only
    // closes the menu and not the panel as well.
    if (foundMenu &&
        (aLastPopup || aPopupType == foundMenu->PopupType())) {

      nsCOMPtr<nsIContent> popupToHide = item->Content();
      nsMenuChainItem* parent = item->GetParent();

      nsCOMPtr<nsIContent> nextPopup;
      if (parent && popupToHide != aLastPopup)
        nextPopup = parent->Content();

      nsMenuPopupFrame* popupFrame = item->Frame();
      nsPopupState state = popupFrame->PopupState();
      if (state == ePopupHiding)
        return;
      if (state != ePopupInvisible)
        popupFrame->SetPopupState(ePopupHiding);

      FirePopupHidingEvent(popupToHide, nextPopup, aLastPopup,
                           popupFrame->PresContext(),
                           foundMenu->PopupType(), aDeselectMenu);
    }
  }
}

void
nsXULPopupManager::HidePopup(nsIFrame* aFrame)
{
  nsMenuPopupFrame* popup = do_QueryFrame(aFrame);
  if (popup)
    HidePopup(aFrame->GetContent(), false, true, false);
}

void
nsXULPopupManager::HidePopupAfterDelay(nsMenuPopupFrame* aPopup)
{
  // Don't close up immediately.
  // Kick off a close timer.
  KillMenuTimer();

  int32_t menuDelay =
    LookAndFeel::GetInt(LookAndFeel::eIntID_SubmenuDelay, 300); // ms

  // Kick off the timer.
  mCloseTimer = do_CreateInstance("@mozilla.org/timer;1");
  mCloseTimer->InitWithCallback(this, menuDelay, nsITimer::TYPE_ONE_SHOT);

  // the popup will call PopupDestroyed if it is destroyed, which checks if it
  // is set to mTimerMenu, so it should be safe to keep a reference to it
  mTimerMenu = aPopup;
}

void
nsXULPopupManager::HidePopupsInList(const nsTArray<nsMenuPopupFrame *> &aFrames,
                                    bool aDeselectMenu)
{
  // Create a weak frame list. This is done in a separate array with the
  // right capacity predetermined, otherwise the array would get resized and
  // move the weak frame pointers around.
  nsTArray<nsWeakFrame> weakPopups(aFrames.Length());
  uint32_t f;
  for (f = 0; f < aFrames.Length(); f++) {
    nsWeakFrame* wframe = weakPopups.AppendElement();
    if (wframe)
      *wframe = aFrames[f];
  }

  for (f = 0; f < weakPopups.Length(); f++) {
    // check to ensure that the frame is still alive before hiding it.
    if (weakPopups[f].IsAlive()) {
      nsMenuPopupFrame* frame =
        static_cast<nsMenuPopupFrame *>(weakPopups[f].GetFrame());
      frame->HidePopup(true, ePopupInvisible);
    }
  }

  SetCaptureState(nullptr);
}

bool
nsXULPopupManager::IsChildOfDocShell(nsIDocument* aDoc, nsIDocShellTreeItem* aExpected)
{
  nsCOMPtr<nsIDocShellTreeItem> docShellItem(aDoc->GetDocShell());
  while(docShellItem) {
    if (docShellItem == aExpected)
      return true;

    nsCOMPtr<nsIDocShellTreeItem> parent;
    docShellItem->GetParent(getter_AddRefs(parent));
    docShellItem = parent;
  }

  return false;
}

void
nsXULPopupManager::HidePopupsInDocShell(nsIDocShellTreeItem* aDocShellToHide)
{
  nsTArray<nsMenuPopupFrame *> popupsToHide;

  // iterate to get the set of popup frames to hide
  nsMenuChainItem* item = mPopups;
  while (item) {
    nsMenuChainItem* parent = item->GetParent();
    if (item->Frame()->PopupState() != ePopupInvisible &&
        IsChildOfDocShell(item->Content()->OwnerDoc(), aDocShellToHide)) {
      nsMenuPopupFrame* frame = item->Frame();
      item->Detach(&mPopups);
      delete item;
      popupsToHide.AppendElement(frame);
    }
    item = parent;
  }

  // now look for panels to hide
  item = mNoHidePanels;
  while (item) {
    nsMenuChainItem* parent = item->GetParent();
    if (item->Frame()->PopupState() != ePopupInvisible &&
        IsChildOfDocShell(item->Content()->OwnerDoc(), aDocShellToHide)) {
      nsMenuPopupFrame* frame = item->Frame();
      item->Detach(&mNoHidePanels);
      delete item;
      popupsToHide.AppendElement(frame);
    }
    item = parent;
  }

  HidePopupsInList(popupsToHide, true);
}

void
nsXULPopupManager::ExecuteMenu(nsIContent* aMenu, nsXULMenuCommandEvent* aEvent)
{
  CloseMenuMode cmm = CloseMenuMode_Auto;

  static nsIContent::AttrValuesArray strings[] =
    {&nsGkAtoms::none, &nsGkAtoms::single, nullptr};

  switch (aMenu->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::closemenu,
                                 strings, eCaseMatters)) {
    case 0:
      cmm = CloseMenuMode_None;
      break;
    case 1:
      cmm = CloseMenuMode_Single;
      break;
    default:
      break;
  }

  // When a menuitem is selected to be executed, first hide all the open
  // popups, but don't remove them yet. This is needed when a menu command
  // opens a modal dialog. The views associated with the popups needed to be
  // hidden and the accesibility events fired before the command executes, but
  // the popuphiding/popuphidden events are fired afterwards.
  nsTArray<nsMenuPopupFrame *> popupsToHide;
  nsMenuChainItem* item = GetTopVisibleMenu();
  if (cmm != CloseMenuMode_None) {
    while (item) {
      // if it isn't a <menupopup>, don't close it automatically
      if (!item->IsMenu())
        break;
      nsMenuChainItem* next = item->GetParent();
      popupsToHide.AppendElement(item->Frame());
      if (cmm == CloseMenuMode_Single) // only close one level of menu
        break;
      item = next;
    }

    // Now hide the popups. If the closemenu mode is auto, deselect the menu,
    // otherwise only one popup is closing, so keep the parent menu selected.
    HidePopupsInList(popupsToHide, cmm == CloseMenuMode_Auto);
  }

  aEvent->SetCloseMenuMode(cmm);
  nsCOMPtr<nsIRunnable> event = aEvent;
  NS_DispatchToCurrentThread(event);
}

void
nsXULPopupManager::FirePopupShowingEvent(nsIContent* aPopup,
                                         bool aIsContextMenu,
                                         bool aSelectFirstItem)
{
  nsCOMPtr<nsIContent> popup = aPopup; // keep a strong reference to the popup

  nsMenuPopupFrame* popupFrame = do_QueryFrame(aPopup->GetPrimaryFrame());
  if (!popupFrame)
    return;

  nsPresContext *presContext = popupFrame->PresContext();
  nsCOMPtr<nsIPresShell> presShell = presContext->PresShell();
  nsPopupType popupType = popupFrame->PopupType();

  // generate the child frames if they have not already been generated
  if (!popupFrame->HasGeneratedChildren()) {
    popupFrame->SetGeneratedChildren();
    presShell->FrameConstructor()->GenerateChildFrames(popupFrame);
  }

  // get the frame again
  nsIFrame* frame = aPopup->GetPrimaryFrame();
  if (!frame)
    return;

  presShell->FrameNeedsReflow(frame, nsIPresShell::eTreeChange,
                              NS_FRAME_HAS_DIRTY_CHILDREN);

  // cache the popup so that document.popupNode can retrieve the trigger node
  // during the popupshowing event. It will be cleared below after the event
  // has fired.
  mOpeningPopup = aPopup;

  nsEventStatus status = nsEventStatus_eIgnore;
  WidgetMouseEvent event(true, NS_XUL_POPUP_SHOWING, nullptr,
                         WidgetMouseEvent::eReal);

  // coordinates are relative to the root widget
  nsPresContext* rootPresContext =
    presShell->GetPresContext()->GetRootPresContext();
  if (rootPresContext) {
    rootPresContext->PresShell()->GetViewManager()->
      GetRootWidget(getter_AddRefs(event.widget));
  }
  else {
    event.widget = nullptr;
  }

  event.refPoint = LayoutDeviceIntPoint::FromUntyped(mCachedMousePoint);
  event.modifiers = mCachedModifiers;
  nsEventDispatcher::Dispatch(popup, presContext, &event, nullptr, &status);

  mCachedMousePoint = nsIntPoint(0, 0);
  mOpeningPopup = nullptr;

  mCachedModifiers = 0;

  // if a panel, blur whatever has focus so that the panel can take the focus.
  // This is done after the popupshowing event in case that event is cancelled.
  // Using noautofocus="true" will disable this behaviour, which is needed for
  // the autocomplete widget as it manages focus itself.
  if (popupType == ePopupTypePanel &&
      !popup->AttrValueIs(kNameSpaceID_None, nsGkAtoms::noautofocus,
                           nsGkAtoms::_true, eCaseMatters)) {
    nsIFocusManager* fm = nsFocusManager::GetFocusManager();
    if (fm) {
      nsIDocument* doc = popup->GetCurrentDoc();

      // Only remove the focus if the currently focused item is ouside the
      // popup. It isn't a big deal if the current focus is in a child popup
      // inside the popup as that shouldn't be visible. This check ensures that
      // a node inside the popup that is focused during a popupshowing event
      // remains focused.
      nsCOMPtr<nsIDOMElement> currentFocusElement;
      fm->GetFocusedElement(getter_AddRefs(currentFocusElement));
      nsCOMPtr<nsIContent> currentFocus = do_QueryInterface(currentFocusElement);
      if (doc && currentFocus &&
          !nsContentUtils::ContentIsCrossDocDescendantOf(currentFocus, popup)) {
        fm->ClearFocus(doc->GetWindow());
      }
    }
  }

  // clear these as they are no longer valid
  mRangeParent = nullptr;
  mRangeOffset = 0;

  // get the frame again in case it went away
  popupFrame = do_QueryFrame(aPopup->GetPrimaryFrame());
  if (popupFrame) {
    // if the event was cancelled, don't open the popup, reset its state back
    // to closed and clear its trigger content.
    if (status == nsEventStatus_eConsumeNoDefault) {
      popupFrame->SetPopupState(ePopupClosed);
      popupFrame->ClearTriggerContent();
    }
    else {
      ShowPopupCallback(aPopup, popupFrame, aIsContextMenu, aSelectFirstItem);
    }
  }
}

void
nsXULPopupManager::FirePopupHidingEvent(nsIContent* aPopup,
                                        nsIContent* aNextPopup,
                                        nsIContent* aLastPopup,
                                        nsPresContext *aPresContext,
                                        nsPopupType aPopupType,
                                        bool aDeselectMenu)
{
  nsCOMPtr<nsIPresShell> presShell = aPresContext->PresShell();

  nsEventStatus status = nsEventStatus_eIgnore;
  WidgetMouseEvent event(true, NS_XUL_POPUP_HIDING, nullptr,
                         WidgetMouseEvent::eReal);
  nsEventDispatcher::Dispatch(aPopup, aPresContext, &event, nullptr, &status);

  // when a panel is closed, blur whatever has focus inside the popup
  if (aPopupType == ePopupTypePanel &&
      !aPopup->AttrValueIs(kNameSpaceID_None, nsGkAtoms::noautofocus,
                           nsGkAtoms::_true, eCaseMatters)) {
    nsIFocusManager* fm = nsFocusManager::GetFocusManager();
    if (fm) {
      nsIDocument* doc = aPopup->GetCurrentDoc();

      // Remove the focus from the focused node only if it is inside the popup.
      nsCOMPtr<nsIDOMElement> currentFocusElement;
      fm->GetFocusedElement(getter_AddRefs(currentFocusElement));
      nsCOMPtr<nsIContent> currentFocus = do_QueryInterface(currentFocusElement);
      if (doc && currentFocus &&
          nsContentUtils::ContentIsCrossDocDescendantOf(currentFocus, aPopup)) {
        fm->ClearFocus(doc->GetWindow());
      }
    }
  }

  // get frame again in case it went away
  nsMenuPopupFrame* popupFrame = do_QueryFrame(aPopup->GetPrimaryFrame());
  if (popupFrame) {
    // if the event was cancelled, don't hide the popup, and reset its
    // state back to open. Only popups in chrome shells can prevent a popup
    // from hiding.
    if (status == nsEventStatus_eConsumeNoDefault &&
        !popupFrame->IsInContentShell()) {
      popupFrame->SetPopupState(ePopupOpenAndVisible);
    }
    else {
      HidePopupCallback(aPopup, popupFrame, aNextPopup, aLastPopup,
                        aPopupType, aDeselectMenu);
    }
  }
}

bool
nsXULPopupManager::IsPopupOpen(nsIContent* aPopup)
{
  // a popup is open if it is in the open list. The assertions ensure that the
  // frame is in the correct state. If the popup is in the hiding or invisible
  // state, it will still be in the open popup list until it is closed.
  nsMenuChainItem* item = mPopups;
  while (item) {
    if (item->Content() == aPopup) {
      NS_ASSERTION(item->Frame()->IsOpen() ||
                   item->Frame()->PopupState() == ePopupHiding ||
                   item->Frame()->PopupState() == ePopupInvisible,
                   "popup in open list not actually open");
      return true;
    }
    item = item->GetParent();
  }

  item = mNoHidePanels;
  while (item) {
    if (item->Content() == aPopup) {
      NS_ASSERTION(item->Frame()->IsOpen() ||
                   item->Frame()->PopupState() == ePopupHiding ||
                   item->Frame()->PopupState() == ePopupInvisible,
                   "popup in open list not actually open");
      return true;
    }
    item = item->GetParent();
  }

  return false;
}

bool
nsXULPopupManager::IsPopupOpenForMenuParent(nsMenuParent* aMenuParent)
{
  nsMenuChainItem* item = GetTopVisibleMenu();
  while (item) {
    nsMenuPopupFrame* popup = item->Frame();
    if (popup && popup->IsOpen()) {
      nsMenuFrame* menuFrame = do_QueryFrame(popup->GetParent());
      if (menuFrame && menuFrame->GetMenuParent() == aMenuParent) {
        return true;
      }
    }
    item = item->GetParent();
  }

  return false;
}

nsIFrame*
nsXULPopupManager::GetTopPopup(nsPopupType aType)
{
  if ((aType == ePopupTypePanel || aType == ePopupTypeTooltip) && mNoHidePanels)
    return mNoHidePanels->Frame();

  nsMenuChainItem* item = GetTopVisibleMenu();
  while (item) {
    if (item->PopupType() == aType || aType == ePopupTypeAny)
      return item->Frame();
    item = item->GetParent();
  }

  return nullptr;
}

void
nsXULPopupManager::GetVisiblePopups(nsTArray<nsIFrame *>& aPopups)
{
  aPopups.Clear();

  nsMenuChainItem* item = mPopups;
  while (item) {
    if (item->Frame()->PopupState() == ePopupOpenAndVisible)
      aPopups.AppendElement(static_cast<nsIFrame*>(item->Frame()));
    item = item->GetParent();
  }

  item = mNoHidePanels;
  while (item) {
    // skip panels which are not open and visible as well as draggable popups,
    // as those don't respond to events.
    if (item->Frame()->PopupState() == ePopupOpenAndVisible && !item->Frame()->IsDragPopup()) {
      aPopups.AppendElement(static_cast<nsIFrame*>(item->Frame()));
    }
    item = item->GetParent();
  }
}

already_AddRefed<nsIDOMNode>
nsXULPopupManager::GetLastTriggerNode(nsIDocument* aDocument, bool aIsTooltip)
{
  if (!aDocument)
    return nullptr;

  nsCOMPtr<nsIDOMNode> node;

  // if mOpeningPopup is set, it means that a popupshowing event is being
  // fired. In this case, just use the cached node, as the popup is not yet in
  // the list of open popups.
  if (mOpeningPopup && mOpeningPopup->GetCurrentDoc() == aDocument &&
      aIsTooltip == (mOpeningPopup->Tag() == nsGkAtoms::tooltip)) {
    node = do_QueryInterface(nsMenuPopupFrame::GetTriggerContent(GetPopupFrameForContent(mOpeningPopup, false)));
  }
  else {
    nsMenuChainItem* item = aIsTooltip ? mNoHidePanels : mPopups;
    while (item) {
      // look for a popup of the same type and document.
      if ((item->PopupType() == ePopupTypeTooltip) == aIsTooltip &&
          item->Content()->GetCurrentDoc() == aDocument) {
        node = do_QueryInterface(nsMenuPopupFrame::GetTriggerContent(item->Frame()));
        if (node)
          break;
      }
      item = item->GetParent();
    }
  }

  return node.forget();
}

bool
nsXULPopupManager::MayShowPopup(nsMenuPopupFrame* aPopup)
{
  // if a popup's IsOpen method returns true, then the popup must always be in
  // the popup chain scanned in IsPopupOpen.
  NS_ASSERTION(!aPopup->IsOpen() || IsPopupOpen(aPopup->GetContent()),
               "popup frame state doesn't match XULPopupManager open state");

  nsPopupState state = aPopup->PopupState();

  // if the popup is not in the open popup chain, then it must have a state that
  // is either closed, in the process of being shown, or invisible.
  NS_ASSERTION(IsPopupOpen(aPopup->GetContent()) || state == ePopupClosed ||
               state == ePopupShowing || state == ePopupInvisible,
               "popup not in XULPopupManager open list is open");

  // don't show popups unless they are closed or invisible
  if (state != ePopupClosed && state != ePopupInvisible)
    return false;

  // Don't show popups that we already have in our popup chain
  if (IsPopupOpen(aPopup->GetContent())) {
    NS_WARNING("Refusing to show duplicate popup");
    return false;
  }

  // if the popup was just rolled up, don't reopen it
  nsCOMPtr<nsIWidget> widget = aPopup->GetWidget();
  if (widget && widget->GetLastRollup() == aPopup->GetContent())
      return false;

  nsCOMPtr<nsISupports> cont = aPopup->PresContext()->GetContainer();
  nsCOMPtr<nsIDocShellTreeItem> dsti = do_QueryInterface(cont);
  nsCOMPtr<nsIBaseWindow> baseWin = do_QueryInterface(dsti);
  if (!baseWin)
    return false;

  int32_t type = -1;
  if (NS_FAILED(dsti->GetItemType(&type)))
    return false;

  // chrome shells can always open popups, but other types of shells can only
  // open popups when they are focused and visible
  if (type != nsIDocShellTreeItem::typeChrome) {
    // only allow popups in active windows
    nsCOMPtr<nsIDocShellTreeItem> root;
    dsti->GetRootTreeItem(getter_AddRefs(root));
    nsCOMPtr<nsIDOMWindow> rootWin = do_GetInterface(root);

    nsIFocusManager* fm = nsFocusManager::GetFocusManager();
    if (!fm || !rootWin)
      return false;

    nsCOMPtr<nsIDOMWindow> activeWindow;
    fm->GetActiveWindow(getter_AddRefs(activeWindow));
    if (activeWindow != rootWin)
      return false;

    // only allow popups in visible frames
    bool visible;
    baseWin->GetVisibility(&visible);
    if (!visible)
      return false;
  }

  // platforms respond differently when an popup is opened in a minimized
  // window, so this is always disabled.
  nsCOMPtr<nsIWidget> mainWidget;
  baseWin->GetMainWidget(getter_AddRefs(mainWidget));
  if (mainWidget && mainWidget->SizeMode() == nsSizeMode_Minimized) {
    return false;
  }

  // cannot open a popup that is a submenu of a menupopup that isn't open.
  nsMenuFrame* menuFrame = do_QueryFrame(aPopup->GetParent());
  if (menuFrame) {
    nsMenuParent* parentPopup = menuFrame->GetMenuParent();
    if (parentPopup && !parentPopup->IsOpen())
      return false;
  }

  return true;
}

void
nsXULPopupManager::PopupDestroyed(nsMenuPopupFrame* aPopup)
{
  // when a popup frame is destroyed, just unhook it from the list of popups
  if (mTimerMenu == aPopup) {
    if (mCloseTimer) {
      mCloseTimer->Cancel();
      mCloseTimer = nullptr;
    }
    mTimerMenu = nullptr;
  }

  nsMenuChainItem* item = mNoHidePanels;
  while (item) {
    if (item->Frame() == aPopup) {
      item->Detach(&mNoHidePanels);
      delete item;
      break;
    }
    item = item->GetParent();
  }

  nsTArray<nsMenuPopupFrame *> popupsToHide;

  item = mPopups;
  while (item) {
    nsMenuPopupFrame* frame = item->Frame();
    if (frame == aPopup) {
      if (frame->PopupState() != ePopupInvisible) {
        // Iterate through any child menus and hide them as well, since the
        // parent is going away. We won't remove them from the list yet, just
        // hide them, as they will be removed from the list when this function
        // gets called for that child frame.
        nsMenuChainItem* child = item->GetChild();
        while (child) {
          // if the popup is a child frame of the menu that was destroyed, add
          // it to the list of popups to hide. Don't bother with the events
          // since the frames are going away. If the child menu is not a child
          // frame, for example, a context menu, use HidePopup instead, but call
          // it asynchronously since we are in the middle of frame destruction.
          nsMenuPopupFrame* childframe = child->Frame();
          if (nsLayoutUtils::IsProperAncestorFrame(frame, childframe)) {
            popupsToHide.AppendElement(childframe);
          }
          else {
            // HidePopup will take care of hiding any of its children, so
            // break out afterwards
            HidePopup(child->Content(), false, false, true);
            break;
          }

          child = child->GetChild();
        }
      }

      item->Detach(&mPopups);
      delete item;
      break;
    }

    item = item->GetParent();
  }

  HidePopupsInList(popupsToHide, false);
}

bool
nsXULPopupManager::HasContextMenu(nsMenuPopupFrame* aPopup)
{
  nsMenuChainItem* item = GetTopVisibleMenu();
  while (item && item->Frame() != aPopup) {
    if (item->IsContextMenu())
      return true;
    item = item->GetParent();
  }

  return false;
}

void
nsXULPopupManager::SetCaptureState(nsIContent* aOldPopup)
{
  nsMenuChainItem* item = GetTopVisibleMenu();
  if (item && aOldPopup == item->Content())
    return;

  if (mWidget) {
    mWidget->CaptureRollupEvents(nullptr, false);
    mWidget = nullptr;
  }

  if (item) {
    nsMenuPopupFrame* popup = item->Frame();
    mWidget = popup->GetWidget();
    if (mWidget) {
      mWidget->CaptureRollupEvents(nullptr, true);
      popup->AttachedDismissalListener();
    }
  }

  UpdateKeyboardListeners();
}

void
nsXULPopupManager::UpdateKeyboardListeners()
{
  nsCOMPtr<EventTarget> newTarget;
  bool isForMenu = false;
  nsMenuChainItem* item = GetTopVisibleMenu();
  if (item) {
    if (!item->IgnoreKeys())
      newTarget = item->Content()->GetDocument();
    isForMenu = item->PopupType() == ePopupTypeMenu;
  }
  else if (mActiveMenuBar) {
    newTarget = mActiveMenuBar->GetContent()->GetDocument();
    isForMenu = true;
  }

  if (mKeyListener != newTarget) {
    if (mKeyListener) {
      mKeyListener->RemoveEventListener(NS_LITERAL_STRING("keypress"), this, true);
      mKeyListener->RemoveEventListener(NS_LITERAL_STRING("keydown"), this, true);
      mKeyListener->RemoveEventListener(NS_LITERAL_STRING("keyup"), this, true);
      mKeyListener = nullptr;
      nsContentUtils::NotifyInstalledMenuKeyboardListener(false);
    }

    if (newTarget) {
      newTarget->AddEventListener(NS_LITERAL_STRING("keypress"), this, true);
      newTarget->AddEventListener(NS_LITERAL_STRING("keydown"), this, true);
      newTarget->AddEventListener(NS_LITERAL_STRING("keyup"), this, true);
      nsContentUtils::NotifyInstalledMenuKeyboardListener(isForMenu);
      mKeyListener = newTarget;
    }
  }
}

void
nsXULPopupManager::UpdateMenuItems(nsIContent* aPopup)
{
  // Walk all of the menu's children, checking to see if any of them has a
  // command attribute. If so, then several attributes must potentially be updated.

  nsCOMPtr<nsIDocument> document = aPopup->GetCurrentDoc();
  if (!document) {
    return;
  }

  for (nsCOMPtr<nsIContent> grandChild = aPopup->GetFirstChild();
       grandChild;
       grandChild = grandChild->GetNextSibling()) {
    if (grandChild->NodeInfo()->Equals(nsGkAtoms::menuitem, kNameSpaceID_XUL)) {
      // See if we have a command attribute.
      nsAutoString command;
      grandChild->GetAttr(kNameSpaceID_None, nsGkAtoms::command, command);
      if (!command.IsEmpty()) {
        // We do! Look it up in our document
        nsRefPtr<dom::Element> commandElement =
          document->GetElementById(command);
        if (commandElement) {
          nsAutoString commandValue;
          // The menu's disabled state needs to be updated to match the command.
          if (commandElement->GetAttr(kNameSpaceID_None, nsGkAtoms::disabled, commandValue))
            grandChild->SetAttr(kNameSpaceID_None, nsGkAtoms::disabled, commandValue, true);
          else
            grandChild->UnsetAttr(kNameSpaceID_None, nsGkAtoms::disabled, true);

          // The menu's label, accesskey checked and hidden states need to be updated
          // to match the command. Note that unlike the disabled state if the
          // command has *no* value, we assume the menu is supplying its own.
          if (commandElement->GetAttr(kNameSpaceID_None, nsGkAtoms::label, commandValue))
            grandChild->SetAttr(kNameSpaceID_None, nsGkAtoms::label, commandValue, true);

          if (commandElement->GetAttr(kNameSpaceID_None, nsGkAtoms::accesskey, commandValue))
            grandChild->SetAttr(kNameSpaceID_None, nsGkAtoms::accesskey, commandValue, true);

          if (commandElement->GetAttr(kNameSpaceID_None, nsGkAtoms::checked, commandValue))
            grandChild->SetAttr(kNameSpaceID_None, nsGkAtoms::checked, commandValue, true);

          if (commandElement->GetAttr(kNameSpaceID_None, nsGkAtoms::hidden, commandValue))
            grandChild->SetAttr(kNameSpaceID_None, nsGkAtoms::hidden, commandValue, true);
        }
      }
    }
  }
}

// Notify
//
// The item selection timer has fired, we might have to readjust the 
// selected item. There are two cases here that we are trying to deal with:
//   (1) diagonal movement from a parent menu to a submenu passing briefly over
//       other items, and
//   (2) moving out from a submenu to a parent or grandparent menu.
// In both cases, |mTimerMenu| is the menu item that might have an open submenu and
// the first item in |mPopups| is the item the mouse is currently over, which could be
// none of them.
//
// case (1):
//  As the mouse moves from the parent item of a submenu (we'll call 'A') diagonally into the
//  submenu, it probably passes through one or more sibilings (B). As the mouse passes
//  through B, it becomes the current menu item and the timer is set and mTimerMenu is 
//  set to A. Before the timer fires, the mouse leaves the menu containing A and B and
//  enters the submenus. Now when the timer fires, |mPopups| is null (!= |mTimerMenu|)
//  so we have to see if anything in A's children is selected (recall that even disabled
//  items are selected, the style just doesn't show it). If that is the case, we need to
//  set the selected item back to A.
//
// case (2);
//  Item A has an open submenu, and in it there is an item (B) which also has an open
//  submenu (so there are 3 menus displayed right now). The mouse then leaves B's child
//  submenu and selects an item that is a sibling of A, call it C. When the mouse enters C,
//  the timer is set and |mTimerMenu| is A and |mPopups| is C. As the timer fires,
//  the mouse is still within C. The correct behavior is to set the current item to C
//  and close up the chain parented at A.
//
//  This brings up the question of is the logic of case (1) enough? The answer is no,
//  and is discussed in bugzilla bug 29400. Case (1) asks if A's submenu has a selected
//  child, and if it does, set the selected item to A. Because B has a submenu open, it
//  is selected and as a result, A is set to be the selected item even though the mouse
//  rests in C -- very wrong. 
//
//  The solution is to use the same idea, but instead of only checking one level, 
//  drill all the way down to the deepest open submenu and check if it has something 
//  selected. Since the mouse is in a grandparent, it won't, and we know that we can
//  safely close up A and all its children.
//
// The code below melds the two cases together.
//
nsresult
nsXULPopupManager::Notify(nsITimer* aTimer)
{
  if (aTimer == mCloseTimer)
    KillMenuTimer();

  return NS_OK;
}

void
nsXULPopupManager::KillMenuTimer()
{
  if (mCloseTimer && mTimerMenu) {
    mCloseTimer->Cancel();
    mCloseTimer = nullptr;

    if (mTimerMenu->IsOpen())
      HidePopup(mTimerMenu->GetContent(), false, false, true);
  }

  mTimerMenu = nullptr;
}

void
nsXULPopupManager::CancelMenuTimer(nsMenuParent* aMenuParent)
{
  if (mCloseTimer && mTimerMenu == aMenuParent) {
    mCloseTimer->Cancel();
    mCloseTimer = nullptr;
    mTimerMenu = nullptr;
  }
}

bool
nsXULPopupManager::HandleShortcutNavigation(nsIDOMKeyEvent* aKeyEvent,
                                            nsMenuPopupFrame* aFrame)
{
  nsMenuChainItem* item = GetTopVisibleMenu();
  if (!aFrame && item)
    aFrame = item->Frame();

  if (aFrame) {
    bool action;
    nsMenuFrame* result = aFrame->FindMenuWithShortcut(aKeyEvent, action);
    if (result) {
      aFrame->ChangeMenuItem(result, false);
      if (action) {
        WidgetGUIEvent* evt = aKeyEvent->GetInternalNSEvent()->AsGUIEvent();
        nsMenuFrame* menuToOpen = result->Enter(evt);
        if (menuToOpen) {
          nsCOMPtr<nsIContent> content = menuToOpen->GetContent();
          ShowMenu(content, true, false);
        }
      }
      return true;
    }

    return false;
  }

  if (mActiveMenuBar) {
    nsMenuFrame* result = mActiveMenuBar->FindMenuWithShortcut(aKeyEvent);
    if (result) {
      mActiveMenuBar->SetActive(true);
      result->OpenMenu(true);
      return true;
    }
  }

  return false;
}


bool
nsXULPopupManager::HandleKeyboardNavigation(uint32_t aKeyCode)
{
  // navigate up through the open menus, looking for the topmost one
  // in the same hierarchy
  nsMenuChainItem* item = nullptr;
  nsMenuChainItem* nextitem = GetTopVisibleMenu();

  while (nextitem) {
    item = nextitem;
    nextitem = item->GetParent();

    if (nextitem) {
      // stop if the parent isn't a menu
      if (!nextitem->IsMenu())
        break;

      // check to make sure that the parent is actually the parent menu. It won't
      // be if the parent is in a different frame hierarchy, for example, for a
      // context menu opened on another menu.
      nsMenuParent* expectedParent = static_cast<nsMenuParent *>(nextitem->Frame());
      nsMenuFrame* menuFrame = do_QueryFrame(item->Frame()->GetParent());
      if (!menuFrame || menuFrame->GetMenuParent() != expectedParent) {
        break;
      }
    }
  }

  nsIFrame* itemFrame;
  if (item)
    itemFrame = item->Frame();
  else if (mActiveMenuBar)
    itemFrame = mActiveMenuBar;
  else
    return false;

  nsNavigationDirection theDirection;
  NS_ASSERTION(aKeyCode >= nsIDOMKeyEvent::DOM_VK_END &&
                 aKeyCode <= nsIDOMKeyEvent::DOM_VK_DOWN, "Illegal key code");
  theDirection = NS_DIRECTION_FROM_KEY_CODE(itemFrame, aKeyCode);

  // if a popup is open, first check for navigation within the popup
  if (item && HandleKeyboardNavigationInPopup(item, theDirection))
    return true;

  // no popup handled the key, so check the active menubar, if any
  if (mActiveMenuBar) {
    nsMenuFrame* currentMenu = mActiveMenuBar->GetCurrentMenuItem();
  
    if (NS_DIRECTION_IS_INLINE(theDirection)) {
      nsMenuFrame* nextItem = (theDirection == eNavigationDirection_End) ?
                              GetNextMenuItem(mActiveMenuBar, currentMenu, false) : 
                              GetPreviousMenuItem(mActiveMenuBar, currentMenu, false);
      mActiveMenuBar->ChangeMenuItem(nextItem, true);
      return true;
    }
    else if (NS_DIRECTION_IS_BLOCK(theDirection)) {
      // Open the menu and select its first item.
      if (currentMenu) {
        nsCOMPtr<nsIContent> content = currentMenu->GetContent();
        ShowMenu(content, true, false);
      }
      return true;
    }
  }

  return false;
}

bool
nsXULPopupManager::HandleKeyboardNavigationInPopup(nsMenuChainItem* item,
                                                   nsMenuPopupFrame* aFrame,
                                                   nsNavigationDirection aDir)
{
  NS_ASSERTION(aFrame, "aFrame is null");
  NS_ASSERTION(!item || item->Frame() == aFrame,
               "aFrame is expected to be equal to item->Frame()");

  nsMenuFrame* currentMenu = aFrame->GetCurrentMenuItem();

  aFrame->ClearIncrementalString();

  // This method only gets called if we're open.
  if (!currentMenu && NS_DIRECTION_IS_INLINE(aDir)) {
    // We've been opened, but we haven't had anything selected.
    // We can handle End, but our parent handles Start.
    if (aDir == eNavigationDirection_End) {
      nsMenuFrame* nextItem = GetNextMenuItem(aFrame, nullptr, true);
      if (nextItem) {
        aFrame->ChangeMenuItem(nextItem, false);
        return true;
      }
    }
    return false;
  }

  bool isContainer = false;
  bool isOpen = false;
  if (currentMenu) {
    isOpen = currentMenu->IsOpen();
    isContainer = currentMenu->IsMenu();
    if (isOpen) {
      // for an open popup, have the child process the event
      nsMenuChainItem* child = item ? item->GetChild() : nullptr;
      if (child && HandleKeyboardNavigationInPopup(child, aDir))
        return true;
    }
    else if (aDir == eNavigationDirection_End &&
             isContainer && !currentMenu->IsDisabled()) {
      // The menu is not yet open. Open it and select the first item.
      nsCOMPtr<nsIContent> content = currentMenu->GetContent();
      ShowMenu(content, true, false);
      return true;
    }
  }

  // For block progression, we can move in either direction
  if (NS_DIRECTION_IS_BLOCK(aDir) ||
      NS_DIRECTION_IS_BLOCK_TO_EDGE(aDir)) {
    nsMenuFrame* nextItem;

    if (aDir == eNavigationDirection_Before)
      nextItem = GetPreviousMenuItem(aFrame, currentMenu, true);
    else if (aDir == eNavigationDirection_After)
      nextItem = GetNextMenuItem(aFrame, currentMenu, true);
    else if (aDir == eNavigationDirection_First)
      nextItem = GetNextMenuItem(aFrame, nullptr, true);
    else
      nextItem = GetPreviousMenuItem(aFrame, nullptr, true);

    if (nextItem) {
      aFrame->ChangeMenuItem(nextItem, false);
      return true;
    }
  }
  else if (currentMenu && isContainer && isOpen) {
    if (aDir == eNavigationDirection_Start) {
      // close a submenu when Left is pressed
      nsMenuPopupFrame* popupFrame = currentMenu->GetPopup();
      if (popupFrame)
        HidePopup(popupFrame->GetContent(), false, false, false);
      return true;
    }
  }

  return false;
}

bool
nsXULPopupManager::HandleKeyboardEventWithKeyCode(
                        nsIDOMKeyEvent* aKeyEvent,
                        nsMenuChainItem* aTopVisibleMenuItem)
{
  uint32_t keyCode;
  aKeyEvent->GetKeyCode(&keyCode);

  // Escape should close panels, but the other keys should have no effect.
  if (aTopVisibleMenuItem &&
      aTopVisibleMenuItem->PopupType() != ePopupTypeMenu) {
    if (keyCode == nsIDOMKeyEvent::DOM_VK_ESCAPE) {
      HidePopup(aTopVisibleMenuItem->Content(), false, false, false);
      aKeyEvent->StopPropagation();
      aKeyEvent->PreventDefault();
    }
    return true;
  }

  bool consume = (mPopups || mActiveMenuBar);
  switch (keyCode) {
    case nsIDOMKeyEvent::DOM_VK_LEFT:
    case nsIDOMKeyEvent::DOM_VK_RIGHT:
    case nsIDOMKeyEvent::DOM_VK_UP:
    case nsIDOMKeyEvent::DOM_VK_DOWN:
    case nsIDOMKeyEvent::DOM_VK_HOME:
    case nsIDOMKeyEvent::DOM_VK_END:
      HandleKeyboardNavigation(keyCode);
      break;

    case nsIDOMKeyEvent::DOM_VK_ESCAPE:
      // Pressing Escape hides one level of menus only. If no menu is open,
      // check if a menubar is active and inform it that a menu closed. Even
      // though in this latter case, a menu didn't actually close, the effect
      // ends up being the same. Similar for the tab key below.
      if (aTopVisibleMenuItem) {
        HidePopup(aTopVisibleMenuItem->Content(), false, false, false);
      } else if (mActiveMenuBar) {
        mActiveMenuBar->MenuClosed();
      }
      break;

    case nsIDOMKeyEvent::DOM_VK_TAB:
#ifndef XP_MACOSX
    case nsIDOMKeyEvent::DOM_VK_F10:
#endif
      // close popups or deactivate menubar when Tab or F10 are pressed
      if (aTopVisibleMenuItem) {
        Rollup(0, nullptr, nullptr);
      } else if (mActiveMenuBar) {
        mActiveMenuBar->MenuClosed();
      }
      break;

    case nsIDOMKeyEvent::DOM_VK_ENTER:
    case nsIDOMKeyEvent::DOM_VK_RETURN: {
      // If there is a popup open, check if the current item needs to be opened.
      // Otherwise, tell the active menubar, if any, to activate the menu. The
      // Enter method will return a menu if one needs to be opened as a result.
      nsMenuFrame* menuToOpen = nullptr;
      WidgetGUIEvent* GUIEvent = aKeyEvent->GetInternalNSEvent()->AsGUIEvent();
      if (aTopVisibleMenuItem) {
        menuToOpen = aTopVisibleMenuItem->Frame()->Enter(GUIEvent);
      } else if (mActiveMenuBar) {
        menuToOpen = mActiveMenuBar->Enter(GUIEvent);
      }
      if (menuToOpen) {
        nsCOMPtr<nsIContent> content = menuToOpen->GetContent();
        ShowMenu(content, true, false);
      }
      break;
    }

    default:
      return false;
  }

  if (consume) {
    aKeyEvent->StopPropagation();
    aKeyEvent->PreventDefault();
  }
  return true;
}

nsMenuFrame*
nsXULPopupManager::GetNextMenuItem(nsIFrame* aParent,
                                   nsMenuFrame* aStart,
                                   bool aIsPopup)
{
  nsPresContext* presContext = aParent->PresContext();
  nsIFrame* immediateParent = presContext->PresShell()->
    FrameConstructor()->GetInsertionPoint(aParent->GetContent(), nullptr);
  if (!immediateParent)
    immediateParent = aParent;

  nsIFrame* currFrame = nullptr;
  if (aStart)
    currFrame = aStart->GetNextSibling();
  else 
    currFrame = immediateParent->GetFirstPrincipalChild();
  
  while (currFrame) {
    // See if it's a menu item.
    if (IsValidMenuItem(presContext, currFrame->GetContent(), aIsPopup)) {
      return do_QueryFrame(currFrame);
    }
    currFrame = currFrame->GetNextSibling();
  }

  currFrame = immediateParent->GetFirstPrincipalChild();

  // Still don't have anything. Try cycling from the beginning.
  while (currFrame && currFrame != aStart) {
    // See if it's a menu item.
    if (IsValidMenuItem(presContext, currFrame->GetContent(), aIsPopup)) {
      return do_QueryFrame(currFrame);
    }

    currFrame = currFrame->GetNextSibling();
  }

  // No luck. Just return our start value.
  return aStart;
}

nsMenuFrame*
nsXULPopupManager::GetPreviousMenuItem(nsIFrame* aParent,
                                       nsMenuFrame* aStart,
                                       bool aIsPopup)
{
  nsPresContext* presContext = aParent->PresContext();
  nsIFrame* immediateParent = presContext->PresShell()->
    FrameConstructor()->GetInsertionPoint(aParent->GetContent(), nullptr);
  if (!immediateParent)
    immediateParent = aParent;

  const nsFrameList& frames(immediateParent->PrincipalChildList());

  nsIFrame* currFrame = nullptr;
  if (aStart)
    currFrame = aStart->GetPrevSibling();
  else
    currFrame = frames.LastChild();

  while (currFrame) {
    // See if it's a menu item.
    if (IsValidMenuItem(presContext, currFrame->GetContent(), aIsPopup)) {
      return do_QueryFrame(currFrame);
    }
    currFrame = currFrame->GetPrevSibling();
  }

  currFrame = frames.LastChild();

  // Still don't have anything. Try cycling from the end.
  while (currFrame && currFrame != aStart) {
    // See if it's a menu item.
    if (IsValidMenuItem(presContext, currFrame->GetContent(), aIsPopup)) {
      return do_QueryFrame(currFrame);
    }

    currFrame = currFrame->GetPrevSibling();
  }

  // No luck. Just return our start value.
  return aStart;
}

bool
nsXULPopupManager::IsValidMenuItem(nsPresContext* aPresContext,
                                   nsIContent* aContent,
                                   bool aOnPopup)
{
  int32_t ns = aContent->GetNameSpaceID();
  nsIAtom *tag = aContent->Tag();
  if (ns == kNameSpaceID_XUL) {
    if (tag != nsGkAtoms::menu && tag != nsGkAtoms::menuitem)
      return false;
  }
  else if (ns != kNameSpaceID_XHTML || !aOnPopup || tag != nsGkAtoms::option) {
    return false;
  }

  bool skipNavigatingDisabledMenuItem = true;
  if (aOnPopup) {
    skipNavigatingDisabledMenuItem =
      LookAndFeel::GetInt(LookAndFeel::eIntID_SkipNavigatingDisabledMenuItem,
                          0) != 0;
  }

  return !(skipNavigatingDisabledMenuItem &&
           aContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::disabled,
                                 nsGkAtoms::_true, eCaseMatters));
}

nsresult
nsXULPopupManager::HandleEvent(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMKeyEvent> keyEvent = do_QueryInterface(aEvent);
  NS_ENSURE_TRUE(keyEvent, NS_ERROR_UNEXPECTED);

  //handlers shouldn't be triggered by non-trusted events.
  bool trustedEvent = false;
  aEvent->GetIsTrusted(&trustedEvent);
  if (!trustedEvent) {
    return NS_OK;
  }

  nsAutoString eventType;
  keyEvent->GetType(eventType);
  if (eventType.EqualsLiteral("keyup")) {
    return KeyUp(keyEvent);
  }
  if (eventType.EqualsLiteral("keydown")) {
    return KeyDown(keyEvent);
  }
  if (eventType.EqualsLiteral("keypress")) {
    return KeyPress(keyEvent);
  }

  NS_ABORT();

  return NS_OK;
}

nsresult
nsXULPopupManager::KeyUp(nsIDOMKeyEvent* aKeyEvent)
{
  // don't do anything if a menu isn't open or a menubar isn't active
  if (!mActiveMenuBar) {
    nsMenuChainItem* item = GetTopVisibleMenu();
    if (!item || item->PopupType() != ePopupTypeMenu)
      return NS_OK;
  }

  aKeyEvent->StopPropagation();
  aKeyEvent->PreventDefault();

  return NS_OK; // I am consuming event
}

nsresult
nsXULPopupManager::KeyDown(nsIDOMKeyEvent* aKeyEvent)
{
  nsMenuChainItem* item = GetTopVisibleMenu();
  if (item && item->Frame()->IsMenuLocked())
    return NS_OK;

  if (HandleKeyboardEventWithKeyCode(aKeyEvent, item)) {
    return NS_OK;
  }

  // don't do anything if a menu isn't open or a menubar isn't active
  if (!mActiveMenuBar && (!item || item->PopupType() != ePopupTypeMenu))
    return NS_OK;

  int32_t menuAccessKey = -1;

  // If the key just pressed is the access key (usually Alt),
  // dismiss and unfocus the menu.

  nsMenuBarListener::GetMenuAccessKey(&menuAccessKey);
  if (menuAccessKey) {
    uint32_t theChar;
    aKeyEvent->GetKeyCode(&theChar);

    if (theChar == (uint32_t)menuAccessKey) {
      bool ctrl = false;
      if (menuAccessKey != nsIDOMKeyEvent::DOM_VK_CONTROL)
        aKeyEvent->GetCtrlKey(&ctrl);
      bool alt=false;
      if (menuAccessKey != nsIDOMKeyEvent::DOM_VK_ALT)
        aKeyEvent->GetAltKey(&alt);
      bool shift=false;
      if (menuAccessKey != nsIDOMKeyEvent::DOM_VK_SHIFT)
        aKeyEvent->GetShiftKey(&shift);
      bool meta=false;
      if (menuAccessKey != nsIDOMKeyEvent::DOM_VK_META)
        aKeyEvent->GetMetaKey(&meta);
      if (!(ctrl || alt || shift || meta)) {
        // The access key just went down and no other
        // modifiers are already down.
        if (mPopups)
          Rollup(0, nullptr, nullptr);
        else if (mActiveMenuBar)
          mActiveMenuBar->MenuClosed();
      }
      aKeyEvent->PreventDefault();
    }
  }

  // Since a menu was open, stop propagation of the event to keep other event
  // listeners from becoming confused.
  aKeyEvent->StopPropagation();
  return NS_OK;
}

nsresult
nsXULPopupManager::KeyPress(nsIDOMKeyEvent* aKeyEvent)
{
  // Don't check prevent default flag -- menus always get first shot at key events.

  nsMenuChainItem* item = GetTopVisibleMenu();
  if (item &&
      (item->Frame()->IsMenuLocked() || item->PopupType() != ePopupTypeMenu)) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMKeyEvent> keyEvent = do_QueryInterface(aKeyEvent);
  NS_ENSURE_TRUE(keyEvent, NS_ERROR_UNEXPECTED);
  // if a menu is open or a menubar is active, it consumes the key event
  bool consume = (mPopups || mActiveMenuBar);
  HandleShortcutNavigation(keyEvent, nullptr);
  if (consume) {
    aKeyEvent->StopPropagation();
    aKeyEvent->PreventDefault();
  }

  return NS_OK; // I am consuming event
}

NS_IMETHODIMP
nsXULPopupShowingEvent::Run()
{
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm) {
    pm->FirePopupShowingEvent(mPopup, mIsContextMenu, mSelectFirstItem);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXULPopupHidingEvent::Run()
{
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();

  nsIDocument *document = mPopup->GetCurrentDoc();
  if (pm && document) {
    nsIPresShell* presShell = document->GetShell();
    if (presShell) {
      nsPresContext* context = presShell->GetPresContext();
      if (context) {
        pm->FirePopupHidingEvent(mPopup, mNextPopup, mLastPopup,
                                 context, mPopupType, mDeselectMenu);
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXULMenuCommandEvent::Run()
{
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (!pm)
    return NS_OK;

  // The order of the nsViewManager and nsIPresShell COM pointers is
  // important below.  We want the pres shell to get released before the
  // associated view manager on exit from this function.
  // See bug 54233.
  // XXXndeakin is this still needed?

  nsCOMPtr<nsIContent> popup;
  nsMenuFrame* menuFrame = do_QueryFrame(mMenu->GetPrimaryFrame());
  nsWeakFrame weakFrame(menuFrame);
  if (menuFrame && mFlipChecked) {
    if (menuFrame->IsChecked()) {
      mMenu->UnsetAttr(kNameSpaceID_None, nsGkAtoms::checked, true);
    } else {
      mMenu->SetAttr(kNameSpaceID_None, nsGkAtoms::checked,
                     NS_LITERAL_STRING("true"), true);
    }
  }

  if (menuFrame && weakFrame.IsAlive()) {
    // Find the popup that the menu is inside. Below, this popup will
    // need to be hidden.
    nsIFrame* frame = menuFrame->GetParent();
    while (frame) {
      nsMenuPopupFrame* popupFrame = do_QueryFrame(frame);
      if (popupFrame) {
        popup = popupFrame->GetContent();
        break;
      }
      frame = frame->GetParent();
    }

    nsPresContext* presContext = menuFrame->PresContext();
    nsCOMPtr<nsIPresShell> shell = presContext->PresShell();
    nsRefPtr<nsViewManager> kungFuDeathGrip = shell->GetViewManager();

    // Deselect ourselves.
    if (mCloseMenuMode != CloseMenuMode_None)
      menuFrame->SelectMenu(false);

    AutoHandlingUserInputStatePusher userInpStatePusher(mUserInput, nullptr,
                                                        shell->GetDocument());
    nsContentUtils::DispatchXULCommand(mMenu, mIsTrusted, nullptr, shell,
                                       mControl, mAlt, mShift, mMeta);
  }

  if (popup && mCloseMenuMode != CloseMenuMode_None)
    pm->HidePopup(popup, mCloseMenuMode == CloseMenuMode_Auto, true, false);

  return NS_OK;
}
