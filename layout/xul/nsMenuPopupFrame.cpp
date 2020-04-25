/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMenuPopupFrame.h"
#include "nsGkAtoms.h"
#include "nsIContent.h"
#include "nsAtom.h"
#include "nsPresContext.h"
#include "mozilla/ComputedStyle.h"
#include "nsCSSRendering.h"
#include "nsNameSpaceManager.h"
#include "nsViewManager.h"
#include "nsWidgetsCID.h"
#include "nsMenuFrame.h"
#include "nsMenuBarFrame.h"
#include "nsPopupSetFrame.h"
#include "nsPIDOMWindow.h"
#include "nsFrameManager.h"
#include "mozilla/dom/Document.h"
#include "nsRect.h"
#include "nsBoxLayoutState.h"
#include "nsIScrollableFrame.h"
#include "nsIPopupContainer.h"
#include "nsIDocShell.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsLayoutUtils.h"
#include "nsContentUtils.h"
#include "nsCSSFrameConstructor.h"
#include "nsPIWindowRoot.h"
#include "nsIReflowCallback.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIBaseWindow.h"
#include "nsISound.h"
#include "nsIScreenManager.h"
#include "nsStyleConsts.h"
#include "nsStyleStructInlines.h"
#include "nsTransitionManager.h"
#include "nsDisplayList.h"
#include "nsIDOMXULSelectCntrlEl.h"
#include "mozilla/AnimationUtils.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/EventStates.h"
#include "mozilla/Preferences.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/PresShell.h"
#include "mozilla/StaticPrefs_xul.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/KeyboardEvent.h"
#include "mozilla/dom/KeyboardEventBinding.h"
#include <algorithm>
#ifdef MOZ_WAYLAND
#  include <gdk/gdk.h>
#  include <gdk/gdkx.h>
#  include <gdk/gdkwayland.h>
#endif /* MOZ_WAYLAND */

#include "X11UndefineNone.h"

using namespace mozilla;
using mozilla::dom::Document;
using mozilla::dom::Element;
using mozilla::dom::Event;
using mozilla::dom::KeyboardEvent;

int8_t nsMenuPopupFrame::sDefaultLevelIsTop = -1;

DOMTimeStamp nsMenuPopupFrame::sLastKeyTime = 0;

// XXX, kyle.yuan@sun.com, there are 4 definitions for the same purpose:
//  nsMenuPopupFrame.h, nsListControlFrame.cpp, listbox.xml, tree.xml
//  need to find a good place to put them together.
//  if someone changes one, please also change the other.
uint32_t nsMenuPopupFrame::sTimeoutOfIncrementalSearch = 1000;

const char kPrefIncrementalSearchTimeout[] =
    "ui.menu.incremental_search.timeout";

// NS_NewMenuPopupFrame
//
// Wrapper for creating a new menu popup container
//
nsIFrame* NS_NewMenuPopupFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
  return new (aPresShell)
      nsMenuPopupFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsMenuPopupFrame)

NS_QUERYFRAME_HEAD(nsMenuPopupFrame)
  NS_QUERYFRAME_ENTRY(nsMenuPopupFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsBoxFrame)

//
// nsMenuPopupFrame ctor
//
nsMenuPopupFrame::nsMenuPopupFrame(ComputedStyle* aStyle,
                                   nsPresContext* aPresContext)
    : nsBoxFrame(aStyle, aPresContext, kClassID),
      mCurrentMenu(nullptr),
      mView(nullptr),
      mPrefSize(-1, -1),
      mXPos(0),
      mYPos(0),
      mAlignmentOffset(0),
      mLastClientOffset(0, 0),
      mPopupType(ePopupTypePanel),
      mPopupState(ePopupClosed),
      mPopupAlignment(POPUPALIGNMENT_NONE),
      mPopupAnchor(POPUPALIGNMENT_NONE),
      mPosition(POPUPPOSITION_UNKNOWN),
      mFlip(FlipType_Default),
      mIsOpenChanged(false),
      mIsContextMenu(false),
      mAdjustOffsetForContextMenu(false),
      mGeneratedChildren(false),
      mMenuCanOverlapOSBar(false),
      mShouldAutoPosition(true),
      mInContentShell(true),
      mIsMenuLocked(false),
      mMouseTransparent(false),
      mIsOffset(false),
      mHFlip(false),
      mVFlip(false),
      mPositionedOffset(0),
      mAnchorType(MenuPopupAnchorType_Node) {
  // the preference name is backwards here. True means that the 'top' level is
  // the default, and false means that the 'parent' level is the default.
  if (sDefaultLevelIsTop >= 0) return;
  sDefaultLevelIsTop =
      Preferences::GetBool("ui.panel.default_level_parent", false);
  Preferences::AddUintVarCache(&sTimeoutOfIncrementalSearch,
                               kPrefIncrementalSearchTimeout, 1000);
}  // ctor

void nsMenuPopupFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                            nsIFrame* aPrevInFlow) {
  nsBoxFrame::Init(aContent, aParent, aPrevInFlow);

  // lookup if we're allowed to overlap the OS bar (menubar/taskbar) from the
  // look&feel object
  mMenuCanOverlapOSBar =
      LookAndFeel::GetInt(LookAndFeel::eIntID_MenusCanOverlapOSBar) != 0;

  CreatePopupView();

  // XXX Hack. The popup's view should float above all other views,
  // so we use the nsView::SetFloating() to tell the view manager
  // about that constraint.
  nsView* ourView = GetView();
  nsViewManager* viewManager = ourView->GetViewManager();
  viewManager->SetViewFloating(ourView, true);

  mPopupType = ePopupTypePanel;
  if (aContent->IsAnyOfXULElements(nsGkAtoms::menupopup, nsGkAtoms::popup)) {
    mPopupType = ePopupTypeMenu;
  } else if (aContent->IsXULElement(nsGkAtoms::tooltip)) {
    mPopupType = ePopupTypeTooltip;
  }

  if (PresContext()->IsChrome()) {
    mInContentShell = false;
  }

  // Support incontentshell=false attribute to allow popups to be displayed
  // outside of the content shell. Chrome only.
  if (aContent->NodePrincipal()->IsSystemPrincipal()) {
    if (aContent->AsElement()->AttrValueIs(kNameSpaceID_None,
                                           nsGkAtoms::incontentshell,
                                           nsGkAtoms::_true, eCaseMatters)) {
      mInContentShell = true;
    } else if (aContent->AsElement()->AttrValueIs(
                   kNameSpaceID_None, nsGkAtoms::incontentshell,
                   nsGkAtoms::_false, eCaseMatters)) {
      mInContentShell = false;
    }
  }
  // To improve performance, create the widget for the popup only if it is not
  // a leaf. Leaf popups such as menus will create their widgets later when
  // the popup opens.
  if (!IsLeaf() && !ourView->HasWidget()) {
    CreateWidgetForView(ourView);
  }

  if (aContent->NodeInfo()->Equals(nsGkAtoms::tooltip, kNameSpaceID_XUL) &&
      aContent->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::_default,
                                         nsGkAtoms::_true, eIgnoreCase)) {
    nsIPopupContainer* popupContainer =
        nsIPopupContainer::GetPopupContainer(PresContext()->GetPresShell());
    if (popupContainer) {
      popupContainer->SetDefaultTooltip(aContent->AsElement());
    }
  }

  AddStateBits(NS_FRAME_IN_POPUP);
}

bool nsMenuPopupFrame::HasRemoteContent() const {
  return (!mInContentShell && mPopupType == ePopupTypePanel &&
          mContent->AsElement()->AttrValueIs(kNameSpaceID_None,
                                             nsGkAtoms::remote,
                                             nsGkAtoms::_true, eIgnoreCase));
}

bool nsMenuPopupFrame::IsNoAutoHide() const {
  // Panels with noautohide="true" don't hide when the mouse is clicked
  // outside of them, or when another application is made active. Non-autohide
  // panels cannot be used in content windows.
  return (!mInContentShell && mPopupType == ePopupTypePanel &&
          mContent->AsElement()->AttrValueIs(kNameSpaceID_None,
                                             nsGkAtoms::noautohide,
                                             nsGkAtoms::_true, eIgnoreCase));
}

nsPopupLevel nsMenuPopupFrame::PopupLevel(bool aIsNoAutoHide) const {
  // The popup level is determined as follows, in this order:
  //   1. non-panels (menus and tooltips) are always topmost
  //   2. any specified level attribute
  //   3. if a titlebar attribute is set, use the 'floating' level
  //   4. if this is a noautohide panel, use the 'parent' level
  //   5. use the platform-specific default level

  // If this is not a panel, this is always a top-most popup.
  if (mPopupType != ePopupTypePanel) return ePopupLevelTop;

  // If the level attribute has been set, use that.
  static Element::AttrValuesArray strings[] = {
      nsGkAtoms::top, nsGkAtoms::parent, nsGkAtoms::floating, nullptr};
  switch (mContent->AsElement()->FindAttrValueIn(
      kNameSpaceID_None, nsGkAtoms::level, strings, eCaseMatters)) {
    case 0:
      return ePopupLevelTop;
    case 1:
      return ePopupLevelParent;
    case 2:
      return ePopupLevelFloating;
  }

  // Panels with titlebars most likely want to be floating popups.
  if (mContent->AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::titlebar))
    return ePopupLevelFloating;

  // If this panel is a noautohide panel, the default is the parent level.
  if (aIsNoAutoHide) return ePopupLevelParent;

  // Otherwise, the result depends on the platform.
  return sDefaultLevelIsTop ? ePopupLevelTop : ePopupLevelParent;
}

void nsMenuPopupFrame::EnsureWidget(bool aRecreate) {
  nsView* ourView = GetView();
  if (aRecreate) {
    ourView->DestroyWidget();
  }
  if (!ourView->HasWidget()) {
    NS_ASSERTION(!mGeneratedChildren && !PrincipalChildList().FirstChild(),
                 "Creating widget for MenuPopupFrame with children");
    CreateWidgetForView(ourView);
  }
}

nsresult nsMenuPopupFrame::CreateWidgetForView(nsView* aView) {
  // Create a widget for ourselves.
  nsWidgetInitData widgetData;
  widgetData.mWindowType = eWindowType_popup;
  widgetData.mBorderStyle = eBorderStyle_default;
  widgetData.clipSiblings = true;
  widgetData.mPopupHint = mPopupType;
  widgetData.mNoAutoHide = IsNoAutoHide();

  if (!mInContentShell) {
    // A drag popup may be used for non-static translucent drag feedback
    if (mPopupType == ePopupTypePanel &&
        mContent->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                                           nsGkAtoms::drag, eIgnoreCase)) {
      widgetData.mIsDragPopup = true;
    }

    // If pointer-events: none; is set on the popup, then the widget should
    // ignore mouse events, passing them through to the content behind.
    mMouseTransparent =
        StyleUI()->GetEffectivePointerEvents(this) == StylePointerEvents::None;
    widgetData.mMouseTransparent = mMouseTransparent;
  }

  nsAutoString title;
  if (widgetData.mNoAutoHide) {
    if (mContent->AsElement()->AttrValueIs(kNameSpaceID_None,
                                           nsGkAtoms::titlebar,
                                           nsGkAtoms::normal, eCaseMatters)) {
      widgetData.mBorderStyle = eBorderStyle_title;

      mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::label,
                                     title);

      if (mContent->AsElement()->AttrValueIs(kNameSpaceID_None,
                                             nsGkAtoms::close, nsGkAtoms::_true,
                                             eCaseMatters)) {
        widgetData.mBorderStyle = static_cast<enum nsBorderStyle>(
            widgetData.mBorderStyle | eBorderStyle_close);
      }
    }
  }

  bool remote = HasRemoteContent();

  nsTransparencyMode mode = nsLayoutUtils::GetFrameTransparency(this, this);
  nsIContent* parentContent = GetContent()->GetParent();
  nsAtom* tag = nullptr;
  if (parentContent && parentContent->IsXULElement())
    tag = parentContent->NodeInfo()->NameAtom();
  widgetData.mHasRemoteContent = remote;
  widgetData.mSupportTranslucency = mode == eTransparencyTransparent;
  widgetData.mDropShadow =
      !(mode == eTransparencyTransparent || tag == nsGkAtoms::menulist);
  widgetData.mPopupLevel = PopupLevel(widgetData.mNoAutoHide);

  // panels which have a parent level need a parent widget. This allows them to
  // always appear in front of the parent window but behind other windows that
  // should be in front of it.
  nsCOMPtr<nsIWidget> parentWidget;
  if (widgetData.mPopupLevel != ePopupLevelTop) {
    nsCOMPtr<nsIDocShellTreeItem> dsti = PresContext()->GetDocShell();
    if (!dsti) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
    dsti->GetTreeOwner(getter_AddRefs(treeOwner));
    if (!treeOwner) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIBaseWindow> baseWindow(do_QueryInterface(treeOwner));
    if (baseWindow) baseWindow->GetMainWidget(getter_AddRefs(parentWidget));
  }

  nsresult rv =
      aView->CreateWidgetForPopup(&widgetData, parentWidget, true, true);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsIWidget* widget = aView->GetWidget();
  widget->SetTransparencyMode(mode);
  widget->SetWindowShadowStyle(GetShadowStyle());

  // most popups don't have a title so avoid setting the title if there isn't
  // one
  if (!title.IsEmpty()) {
    widget->SetTitle(title);
  }

  return NS_OK;
}

StyleWindowShadow nsMenuPopupFrame::GetShadowStyle() {
  StyleWindowShadow shadow = StyleUIReset()->mWindowShadow;
  if (shadow != StyleWindowShadow::Default) return shadow;

  switch (StyleDisplay()->mAppearance) {
    case StyleAppearance::Tooltip:
      return StyleWindowShadow::Tooltip;
    case StyleAppearance::Menupopup:
      return StyleWindowShadow::Menu;
    default:
      return StyleWindowShadow::Default;
  }
}

NS_IMETHODIMP nsXULPopupShownEvent::Run() {
  nsMenuPopupFrame* popup = do_QueryFrame(mPopup->GetPrimaryFrame());
  // Set the state to visible if the popup is still open.
  if (popup && popup->IsOpen()) {
    popup->SetPopupState(ePopupShown);
  }

  WidgetMouseEvent event(true, eXULPopupShown, nullptr,
                         WidgetMouseEvent::eReal);
  return EventDispatcher::Dispatch(mPopup, mPresContext, &event);
}

NS_IMETHODIMP nsXULPopupShownEvent::HandleEvent(Event* aEvent) {
  nsMenuPopupFrame* popup = do_QueryFrame(mPopup->GetPrimaryFrame());
  // Ignore events not targeted at the popup itself (ie targeted at
  // descendants):
  if (mPopup != aEvent->GetTarget()) {
    return NS_OK;
  }
  if (popup) {
    // ResetPopupShownDispatcher will delete the reference to this, so keep
    // another one until Run is finished.
    RefPtr<nsXULPopupShownEvent> event = this;
    // Only call Run if it the dispatcher was assigned. This avoids calling the
    // Run method if the transitionend event fires multiple times.
    if (popup->ClearPopupShownDispatcher()) {
      return Run();
    }
  }

  CancelListener();
  return NS_OK;
}

void nsXULPopupShownEvent::CancelListener() {
  mPopup->RemoveSystemEventListener(NS_LITERAL_STRING("transitionend"), this,
                                    false);
}

NS_IMPL_ISUPPORTS_INHERITED(nsXULPopupShownEvent, Runnable,
                            nsIDOMEventListener);

bool nsMenuPopupFrame::IsLeafDynamic() const {
  if (mGeneratedChildren) return false;

  if (mPopupType != ePopupTypeMenu) {
    // any panel with a type attribute, such as the autocomplete popup,
    // is always generated right away.
    return !mContent->AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::type);
  }

  // menu popups generate their child frames lazily only when opened, so
  // behave like a leaf frame. However, generate child frames normally if the
  // following is true:
  //
  // - If the parent menu is a <menulist> unless its sizetopopup is set to
  // "none".
  // - For other elements, if the parent menu has a sizetopopup attribute.
  //
  // In these cases the size of the parent menu is dependent on the size of
  // the popup, so the frames need to exist in order to calculate this size.
  nsIContent* parentContent = mContent->GetParent();
  if (!parentContent) {
    return false;
  }

  if (parentContent->IsXULElement(nsGkAtoms::menulist)) {
    Element* parent = parentContent->AsElement();
    if (!parent->HasAttr(kNameSpaceID_None, nsGkAtoms::sizetopopup)) {
      // No prop set, generate child frames normally for the
      // default value ("pref").
      return false;
    }

    nsAutoString sizedToPopup;
    parent->GetAttr(kNameSpaceID_None, nsGkAtoms::sizetopopup, sizedToPopup);
    // Don't generate child frame only if the property is set to none.
    return sizedToPopup.EqualsLiteral("none");
  }

  return (!parentContent->IsElement() ||
          !parentContent->AsElement()->HasAttr(kNameSpaceID_None,
                                               nsGkAtoms::sizetopopup));
}

void nsMenuPopupFrame::DidSetComputedStyle(ComputedStyle* aOldStyle) {
  nsBoxFrame::DidSetComputedStyle(aOldStyle);

  if (!aOldStyle) {
    return;
  }

  auto& newUI = *StyleUIReset();
  auto& oldUI = *aOldStyle->StyleUIReset();
  if (newUI.mWindowOpacity != oldUI.mWindowOpacity) {
    if (nsIWidget* widget = GetWidget()) {
      widget->SetWindowOpacity(newUI.mWindowOpacity);
    }
  }

  if (newUI.mMozWindowTransform != oldUI.mMozWindowTransform) {
    if (nsIWidget* widget = GetWidget()) {
      widget->SetWindowTransform(ComputeWidgetTransform());
    }
  }

  bool newMouseTransparent =
      StyleUI()->GetEffectivePointerEvents(this) == StylePointerEvents::None;
  bool oldMouseTransparent = aOldStyle->StyleUI()->GetEffectivePointerEvents(
                                 this) == StylePointerEvents::None;

  if (newMouseTransparent != oldMouseTransparent) {
    if (nsIWidget* widget = GetWidget()) {
      widget->SetWindowMouseTransparent(newMouseTransparent);
      mMouseTransparent = newMouseTransparent;
    }
  }
}

void nsMenuPopupFrame::LayoutPopup(nsBoxLayoutState& aState,
                                   nsIFrame* aParentMenu, bool aSizedToPopup) {
  if (IsLeaf()) {
    return;
  }

  SchedulePaint();

  bool shouldPosition = true;
  bool isOpen = IsOpen();
  if (!isOpen) {
    // if the popup is not open, only do layout while showing or if the menu
    // is sized to the popup
    shouldPosition =
        (mPopupState == ePopupShowing || mPopupState == ePopupPositioning);
    if (!shouldPosition && !aSizedToPopup) {
      RemoveStateBits(NS_FRAME_FIRST_REFLOW);
      return;
    }
  }

  // if the popup has just been opened, make sure the scrolled window is at 0,0
  // Don't scroll menulists as they will scroll to their selected item on their
  // own.
  if (mIsOpenChanged && !IsMenuList()) {
    nsIScrollableFrame* scrollframe =
        do_QueryFrame(nsIFrame::GetChildXULBox(this));
    if (scrollframe) {
      AutoWeakFrame weakFrame(this);
      scrollframe->ScrollTo(nsPoint(0, 0), ScrollMode::Instant);
      if (!weakFrame.IsAlive()) {
        return;
      }
    }
  }

  // get the preferred, minimum and maximum size. If the menu is sized to the
  // popup, then the popup's width is the menu's width.
  nsSize prefSize = GetXULPrefSize(aState);
  nsSize minSize = GetXULMinSize(aState);
  nsSize maxSize = GetXULMaxSize(aState);

  if (aSizedToPopup) {
    prefSize.width = aParentMenu->GetRect().width;
  }
  prefSize = XULBoundsCheck(minSize, prefSize, maxSize);

  bool sizeChanged = (mPrefSize != prefSize);
  // if the size changed then set the bounds to be the preferred size
  if (sizeChanged) {
    SetXULBounds(aState, nsRect(0, 0, prefSize.width, prefSize.height), false);
    mPrefSize = prefSize;
#if MOZ_WAYLAND
    nsIWidget* widget = GetWidget();
    if (widget) {
      // When the popup size changed in the DOM, we need to flush widget
      // preferred popup rect to avoid showing it in wrong size.
      widget->FlushPreferredPopupRect();
    }
#endif
  }

  bool needCallback = false;
  if (shouldPosition) {
    SetPopupPosition(aParentMenu, false, aSizedToPopup,
                     mPopupState == ePopupPositioning);
    needCallback = true;
  }

  nsRect bounds(GetRect());
  XULLayout(aState);

  // if the width or height changed, readjust the popup position. This is a
  // special case for tooltips where the preferred height doesn't include the
  // real height for its inline element, but does once it is laid out.
  // This is bug 228673 which doesn't have a simple fix.
  bool rePosition = shouldPosition && (mPosition == POPUPPOSITION_SELECTION);
  if (!aParentMenu) {
    nsSize newsize = GetSize();
    if (newsize.width > bounds.width || newsize.height > bounds.height) {
      // the size after layout was larger than the preferred size,
      // so set the preferred size accordingly
      mPrefSize = newsize;
      if (isOpen) {
        rePosition = true;
        needCallback = true;
      }
    }
  }

  if (rePosition) {
    SetPopupPosition(aParentMenu, false, aSizedToPopup, false);
  }

  nsPresContext* pc = PresContext();
  nsView* view = GetView();

  if (sizeChanged) {
    // If the size of the popup changed, apply any size constraints.
    nsIWidget* widget = view->GetWidget();
    if (widget) {
      SetSizeConstraints(pc, widget, minSize, maxSize);
    }
  }

  if (isOpen) {
    nsViewManager* viewManager = view->GetViewManager();
    nsRect rect = GetRect();
    rect.x = rect.y = 0;
    rect.SizeTo(XULBoundsCheck(minSize, rect.Size(), maxSize));
    viewManager->ResizeView(view, rect);

    if (mPopupState == ePopupOpening) {
      mPopupState = ePopupVisible;
    }

    viewManager->SetViewVisibility(view, nsViewVisibility_kShow);
    SyncFrameViewProperties(view);
  }

  // finally, if the popup just opened, send a popupshown event
  bool openChanged = mIsOpenChanged;
  if (openChanged) {
    mIsOpenChanged = false;

    // Make sure the current selection in a menulist is visible.
    if (IsMenuList() && mCurrentMenu) {
      EnsureMenuItemIsVisible(mCurrentMenu);
    }

    // If the animate attribute is set to open, check for a transition and wait
    // for it to finish before firing the popupshown event.
    if (StaticPrefs::xul_panel_animations_enabled() &&
        mContent->AsElement()->AttrValueIs(kNameSpaceID_None,
                                           nsGkAtoms::animate, nsGkAtoms::open,
                                           eCaseMatters) &&
        AnimationUtils::HasCurrentTransitions(mContent->AsElement(),
                                              PseudoStyleType::NotPseudo)) {
      mPopupShownDispatcher = new nsXULPopupShownEvent(mContent, pc);
      mContent->AddSystemEventListener(NS_LITERAL_STRING("transitionend"),
                                       mPopupShownDispatcher, false, false);
      return;
    }

    // If there are no transitions, fire the popupshown event right away.
    nsCOMPtr<nsIRunnable> event = new nsXULPopupShownEvent(GetContent(), pc);
    mContent->OwnerDoc()->Dispatch(TaskCategory::Other, event.forget());
  }

  if (needCallback && !mReflowCallbackData.mPosted) {
    pc->PresShell()->PostReflowCallback(this);
    mReflowCallbackData.MarkPosted(aParentMenu, aSizedToPopup, openChanged);
  }
}

bool nsMenuPopupFrame::ReflowFinished() {
  SetPopupPosition(mReflowCallbackData.mAnchor, false,
                   mReflowCallbackData.mSizedToPopup, true);

  mReflowCallbackData.Clear();

  return false;
}

void nsMenuPopupFrame::ReflowCallbackCanceled() { mReflowCallbackData.Clear(); }

bool nsMenuPopupFrame::IsMenuList() {
  nsIFrame* parentMenu = GetParent();
  return (parentMenu && parentMenu->GetContent() &&
          parentMenu->GetContent()->IsXULElement(nsGkAtoms::menulist));
}

nsIContent* nsMenuPopupFrame::GetTriggerContent(
    nsMenuPopupFrame* aMenuPopupFrame) {
  while (aMenuPopupFrame) {
    if (aMenuPopupFrame->mTriggerContent)
      return aMenuPopupFrame->mTriggerContent;

    // check up the menu hierarchy until a popup with a trigger node is found
    nsMenuFrame* menuFrame = do_QueryFrame(aMenuPopupFrame->GetParent());
    if (!menuFrame) break;

    nsMenuParent* parentPopup = menuFrame->GetMenuParent();
    if (!parentPopup || !parentPopup->IsMenu()) break;

    aMenuPopupFrame = static_cast<nsMenuPopupFrame*>(parentPopup);
  }

  return nullptr;
}

void nsMenuPopupFrame::InitPositionFromAnchorAlign(const nsAString& aAnchor,
                                                   const nsAString& aAlign) {
  mTriggerContent = nullptr;

  if (aAnchor.EqualsLiteral("topleft"))
    mPopupAnchor = POPUPALIGNMENT_TOPLEFT;
  else if (aAnchor.EqualsLiteral("topright"))
    mPopupAnchor = POPUPALIGNMENT_TOPRIGHT;
  else if (aAnchor.EqualsLiteral("bottomleft"))
    mPopupAnchor = POPUPALIGNMENT_BOTTOMLEFT;
  else if (aAnchor.EqualsLiteral("bottomright"))
    mPopupAnchor = POPUPALIGNMENT_BOTTOMRIGHT;
  else if (aAnchor.EqualsLiteral("leftcenter"))
    mPopupAnchor = POPUPALIGNMENT_LEFTCENTER;
  else if (aAnchor.EqualsLiteral("rightcenter"))
    mPopupAnchor = POPUPALIGNMENT_RIGHTCENTER;
  else if (aAnchor.EqualsLiteral("topcenter"))
    mPopupAnchor = POPUPALIGNMENT_TOPCENTER;
  else if (aAnchor.EqualsLiteral("bottomcenter"))
    mPopupAnchor = POPUPALIGNMENT_BOTTOMCENTER;
  else
    mPopupAnchor = POPUPALIGNMENT_NONE;

  if (aAlign.EqualsLiteral("topleft"))
    mPopupAlignment = POPUPALIGNMENT_TOPLEFT;
  else if (aAlign.EqualsLiteral("topright"))
    mPopupAlignment = POPUPALIGNMENT_TOPRIGHT;
  else if (aAlign.EqualsLiteral("bottomleft"))
    mPopupAlignment = POPUPALIGNMENT_BOTTOMLEFT;
  else if (aAlign.EqualsLiteral("bottomright"))
    mPopupAlignment = POPUPALIGNMENT_BOTTOMRIGHT;
  else
    mPopupAlignment = POPUPALIGNMENT_NONE;

  mPosition = POPUPPOSITION_UNKNOWN;
}

void nsMenuPopupFrame::InitializePopup(nsIContent* aAnchorContent,
                                       nsIContent* aTriggerContent,
                                       const nsAString& aPosition,
                                       int32_t aXPos, int32_t aYPos,
                                       MenuPopupAnchorType aAnchorType,
                                       bool aAttributesOverride) {
  EnsureWidget();

  mPopupState = ePopupShowing;
  mAnchorContent = aAnchorContent;
  mTriggerContent = aTriggerContent;
  mXPos = aXPos;
  mYPos = aYPos;
  mAdjustOffsetForContextMenu = false;
  mVFlip = false;
  mHFlip = false;
  mAlignmentOffset = 0;
  mPositionedOffset = 0;

  mAnchorType = aAnchorType;

  // if aAttributesOverride is true, then the popupanchor, popupalign and
  // position attributes on the <menupopup> override those values passed in.
  // If false, those attributes are only used if the values passed in are empty
  if (aAnchorContent || aAnchorType == MenuPopupAnchorType_Rect) {
    nsAutoString anchor, align, position, flip;
    mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::popupanchor,
                                   anchor);
    mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::popupalign,
                                   align);
    mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::position,
                                   position);
    mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::flip, flip);

    if (aAttributesOverride) {
      // if the attributes are set, clear the offset position. Otherwise,
      // the offset is used to adjust the position from the anchor point
      if (anchor.IsEmpty() && align.IsEmpty() && position.IsEmpty())
        position.Assign(aPosition);
      else
        mXPos = mYPos = 0;
    } else if (!aPosition.IsEmpty()) {
      position.Assign(aPosition);
    }

    if (flip.EqualsLiteral("none")) {
      mFlip = FlipType_None;
    } else if (flip.EqualsLiteral("both")) {
      mFlip = FlipType_Both;
    } else if (flip.EqualsLiteral("slide")) {
      mFlip = FlipType_Slide;
    }

    position.CompressWhitespace();
    int32_t spaceIdx = position.FindChar(' ');
    // if there is a space in the position, assume it is the anchor and
    // alignment as two separate tokens.
    if (spaceIdx >= 0) {
      InitPositionFromAnchorAlign(Substring(position, 0, spaceIdx),
                                  Substring(position, spaceIdx + 1));
    } else if (position.EqualsLiteral("before_start")) {
      mPopupAnchor = POPUPALIGNMENT_TOPLEFT;
      mPopupAlignment = POPUPALIGNMENT_BOTTOMLEFT;
      mPosition = POPUPPOSITION_BEFORESTART;
    } else if (position.EqualsLiteral("before_end")) {
      mPopupAnchor = POPUPALIGNMENT_TOPRIGHT;
      mPopupAlignment = POPUPALIGNMENT_BOTTOMRIGHT;
      mPosition = POPUPPOSITION_BEFOREEND;
    } else if (position.EqualsLiteral("after_start")) {
      mPopupAnchor = POPUPALIGNMENT_BOTTOMLEFT;
      mPopupAlignment = POPUPALIGNMENT_TOPLEFT;
      mPosition = POPUPPOSITION_AFTERSTART;
    } else if (position.EqualsLiteral("after_end")) {
      mPopupAnchor = POPUPALIGNMENT_BOTTOMRIGHT;
      mPopupAlignment = POPUPALIGNMENT_TOPRIGHT;
      mPosition = POPUPPOSITION_AFTEREND;
    } else if (position.EqualsLiteral("start_before")) {
      mPopupAnchor = POPUPALIGNMENT_TOPLEFT;
      mPopupAlignment = POPUPALIGNMENT_TOPRIGHT;
      mPosition = POPUPPOSITION_STARTBEFORE;
    } else if (position.EqualsLiteral("start_after")) {
      mPopupAnchor = POPUPALIGNMENT_BOTTOMLEFT;
      mPopupAlignment = POPUPALIGNMENT_BOTTOMRIGHT;
      mPosition = POPUPPOSITION_STARTAFTER;
    } else if (position.EqualsLiteral("end_before")) {
      mPopupAnchor = POPUPALIGNMENT_TOPRIGHT;
      mPopupAlignment = POPUPALIGNMENT_TOPLEFT;
      mPosition = POPUPPOSITION_ENDBEFORE;
    } else if (position.EqualsLiteral("end_after")) {
      mPopupAnchor = POPUPALIGNMENT_BOTTOMRIGHT;
      mPopupAlignment = POPUPALIGNMENT_BOTTOMLEFT;
      mPosition = POPUPPOSITION_ENDAFTER;
    } else if (position.EqualsLiteral("overlap")) {
      mPopupAnchor = POPUPALIGNMENT_TOPLEFT;
      mPopupAlignment = POPUPALIGNMENT_TOPLEFT;
      mPosition = POPUPPOSITION_OVERLAP;
    } else if (position.EqualsLiteral("after_pointer")) {
      mPopupAnchor = POPUPALIGNMENT_TOPLEFT;
      mPopupAlignment = POPUPALIGNMENT_TOPLEFT;
      mPosition = POPUPPOSITION_AFTERPOINTER;
      // XXXndeakin this is supposed to anchor vertically after, but with the
      // horizontal position as the mouse pointer.
      mYPos += 21;
    } else if (position.EqualsLiteral("selection")) {
      mPopupAnchor = POPUPALIGNMENT_BOTTOMLEFT;
      mPopupAlignment = POPUPALIGNMENT_TOPLEFT;
      mPosition = POPUPPOSITION_SELECTION;
    } else {
      InitPositionFromAnchorAlign(anchor, align);
    }
  }

  mScreenRect = nsIntRect(-1, -1, 0, 0);

  if (aAttributesOverride) {
    // Use |left| and |top| dimension attributes to position the popup if
    // present, as they may have been persisted.
    nsAutoString left, top;
    mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::left, left);
    mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::top, top);

    nsresult err;
    if (!left.IsEmpty()) {
      int32_t x = left.ToInteger(&err);
      if (NS_SUCCEEDED(err)) mScreenRect.x = x;
    }
    if (!top.IsEmpty()) {
      int32_t y = top.ToInteger(&err);
      if (NS_SUCCEEDED(err)) mScreenRect.y = y;
    }
  }
}

void nsMenuPopupFrame::InitializePopupAtScreen(nsIContent* aTriggerContent,
                                               int32_t aXPos, int32_t aYPos,
                                               bool aIsContextMenu) {
  EnsureWidget();

  mPopupState = ePopupShowing;
  mAnchorContent = nullptr;
  mTriggerContent = aTriggerContent;
  mScreenRect = nsIntRect(aXPos, aYPos, 0, 0);
  mXPos = 0;
  mYPos = 0;
  mFlip = FlipType_Default;
  mPopupAnchor = POPUPALIGNMENT_NONE;
  mPopupAlignment = POPUPALIGNMENT_NONE;
  mPosition = POPUPPOSITION_UNKNOWN;
  mIsContextMenu = aIsContextMenu;
  mAdjustOffsetForContextMenu = aIsContextMenu;
  mAnchorType = MenuPopupAnchorType_Point;
  mPositionedOffset = 0;
}

void nsMenuPopupFrame::InitializePopupAtRect(nsIContent* aTriggerContent,
                                             const nsAString& aPosition,
                                             const nsIntRect& aRect,
                                             bool aAttributesOverride) {
  InitializePopup(nullptr, aTriggerContent, aPosition, 0, 0,
                  MenuPopupAnchorType_Rect, aAttributesOverride);
  mScreenRect = aRect;
}

void nsMenuPopupFrame::ShowPopup(bool aIsContextMenu) {
  mIsContextMenu = aIsContextMenu;

  InvalidateFrameSubtree();

  if (mPopupState == ePopupShowing || mPopupState == ePopupPositioning) {
    mPopupState = ePopupOpening;
    mIsOpenChanged = true;

    // Clear mouse capture when a popup is opened.
    if (mPopupType == ePopupTypeMenu) {
      EventStateManager* activeESM = static_cast<EventStateManager*>(
          EventStateManager::GetActiveEventStateManager());
      if (activeESM) {
        EventStateManager::ClearGlobalActiveContent(activeESM);
      }

      PresShell::ReleaseCapturingContent();
    }

    nsMenuFrame* menuFrame = do_QueryFrame(GetParent());
    if (menuFrame) {
      AutoWeakFrame weakFrame(this);
      menuFrame->PopupOpened();
      if (!weakFrame.IsAlive()) return;
    }

    // do we need an actual reflow here?
    // is SetPopupPosition all that is needed?
    PresShell()->FrameNeedsReflow(this, IntrinsicDirty::TreeChange,
                                  NS_FRAME_HAS_DIRTY_CHILDREN);

    if (mPopupType == ePopupTypeMenu) {
      nsCOMPtr<nsISound> sound(do_CreateInstance("@mozilla.org/sound;1"));
      if (sound) sound->PlayEventSound(nsISound::EVENT_MENU_POPUP);
    }
  }

  mShouldAutoPosition = true;
}

void nsMenuPopupFrame::HidePopup(bool aDeselectMenu, nsPopupState aNewState) {
  NS_ASSERTION(aNewState == ePopupClosed || aNewState == ePopupInvisible,
               "popup being set to unexpected state");

  ClearPopupShownDispatcher();

  // don't hide the popup when it isn't open
  if (mPopupState == ePopupClosed || mPopupState == ePopupShowing ||
      mPopupState == ePopupPositioning)
    return;

  // clear the trigger content if the popup is being closed. But don't clear
  // it if the popup is just being made invisible as a popuphiding or command
  // event may want to retrieve it.
  if (aNewState == ePopupClosed) {
    // if the popup had a trigger node set, clear the global window popup node
    // as well
    if (mTriggerContent) {
      Document* doc = mContent->GetUncomposedDoc();
      if (doc) {
        if (nsPIDOMWindowOuter* win = doc->GetWindow()) {
          nsCOMPtr<nsPIWindowRoot> root = win->GetTopWindowRoot();
          if (root) {
            root->SetPopupNode(nullptr);
          }
        }
      }
    }
    mTriggerContent = nullptr;
    mAnchorContent = nullptr;
  }

  // when invisible and about to be closed, HidePopup has already been called,
  // so just set the new state to closed and return
  if (mPopupState == ePopupInvisible) {
    if (aNewState == ePopupClosed) mPopupState = ePopupClosed;
    return;
  }

  mPopupState = aNewState;

  if (IsMenu()) SetCurrentMenuItem(nullptr);

  mIncrementalString.Truncate();

  LockMenuUntilClosed(false);

  mIsOpenChanged = false;
  mCurrentMenu = nullptr;  // make sure no current menu is set
  mHFlip = mVFlip = false;

  nsView* view = GetView();
  nsViewManager* viewManager = view->GetViewManager();
  viewManager->SetViewVisibility(view, nsViewVisibility_kHide);

  FireDOMEvent(NS_LITERAL_STRING("DOMMenuInactive"), mContent);

  // XXX, bug 137033, In Windows, if mouse is outside the window when the
  // menupopup closes, no mouse_enter/mouse_exit event will be fired to clear
  // current hover state, we should clear it manually. This code may not the
  // best solution, but we can leave it here until we find the better approach.
  NS_ASSERTION(mContent->IsElement(), "How do we have a non-element?");
  EventStates state = mContent->AsElement()->State();

  if (state.HasState(NS_EVENT_STATE_HOVER)) {
    EventStateManager* esm = PresContext()->EventStateManager();
    esm->SetContentState(nullptr, NS_EVENT_STATE_HOVER);
  }

  nsMenuFrame* menuFrame = do_QueryFrame(GetParent());
  if (menuFrame) {
    menuFrame->PopupClosed(aDeselectMenu);
  }
}

nsIFrame::ReflowChildFlags nsMenuPopupFrame::GetXULLayoutFlags() {
  return ReflowChildFlags::NoSizeView | ReflowChildFlags::NoMoveView;
}

nsPoint nsMenuPopupFrame::AdjustPositionForAnchorAlign(nsRect& anchorRect,
                                                       FlipStyle& aHFlip,
                                                       FlipStyle& aVFlip) {
  // flip the anchor and alignment for right-to-left
  int8_t popupAnchor(mPopupAnchor);
  int8_t popupAlign(mPopupAlignment);
  if (IsDirectionRTL()) {
    // no need to flip the centered anchor types vertically
    if (popupAnchor <= POPUPALIGNMENT_LEFTCENTER) {
      popupAnchor = -popupAnchor;
    }
    popupAlign = -popupAlign;
  }

  nsRect originalAnchorRect(anchorRect);

  // first, determine at which corner of the anchor the popup should appear
  nsPoint pnt;
  switch (popupAnchor) {
    case POPUPALIGNMENT_LEFTCENTER:
      pnt = nsPoint(anchorRect.x, anchorRect.y + anchorRect.height / 2);
      anchorRect.y = pnt.y;
      anchorRect.height = 0;
      break;
    case POPUPALIGNMENT_RIGHTCENTER:
      pnt = nsPoint(anchorRect.XMost(), anchorRect.y + anchorRect.height / 2);
      anchorRect.y = pnt.y;
      anchorRect.height = 0;
      break;
    case POPUPALIGNMENT_TOPCENTER:
      pnt = nsPoint(anchorRect.x + anchorRect.width / 2, anchorRect.y);
      anchorRect.x = pnt.x;
      anchorRect.width = 0;
      break;
    case POPUPALIGNMENT_BOTTOMCENTER:
      pnt = nsPoint(anchorRect.x + anchorRect.width / 2, anchorRect.YMost());
      anchorRect.x = pnt.x;
      anchorRect.width = 0;
      break;
    case POPUPALIGNMENT_TOPRIGHT:
      pnt = anchorRect.TopRight();
      break;
    case POPUPALIGNMENT_BOTTOMLEFT:
      pnt = anchorRect.BottomLeft();
      break;
    case POPUPALIGNMENT_BOTTOMRIGHT:
      pnt = anchorRect.BottomRight();
      break;
    case POPUPALIGNMENT_TOPLEFT:
    default:
      pnt = anchorRect.TopLeft();
      break;
  }

  // If the alignment is on the right edge of the popup, move the popup left
  // by the width. Similarly, if the alignment is on the bottom edge of the
  // popup, move the popup up by the height. In addition, account for the
  // margins of the popup on the edge on which it is aligned.
  nsMargin margin(0, 0, 0, 0);
  StyleMargin()->GetMargin(margin);
  switch (popupAlign) {
    case POPUPALIGNMENT_TOPRIGHT:
      pnt.MoveBy(-mRect.width - margin.right, margin.top);
      break;
    case POPUPALIGNMENT_BOTTOMLEFT:
      pnt.MoveBy(margin.left, -mRect.height - margin.bottom);
      break;
    case POPUPALIGNMENT_BOTTOMRIGHT:
      pnt.MoveBy(-mRect.width - margin.right, -mRect.height - margin.bottom);
      break;
    case POPUPALIGNMENT_TOPLEFT:
    default:
      pnt.MoveBy(margin.left, margin.top);
      break;
  }

  // If we aligning to the selected item in the popup, adjust the vertical
  // position by the height of the menulist label and the selected item's
  // position.
  if (mPosition == POPUPPOSITION_SELECTION) {
    MOZ_ASSERT(popupAnchor == POPUPALIGNMENT_BOTTOMLEFT ||
               popupAnchor == POPUPALIGNMENT_BOTTOMRIGHT);
    MOZ_ASSERT(popupAlign == POPUPALIGNMENT_TOPLEFT ||
               popupAlign == POPUPALIGNMENT_TOPRIGHT);

    // Only adjust the popup if it just opened, otherwise the popup will move
    // around if its gets resized or the selection changed. Cache the value in
    // mPositionedOffset and use that instead for any future calculations.
    if (mIsOpenChanged || mReflowCallbackData.mIsOpenChanged) {
      nsIFrame* selectedItemFrame = GetSelectedItemForAlignment();
      if (selectedItemFrame) {
        int32_t scrolly = 0;
        nsIScrollableFrame* scrollframe = GetScrollFrame(this);
        if (scrollframe) {
          scrolly = scrollframe->GetScrollPosition().y;
        }

        mPositionedOffset = originalAnchorRect.height +
                            selectedItemFrame->GetRect().y - scrolly;
      }
    }

    pnt.y -= mPositionedOffset;
  }

  // Flipping horizontally is allowed as long as the popup is above or below
  // the anchor. This will happen if both the anchor and alignment are top or
  // both are bottom, but different values. Similarly, flipping vertically is
  // allowed if the popup is to the left or right of the anchor. In this case,
  // the values of the constants are such that both must be positive or both
  // must be negative. A special case, used for overlap, allows flipping
  // vertically as well.
  // If we are flipping in both directions, we want to set a flip style both
  // horizontally and vertically. However, we want to flip on the inside edge
  // of the anchor. Consider the example of a typical dropdown menu.
  // Vertically, we flip the popup on the outside edges of the anchor menu,
  // however horizontally, we want to to use the inside edges so the popup
  // still appears underneath the anchor menu instead of floating off the
  // side of the menu.
  switch (popupAnchor) {
    case POPUPALIGNMENT_LEFTCENTER:
    case POPUPALIGNMENT_RIGHTCENTER:
      aHFlip = FlipStyle_Outside;
      aVFlip = FlipStyle_Inside;
      break;
    case POPUPALIGNMENT_TOPCENTER:
    case POPUPALIGNMENT_BOTTOMCENTER:
      aHFlip = FlipStyle_Inside;
      aVFlip = FlipStyle_Outside;
      break;
    default: {
      FlipStyle anchorEdge =
          mFlip == FlipType_Both ? FlipStyle_Inside : FlipStyle_None;
      aHFlip = (popupAnchor == -popupAlign) ? FlipStyle_Outside : anchorEdge;
      if (((popupAnchor > 0) == (popupAlign > 0)) ||
          (popupAnchor == POPUPALIGNMENT_TOPLEFT &&
           popupAlign == POPUPALIGNMENT_TOPLEFT))
        aVFlip = FlipStyle_Outside;
      else
        aVFlip = anchorEdge;
      break;
    }
  }

  return pnt;
}

nsIFrame* nsMenuPopupFrame::GetSelectedItemForAlignment() {
  // This method adjusts a menulist's popup such that the selected item is under
  // the cursor, aligned with the menulist label.
  nsCOMPtr<nsIDOMXULSelectControlElement> select;
  if (mAnchorContent) {
    select = mAnchorContent->AsElement()->AsXULSelectControl();
  }

  if (!select) {
    // If there isn't an anchor, then try just getting the parent of the popup.
    select = mContent->GetParent()->AsElement()->AsXULSelectControl();
    if (!select) {
      return nullptr;
    }
  }

  nsCOMPtr<Element> selectedElement;
  select->GetSelectedItem(getter_AddRefs(selectedElement));
  return selectedElement ? selectedElement->GetPrimaryFrame() : nullptr;
}

nscoord nsMenuPopupFrame::SlideOrResize(nscoord& aScreenPoint, nscoord aSize,
                                        nscoord aScreenBegin,
                                        nscoord aScreenEnd, nscoord* aOffset) {
  // The popup may be positioned such that either the left/top or bottom/right
  // is outside the screen - but never both.
  nscoord newPos =
      std::max(aScreenBegin, std::min(aScreenEnd - aSize, aScreenPoint));
  *aOffset = newPos - aScreenPoint;
  aScreenPoint = newPos;
  return std::min(aSize, aScreenEnd - aScreenPoint);
}

nscoord nsMenuPopupFrame::FlipOrResize(nscoord& aScreenPoint, nscoord aSize,
                                       nscoord aScreenBegin, nscoord aScreenEnd,
                                       nscoord aAnchorBegin, nscoord aAnchorEnd,
                                       nscoord aMarginBegin, nscoord aMarginEnd,
                                       nscoord aOffsetForContextMenu,
                                       FlipStyle aFlip, bool aEndAligned,
                                       bool* aFlipSide) {
  // The flip side argument will be set to true if there wasn't room and we
  // flipped to the opposite side.
  *aFlipSide = false;

  // all of the coordinates used here are in app units relative to the screen
  nscoord popupSize = aSize;
  if (aScreenPoint < aScreenBegin) {
    // at its current position, the popup would extend past the left or top
    // edge of the screen, so it will have to be moved or resized.
    if (aFlip) {
      // for inside flips, we flip on the opposite side of the anchor
      nscoord startpos = aFlip == FlipStyle_Outside ? aAnchorBegin : aAnchorEnd;
      nscoord endpos = aFlip == FlipStyle_Outside ? aAnchorEnd : aAnchorBegin;

      // check whether there is more room to the left and right (or top and
      // bottom) of the anchor and put the popup on the side with more room.
      if (startpos - aScreenBegin >= aScreenEnd - endpos) {
        aScreenPoint = aScreenBegin;
        popupSize = startpos - aScreenPoint - aMarginEnd;
        *aFlipSide = !aEndAligned;
      } else {
        // If the newly calculated position is different than the existing
        // position, flip such that the popup is to the right or bottom of the
        // anchor point instead . However, when flipping use the same margin
        // size.
        nscoord newScreenPoint = endpos + aMarginEnd;
        if (newScreenPoint != aScreenPoint) {
          *aFlipSide = aEndAligned;
          aScreenPoint = newScreenPoint;
          // check if the new position is still off the right or bottom edge of
          // the screen. If so, resize the popup.
          if (aScreenPoint + aSize > aScreenEnd) {
            popupSize = aScreenEnd - aScreenPoint;
          }
        }
      }
    } else {
      aScreenPoint = aScreenBegin;
    }
  } else if (aScreenPoint + aSize > aScreenEnd) {
    // at its current position, the popup would extend past the right or
    // bottom edge of the screen, so it will have to be moved or resized.
    if (aFlip) {
      // for inside flips, we flip on the opposite side of the anchor
      nscoord startpos = aFlip == FlipStyle_Outside ? aAnchorBegin : aAnchorEnd;
      nscoord endpos = aFlip == FlipStyle_Outside ? aAnchorEnd : aAnchorBegin;

      // check whether there is more room to the left and right (or top and
      // bottom) of the anchor and put the popup on the side with more room.
      if (aScreenEnd - endpos >= startpos - aScreenBegin) {
        *aFlipSide = aEndAligned;
        if (mIsContextMenu) {
          aScreenPoint = aScreenEnd - aSize;
        } else {
          aScreenPoint = endpos + aMarginBegin;
          popupSize = aScreenEnd - aScreenPoint;
        }
      } else {
        // if the newly calculated position is different than the existing
        // position, we flip such that the popup is to the left or top of the
        // anchor point instead.
        nscoord newScreenPoint = startpos - aSize - aMarginBegin -
                                 std::max(aOffsetForContextMenu, 0);
        if (newScreenPoint != aScreenPoint) {
          *aFlipSide = !aEndAligned;
          aScreenPoint = newScreenPoint;

          // check if the new position is still off the left or top edge of the
          // screen. If so, resize the popup.
          if (aScreenPoint < aScreenBegin) {
            aScreenPoint = aScreenBegin;
            if (!mIsContextMenu) {
              popupSize = startpos - aScreenPoint - aMarginBegin;
            }
          }
        }
      }
    } else {
      aScreenPoint = aScreenEnd - aSize;
    }
  }

  // Make sure that the point is within the screen boundaries and that the
  // size isn't off the edge of the screen. This can happen when a large
  // positive or negative margin is used.
  if (aScreenPoint < aScreenBegin) {
    aScreenPoint = aScreenBegin;
  }
  if (aScreenPoint > aScreenEnd) {
    aScreenPoint = aScreenEnd - aSize;
  }

  // If popupSize ended up being negative, or the original size was actually
  // smaller than the calculated popup size, just use the original size instead.
  if (popupSize <= 0 || aSize < popupSize) {
    popupSize = aSize;
  }

  return std::min(popupSize, aScreenEnd - aScreenPoint);
}

nsRect nsMenuPopupFrame::ComputeAnchorRect(nsPresContext* aRootPresContext,
                                           nsIFrame* aAnchorFrame) {
  // Get the root frame for a reference
  nsIFrame* rootFrame = aRootPresContext->PresShell()->GetRootFrame();

  // The dimensions of the anchor
  nsRect anchorRect = aAnchorFrame->GetRectRelativeToSelf();

  // Relative to the root
  anchorRect = nsLayoutUtils::TransformFrameRectToAncestor(
      aAnchorFrame, anchorRect, rootFrame);
  // Relative to the screen
  anchorRect.MoveBy(rootFrame->GetScreenRectInAppUnits().TopLeft());

  // In its own app units
  return anchorRect.ScaleToOtherAppUnitsRoundOut(
      aRootPresContext->AppUnitsPerDevPixel(),
      PresContext()->AppUnitsPerDevPixel());
}

nsresult nsMenuPopupFrame::SetPopupPosition(nsIFrame* aAnchorFrame,
                                            bool aIsMove, bool aSizedToPopup,
                                            bool aNotify) {
  if (!mShouldAutoPosition) return NS_OK;

  // If this is due to a move, return early if the popup hasn't been laid out
  // yet. On Windows, this can happen when using a drag popup before it opens.
  if (aIsMove && (mPrefSize.width == -1 || mPrefSize.height == -1)) {
    return NS_OK;
  }

  nsPresContext* presContext = PresContext();
  nsIFrame* rootFrame = presContext->PresShell()->GetRootFrame();
  NS_ASSERTION(rootFrame->GetView() && GetView() &&
                   rootFrame->GetView() == GetView()->GetParent(),
               "rootFrame's view is not our view's parent???");

  // For anchored popups, the anchor rectangle. For non-anchored popups, the
  // size will be 0.
  nsRect anchorRect;

  // Width of the parent, used when aSizedToPopup is true.
  int32_t parentWidth = 0;

  bool anchored = IsAnchored();
  if (anchored || aSizedToPopup) {
    // In order to deal with transforms, we need the root prescontext:
    nsPresContext* rootPresContext = presContext->GetRootPresContext();

    // If we can't reach a root pres context, don't bother continuing:
    if (!rootPresContext) {
      return NS_OK;
    }

    // If anchored to a rectangle, use that rectangle. Otherwise, determine the
    // rectangle from the anchor.
    if (mAnchorType == MenuPopupAnchorType_Rect) {
      anchorRect = ToAppUnits(mScreenRect, AppUnitsPerCSSPixel());
    } else {
      // if the frame is not specified, use the anchor node passed to OpenPopup.
      // If that wasn't specified either, use the root frame. Note that
      // mAnchorContent might be a different document so its presshell must be
      // used.
      if (!aAnchorFrame) {
        if (mAnchorContent) {
          aAnchorFrame = mAnchorContent->GetPrimaryFrame();
        }

        if (!aAnchorFrame) {
          aAnchorFrame = rootFrame;
          if (!aAnchorFrame) return NS_OK;
        }
      }

      anchorRect = ComputeAnchorRect(rootPresContext, aAnchorFrame);
    }

    // The width is needed when aSizedToPopup is true
    parentWidth = anchorRect.width;
  }

  // Set the popup's size to the preferred size. Below, this size will be
  // adjusted to fit on the screen or within the content area. If the anchor
  // is sized to the popup, use the anchor's width instead of the preferred
  // width. The preferred size should already be set by the parent frame.
  NS_ASSERTION(mPrefSize.width >= 0 || mPrefSize.height >= 0,
               "preferred size of popup not set");
  mRect.width = aSizedToPopup ? parentWidth : mPrefSize.width;
  mRect.height = mPrefSize.height;

  // If we're anchoring to a rect, and the rect is smaller than the preferred
  // size of the popup, change its width accordingly.
  if (mAnchorType == MenuPopupAnchorType_Rect &&
      parentWidth < mPrefSize.width) {
    mRect.width = mPrefSize.width;
  }

  // the screen position in app units where the popup should appear
  nsPoint screenPoint;

  // indicators of whether the popup should be flipped or resized.
  FlipStyle hFlip = FlipStyle_None, vFlip = FlipStyle_None;

  nsMargin margin(0, 0, 0, 0);
  StyleMargin()->GetMargin(margin);

  // the screen rectangle of the root frame, in dev pixels.
  nsRect rootScreenRect = rootFrame->GetScreenRectInAppUnits();

  nsDeviceContext* devContext = presContext->DeviceContext();
  nsPoint offsetForContextMenu;

  bool isNoAutoHide = IsNoAutoHide();
  nsPopupLevel popupLevel = PopupLevel(isNoAutoHide);

  if (anchored) {
    // if we are anchored, there are certain things we don't want to do when
    // repositioning the popup to fit on the screen, such as end up positioned
    // over the anchor, for instance a popup appearing over the menu label.
    // When doing this reposition, we want to move the popup to the side with
    // the most room. The combination of anchor and alignment dictate if we
    // readjust above/below or to the left/right.
    if (mAnchorContent || mAnchorType == MenuPopupAnchorType_Rect) {
      // move the popup according to the anchor and alignment. This will also
      // tell us which axis the popup is flush against in case we have to move
      // it around later. The AdjustPositionForAnchorAlign method accounts for
      // the popup's margin.

#ifdef MOZ_WAYLAND
      if (gdk_display_get_default() &&
          !GDK_IS_X11_DISPLAY(gdk_display_get_default())) {
        screenPoint = nsPoint(anchorRect.x, anchorRect.y);
        mAnchorRect = anchorRect;
      } else
#endif
      {
        screenPoint = AdjustPositionForAnchorAlign(anchorRect, hFlip, vFlip);
      }
    } else {
      // with no anchor, the popup is positioned relative to the root frame
      anchorRect = rootScreenRect;
      screenPoint = anchorRect.TopLeft() + nsPoint(margin.left, margin.top);
    }

    // mXPos and mYPos specify an additonal offset passed to OpenPopup that
    // should be added to the position.  We also add the offset to the anchor
    // pos so a later flip/resize takes the offset into account.
    nscoord anchorXOffset = nsPresContext::CSSPixelsToAppUnits(mXPos);
    if (IsDirectionRTL()) {
      screenPoint.x -= anchorXOffset;
      anchorRect.x -= anchorXOffset;
    } else {
      screenPoint.x += anchorXOffset;
      anchorRect.x += anchorXOffset;
    }
    nscoord anchorYOffset = nsPresContext::CSSPixelsToAppUnits(mYPos);
    screenPoint.y += anchorYOffset;
    anchorRect.y += anchorYOffset;

    // If this is a noautohide popup, set the screen coordinates of the popup.
    // This way, the popup stays at the location where it was opened even when
    // the window is moved. Popups at the parent level follow the parent
    // window as it is moved and remained anchored, so we want to maintain the
    // anchoring instead.
    if (isNoAutoHide && (popupLevel != ePopupLevelParent ||
                         mAnchorType == MenuPopupAnchorType_Rect)) {
      // Account for the margin that will end up being added to the screen
      // coordinate the next time SetPopupPosition is called.
      mAnchorType = MenuPopupAnchorType_Point;
      mScreenRect.x =
          nsPresContext::AppUnitsToIntCSSPixels(screenPoint.x - margin.left);
      mScreenRect.y =
          nsPresContext::AppUnitsToIntCSSPixels(screenPoint.y - margin.top);
    }
  } else {
    // The popup is positioned at a screen coordinate.
    // First convert the screen position in mScreenRect from CSS pixels into
    // device pixels, ignoring any zoom as mScreenRect holds unzoomed screen
    // coordinates.
    int32_t factor = devContext->AppUnitsPerDevPixelAtUnitFullZoom();

    // Depending on the platform, context menus should be offset by varying
    // amounts to ensure that they don't appear directly where the cursor is.
    // Otherwise, it is too easy to have the context menu close up again.
    if (mAdjustOffsetForContextMenu) {
      nsPoint offsetForContextMenuDev;
      offsetForContextMenuDev.x =
          nsPresContext::CSSPixelsToAppUnits(LookAndFeel::GetInt(
              LookAndFeel::eIntID_ContextMenuOffsetHorizontal)) /
          factor;
      offsetForContextMenuDev.y =
          nsPresContext::CSSPixelsToAppUnits(LookAndFeel::GetInt(
              LookAndFeel::eIntID_ContextMenuOffsetVertical)) /
          factor;
      offsetForContextMenu.x =
          presContext->DevPixelsToAppUnits(offsetForContextMenuDev.x);
      offsetForContextMenu.y =
          presContext->DevPixelsToAppUnits(offsetForContextMenuDev.y);
    }

    // next, convert into app units accounting for the zoom
    screenPoint.x = presContext->DevPixelsToAppUnits(
        nsPresContext::CSSPixelsToAppUnits(mScreenRect.x) / factor);
    screenPoint.y = presContext->DevPixelsToAppUnits(
        nsPresContext::CSSPixelsToAppUnits(mScreenRect.y) / factor);
    anchorRect = nsRect(screenPoint, nsSize(0, 0));

    // add the margins on the popup
    screenPoint.MoveBy(margin.left + offsetForContextMenu.x,
                       margin.top + offsetForContextMenu.y);

#ifdef XP_MACOSX
    // OSX tooltips follow standard flip rule but other popups flip horizontally
    // not vertically
    if (mPopupType == ePopupTypeTooltip) {
      vFlip = FlipStyle_Outside;
    } else {
      hFlip = FlipStyle_Outside;
    }
#else
    // Other OS screen positioned popups can be flipped vertically but never
    // horizontally
    vFlip = FlipStyle_Outside;
#endif  // #ifdef XP_MACOSX
  }

  nscoord oldAlignmentOffset = mAlignmentOffset;

  // If a panel is being moved or has flip="none", don't constrain or flip it,
  // in order to avoid visual noise when moving windows between screens.
  // However, if a panel is already constrained or flipped (mIsOffset), then we
  // want to continue to calculate this. Also, always do this for content
  // shells, so that the popup doesn't extend outside the containing frame.
  if (mInContentShell ||
      (mFlip != FlipType_None &&
       (!aIsMove || mIsOffset || mPopupType != ePopupTypePanel))) {
    int32_t appPerDev = presContext->AppUnitsPerDevPixel();
    LayoutDeviceIntRect anchorRectDevPix =
        LayoutDeviceIntRect::FromAppUnitsToNearest(anchorRect, appPerDev);
    LayoutDeviceIntRect rootScreenRectDevPix =
        LayoutDeviceIntRect::FromAppUnitsToNearest(rootScreenRect, appPerDev);
    LayoutDeviceIntRect screenRectDevPix =
        GetConstraintRect(anchorRectDevPix, rootScreenRectDevPix, popupLevel);
    nsRect screenRect =
        LayoutDeviceIntRect::ToAppUnits(screenRectDevPix, appPerDev);

    // Ensure that anchorRect is on screen.
    anchorRect = anchorRect.Intersect(screenRect);

#ifdef MOZ_WAYLAND
    nsIWidget* widget = GetWidget();
    if (widget) {
      nsRect prefRect = widget->GetPreferredPopupRect();
      if (prefRect.width > 0 && prefRect.height > 0) {
        screenRect = prefRect;
      }
    } else {
      NS_WARNING("No widget associated with popup frame.");
    }
#endif
    // shrink the the popup down if it is larger than the screen size
    if (mRect.width > screenRect.width) mRect.width = screenRect.width;
    if (mRect.height > screenRect.height) mRect.height = screenRect.height;

      // We can't get the subsequent change of the popup position under
      // waylande where gdk_window_move_to_rect is used to place them
      // because we don't know the absolute position of the window on the
      // screen.
#ifdef MOZ_WAYLAND
    static bool inWayland = gdk_display_get_default() &&
                            !GDK_IS_X11_DISPLAY(gdk_display_get_default());
#else
    static bool inWayland = false;
#endif
    if (!inWayland) {
      // at this point the anchor (anchorRect) is within the available screen
      // area (screenRect) and the popup is known to be no larger than the
      // screen.

      // We might want to "slide" an arrow if the panel is of the correct type -
      // but we can only slide on one axis - the other axis must be "flipped or
      // resized" as normal.
      bool slideHorizontal = false, slideVertical = false;
      if (mFlip == FlipType_Slide) {
        int8_t position = GetAlignmentPosition();
        slideHorizontal = position >= POPUPPOSITION_BEFORESTART &&
                          position <= POPUPPOSITION_AFTEREND;
        slideVertical = position >= POPUPPOSITION_STARTBEFORE &&
                        position <= POPUPPOSITION_ENDAFTER;
      }

      // Next, check if there is enough space to show the popup at full size
      // when positioned at screenPoint. If not, flip the popups to the opposite
      // side of their anchor point, or resize them as necessary.
      bool endAligned = IsDirectionRTL()
                            ? mPopupAlignment == POPUPALIGNMENT_TOPLEFT ||
                                  mPopupAlignment == POPUPALIGNMENT_BOTTOMLEFT
                            : mPopupAlignment == POPUPALIGNMENT_TOPRIGHT ||
                                  mPopupAlignment == POPUPALIGNMENT_BOTTOMRIGHT;
      nscoord preOffsetScreenPoint = screenPoint.x;
      if (slideHorizontal) {
        mRect.width = SlideOrResize(screenPoint.x, mRect.width, screenRect.x,
                                    screenRect.XMost(), &mAlignmentOffset);
      } else {
        mRect.width = FlipOrResize(
            screenPoint.x, mRect.width, screenRect.x, screenRect.XMost(),
            anchorRect.x, anchorRect.XMost(), margin.left, margin.right,
            offsetForContextMenu.x, hFlip, endAligned, &mHFlip);
      }
      mIsOffset = preOffsetScreenPoint != screenPoint.x;

      endAligned = mPopupAlignment == POPUPALIGNMENT_BOTTOMLEFT ||
                   mPopupAlignment == POPUPALIGNMENT_BOTTOMRIGHT;
      preOffsetScreenPoint = screenPoint.y;
      if (slideVertical) {
        mRect.height = SlideOrResize(screenPoint.y, mRect.height, screenRect.y,
                                     screenRect.YMost(), &mAlignmentOffset);
      } else {
        mRect.height = FlipOrResize(
            screenPoint.y, mRect.height, screenRect.y, screenRect.YMost(),
            anchorRect.y, anchorRect.YMost(), margin.top, margin.bottom,
            offsetForContextMenu.y, vFlip, endAligned, &mVFlip);
      }
      mIsOffset = mIsOffset || (preOffsetScreenPoint != screenPoint.y);

      NS_ASSERTION(screenPoint.x >= screenRect.x &&
                       screenPoint.y >= screenRect.y &&
                       screenPoint.x + mRect.width <= screenRect.XMost() &&
                       screenPoint.y + mRect.height <= screenRect.YMost(),
                   "Popup is offscreen");
    }
  }

  // snap the popup's position in screen coordinates to device pixels,
  // see bug 622507, bug 961431
  screenPoint.x = presContext->RoundAppUnitsToNearestDevPixels(screenPoint.x);
  screenPoint.y = presContext->RoundAppUnitsToNearestDevPixels(screenPoint.y);

  // determine the x and y position of the view by subtracting the desired
  // screen position from the screen position of the root frame.
  nsPoint viewPoint = screenPoint - rootScreenRect.TopLeft();

  nsView* view = GetView();
  NS_ASSERTION(view, "popup with no view");

  // Offset the position by the width and height of the borders and titlebar.
  // Even though GetClientOffset should return (0, 0) when there is no
  // titlebar or borders, we skip these calculations anyway for non-panels
  // to save time since they will never have a titlebar.
  nsIWidget* widget = view->GetWidget();
  if (mPopupType == ePopupTypePanel && widget) {
    mLastClientOffset = widget->GetClientOffset();
    viewPoint.x += presContext->DevPixelsToAppUnits(mLastClientOffset.x);
    viewPoint.y += presContext->DevPixelsToAppUnits(mLastClientOffset.y);
  }

  presContext->GetPresShell()->GetViewManager()->MoveViewTo(view, viewPoint.x,
                                                            viewPoint.y);

  // Now that we've positioned the view, sync up the frame's origin.
  nsBoxFrame::SetPosition(viewPoint - GetParent()->GetOffsetTo(rootFrame));

  if (aSizedToPopup) {
    nsBoxLayoutState state(PresContext());
    // XXXndeakin can parentSize.width still extend outside?
    SetXULBounds(state, mRect);
  }

  // If the popup is in the positioned state or if it is shown and the position
  // or size changed, dispatch a popuppositioned event if the popup wants it.
  nsIntRect newRect(screenPoint.x, screenPoint.y, mRect.width, mRect.height);
  if (mPopupState == ePopupPositioning ||
      (mPopupState == ePopupShown && !newRect.IsEqualEdges(mUsedScreenRect)) ||
      (mPopupState == ePopupShown && oldAlignmentOffset != mAlignmentOffset)) {
    mUsedScreenRect = newRect;
    if (aNotify) {
      nsXULPopupPositionedEvent::DispatchIfNeeded(mContent, false, false);
    }
  }

  return NS_OK;
}

void nsMenuPopupFrame::GenerateFrames() {
  const bool generateFrames = IsLeaf();
  MOZ_ASSERT_IF(generateFrames, !mGeneratedChildren);
  mGeneratedChildren = true;
  if (generateFrames) {
    MOZ_ASSERT(PrincipalChildList().IsEmpty());
    RefPtr<mozilla::PresShell> presShell = PresContext()->PresShell();
    presShell->FrameConstructor()->GenerateChildFrames(this);
  }
}

/* virtual */
nsMenuFrame* nsMenuPopupFrame::GetCurrentMenuItem() { return mCurrentMenu; }

LayoutDeviceIntRect nsMenuPopupFrame::GetConstraintRect(
    const LayoutDeviceIntRect& aAnchorRect,
    const LayoutDeviceIntRect& aRootScreenRect, nsPopupLevel aPopupLevel) {
  LayoutDeviceIntRect screenRectPixels;

  // determine the available screen space. It will be reduced by the OS chrome
  // such as menubars. It addition, for content shells, it will be the area of
  // the content rather than the screen.
  nsCOMPtr<nsIScreen> screen;
  nsCOMPtr<nsIScreenManager> sm(
      do_GetService("@mozilla.org/gfx/screenmanager;1"));
#ifdef MOZ_WAYLAND
  static bool inWayland = gdk_display_get_default() &&
                          !GDK_IS_X11_DISPLAY(gdk_display_get_default());
#else
  static bool inWayland = false;
#endif
  if (sm && !inWayland) {
    // for content shells, get the screen where the root frame is located.
    // This is because we need to constrain the content to this content area,
    // so we should use the same screen. Otherwise, use the screen where the
    // anchor is located.
    DesktopToLayoutDeviceScale scale =
        PresContext()->DeviceContext()->GetDesktopToDeviceScale();
    DesktopRect rect =
        (mInContentShell ? aRootScreenRect : aAnchorRect) / scale;
    int32_t width = std::max(1, NSToIntRound(rect.width));
    int32_t height = std::max(1, NSToIntRound(rect.height));
    sm->ScreenForRect(rect.x, rect.y, width, height, getter_AddRefs(screen));
    if (screen) {
      // Non-top-level popups (which will always be panels)
      // should never overlap the OS bar:
      bool dontOverlapOSBar = aPopupLevel != ePopupLevelTop;
      // get the total screen area if the popup is allowed to overlap it.
      if (!dontOverlapOSBar && mMenuCanOverlapOSBar && !mInContentShell)
        screen->GetRect(&screenRectPixels.x, &screenRectPixels.y,
                        &screenRectPixels.width, &screenRectPixels.height);
      else
        screen->GetAvailRect(&screenRectPixels.x, &screenRectPixels.y,
                             &screenRectPixels.width, &screenRectPixels.height);
    }
  }
#ifdef MOZ_WAYLAND
  else {
    if (GetWidget() && GetWidget()->GetScreenRect(&screenRectPixels) != NS_OK) {
      NS_WARNING("Cannot get screen rect from widget!");
    }
  }
#endif

  if (mInContentShell) {
    // for content shells, clip to the client area rather than the screen area
    screenRectPixels.IntersectRect(screenRectPixels, aRootScreenRect);
  } else if (!mOverrideConstraintRect.IsEmpty()) {
    LayoutDeviceIntRect overrideConstrainRect =
        LayoutDeviceIntRect::FromAppUnitsToNearest(
            mOverrideConstraintRect, PresContext()->AppUnitsPerDevPixel());
    // This is currently only used for <select> elements where we want to
    // constrain vertically to the screen but not horizontally, so do the
    // intersection and then reset the horizontal values.
    screenRectPixels.IntersectRect(screenRectPixels, overrideConstrainRect);
    screenRectPixels.x = overrideConstrainRect.x;
    screenRectPixels.width = overrideConstrainRect.width;
  }

  return screenRectPixels;
}

void nsMenuPopupFrame::CanAdjustEdges(Side aHorizontalSide, Side aVerticalSide,
                                      LayoutDeviceIntPoint& aChange) {
  int8_t popupAlign(mPopupAlignment);
  if (IsDirectionRTL()) {
    popupAlign = -popupAlign;
  }

  if (aHorizontalSide == (mHFlip ? eSideRight : eSideLeft)) {
    if (popupAlign == POPUPALIGNMENT_TOPLEFT ||
        popupAlign == POPUPALIGNMENT_BOTTOMLEFT) {
      aChange.x = 0;
    }
  } else if (aHorizontalSide == (mHFlip ? eSideLeft : eSideRight)) {
    if (popupAlign == POPUPALIGNMENT_TOPRIGHT ||
        popupAlign == POPUPALIGNMENT_BOTTOMRIGHT) {
      aChange.x = 0;
    }
  }

  if (aVerticalSide == (mVFlip ? eSideBottom : eSideTop)) {
    if (popupAlign == POPUPALIGNMENT_TOPLEFT ||
        popupAlign == POPUPALIGNMENT_TOPRIGHT) {
      aChange.y = 0;
    }
  } else if (aVerticalSide == (mVFlip ? eSideTop : eSideBottom)) {
    if (popupAlign == POPUPALIGNMENT_BOTTOMLEFT ||
        popupAlign == POPUPALIGNMENT_BOTTOMRIGHT) {
      aChange.y = 0;
    }
  }
}

ConsumeOutsideClicksResult nsMenuPopupFrame::ConsumeOutsideClicks() {
  if (mContent->AsElement()->AttrValueIs(kNameSpaceID_None,
                                         nsGkAtoms::consumeoutsideclicks,
                                         nsGkAtoms::_true, eCaseMatters)) {
    return ConsumeOutsideClicks_True;
  }
  if (mContent->AsElement()->AttrValueIs(kNameSpaceID_None,
                                         nsGkAtoms::consumeoutsideclicks,
                                         nsGkAtoms::_false, eCaseMatters)) {
    return ConsumeOutsideClicks_ParentOnly;
  }
  if (mContent->AsElement()->AttrValueIs(kNameSpaceID_None,
                                         nsGkAtoms::consumeoutsideclicks,
                                         nsGkAtoms::never, eCaseMatters)) {
    return ConsumeOutsideClicks_Never;
  }

  nsCOMPtr<nsIContent> parentContent = mContent->GetParent();
  if (parentContent) {
    dom::NodeInfo* ni = parentContent->NodeInfo();
    if (ni->Equals(nsGkAtoms::menulist, kNameSpaceID_XUL)) {
      return ConsumeOutsideClicks_True;  // Consume outside clicks for combo
                                         // boxes on all platforms
    }
#if defined(XP_WIN)
    // Don't consume outside clicks for menus in Windows
    if (ni->Equals(nsGkAtoms::menu, kNameSpaceID_XUL) ||
        ni->Equals(nsGkAtoms::popupset, kNameSpaceID_XUL) ||
        ((ni->Equals(nsGkAtoms::button, kNameSpaceID_XUL) ||
          ni->Equals(nsGkAtoms::toolbarbutton, kNameSpaceID_XUL)) &&
         parentContent->AsElement()->AttrValueIs(
             kNameSpaceID_None, nsGkAtoms::type, nsGkAtoms::menu,
             eCaseMatters))) {
      return ConsumeOutsideClicks_Never;
    }
#endif
  }

  return ConsumeOutsideClicks_True;
}

// XXXroc this is megalame. Fossicking around for a frame of the right
// type is a recipe for disaster in the long term.
nsIScrollableFrame* nsMenuPopupFrame::GetScrollFrame(nsIFrame* aStart) {
  if (!aStart) return nullptr;

  // try start frame and siblings
  nsIFrame* currFrame = aStart;
  do {
    nsIScrollableFrame* sf = do_QueryFrame(currFrame);
    if (sf) return sf;
    currFrame = currFrame->GetNextSibling();
  } while (currFrame);

  // try children
  currFrame = aStart;
  do {
    nsIFrame* childFrame = currFrame->PrincipalChildList().FirstChild();
    nsIScrollableFrame* sf = GetScrollFrame(childFrame);
    if (sf) return sf;
    currFrame = currFrame->GetNextSibling();
  } while (currFrame);

  return nullptr;
}

void nsMenuPopupFrame::EnsureMenuItemIsVisible(nsMenuFrame* aMenuItem) {
  if (aMenuItem) {
    RefPtr<mozilla::PresShell> presShell = aMenuItem->PresShell();
    presShell->ScrollFrameRectIntoView(
        aMenuItem, nsRect(nsPoint(0, 0), aMenuItem->GetRect().Size()),
        ScrollAxis(), ScrollAxis(),
        ScrollFlags::ScrollOverflowHidden |
            ScrollFlags::ScrollFirstAncestorOnly);
  }
}

void nsMenuPopupFrame::ChangeByPage(bool aIsUp) {
  // Only scroll by page within menulists.
  if (!IsMenuList()) {
    return;
  }

  nsMenuFrame* newMenu = nullptr;
  nsIFrame* currentMenu = mCurrentMenu;
  if (!currentMenu) {
    // If there is no current menu item, get the first item. When moving up,
    // just use this as the newMenu and leave currentMenu null so that no
    // check for a later element is performed. When moving down, set currentMenu
    // so that we look for one page down from the first item.
    newMenu = nsXULPopupManager::GetNextMenuItem(this, nullptr, true, false);
    if (!aIsUp) {
      currentMenu = newMenu;
    }
  }

  if (currentMenu) {
    nscoord scrollHeight = mRect.height;
    nsIScrollableFrame* scrollframe = GetScrollFrame(this);
    if (scrollframe) {
      scrollHeight = scrollframe->GetScrollPortRect().height;
    }

    // Get the position of the current item and add or subtract one popup's
    // height to or from it.
    nscoord targetPosition = aIsUp
                                 ? currentMenu->GetRect().YMost() - scrollHeight
                                 : currentMenu->GetRect().y + scrollHeight;

    // Indicates that the last visible child was a valid menuitem.
    bool lastWasValid = false;

    // Look for the next child which is just past the target position. This
    // child will need to be selected.
    while (currentMenu) {
      // Only consider menu frames.
      nsMenuFrame* menuFrame = do_QueryFrame(currentMenu);
      if (menuFrame &&
          nsXULPopupManager::IsValidMenuItem(menuFrame->GetContent(), true)) {
        // If the right position was found, break out. Otherwise, look for
        // another item.
        if ((!aIsUp && currentMenu->GetRect().YMost() > targetPosition) ||
            (aIsUp && currentMenu->GetRect().y < targetPosition)) {
          // If the last visible child was not a valid menuitem or was disabled,
          // use this as the menu to select, skipping over any non-valid items
          // at the edge of the page.
          if (!lastWasValid) {
            newMenu = menuFrame;
          }

          break;
        }

        // Assign this item to newMenu. This item will be selected in case we
        // don't find any more.
        lastWasValid = true;
        newMenu = menuFrame;
      } else {
        lastWasValid = false;
      }

      currentMenu =
          aIsUp ? currentMenu->GetPrevSibling() : currentMenu->GetNextSibling();
    }
  }

  // Select the new menuitem.
  if (newMenu) {
    ChangeMenuItem(newMenu, false, true);
  }
}

NS_IMETHODIMP nsMenuPopupFrame::SetCurrentMenuItem(nsMenuFrame* aMenuItem) {
  if (mCurrentMenu == aMenuItem) return NS_OK;

  if (mCurrentMenu) {
    mCurrentMenu->SelectMenu(false);
  }

  if (aMenuItem) {
    EnsureMenuItemIsVisible(aMenuItem);
    aMenuItem->SelectMenu(true);
  }

  mCurrentMenu = aMenuItem;

  return NS_OK;
}

void nsMenuPopupFrame::CurrentMenuIsBeingDestroyed() { mCurrentMenu = nullptr; }

NS_IMETHODIMP
nsMenuPopupFrame::ChangeMenuItem(nsMenuFrame* aMenuItem, bool aSelectFirstItem,
                                 bool aFromKey) {
  if (mCurrentMenu == aMenuItem) return NS_OK;

  // When a context menu is open, the current menu is locked, and no change
  // to the menu is allowed.
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (!mIsContextMenu && pm && pm->HasContextMenu(this)) return NS_OK;

  // Unset the current child.
  if (mCurrentMenu) {
    mCurrentMenu->SelectMenu(false);
    nsMenuPopupFrame* popup = mCurrentMenu->GetPopup();
    if (popup) {
      if (mCurrentMenu->IsOpen()) {
        if (pm) pm->HidePopupAfterDelay(popup);
      }
    }
  }

  // Set the new child.
  if (aMenuItem) {
    EnsureMenuItemIsVisible(aMenuItem);
    aMenuItem->SelectMenu(true);

    // On Windows, a menulist should update its value whenever navigation was
    // done by the keyboard.
#ifdef XP_WIN
    if (aFromKey && IsOpen() && IsMenuList()) {
      // Fire a command event as the new item, but we don't want to close
      // the menu, blink it, or update any other state of the menuitem. The
      // command event will cause the item to be selected.
      nsCOMPtr<nsIContent> menuItemContent = aMenuItem->GetContent();
      RefPtr<mozilla::PresShell> presShell = PresShell();
      nsContentUtils::DispatchXULCommand(menuItemContent, /* aTrusted = */ true,
                                         nullptr, presShell, false, false,
                                         false, false);
    }
#endif
  }

  mCurrentMenu = aMenuItem;

  return NS_OK;
}

nsMenuFrame* nsMenuPopupFrame::Enter(WidgetGUIEvent* aEvent) {
  mIncrementalString.Truncate();

  // Give it to the child.
  if (mCurrentMenu) return mCurrentMenu->Enter(aEvent);

  return nullptr;
}

nsMenuFrame* nsMenuPopupFrame::FindMenuWithShortcut(KeyboardEvent* aKeyEvent,
                                                    bool& doAction) {
  uint32_t charCode = aKeyEvent->CharCode();
  uint32_t keyCode = aKeyEvent->KeyCode();

  doAction = false;

  // Enumerate over our list of frames.
  nsContainerFrame* immediateParent =
      nsXULPopupManager::ImmediateParentFrame(this);
  uint32_t matchCount = 0, matchShortcutCount = 0;
  bool foundActive = false;
  nsMenuFrame* frameBefore = nullptr;
  nsMenuFrame* frameAfter = nullptr;
  nsMenuFrame* frameShortcut = nullptr;

  nsIContent* parentContent = mContent->GetParent();

  bool isMenu = parentContent && !parentContent->NodeInfo()->Equals(
                                     nsGkAtoms::menulist, kNameSpaceID_XUL);

  DOMTimeStamp keyTime = aKeyEvent->TimeStamp();

  if (charCode == 0) {
    if (keyCode == dom::KeyboardEvent_Binding::DOM_VK_BACK_SPACE) {
      if (!isMenu && !mIncrementalString.IsEmpty()) {
        mIncrementalString.SetLength(mIncrementalString.Length() - 1);
        return nullptr;
      } else {
#ifdef XP_WIN
        nsCOMPtr<nsISound> soundInterface =
            do_CreateInstance("@mozilla.org/sound;1");
        if (soundInterface) soundInterface->Beep();
#endif  // #ifdef XP_WIN
      }
    }
    return nullptr;
  } else {
    char16_t uniChar = ToLowerCase(static_cast<char16_t>(charCode));
    if (isMenu) {
      // Menu supports only first-letter navigation
      mIncrementalString = uniChar;
    } else if (IsWithinIncrementalTime(keyTime)) {
      mIncrementalString.Append(uniChar);
    } else {
      // Interval too long, treat as new typing
      mIncrementalString = uniChar;
    }
  }

  // See bug 188199 & 192346, if all letters in incremental string are same,
  // just try to match the first one
  nsAutoString incrementalString(mIncrementalString);
  uint32_t charIndex = 1, stringLength = incrementalString.Length();
  while (charIndex < stringLength &&
         incrementalString[charIndex] == incrementalString[charIndex - 1]) {
    charIndex++;
  }
  if (charIndex == stringLength) {
    incrementalString.Truncate(1);
    stringLength = 1;
  }

  sLastKeyTime = keyTime;

  // NOTE: If you crashed here due to a bogus |immediateParent| it is
  //       possible that the menu whose shortcut is being looked up has
  //       been destroyed already.  One strategy would be to
  //       setTimeout(<func>,0) as detailed in:
  //       <http://bugzilla.mozilla.org/show_bug.cgi?id=126675#c32>
  nsIFrame* firstMenuItem =
      nsXULPopupManager::GetNextMenuItem(immediateParent, nullptr, true, false);
  nsIFrame* currFrame = firstMenuItem;

  int32_t menuAccessKey = -1;
  nsMenuBarListener::GetMenuAccessKey(&menuAccessKey);

  // We start searching from first child. This process is divided into two parts
  //   -- before current and after current -- by the current item
  while (currFrame) {
    nsIContent* current = currFrame->GetContent();
    nsAutoString textKey;
    bool isShortcut = false;
    if (current->IsElement()) {
      if (menuAccessKey >= 0) {
        // Get the shortcut attribute.
        current->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::accesskey,
                                      textKey);
      }
      isShortcut = !textKey.IsEmpty();
      if (textKey.IsEmpty()) {  // No shortcut, try first letter
        current->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::label,
                                      textKey);
        if (textKey.IsEmpty())  // No label, try another attribute (value)
          current->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::value,
                                        textKey);
      }
    }

    if (StringBeginsWith(textKey, incrementalString,
                         nsCaseInsensitiveStringComparator())) {
      // mIncrementalString is a prefix of textKey
      nsMenuFrame* menu = do_QueryFrame(currFrame);
      if (menu) {
        // There is one match
        matchCount++;
        if (isShortcut) {
          // There is one shortcut-key match
          matchShortcutCount++;
          // Record the matched item. If there is only one matched shortcut
          // item, do it
          frameShortcut = menu;
        }
        if (!foundActive) {
          // It's a first candidate item located before/on the current item
          if (!frameBefore) frameBefore = menu;
        } else {
          // It's a first candidate item located after the current item
          if (!frameAfter) frameAfter = menu;
        }
      } else
        return nullptr;
    }

    // Get the active status
    if (current->IsElement() && current->AsElement()->AttrValueIs(
                                    kNameSpaceID_None, nsGkAtoms::menuactive,
                                    nsGkAtoms::_true, eCaseMatters)) {
      foundActive = true;
      if (stringLength > 1) {
        // If there is more than one char typed, the current item has highest
        // priority,
        //   otherwise the item next to current has highest priority
        if (currFrame == frameBefore) return frameBefore;
      }
    }

    nsMenuFrame* menu = do_QueryFrame(currFrame);
    currFrame =
        nsXULPopupManager::GetNextMenuItem(immediateParent, menu, true, true);
    if (currFrame == firstMenuItem) break;
  }

  doAction = (isMenu && (matchCount == 1 || matchShortcutCount == 1));

  if (matchShortcutCount == 1)  // We have one matched shortcut item
    return frameShortcut;
  if (frameAfter)  // If we have matched item after the current, use it
    return frameAfter;
  else if (frameBefore)  // If we haven't, use the item before the current
    return frameBefore;

  // If we don't match anything, rollback the last typing
  mIncrementalString.SetLength(mIncrementalString.Length() - 1);

  // didn't find a matching menu item
#ifdef XP_WIN
  // behavior on Windows - this item is in a menu popup off of the
  // menu bar, so beep and do nothing else
  if (isMenu) {
    nsCOMPtr<nsISound> soundInterface =
        do_CreateInstance("@mozilla.org/sound;1");
    if (soundInterface) soundInterface->Beep();
  }
#endif  // #ifdef XP_WIN

  return nullptr;
}

void nsMenuPopupFrame::LockMenuUntilClosed(bool aLock) {
  mIsMenuLocked = aLock;

  // Lock / unlock the parent, too.
  nsMenuFrame* menu = do_QueryFrame(GetParent());
  if (menu) {
    nsMenuParent* parentParent = menu->GetMenuParent();
    if (parentParent) {
      parentParent->LockMenuUntilClosed(aLock);
    }
  }
}

nsIWidget* nsMenuPopupFrame::GetWidget() {
  if (!mView) return nullptr;

  return mView->GetWidget();
}

// helpers /////////////////////////////////////////////////////////////

nsresult nsMenuPopupFrame::AttributeChanged(int32_t aNameSpaceID,
                                            nsAtom* aAttribute,
                                            int32_t aModType)

{
  nsresult rv =
      nsBoxFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);

  if (aAttribute == nsGkAtoms::left || aAttribute == nsGkAtoms::top)
    MoveToAttributePosition();

  if (aAttribute == nsGkAtoms::remote) {
    // When the remote attribute changes, we need to create a new widget to
    // ensure that it has the correct compositor and transparency settings to
    // match the new value.
    EnsureWidget(true);
  }

  if (aAttribute == nsGkAtoms::followanchor) {
    nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
    if (pm) {
      pm->UpdateFollowAnchor(this);
    }
  }

  if (aAttribute == nsGkAtoms::label) {
    // set the label for the titlebar
    nsView* view = GetView();
    if (view) {
      nsIWidget* widget = view->GetWidget();
      if (widget) {
        nsAutoString title;
        mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::label,
                                       title);
        if (!title.IsEmpty()) {
          widget->SetTitle(title);
        }
      }
    }
  } else if (aAttribute == nsGkAtoms::ignorekeys) {
    nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
    if (pm) {
      nsAutoString ignorekeys;
      mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::ignorekeys,
                                     ignorekeys);
      pm->UpdateIgnoreKeys(ignorekeys.EqualsLiteral("true"));
    }
  }

  return rv;
}

void nsMenuPopupFrame::MoveToAttributePosition() {
  // Move the widget around when the user sets the |left| and |top| attributes.
  // Note that this is not the best way to move the widget, as it results in
  // lots of FE notifications and is likely to be slow as molasses. Use |moveTo|
  // on the element if possible.
  nsAutoString left, top;
  mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::left, left);
  mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::top, top);
  nsresult err1, err2;
  mozilla::CSSIntPoint pos(left.ToInteger(&err1), top.ToInteger(&err2));

  if (NS_SUCCEEDED(err1) && NS_SUCCEEDED(err2)) MoveTo(pos, false);
}

void nsMenuPopupFrame::DestroyFrom(nsIFrame* aDestructRoot,
                                   PostDestroyData& aPostDestroyData) {
  if (mReflowCallbackData.mPosted) {
    PresShell()->CancelReflowCallback(this);
    mReflowCallbackData.Clear();
  }

  nsMenuFrame* menu = do_QueryFrame(GetParent());
  if (menu) {
    // clear the open attribute on the parent menu
    nsContentUtils::AddScriptRunner(new nsUnsetAttrRunnable(
        menu->GetContent()->AsElement(), nsGkAtoms::open));
  }

  ClearPopupShownDispatcher();

  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm) pm->PopupDestroyed(this);

  nsIPopupContainer* popupContainer =
      nsIPopupContainer::GetPopupContainer(PresContext()->GetPresShell());
  if (popupContainer && popupContainer->GetDefaultTooltip() == mContent) {
    popupContainer->SetDefaultTooltip(nullptr);
  }

  nsBoxFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

void nsMenuPopupFrame::MoveTo(const CSSIntPoint& aPos, bool aUpdateAttrs) {
  nsIWidget* widget = GetWidget();
  if ((mScreenRect.x == aPos.x && mScreenRect.y == aPos.y) &&
      (!widget || widget->GetClientOffset() == mLastClientOffset)) {
    return;
  }

  // reposition the popup at the specified coordinates. Don't clear the anchor
  // and position, because the popup can be reset to its anchor position by
  // using (-1, -1) as coordinates. Subtract off the margin as it will be
  // added to the position when SetPopupPosition is called.
  nsMargin margin(0, 0, 0, 0);
  StyleMargin()->GetMargin(margin);

  // Workaround for bug 788189.  See also bug 708278 comment #25 and following.
  if (mAdjustOffsetForContextMenu) {
    margin.left += nsPresContext::CSSPixelsToAppUnits(
        LookAndFeel::GetInt(LookAndFeel::eIntID_ContextMenuOffsetHorizontal));
    margin.top += nsPresContext::CSSPixelsToAppUnits(
        LookAndFeel::GetInt(LookAndFeel::eIntID_ContextMenuOffsetVertical));
  }

  mAnchorType = MenuPopupAnchorType_Point;
  mScreenRect.x = aPos.x - nsPresContext::AppUnitsToIntCSSPixels(margin.left);
  mScreenRect.y = aPos.y - nsPresContext::AppUnitsToIntCSSPixels(margin.top);

  SetPopupPosition(nullptr, true, false, true);

  RefPtr<Element> popup = mContent->AsElement();
  if (aUpdateAttrs && (popup->HasAttr(kNameSpaceID_None, nsGkAtoms::left) ||
                       popup->HasAttr(kNameSpaceID_None, nsGkAtoms::top))) {
    nsAutoString left, top;
    left.AppendInt(aPos.x);
    top.AppendInt(aPos.y);
    popup->SetAttr(kNameSpaceID_None, nsGkAtoms::left, left, false);
    popup->SetAttr(kNameSpaceID_None, nsGkAtoms::top, top, false);
  }
}

void nsMenuPopupFrame::MoveToAnchor(nsIContent* aAnchorContent,
                                    const nsAString& aPosition, int32_t aXPos,
                                    int32_t aYPos, bool aAttributesOverride) {
  NS_ASSERTION(IsVisible(), "popup must be visible to move it");

  nsPopupState oldstate = mPopupState;
  InitializePopup(aAnchorContent, mTriggerContent, aPosition, aXPos, aYPos,
                  MenuPopupAnchorType_Node, aAttributesOverride);
  // InitializePopup changed the state so reset it.
  mPopupState = oldstate;

  // Pass false here so that flipping and adjusting to fit on the screen happen.
  SetPopupPosition(nullptr, false, false, true);
}

bool nsMenuPopupFrame::GetAutoPosition() { return mShouldAutoPosition; }

void nsMenuPopupFrame::SetAutoPosition(bool aShouldAutoPosition) {
  mShouldAutoPosition = aShouldAutoPosition;
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm) {
    pm->UpdateFollowAnchor(this);
  }
}

int8_t nsMenuPopupFrame::GetAlignmentPosition() const {
  // The code below handles most cases of alignment, anchor and position values.
  // Those that are not handled just return POPUPPOSITION_UNKNOWN.

  if (mPosition == POPUPPOSITION_OVERLAP ||
      mPosition == POPUPPOSITION_AFTERPOINTER ||
      mPosition == POPUPPOSITION_SELECTION)
    return mPosition;

  int8_t position = mPosition;

  if (position == POPUPPOSITION_UNKNOWN) {
    switch (mPopupAnchor) {
      case POPUPALIGNMENT_BOTTOMCENTER:
        position = mPopupAlignment == POPUPALIGNMENT_TOPRIGHT
                       ? POPUPPOSITION_AFTEREND
                       : POPUPPOSITION_AFTERSTART;
        break;
      case POPUPALIGNMENT_TOPCENTER:
        position = mPopupAlignment == POPUPALIGNMENT_BOTTOMRIGHT
                       ? POPUPPOSITION_BEFOREEND
                       : POPUPPOSITION_BEFORESTART;
        break;
      case POPUPALIGNMENT_LEFTCENTER:
        position = mPopupAlignment == POPUPALIGNMENT_BOTTOMRIGHT
                       ? POPUPPOSITION_STARTAFTER
                       : POPUPPOSITION_STARTBEFORE;
        break;
      case POPUPALIGNMENT_RIGHTCENTER:
        position = mPopupAlignment == POPUPALIGNMENT_BOTTOMLEFT
                       ? POPUPPOSITION_ENDAFTER
                       : POPUPPOSITION_ENDBEFORE;
        break;
      default:
        break;
    }
  }

  if (mHFlip) {
    position = POPUPPOSITION_HFLIP(position);
  }

  if (mVFlip) {
    position = POPUPPOSITION_VFLIP(position);
  }

  return position;
}

/**
 * KEEP THIS IN SYNC WITH nsFrame::CreateView
 * as much as possible. Until we get rid of views finally...
 */
void nsMenuPopupFrame::CreatePopupView() {
  if (HasView()) {
    return;
  }

  nsViewManager* viewManager = PresContext()->GetPresShell()->GetViewManager();
  NS_ASSERTION(nullptr != viewManager, "null view manager");

  // Create a view
  nsView* parentView = viewManager->GetRootView();
  nsViewVisibility visibility = nsViewVisibility_kHide;
  int32_t zIndex = INT32_MAX;
  bool autoZIndex = false;

  NS_ASSERTION(parentView, "no parent view");

  // Create a view
  nsView* view = viewManager->CreateView(GetRect(), parentView, visibility);
  viewManager->SetViewZIndex(view, autoZIndex, zIndex);
  // XXX put view last in document order until we can do better
  viewManager->InsertChild(parentView, view, nullptr, true);

  // Remember our view
  SetView(view);

  NS_FRAME_LOG(
      NS_FRAME_TRACE_CALLS,
      ("nsMenuPopupFrame::CreatePopupView: frame=%p view=%p", this, view));
}

bool nsMenuPopupFrame::ShouldFollowAnchor() {
  if (!mShouldAutoPosition || mAnchorType != MenuPopupAnchorType_Node ||
      !mAnchorContent) {
    return false;
  }

  // Follow anchor mode is used when followanchor="true" is set or for arrow
  // panels.
  if (mContent->AsElement()->AttrValueIs(kNameSpaceID_None,
                                         nsGkAtoms::followanchor,
                                         nsGkAtoms::_true, eCaseMatters)) {
    return true;
  }

  if (mContent->AsElement()->AttrValueIs(kNameSpaceID_None,
                                         nsGkAtoms::followanchor,
                                         nsGkAtoms::_false, eCaseMatters)) {
    return false;
  }

  return (mPopupType == ePopupTypePanel &&
          mContent->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                                             nsGkAtoms::arrow, eCaseMatters));
}

bool nsMenuPopupFrame::ShouldFollowAnchor(nsRect& aRect) {
  if (!ShouldFollowAnchor()) {
    return false;
  }

  nsIFrame* anchorFrame = mAnchorContent->GetPrimaryFrame();
  if (anchorFrame) {
    nsPresContext* rootPresContext = PresContext()->GetRootPresContext();
    if (rootPresContext) {
      aRect = ComputeAnchorRect(rootPresContext, anchorFrame);
    }
  }

  return true;
}

void nsMenuPopupFrame::CheckForAnchorChange(nsRect& aRect) {
  // Don't update if the popup isn't visible or we shouldn't be following the
  // anchor.
  if (!IsVisible() || !ShouldFollowAnchor()) {
    return;
  }

  bool shouldHide = false;

  nsPresContext* rootPresContext = PresContext()->GetRootPresContext();

  // If the frame for the anchor has gone away, hide the popup.
  nsIFrame* anchor = mAnchorContent->GetPrimaryFrame();
  if (!anchor || !rootPresContext) {
    shouldHide = true;
  } else if (!anchor->IsVisibleConsideringAncestors(
                 VISIBILITY_CROSS_CHROME_CONTENT_BOUNDARY)) {
    // If the anchor is now inside something that is invisible, hide the popup.
    shouldHide = true;
  } else {
    // If the anchor is now inside a hidden parent popup, hide the popup.
    nsIFrame* frame = anchor;
    while (frame) {
      nsMenuPopupFrame* popup = do_QueryFrame(frame);
      if (popup && popup->PopupState() != ePopupShown) {
        shouldHide = true;
        break;
      }

      frame = frame->GetParent();
    }
  }

  if (shouldHide) {
    nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
    if (pm) {
      // As the caller will be iterating over the open popups, hide
      // asyncronously.
      pm->HidePopup(mContent, false, true, true, false);
    }

    return;
  }

  nsRect anchorRect = ComputeAnchorRect(rootPresContext, anchor);

  // If the rectangles are different, move the popup.
  if (!anchorRect.IsEqualEdges(aRect)) {
    aRect = anchorRect;
    SetPopupPosition(nullptr, true, false, true);
  }
}

nsIWidget* nsMenuPopupFrame::GetParentMenuWidget() {
  nsMenuFrame* menuFrame = do_QueryFrame(GetParent());
  if (menuFrame) {
    nsMenuParent* parentPopup = menuFrame->GetMenuParent();
    if (parentPopup && (parentPopup->IsMenu() || parentPopup->IsMenuBar())) {
      return static_cast<nsMenuPopupFrame*>(parentPopup)->GetWidget();
    }
  }
  return nullptr;
}
