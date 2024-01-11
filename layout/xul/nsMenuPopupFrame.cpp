/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMenuPopupFrame.h"
#include "LayoutConstants.h"
#include "XULButtonElement.h"
#include "XULPopupElement.h"
#include "mozilla/dom/XULPopupElement.h"
#include "nsGkAtoms.h"
#include "nsIContent.h"
#include "nsIFrameInlines.h"
#include "nsAtom.h"
#include "nsPresContext.h"
#include "mozilla/ComputedStyle.h"
#include "nsCSSRendering.h"
#include "nsNameSpaceManager.h"
#include "nsIFrameInlines.h"
#include "nsViewManager.h"
#include "nsWidgetsCID.h"
#include "nsPIDOMWindow.h"
#include "nsFrameManager.h"
#include "mozilla/dom/Document.h"
#include "nsRect.h"
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
#include "nsServiceManagerUtils.h"
#include "nsStyleConsts.h"
#include "nsStyleStructInlines.h"
#include "nsTransitionManager.h"
#include "nsDisplayList.h"
#include "nsIDOMXULSelectCntrlEl.h"
#include "mozilla/widget/ScreenManager.h"
#include "mozilla/AnimationUtils.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/Preferences.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/PresShell.h"
#include "mozilla/Services.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/KeyboardEvent.h"
#include "mozilla/dom/KeyboardEventBinding.h"
#include <algorithm>

#include "X11UndefineNone.h"
#include "nsXULPopupManager.h"

using namespace mozilla;
using mozilla::dom::Document;
using mozilla::dom::Element;
using mozilla::dom::Event;
using mozilla::dom::XULButtonElement;

int8_t nsMenuPopupFrame::sDefaultLevelIsTop = -1;

TimeStamp nsMenuPopupFrame::sLastKeyTime;

#ifdef MOZ_WAYLAND
#  include "mozilla/WidgetUtilsGtk.h"
#  define IS_WAYLAND_DISPLAY() mozilla::widget::GdkIsWaylandDisplay()
extern mozilla::LazyLogModule gWidgetPopupLog;
#  define LOG_WAYLAND(...) \
    MOZ_LOG(gWidgetPopupLog, mozilla::LogLevel::Debug, (__VA_ARGS__))
#else
#  define IS_WAYLAND_DISPLAY() false
#  define LOG_WAYLAND(...)
#endif

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
NS_QUERYFRAME_TAIL_INHERITING(nsBlockFrame)

//
// nsMenuPopupFrame ctor
//
nsMenuPopupFrame::nsMenuPopupFrame(ComputedStyle* aStyle,
                                   nsPresContext* aPresContext)
    : nsBlockFrame(aStyle, aPresContext, kClassID) {
  // the preference name is backwards here. True means that the 'top' level is
  // the default, and false means that the 'parent' level is the default.
  if (sDefaultLevelIsTop >= 0) return;
  sDefaultLevelIsTop =
      Preferences::GetBool("ui.panel.default_level_parent", false);
}  // ctor

nsMenuPopupFrame::~nsMenuPopupFrame() = default;

static bool IsMouseTransparent(const ComputedStyle& aStyle) {
  // If pointer-events: none; is set on the popup, then the widget should
  // ignore mouse events, passing them through to the content behind.
  return aStyle.PointerEvents() == StylePointerEvents::None;
}

static nsIWidget::InputRegion ComputeInputRegion(const ComputedStyle& aStyle,
                                                 const nsPresContext& aPc) {
  return {IsMouseTransparent(aStyle),
          (aStyle.StyleUIReset()->mMozWindowInputRegionMargin.ToCSSPixels() *
           aPc.CSSToDevPixelScale())
              .Truncated()};
}

bool nsMenuPopupFrame::ShouldCreateWidgetUpfront() const {
  if (mPopupType != PopupType::Menu) {
    // Any panel with a type attribute, such as the autocomplete popup, is
    // always generated right away.
    return mContent->AsElement()->HasAttr(nsGkAtoms::type);
  }

  // Generate the widget up-front if the parent menu is a <menulist> unless its
  // sizetopopup is set to "none".
  return ShouldExpandToInflowParentOrAnchor();
}

void nsMenuPopupFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                            nsIFrame* aPrevInFlow) {
  nsBlockFrame::Init(aContent, aParent, aPrevInFlow);

  CreatePopupView();

  // XXX Hack. The popup's view should float above all other views,
  // so we use the nsView::SetFloating() to tell the view manager
  // about that constraint.
  nsView* ourView = GetView();
  nsViewManager* viewManager = ourView->GetViewManager();
  viewManager->SetViewFloating(ourView, true);

  const auto& el = PopupElement();
  mPopupType = PopupType::Panel;
  if (el.IsMenu()) {
    mPopupType = PopupType::Menu;
  } else if (el.IsXULElement(nsGkAtoms::tooltip)) {
    mPopupType = PopupType::Tooltip;
  }

  if (PresContext()->IsChrome()) {
    mInContentShell = false;
  }

  // Support incontentshell=false attribute to allow popups to be displayed
  // outside of the content shell. Chrome only.
  if (el.NodePrincipal()->IsSystemPrincipal()) {
    if (el.GetXULBoolAttr(nsGkAtoms::incontentshell)) {
      mInContentShell = true;
    } else if (el.AttrValueIs(kNameSpaceID_None, nsGkAtoms::incontentshell,
                              nsGkAtoms::_false, eCaseMatters)) {
      mInContentShell = false;
    }
  }

  // To improve performance, create the widget for the popup if needed. Popups
  // such as menus will create their widgets later when the popup opens.
  //
  // FIXME(emilio): Doing this up-front for all menupopups causes a bunch of
  // assertions, while it's supposed to be just an optimization.
  if (!ourView->HasWidget() && ShouldCreateWidgetUpfront()) {
    CreateWidgetForView(ourView);
  }

  AddStateBits(NS_FRAME_IN_POPUP);
}

bool nsMenuPopupFrame::HasRemoteContent() const {
  return !mInContentShell && mPopupType == PopupType::Panel &&
         mContent->AsElement()->AttrValueIs(kNameSpaceID_None,
                                            nsGkAtoms::remote, nsGkAtoms::_true,
                                            eIgnoreCase);
}

bool nsMenuPopupFrame::IsNoAutoHide() const {
  // Panels with noautohide="true" don't hide when the mouse is clicked
  // outside of them, or when another application is made active. Non-autohide
  // panels cannot be used in content windows.
  return !mInContentShell && mPopupType == PopupType::Panel &&
         mContent->AsElement()->AttrValueIs(kNameSpaceID_None,
                                            nsGkAtoms::noautohide,
                                            nsGkAtoms::_true, eIgnoreCase);
}

widget::PopupLevel nsMenuPopupFrame::GetPopupLevel(bool aIsNoAutoHide) const {
  // The popup level is determined as follows, in this order:
  //   1. non-panels (menus and tooltips) are always topmost
  //   2. any specified level attribute
  //   3. if a titlebar attribute is set, use the 'floating' level
  //   4. if this is a noautohide panel, use the 'parent' level
  //   5. use the platform-specific default level

  // If this is not a panel, this is always a top-most popup.
  if (mPopupType != PopupType::Panel) {
    return PopupLevel::Top;
  }

  // If the level attribute has been set, use that.
  static Element::AttrValuesArray strings[] = {
      nsGkAtoms::top, nsGkAtoms::parent, nsGkAtoms::floating, nullptr};
  switch (mContent->AsElement()->FindAttrValueIn(
      kNameSpaceID_None, nsGkAtoms::level, strings, eCaseMatters)) {
    case 0:
      return PopupLevel::Top;
    case 1:
      return PopupLevel::Parent;
    case 2:
      return PopupLevel::Floating;
  }

  // Panels with titlebars most likely want to be floating popups.
  if (mContent->AsElement()->HasAttr(nsGkAtoms::titlebar)) {
    return PopupLevel::Floating;
  }

  // If this panel is a noautohide panel, the default is the parent level.
  if (aIsNoAutoHide) {
    return PopupLevel::Parent;
  }

  // Otherwise, the result depends on the platform.
  return sDefaultLevelIsTop ? PopupLevel::Top : PopupLevel::Parent;
}

void nsMenuPopupFrame::PrepareWidget(bool aRecreate) {
  nsView* ourView = GetView();
  if (aRecreate) {
    if (auto* widget = GetWidget()) {
      // Widget's WebRender resources needs to be cleared before creating new
      // widget.
      widget->ClearCachedWebrenderResources();
    }
    ourView->DestroyWidget();
  }
  if (!ourView->HasWidget()) {
    CreateWidgetForView(ourView);
  } else {
    PropagateStyleToWidget();
  }
}

nsresult nsMenuPopupFrame::CreateWidgetForView(nsView* aView) {
  // Create a widget for ourselves.
  widget::InitData widgetData;
  widgetData.mWindowType = widget::WindowType::Popup;
  widgetData.mBorderStyle = widget::BorderStyle::Default;
  widgetData.mForMenupopupFrame = true;
  widgetData.mClipSiblings = true;
  widgetData.mPopupHint = mPopupType;
  widgetData.mNoAutoHide = IsNoAutoHide();

  if (!mInContentShell) {
    // A drag popup may be used for non-static translucent drag feedback
    if (mPopupType == PopupType::Panel &&
        mContent->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                                           nsGkAtoms::drag, eIgnoreCase)) {
      widgetData.mIsDragPopup = true;
    }
  }

  nsAutoString title;
  if (widgetData.mNoAutoHide &&
      mContent->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::titlebar,
                                         nsGkAtoms::normal, eCaseMatters)) {
    widgetData.mBorderStyle = widget::BorderStyle::Title;

    mContent->AsElement()->GetAttr(nsGkAtoms::label, title);
    if (mContent->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::close,
                                           nsGkAtoms::_true, eCaseMatters)) {
      widgetData.mBorderStyle =
          widgetData.mBorderStyle | widget::BorderStyle::Close;
    }
  }

  bool remote = HasRemoteContent();

  const auto mode = nsLayoutUtils::GetFrameTransparency(this, this);
  widgetData.mHasRemoteContent = remote;
  widgetData.mTransparencyMode = mode;
  widgetData.mPopupLevel = GetPopupLevel(widgetData.mNoAutoHide);

  // Panels which have a parent level need a parent widget. This allows them to
  // always appear in front of the parent window but behind other windows that
  // should be in front of it.
  nsCOMPtr<nsIWidget> parentWidget;
  if (widgetData.mPopupLevel != PopupLevel::Top) {
    nsCOMPtr<nsIDocShellTreeItem> dsti = PresContext()->GetDocShell();
    if (!dsti) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
    dsti->GetTreeOwner(getter_AddRefs(treeOwner));
    if (!treeOwner) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIBaseWindow> baseWindow(do_QueryInterface(treeOwner));
    if (baseWindow) baseWindow->GetMainWidget(getter_AddRefs(parentWidget));
  }

  nsresult rv = aView->CreateWidgetForPopup(&widgetData, parentWidget);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsIWidget* widget = aView->GetWidget();
  widget->SetTransparencyMode(mode);

  PropagateStyleToWidget();

  // most popups don't have a title so avoid setting the title if there isn't
  // one
  if (!title.IsEmpty()) {
    widget->SetTitle(title);
  }

  return NS_OK;
}

void nsMenuPopupFrame::PropagateStyleToWidget(WidgetStyleFlags aFlags) const {
  if (aFlags.isEmpty()) {
    return;
  }

  nsIWidget* widget = GetWidget();
  if (!widget) {
    return;
  }

  if (aFlags.contains(WidgetStyle::ColorScheme)) {
    widget->SetColorScheme(Some(LookAndFeel::ColorSchemeForFrame(this)));
  }
  if (aFlags.contains(WidgetStyle::InputRegion)) {
    widget->SetInputRegion(ComputeInputRegion(*Style(), *PresContext()));
  }
  if (aFlags.contains(WidgetStyle::Opacity)) {
    widget->SetWindowOpacity(StyleUIReset()->mWindowOpacity);
  }
  if (aFlags.contains(WidgetStyle::Shadow)) {
    widget->SetWindowShadowStyle(GetShadowStyle());
  }
  if (aFlags.contains(WidgetStyle::Transform)) {
    widget->SetWindowTransform(ComputeWidgetTransform());
  }
}

bool nsMenuPopupFrame::IsMouseTransparent() const {
  return ::IsMouseTransparent(*Style());
}

WindowShadow nsMenuPopupFrame::GetShadowStyle() const {
  StyleWindowShadow shadow = StyleUIReset()->mWindowShadow;
  if (shadow != StyleWindowShadow::Auto) {
    MOZ_ASSERT(shadow == StyleWindowShadow::None);
    return WindowShadow::None;
  }

  switch (StyleDisplay()->EffectiveAppearance()) {
    case StyleAppearance::Tooltip:
      return WindowShadow::Tooltip;
    case StyleAppearance::Menupopup:
      return WindowShadow::Menu;
    default:
      return WindowShadow::Panel;
  }
}

void nsMenuPopupFrame::SetPopupState(nsPopupState aState) {
  mPopupState = aState;

  // Work around https://gitlab.gnome.org/GNOME/gtk/-/issues/4166
  if (aState == ePopupShown && IS_WAYLAND_DISPLAY()) {
    if (nsIWidget* widget = GetWidget()) {
      widget->SetInputRegion(ComputeInputRegion(*Style(), *PresContext()));
    }
  }
}

// TODO: Convert this to MOZ_CAN_RUN_SCRIPT (bug 1415230, bug 1535398)
MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHODIMP nsXULPopupShownEvent::Run() {
  nsMenuPopupFrame* popup = do_QueryFrame(mPopup->GetPrimaryFrame());
  // Set the state to visible if the popup is still open.
  if (popup && popup->IsOpen()) {
    popup->SetPopupState(ePopupShown);
  }

  if (!mPopup->IsXULElement(nsGkAtoms::tooltip)) {
    nsCOMPtr<nsIObserverService> obsService =
        mozilla::services::GetObserverService();
    if (obsService) {
      obsService->NotifyObservers(mPopup, "popup-shown", nullptr);
    }
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
  mPopup->RemoveSystemEventListener(u"transitionend"_ns, this, false);
}

NS_IMPL_ISUPPORTS_INHERITED(nsXULPopupShownEvent, Runnable,
                            nsIDOMEventListener);

void nsMenuPopupFrame::DidSetComputedStyle(ComputedStyle* aOldStyle) {
  nsBlockFrame::DidSetComputedStyle(aOldStyle);

  if (!aOldStyle) {
    return;
  }

  WidgetStyleFlags flags;

  if (aOldStyle->StyleUI()->mColorScheme != StyleUI()->mColorScheme) {
    flags += WidgetStyle::ColorScheme;
  }

  auto& newUI = *StyleUIReset();
  auto& oldUI = *aOldStyle->StyleUIReset();
  if (newUI.mWindowOpacity != oldUI.mWindowOpacity) {
    flags += WidgetStyle::Opacity;
  }

  if (newUI.mMozWindowTransform != oldUI.mMozWindowTransform) {
    flags += WidgetStyle::Transform;
  }

  if (newUI.mWindowShadow != oldUI.mWindowShadow) {
    flags += WidgetStyle::Shadow;
  }

  const auto& pc = *PresContext();
  auto oldRegion = ComputeInputRegion(*aOldStyle, pc);
  auto newRegion = ComputeInputRegion(*Style(), pc);
  if (oldRegion.mFullyTransparent != newRegion.mFullyTransparent ||
      oldRegion.mMargin != newRegion.mMargin) {
    flags += WidgetStyle::InputRegion;
  }

  PropagateStyleToWidget(flags);
}

void nsMenuPopupFrame::TweakMinPrefISize(nscoord& aSize) {
  if (!ShouldExpandToInflowParentOrAnchor()) {
    return;
  }
  // Make sure to accommodate for our scrollbar if needed. Do it only for
  // menulists to match previous behavior.
  //
  // NOTE(emilio): This is somewhat hacky. The "right" fix (which would be
  // using scrollbar-gutter: stable on the scroller) isn't great, because even
  // though we want a stable gutter, we want to draw on top of the gutter when
  // there's no scrollbar, otherwise it looks rather weird.
  //
  // Automatically accommodating for the scrollbar otherwise would be bug
  // 764076, but that has its own set of problems.
  if (nsIScrollableFrame* sf = GetScrollFrame()) {
    aSize += sf->GetDesiredScrollbarSizes().LeftRight();
  }

  nscoord menuListOrAnchorWidth = 0;
  if (nsIFrame* menuList = GetInFlowParent()) {
    menuListOrAnchorWidth = menuList->GetRect().width;
  }
  if (mAnchorType == MenuPopupAnchorType_Rect) {
    menuListOrAnchorWidth = std::max(menuListOrAnchorWidth, mScreenRect.width);
  }
  // Input margin doesn't have contents, so account for it for popup sizing
  // purposes.
  menuListOrAnchorWidth +=
      2 * StyleUIReset()->mMozWindowInputRegionMargin.ToAppUnits();
  aSize = std::max(aSize, menuListOrAnchorWidth);
}

nscoord nsMenuPopupFrame::GetMinISize(gfxContext* aRC) {
  nscoord result;
  DISPLAY_PREF_INLINE_SIZE(this, result);

  result = nsBlockFrame::GetMinISize(aRC);
  TweakMinPrefISize(result);
  return result;
}

nscoord nsMenuPopupFrame::GetPrefISize(gfxContext* aRC) {
  nscoord result;
  DISPLAY_PREF_INLINE_SIZE(this, result);

  result = nsBlockFrame::GetPrefISize(aRC);
  TweakMinPrefISize(result);
  return result;
}

void nsMenuPopupFrame::Reflow(nsPresContext* aPresContext,
                              ReflowOutput& aDesiredSize,
                              const ReflowInput& aReflowInput,
                              nsReflowStatus& aStatus) {
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsMenuPopupFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aDesiredSize, aStatus);
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");

  const auto wm = GetWritingMode();
  // Default to preserving our bounds.
  aDesiredSize.SetSize(wm, GetLogicalSize(wm));

  LayoutPopup(aPresContext, aDesiredSize, aReflowInput, aStatus);

  aDesiredSize.SetBlockStartAscent(aDesiredSize.BSize(wm));
  aDesiredSize.SetOverflowAreasToDesiredBounds();
  FinishAndStoreOverflow(&aDesiredSize, aReflowInput.mStyleDisplay);
}

void nsMenuPopupFrame::EnsureActiveMenuListItemIsVisible() {
  if (!IsMenuList() || !IsOpen()) {
    return;
  }
  nsIFrame* frame = GetCurrentMenuItemFrame();
  if (!frame) {
    return;
  }
  RefPtr<mozilla::PresShell> presShell = PresShell();
  presShell->ScrollFrameIntoView(
      frame, Nothing(), ScrollAxis(), ScrollAxis(),
      ScrollFlags::ScrollOverflowHidden | ScrollFlags::ScrollFirstAncestorOnly);
}

void nsMenuPopupFrame::LayoutPopup(nsPresContext* aPresContext,
                                   ReflowOutput& aDesiredSize,
                                   const ReflowInput& aReflowInput,
                                   nsReflowStatus& aStatus) {
  if (IsNativeMenu()) {
    return;
  }

  SchedulePaint();

  const bool isOpen = IsOpen();
  if (!isOpen) {
    // If the popup is not open, only do layout while showing or if we're a
    // menulist.
    //
    // This is needed because the SelectParent code wants to limit the height of
    // the popup before opening it.
    //
    // TODO(emilio): We should consider adding a way to do that more reliably
    // instead, but this preserves existing behavior.
    const bool needsLayout = mPopupState == ePopupShowing ||
                             mPopupState == ePopupPositioning || IsMenuList();
    if (!needsLayout) {
      RemoveStateBits(NS_FRAME_FIRST_REFLOW);
      return;
    }
  }

  // Do a first reflow, with all our content, in order to find our preferred
  // size. Then, we do a second reflow with the updated dimensions.
  const bool needsPrefSize = mPrefSize == nsSize(-1, -1) || IsSubtreeDirty();
  if (needsPrefSize) {
    // Get the preferred, minimum and maximum size. If the menu is sized to the
    // popup, then the popup's width is the menu's width.
    ReflowOutput preferredSize(aReflowInput);
    nsBlockFrame::Reflow(aPresContext, preferredSize, aReflowInput, aStatus);
    mPrefSize = preferredSize.PhysicalSize();
  }

  // Get our desired position and final size, now that we have a preferred size.
  auto constraints = GetRects(mPrefSize);
  const auto finalSize = constraints.mUsedRect.Size();

  // We need to do an extra reflow if we haven't reflowed, our size doesn't
  // match with our final intended size, or our bsize is unconstrained (in which
  // case we need to specify the final size so that percentages work).
  const bool needDefiniteReflow =
      aReflowInput.ComputedBSize() == NS_UNCONSTRAINEDSIZE || !needsPrefSize ||
      finalSize != mPrefSize;

  if (needDefiniteReflow) {
    ReflowInput constrainedReflowInput(aReflowInput);
    const auto& bp = aReflowInput.ComputedPhysicalBorderPadding();
    // TODO: writing-mode handling not terribly correct, but it doesn't matter.
    const nsSize finalContentSize(finalSize.width - bp.LeftRight(),
                                  finalSize.height - bp.TopBottom());
    constrainedReflowInput.SetComputedISize(finalContentSize.width);
    constrainedReflowInput.SetComputedBSize(finalContentSize.height);
    constrainedReflowInput.SetIResize(finalSize.width != mPrefSize.width);
    constrainedReflowInput.SetBResize([&] {
      if (finalSize.height != mPrefSize.height) {
        return true;
      }
      if (needsPrefSize &&
          aReflowInput.ComputedBSize() == NS_UNCONSTRAINEDSIZE &&
          aReflowInput.ComputedMaxBSize() == finalContentSize.height) {
        // If we have measured, and maybe clamped our children via max-height,
        // they might need to get percentages in the block axis re-resolved.
        return true;
      }
      return false;
    }());

    aStatus.Reset();
    nsBlockFrame::Reflow(aPresContext, aDesiredSize, constrainedReflowInput,
                         aStatus);
  }

  // Set our size, since nsAbsoluteContainingBlock won't.
  SetRect(constraints.mUsedRect);

  nsView* view = GetView();
  if (isOpen) {
    nsViewManager* viewManager = view->GetViewManager();
    viewManager->ResizeView(view,
                            nsRect(nsPoint(), constraints.mUsedRect.Size()));
    if (mPopupState == ePopupOpening) {
      mPopupState = ePopupVisible;
    }

    viewManager->SetViewVisibility(view, ViewVisibility::Show);
    SyncFrameViewProperties(view);
  }

  // Perform our move now. That will position the view and so on.
  PerformMove(constraints);

  // finally, if the popup just opened, send a popupshown event
  bool openChanged = mIsOpenChanged;
  if (openChanged) {
    mIsOpenChanged = false;

    // Make sure the current selection in a menulist is visible.
    EnsureActiveMenuListItemIsVisible();

    // If the animate attribute is set to open, check for a transition and wait
    // for it to finish before firing the popupshown event.
    if (LookAndFeel::GetInt(LookAndFeel::IntID::PanelAnimations) &&
        mContent->AsElement()->AttrValueIs(kNameSpaceID_None,
                                           nsGkAtoms::animate, nsGkAtoms::open,
                                           eCaseMatters) &&
        AnimationUtils::HasCurrentTransitions(mContent->AsElement(),
                                              PseudoStyleType::NotPseudo)) {
      mPopupShownDispatcher = new nsXULPopupShownEvent(mContent, aPresContext);
      mContent->AddSystemEventListener(u"transitionend"_ns,
                                       mPopupShownDispatcher, false, false);
      return;
    }

    // If there are no transitions, fire the popupshown event right away.
    nsCOMPtr<nsIRunnable> event =
        new nsXULPopupShownEvent(GetContent(), aPresContext);
    mContent->OwnerDoc()->Dispatch(event.forget());
  }
}

bool nsMenuPopupFrame::IsMenuList() const {
  return PopupElement().IsInMenuList();
}

bool nsMenuPopupFrame::ShouldExpandToInflowParentOrAnchor() const {
  return IsMenuList() && !mContent->GetParent()->AsElement()->AttrValueIs(
                             kNameSpaceID_None, nsGkAtoms::sizetopopup,
                             nsGkAtoms::none, eCaseMatters);
}

nsIContent* nsMenuPopupFrame::GetTriggerContent(
    nsMenuPopupFrame* aMenuPopupFrame) {
  while (aMenuPopupFrame) {
    if (aMenuPopupFrame->mTriggerContent) {
      return aMenuPopupFrame->mTriggerContent;
    }

    auto* button = XULButtonElement::FromNodeOrNull(
        aMenuPopupFrame->GetContent()->GetParent());
    if (!button || !button->IsMenu()) {
      break;
    }

    auto* popup = button->GetContainingPopupElement();
    if (!popup) {
      break;
    }

    // check up the menu hierarchy until a popup with a trigger node is found
    aMenuPopupFrame = do_QueryFrame(popup->GetPrimaryFrame());
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
  else if (aAlign.EqualsLiteral("leftcenter"))
    mPopupAlignment = POPUPALIGNMENT_LEFTCENTER;
  else if (aAlign.EqualsLiteral("rightcenter"))
    mPopupAlignment = POPUPALIGNMENT_RIGHTCENTER;
  else if (aAlign.EqualsLiteral("topcenter"))
    mPopupAlignment = POPUPALIGNMENT_TOPCENTER;
  else if (aAlign.EqualsLiteral("bottomcenter"))
    mPopupAlignment = POPUPALIGNMENT_BOTTOMCENTER;
  else
    mPopupAlignment = POPUPALIGNMENT_NONE;

  mPosition = POPUPPOSITION_UNKNOWN;
}

static FlipType FlipFromAttribute(nsMenuPopupFrame* aFrame) {
  nsAutoString flip;
  aFrame->PopupElement().GetAttr(nsGkAtoms::flip, flip);
  if (flip.EqualsLiteral("none")) {
    return FlipType_None;
  }
  if (flip.EqualsLiteral("both")) {
    return FlipType_Both;
  }
  if (flip.EqualsLiteral("slide")) {
    return FlipType_Slide;
  }
  return FlipType_Default;
}

void nsMenuPopupFrame::InitializePopup(nsIContent* aAnchorContent,
                                       nsIContent* aTriggerContent,
                                       const nsAString& aPosition,
                                       int32_t aXPos, int32_t aYPos,
                                       MenuPopupAnchorType aAnchorType,
                                       bool aAttributesOverride) {
  auto* widget = GetWidget();
  bool recreateWidget = widget && widget->NeedsRecreateToReshow();
  PrepareWidget(recreateWidget);

  mPopupState = ePopupShowing;
  mAnchorContent = aAnchorContent;
  mTriggerContent = aTriggerContent;
  mXPos = aXPos;
  mYPos = aYPos;
  mIsNativeMenu = false;
  mIsTopLevelContextMenu = false;
  mVFlip = false;
  mHFlip = false;
  mConstrainedByLayout = false;
  mAlignmentOffset = 0;
  mPositionedOffset = 0;
  mPositionedByMoveToRect = false;

  mAnchorType = aAnchorType;

  // if aAttributesOverride is true, then the popupanchor, popupalign and
  // position attributes on the <menupopup> override those values passed in.
  // If false, those attributes are only used if the values passed in are empty
  if (aAnchorContent || aAnchorType == MenuPopupAnchorType_Rect) {
    nsAutoString anchor, align, position;
    mContent->AsElement()->GetAttr(nsGkAtoms::popupanchor, anchor);
    mContent->AsElement()->GetAttr(nsGkAtoms::popupalign, align);
    mContent->AsElement()->GetAttr(nsGkAtoms::position, position);

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

    mFlip = FlipFromAttribute(this);

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
  // When converted back to CSSIntRect it is (-1, -1, 0, 0) - as expected in
  // nsXULPopupManager::Rollup
  mScreenRect = nsRect(-AppUnitsPerCSSPixel(), -AppUnitsPerCSSPixel(), 0, 0);

  if (aAttributesOverride) {
    // Use |left| and |top| dimension attributes to position the popup if
    // present, as they may have been persisted.
    nsAutoString left, top;
    mContent->AsElement()->GetAttr(nsGkAtoms::left, left);
    mContent->AsElement()->GetAttr(nsGkAtoms::top, top);

    nsresult err;
    if (!left.IsEmpty()) {
      int32_t x = left.ToInteger(&err);
      if (NS_SUCCEEDED(err)) {
        mScreenRect.x = CSSPixel::ToAppUnits(x);
      }
    }
    if (!top.IsEmpty()) {
      int32_t y = top.ToInteger(&err);
      if (NS_SUCCEEDED(err)) {
        mScreenRect.y = CSSPixel::ToAppUnits(y);
      }
    }
  }
}

void nsMenuPopupFrame::InitializePopupAtScreen(nsIContent* aTriggerContent,
                                               int32_t aXPos, int32_t aYPos,
                                               bool aIsContextMenu) {
  auto* widget = GetWidget();
  bool recreateWidget = widget && widget->NeedsRecreateToReshow();
  PrepareWidget(recreateWidget);

  mPopupState = ePopupShowing;
  mAnchorContent = nullptr;
  mTriggerContent = aTriggerContent;
  mScreenRect =
      nsRect(CSSPixel::ToAppUnits(aXPos), CSSPixel::ToAppUnits(aYPos), 0, 0);
  mXPos = 0;
  mYPos = 0;
  mFlip = FlipFromAttribute(this);
  mPopupAnchor = POPUPALIGNMENT_NONE;
  mPopupAlignment = POPUPALIGNMENT_NONE;
  mPosition = POPUPPOSITION_UNKNOWN;
  mIsContextMenu = aIsContextMenu;
  mIsTopLevelContextMenu = aIsContextMenu;
  mIsNativeMenu = false;
  mAnchorType = MenuPopupAnchorType_Point;
  mPositionedOffset = 0;
  mPositionedByMoveToRect = false;
}

void nsMenuPopupFrame::InitializePopupAsNativeContextMenu(
    nsIContent* aTriggerContent, int32_t aXPos, int32_t aYPos) {
  mTriggerContent = aTriggerContent;
  mPopupState = ePopupShowing;
  mAnchorContent = nullptr;
  mScreenRect =
      nsRect(CSSPixel::ToAppUnits(aXPos), CSSPixel::ToAppUnits(aYPos), 0, 0);
  mXPos = 0;
  mYPos = 0;
  mFlip = FlipType_Default;
  mPopupAnchor = POPUPALIGNMENT_NONE;
  mPopupAlignment = POPUPALIGNMENT_NONE;
  mPosition = POPUPPOSITION_UNKNOWN;
  mIsContextMenu = true;
  mIsTopLevelContextMenu = true;
  mIsNativeMenu = true;
  mAnchorType = MenuPopupAnchorType_Point;
  mPositionedOffset = 0;
  mPositionedByMoveToRect = false;
}

void nsMenuPopupFrame::InitializePopupAtRect(nsIContent* aTriggerContent,
                                             const nsAString& aPosition,
                                             const nsIntRect& aRect,
                                             bool aAttributesOverride) {
  InitializePopup(nullptr, aTriggerContent, aPosition, 0, 0,
                  MenuPopupAnchorType_Rect, aAttributesOverride);
  mScreenRect = ToAppUnits(aRect, AppUnitsPerCSSPixel());
}

void nsMenuPopupFrame::ShowPopup(bool aIsContextMenu) {
  mIsContextMenu = aIsContextMenu;

  InvalidateFrameSubtree();

  if (mPopupState == ePopupShowing || mPopupState == ePopupPositioning) {
    mPopupState = ePopupOpening;
    mIsOpenChanged = true;

    // Clear mouse capture when a popup is opened.
    if (mPopupType == PopupType::Menu) {
      if (auto* activeESM = EventStateManager::GetActiveEventStateManager()) {
        EventStateManager::ClearGlobalActiveContent(activeESM);
      }

      PresShell::ReleaseCapturingContent();
    }

    if (RefPtr menu = PopupElement().GetContainingMenu()) {
      menu->PopupOpened();
    }

    // We skip laying out children if we're closed, so make sure that we do a
    // full dirty reflow when opening to pick up any potential change.
    PresShell()->FrameNeedsReflow(
        this, IntrinsicDirty::FrameAncestorsAndDescendants, NS_FRAME_IS_DIRTY);

    if (mPopupType == PopupType::Menu) {
      nsCOMPtr<nsISound> sound(do_GetService("@mozilla.org/sound;1"));
      if (sound) sound->PlayEventSound(nsISound::EVENT_MENU_POPUP);
    }
  }
}

void nsMenuPopupFrame::ClearTriggerContentIncludingDocument() {
  // clear the trigger content if the popup is being closed. But don't clear
  // it if the popup is just being made invisible as a popuphiding or command
  if (mTriggerContent) {
    // if the popup had a trigger node set, clear the global window popup node
    // as well
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
}

void nsMenuPopupFrame::HidePopup(bool aDeselectMenu, nsPopupState aNewState,
                                 bool aFromFrameDestruction) {
  NS_ASSERTION(aNewState == ePopupClosed || aNewState == ePopupInvisible,
               "popup being set to unexpected state");

  ClearPopupShownDispatcher();

  // don't hide the popup when it isn't open
  if (mPopupState == ePopupClosed || mPopupState == ePopupShowing ||
      mPopupState == ePopupPositioning) {
    return;
  }

  if (aNewState == ePopupClosed) {
    // clear the trigger content if the popup is being closed. But don't clear
    // it if the popup is just being made invisible as a popuphiding or command
    // event may want to retrieve it.
    ClearTriggerContentIncludingDocument();
    mAnchorContent = nullptr;
  }

  // when invisible and about to be closed, HidePopup has already been called,
  // so just set the new state to closed and return
  if (mPopupState == ePopupInvisible) {
    if (aNewState == ePopupClosed) {
      mPopupState = ePopupClosed;
    }
    return;
  }

  mPopupState = aNewState;

  mIncrementalString.Truncate();

  mIsOpenChanged = false;
  mHFlip = mVFlip = false;
  mConstrainedByLayout = false;

  if (auto* widget = GetWidget()) {
    // Ideally we should call ClearCachedWebrenderResources but there are
    // intermittent failures (see bug 1748788), so we currently call
    // ClearWebrenderAnimationResources instead.
    widget->ClearWebrenderAnimationResources();
  }

  nsView* view = GetView();
  nsViewManager* viewManager = view->GetViewManager();
  viewManager->SetViewVisibility(view, ViewVisibility::Hide);

  RefPtr popup = &PopupElement();
  // XXX, bug 137033, In Windows, if mouse is outside the window when the
  // menupopup closes, no mouse_enter/mouse_exit event will be fired to clear
  // current hover state, we should clear it manually. This code may not the
  // best solution, but we can leave it here until we find the better approach.
  if (!aFromFrameDestruction &&
      popup->State().HasState(dom::ElementState::HOVER)) {
    EventStateManager* esm = PresContext()->EventStateManager();
    esm->SetContentState(nullptr, dom::ElementState::HOVER);
  }
  popup->PopupClosed(aDeselectMenu);
}

nsPoint nsMenuPopupFrame::AdjustPositionForAnchorAlign(
    nsRect& anchorRect, const nsSize& aPrefSize, FlipStyle& aHFlip,
    FlipStyle& aVFlip) const {
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
  nsMargin margin = GetMargin();
  switch (popupAlign) {
    case POPUPALIGNMENT_LEFTCENTER:
      pnt.MoveBy(margin.left, -aPrefSize.height / 2);
      break;
    case POPUPALIGNMENT_RIGHTCENTER:
      pnt.MoveBy(-aPrefSize.width - margin.right, -aPrefSize.height / 2);
      break;
    case POPUPALIGNMENT_TOPCENTER:
      pnt.MoveBy(-aPrefSize.width / 2, margin.top);
      break;
    case POPUPALIGNMENT_BOTTOMCENTER:
      pnt.MoveBy(-aPrefSize.width / 2, -aPrefSize.height - margin.bottom);
      break;
    case POPUPALIGNMENT_TOPRIGHT:
      pnt.MoveBy(-aPrefSize.width - margin.right, margin.top);
      break;
    case POPUPALIGNMENT_BOTTOMLEFT:
      pnt.MoveBy(margin.left, -aPrefSize.height - margin.bottom);
      break;
    case POPUPALIGNMENT_BOTTOMRIGHT:
      pnt.MoveBy(-aPrefSize.width - margin.right,
                 -aPrefSize.height - margin.bottom);
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
    if (mIsOpenChanged) {
      if (nsIFrame* selectedItemFrame = GetSelectedItemForAlignment()) {
        const nscoord itemHeight = selectedItemFrame->GetRect().height;
        const nscoord itemOffset =
            selectedItemFrame->GetOffsetToIgnoringScrolling(this).y;
        // We want to line-up the anchor rect with the selected item, but if the
        // selected item is outside of our bounds, we don't want to shift the
        // popup up in a way that our box would no longer intersect with the
        // anchor.
        nscoord maxOffset = aPrefSize.height - itemHeight;
        if (const nsIScrollableFrame* sf = GetScrollFrame()) {
          // HACK: We ideally would want to use the offset from the bottom
          // bottom of our scroll-frame to the bottom of our frame (so as to
          // ensure that the bottom of the scrollport is inside the anchor
          // rect).
          //
          // But at this point of the code, the scroll frame may not be laid out
          // with a definite size (might be overflowing us).
          //
          // So, we assume the offset from the bottom is symmetric to the offset
          // from the top. This holds for all the popups where this matters
          // (menulists on macOS, effectively), and seems better than somehow
          // moving the popup after the fact as we used to do.
          const nsIFrame* f = do_QueryFrame(sf);
          maxOffset -= f->GetOffsetTo(this).y;
        }
        mPositionedOffset =
            originalAnchorRect.height + std::min(itemOffset, maxOffset);
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

nsIFrame* nsMenuPopupFrame::GetSelectedItemForAlignment() const {
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
                                        nscoord aScreenEnd,
                                        nscoord* aOffset) const {
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
                                       FlipStyle aFlip, bool aEndAligned,
                                       bool* aFlipSide) const {
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
        nscoord newScreenPoint = startpos - aSize - aMarginBegin;
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
                                           nsIFrame* aAnchorFrame) const {
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

static nsIFrame* MaybeDelegatedAnchorFrame(nsIFrame* aFrame) {
  if (!aFrame) {
    return nullptr;
  }
  if (auto* element = Element::FromNodeOrNull(aFrame->GetContent())) {
    if (element->HasAttr(nsGkAtoms::delegatesanchor)) {
      for (nsIFrame* f : aFrame->PrincipalChildList()) {
        if (!f->IsPlaceholderFrame()) {
          return f;
        }
      }
    }
  }
  return aFrame;
}

auto nsMenuPopupFrame::GetRects(const nsSize& aPrefSize) const -> Rects {
  if (NS_WARN_IF(aPrefSize == nsSize(-1, -1))) {
    // Return early if the popup hasn't been laid out yet. On Windows, this can
    // happen when using a drag popup before it opens.
    return {};
  }

  nsPresContext* pc = PresContext();
  nsIFrame* rootFrame = pc->PresShell()->GetRootFrame();
  NS_ASSERTION(rootFrame->GetView() && GetView() &&
                   rootFrame->GetView() == GetView()->GetParent(),
               "rootFrame's view is not our view's parent???");

  // Indicators of whether the popup should be flipped or resized.
  FlipStyle hFlip = FlipStyle_None, vFlip = FlipStyle_None;

  const nsMargin margin = GetMargin();

  // the screen rectangle of the root frame, in dev pixels.
  const nsRect rootScreenRect = rootFrame->GetScreenRectInAppUnits();

  const bool isNoAutoHide = IsNoAutoHide();
  const PopupLevel popupLevel = GetPopupLevel(isNoAutoHide);

  Rects result;

  // Set the popup's size to the preferred size. Below, this size will be
  // adjusted to fit on the screen or within the content area. If the anchor is
  // sized to the popup, use the anchor's width instead of the preferred width.
  result.mUsedRect = nsRect(nsPoint(), aPrefSize);

  const bool anchored = IsAnchored();
  if (anchored) {
    // In order to deal with transforms, we need the root prescontext:
    nsPresContext* rootPc = pc->GetRootPresContext();
    if (NS_WARN_IF(!rootPc)) {
      // If we can't reach a root pres context, don't bother continuing.
      return result;
    }

    result.mAnchorRect = result.mUntransformedAnchorRect = [&] {
      // If anchored to a rectangle, use that rectangle. Otherwise, determine
      // the rectangle from the anchor.
      if (mAnchorType == MenuPopupAnchorType_Rect) {
        return mScreenRect;
      }
      // if the frame is not specified, use the anchor node passed to OpenPopup.
      // If that wasn't specified either, use the root frame. Note that
      // mAnchorContent might be a different document so its presshell must be
      // used.
      nsIFrame* anchorFrame = GetAnchorFrame();
      if (!anchorFrame) {
        return rootScreenRect;
      }
      return ComputeAnchorRect(rootPc, anchorFrame);
    }();

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
      result.mUsedRect.MoveTo(AdjustPositionForAnchorAlign(
          result.mAnchorRect, aPrefSize, hFlip, vFlip));
    } else {
      // With no anchor, the popup is positioned relative to the root frame.
      result.mUsedRect.MoveTo(result.mAnchorRect.TopLeft() +
                              nsPoint(margin.left, margin.top));
    }

    // mXPos and mYPos specify an additional offset passed to OpenPopup that
    // should be added to the position.  We also add the offset to the anchor
    // pos so a later flip/resize takes the offset into account.
    // FIXME(emilio): Wayland doesn't seem to be accounting for this offset
    // anywhere, and it probably should.
    {
      nsPoint offset(CSSPixel::ToAppUnits(mXPos), CSSPixel::ToAppUnits(mYPos));
      if (IsDirectionRTL()) {
        offset.x = -offset.x;
      }
      result.mUsedRect.MoveBy(offset);
      result.mAnchorRect.MoveBy(offset);
    }
  } else {
    // Not anchored, use mScreenRect
    result.mUsedRect.MoveTo(mScreenRect.TopLeft());
    result.mAnchorRect = result.mUntransformedAnchorRect =
        nsRect(mScreenRect.TopLeft(), nsSize());

    // Right-align RTL context menus, and apply margin and offsets as per the
    // platform conventions.
    if (mIsContextMenu && IsDirectionRTL()) {
      result.mUsedRect.x -= aPrefSize.Width();
      result.mUsedRect.MoveBy(-margin.right, margin.top);
    } else {
      result.mUsedRect.MoveBy(margin.left, margin.top);
    }
#ifdef XP_MACOSX
    // OSX tooltips follow standard flip rule but other popups flip horizontally
    // not vertically
    if (mPopupType == PopupType::Tooltip) {
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

  const int32_t a2d = pc->AppUnitsPerDevPixel();

  nsView* view = GetView();
  NS_ASSERTION(view, "popup with no view");

  nsIWidget* widget = view->GetWidget();

  // If a panel has flip="none", don't constrain or flip it.
  // Also, always do this for content shells, so that the popup doesn't extend
  // outside the containing frame.
  if (mInContentShell || mFlip != FlipType_None) {
    const Maybe<nsRect> constraintRect =
        GetConstraintRect(result.mAnchorRect, rootScreenRect, popupLevel);

    if (constraintRect) {
      // Ensure that anchorRect is on the constraint rect.
      result.mAnchorRect = result.mAnchorRect.Intersect(*constraintRect);
      // Shrink the popup down if it is larger than the constraint size
      if (result.mUsedRect.width > constraintRect->width) {
        result.mUsedRect.width = constraintRect->width;
      }
      if (result.mUsedRect.height > constraintRect->height) {
        result.mUsedRect.height = constraintRect->height;
      }
      result.mConstrainedByLayout = true;
    }

    if (IS_WAYLAND_DISPLAY() && widget) {
      // Shrink the popup down if it's larger than popup size received from
      // Wayland compositor. We don't know screen size on Wayland so this is the
      // only info we have there.
      const nsSize waylandSize = LayoutDeviceIntRect::ToAppUnits(
          widget->GetMoveToRectPopupSize(), a2d);
      if (waylandSize.width > 0 && result.mUsedRect.width > waylandSize.width) {
        LOG_WAYLAND("Wayland constraint width [%p]:  %d to %d", widget,
                    result.mUsedRect.width, waylandSize.width);
        result.mUsedRect.width = waylandSize.width;
      }
      if (waylandSize.height > 0 &&
          result.mUsedRect.height > waylandSize.height) {
        LOG_WAYLAND("Wayland constraint height [%p]:  %d to %d", widget,
                    result.mUsedRect.height, waylandSize.height);
        result.mUsedRect.height = waylandSize.height;
      }
      if (RefPtr<widget::Screen> s = widget->GetWidgetScreen()) {
        const nsSize screenSize =
            LayoutDeviceIntSize::ToAppUnits(s->GetAvailRect().Size(), a2d);
        if (result.mUsedRect.height > screenSize.height) {
          LOG_WAYLAND("Wayland constraint height to screen [%p]:  %d to %d",
                      widget, result.mUsedRect.height, screenSize.height);
          result.mUsedRect.height = screenSize.height;
        }
        if (result.mUsedRect.width > screenSize.width) {
          LOG_WAYLAND("Wayland constraint widthto screen [%p]:  %d to %d",
                      widget, result.mUsedRect.width, screenSize.width);
          result.mUsedRect.width = screenSize.width;
        }
      }
    }

    // At this point the anchor (anchorRect) is within the available screen
    // area (constraintRect) and the popup is known to be no larger than the
    // screen.
    if (constraintRect) {
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
      if (slideHorizontal) {
        result.mUsedRect.width = SlideOrResize(
            result.mUsedRect.x, result.mUsedRect.width, constraintRect->x,
            constraintRect->XMost(), &result.mAlignmentOffset);
      } else {
        const bool endAligned =
            IsDirectionRTL()
                ? mPopupAlignment == POPUPALIGNMENT_TOPLEFT ||
                      mPopupAlignment == POPUPALIGNMENT_BOTTOMLEFT ||
                      mPopupAlignment == POPUPALIGNMENT_LEFTCENTER
                : mPopupAlignment == POPUPALIGNMENT_TOPRIGHT ||
                      mPopupAlignment == POPUPALIGNMENT_BOTTOMRIGHT ||
                      mPopupAlignment == POPUPALIGNMENT_RIGHTCENTER;
        result.mUsedRect.width = FlipOrResize(
            result.mUsedRect.x, result.mUsedRect.width, constraintRect->x,
            constraintRect->XMost(), result.mAnchorRect.x,
            result.mAnchorRect.XMost(), margin.left, margin.right, hFlip,
            endAligned, &result.mHFlip);
      }
      if (slideVertical) {
        result.mUsedRect.height = SlideOrResize(
            result.mUsedRect.y, result.mUsedRect.height, constraintRect->y,
            constraintRect->YMost(), &result.mAlignmentOffset);
      } else {
        bool endAligned = mPopupAlignment == POPUPALIGNMENT_BOTTOMLEFT ||
                          mPopupAlignment == POPUPALIGNMENT_BOTTOMRIGHT ||
                          mPopupAlignment == POPUPALIGNMENT_BOTTOMCENTER;
        result.mUsedRect.height = FlipOrResize(
            result.mUsedRect.y, result.mUsedRect.height, constraintRect->y,
            constraintRect->YMost(), result.mAnchorRect.y,
            result.mAnchorRect.YMost(), margin.top, margin.bottom, vFlip,
            endAligned, &result.mVFlip);
      }

#ifdef DEBUG
      NS_ASSERTION(constraintRect->Contains(result.mUsedRect),
                   "Popup is offscreen");
      if (!constraintRect->Contains(result.mUsedRect)) {
        NS_WARNING(nsPrintfCString("Popup is offscreen (%s vs. %s)",
                                   ToString(constraintRect).c_str(),
                                   ToString(result.mUsedRect).c_str())
                       .get());
      }
#endif
    }
  }
  // snap the popup's position in screen coordinates to device pixels, see
  // bug 622507, bug 961431
  result.mUsedRect.x = pc->RoundAppUnitsToNearestDevPixels(result.mUsedRect.x);
  result.mUsedRect.y = pc->RoundAppUnitsToNearestDevPixels(result.mUsedRect.y);

  // determine the x and y position of the view by subtracting the desired
  // screen position from the screen position of the root frame.
  result.mViewPoint = result.mUsedRect.TopLeft() - rootScreenRect.TopLeft();

  // Offset the position by the width and height of the borders and titlebar.
  // Even though GetClientOffset should return (0, 0) when there is no titlebar
  // or borders, we skip these calculations anyway for non-panels to save time
  // since they will never have a titlebar.
  if (mPopupType == PopupType::Panel && widget) {
    result.mClientOffset = widget->GetClientOffset();
    result.mViewPoint +=
        LayoutDeviceIntPoint::ToAppUnits(result.mClientOffset, a2d);
  }

  return result;
}

void nsMenuPopupFrame::SetPopupPosition(bool aIsMove) {
  if (aIsMove && (mPrefSize.width == -1 || mPrefSize.height == -1)) {
    return;
  }

  auto rects = GetRects(mPrefSize);
  if (rects.mUsedRect.Size() != mRect.Size()) {
    MOZ_ASSERT(!HasAnyStateBits(NS_FRAME_IN_REFLOW));
    // We need to resize on top of moving, trigger an actual reflow.
    PresShell()->FrameNeedsReflow(this, IntrinsicDirty::FrameAndAncestors,
                                  NS_FRAME_IS_DIRTY);
    return;
  }
  PerformMove(rects);
}

void nsMenuPopupFrame::PerformMove(const Rects& aRects) {
  auto* ps = PresShell();

  // We're just moving, sync frame position and offset as needed.
  ps->GetViewManager()->MoveViewTo(GetView(), aRects.mViewPoint.x,
                                   aRects.mViewPoint.y);

  // Now that we've positioned the view, sync up the frame's origin.
  nsBlockFrame::SetPosition(aRects.mViewPoint -
                            GetParent()->GetOffsetTo(ps->GetRootFrame()));

  // If the popup is in the positioned state or if it is shown and the position
  // or size changed, dispatch a popuppositioned event if the popup wants it.
  if (mPopupState == ePopupPositioning ||
      (mPopupState == ePopupShown &&
       !aRects.mUsedRect.IsEqualEdges(mUsedScreenRect)) ||
      (mPopupState == ePopupShown &&
       aRects.mAlignmentOffset != mAlignmentOffset)) {
    mUsedScreenRect = aRects.mUsedRect;
    if (!HasAnyStateBits(NS_FRAME_FIRST_REFLOW) && !mPendingPositionedEvent) {
      mPendingPositionedEvent =
          nsXULPopupPositionedEvent::DispatchIfNeeded(mContent->AsElement());
    }
  }

  if (!mPositionedByMoveToRect) {
    mUntransformedAnchorRect = aRects.mUntransformedAnchorRect;
  }

  mAlignmentOffset = aRects.mAlignmentOffset;
  mLastClientOffset = aRects.mClientOffset;
  mHFlip = aRects.mHFlip;
  mVFlip = aRects.mVFlip;
  mConstrainedByLayout = aRects.mConstrainedByLayout;

  // If this is a noautohide popup, set the screen coordinates of the popup.
  // This way, the popup stays at the location where it was opened even when the
  // window is moved. Popups at the parent level follow the parent window as it
  // is moved and remained anchored, so we want to maintain the anchoring
  // instead.
  //
  // FIXME: This suffers from issues like bug 1823552, where constraints imposed
  // by the anchor are lost, but this is super-old behavior.
  const bool fixPositionToPoint =
      IsNoAutoHide() && (GetPopupLevel() != PopupLevel::Parent ||
                         mAnchorType == MenuPopupAnchorType_Rect);
  if (fixPositionToPoint) {
    // Account for the margin that will end up being added to the screen
    // coordinate the next time SetPopupPosition is called.
    const auto& margin = GetMargin();
    mAnchorType = MenuPopupAnchorType_Point;
    mScreenRect.x = aRects.mUsedRect.x - margin.left;
    mScreenRect.y = aRects.mUsedRect.y - margin.top;
  }

  // For anchored popups that shouldn't follow the anchor, fix the original
  // anchor rect.
  if (IsAnchored() && !ShouldFollowAnchor() && !mUsedScreenRect.IsEmpty() &&
      mAnchorType != MenuPopupAnchorType_Rect) {
    mAnchorType = MenuPopupAnchorType_Rect;
    mScreenRect = aRects.mUntransformedAnchorRect;
  }

  // NOTE(emilio): This call below is kind of a workaround, but we need to do
  // this here because some position changes don't go through the
  // view system -> popup manager, like:
  //
  //   https://searchfox.org/mozilla-central/rev/477950cf9ca9c9bb5ff6f34e0d0f6ca4718ea798/widget/gtk/nsWindow.cpp#3847
  //
  // So this might be the last chance we have to set the remote browser's
  // position.
  //
  // Ultimately this probably wants to get fixed in the widget size of things,
  // but given this is worst-case a redundant DOM traversal, and that popups
  // usually don't have all that much content, this is probably an ok
  // workaround.
  WidgetPositionOrSizeDidChange();
}

void nsMenuPopupFrame::WidgetPositionOrSizeDidChange() {
  // In the case this popup has remote contents having OOP iframes, it's
  // possible that OOP iframe's nsSubDocumentFrame has been already reflowed
  // thus, we will never have a chance to tell this parent browser's position
  // update to the OOP documents without notifying it explicitly.
  if (!HasRemoteContent()) {
    return;
  }
  for (nsIContent* content = mContent->GetFirstChild(); content;
       content = content->GetNextNode(mContent)) {
    if (content->IsXULElement(nsGkAtoms::browser) &&
        content->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::remote,
                                          nsGkAtoms::_true, eIgnoreCase)) {
      if (auto* browserParent = dom::BrowserParent::GetFrom(content)) {
        browserParent->NotifyPositionUpdatedForContentsInPopup();
      }
    }
  }
}

Maybe<nsRect> nsMenuPopupFrame::GetConstraintRect(
    const nsRect& aAnchorRect, const nsRect& aRootScreenRect,
    PopupLevel aPopupLevel) const {
  const nsPresContext* pc = PresContext();
  const int32_t a2d = PresContext()->AppUnitsPerDevPixel();
  Maybe<nsRect> result;

  auto AddConstraint = [&result](const nsRect& aConstraint) {
    if (result) {
      *result = result->Intersect(aConstraint);
    } else {
      result.emplace(aConstraint);
    }
  };

  // Determine the available screen space. It will be reduced by the OS chrome
  // such as menubars. It addition, for content shells, it will be the area of
  // the content rather than the screen.
  // In Wayland we can't use the screen rect because we can't know absolute
  // window position.
  if (!IS_WAYLAND_DISPLAY()) {
    const DesktopToLayoutDeviceScale scale =
        pc->DeviceContext()->GetDesktopToDeviceScale();
    // For content shells, get the screen where the root frame is located. This
    // is because we need to constrain the content to this content area, so we
    // should use the same screen. Otherwise, use the screen where the anchor is
    // located.
    const nsRect& rect = mInContentShell ? aRootScreenRect : aAnchorRect;
    auto desktopRect = DesktopIntRect::RoundOut(
        LayoutDeviceRect::FromAppUnits(rect, a2d) / scale);
    desktopRect.width = std::max(1, desktopRect.width);
    desktopRect.height = std::max(1, desktopRect.height);

    RefPtr<nsIScreen> screen =
        widget::ScreenManager::GetSingleton().ScreenForRect(desktopRect);
    MOZ_ASSERT(screen, "We always fall back to the primary screen");
    // Non-top-level popups (which will always be panels) should never overlap
    // the OS bar.
    const bool canOverlapOSBar =
        aPopupLevel == PopupLevel::Top &&
        LookAndFeel::GetInt(LookAndFeel::IntID::MenusCanOverlapOSBar) &&
        !mInContentShell;
    // Get the total screen area if the popup is allowed to overlap it.
    const auto screenRect =
        canOverlapOSBar ? screen->GetRect() : screen->GetAvailRect();
    AddConstraint(LayoutDeviceRect::ToAppUnits(screenRect, a2d));
  }

  if (mInContentShell) {
    // For content shells, clip to the client area rather than the screen area
    AddConstraint(aRootScreenRect);
  } else if (!mOverrideConstraintRect.IsEmpty()) {
    AddConstraint(mOverrideConstraintRect);
    // This is currently only used for <select> elements where we want to
    // constrain vertically to the screen but not horizontally, so do the
    // intersection and then reset the horizontal values.
    //
    // FIXME(emilio): This doesn't make any sense to me...
    result->x = mOverrideConstraintRect.x;
    result->width = mOverrideConstraintRect.width;
  }

  // Expand the allowable screen rect by the input margin (which can't be
  // interacted with).
  if (result) {
    const nscoord inputMargin =
        StyleUIReset()->mMozWindowInputRegionMargin.ToAppUnits();
    result->Inflate(inputMargin);
  }
  return result;
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

static nsIScrollableFrame* DoGetScrollFrame(const nsIFrame* aFrame) {
  if (const nsIScrollableFrame* sf = do_QueryFrame(aFrame)) {
    return const_cast<nsIScrollableFrame*>(sf);
  }
  for (nsIFrame* childFrame : aFrame->PrincipalChildList()) {
    if (auto* sf = DoGetScrollFrame(childFrame)) {
      return sf;
    }
  }
  return nullptr;
}

// XXXroc this is megalame. Fossicking around for a frame of the right
// type is a recipe for disaster in the long term.
nsIScrollableFrame* nsMenuPopupFrame::GetScrollFrame() const {
  return DoGetScrollFrame(this);
}

void nsMenuPopupFrame::ChangeByPage(bool aIsUp) {
  // Only scroll by page within menulists.
  if (!IsMenuList()) {
    return;
  }

  nsIScrollableFrame* scrollframe = GetScrollFrame();

  RefPtr popup = &PopupElement();
  XULButtonElement* currentMenu = popup->GetActiveMenuChild();
  XULButtonElement* newMenu = nullptr;
  if (!currentMenu) {
    // If there is no current menu item, get the first item. When moving up,
    // just use this as the newMenu and leave currentMenu null so that no check
    // for a later element is performed. When moving down, set currentMenu so
    // that we look for one page down from the first item.
    newMenu = popup->GetFirstMenuItem();
    if (!aIsUp) {
      currentMenu = newMenu;
    }
  }

  if (currentMenu && currentMenu->GetPrimaryFrame()) {
    const nscoord scrollHeight =
        scrollframe ? scrollframe->GetScrollPortRect().height : mRect.height;
    const nsRect currentRect = currentMenu->GetPrimaryFrame()->GetRect();
    const XULButtonElement* startMenu = currentMenu;

    // Get the position of the current item and add or subtract one popup's
    // height to or from it.
    const nscoord targetPos = aIsUp ? currentRect.YMost() - scrollHeight
                                    : currentRect.y + scrollHeight;
    // Look for the next child which is just past the target position. This
    // child will need to be selected.
    for (; currentMenu;
         currentMenu = aIsUp ? popup->GetPrevMenuItemFrom(*currentMenu)
                             : popup->GetNextMenuItemFrom(*currentMenu)) {
      if (!currentMenu->GetPrimaryFrame()) {
        continue;
      }
      const nsRect curRect = currentMenu->GetPrimaryFrame()->GetRect();
      const nscoord curPos = aIsUp ? curRect.y : curRect.YMost();
      // If the right position was found, break out. Otherwise, look for another
      // item.
      if (aIsUp ? (curPos < targetPos) : (curPos > targetPos)) {
        if (!newMenu || newMenu == startMenu) {
          newMenu = currentMenu;
        }
        break;
      }

      // Assign this item to newMenu. This item will be selected in case we
      // don't find any more.
      newMenu = currentMenu;
    }
  }

  // Select the new menuitem.
  if (RefPtr newMenuRef = newMenu) {
    popup->SetActiveMenuChild(newMenuRef);
  }
}

dom::XULPopupElement& nsMenuPopupFrame::PopupElement() const {
  auto* popup = dom::XULPopupElement::FromNode(GetContent());
  MOZ_DIAGNOSTIC_ASSERT(popup);
  return *popup;
}

XULButtonElement* nsMenuPopupFrame::GetCurrentMenuItem() const {
  return PopupElement().GetActiveMenuChild();
}

nsIFrame* nsMenuPopupFrame::GetCurrentMenuItemFrame() const {
  auto* child = GetCurrentMenuItem();
  return child ? child->GetPrimaryFrame() : nullptr;
}

void nsMenuPopupFrame::HandleEnterKeyPress(WidgetEvent& aEvent) {
  mIncrementalString.Truncate();
  RefPtr popup = &PopupElement();
  popup->HandleEnterKeyPress(aEvent);
}

XULButtonElement* nsMenuPopupFrame::FindMenuWithShortcut(
    mozilla::dom::KeyboardEvent& aKeyEvent, bool& aDoAction) {
  uint32_t charCode = aKeyEvent.CharCode();
  uint32_t keyCode = aKeyEvent.KeyCode();

  aDoAction = false;

  // Enumerate over our list of frames.
  const bool isMenu = !IsMenuList();
  TimeStamp keyTime = aKeyEvent.WidgetEventPtr()->mTimeStamp;
  if (charCode == 0) {
    if (keyCode == dom::KeyboardEvent_Binding::DOM_VK_BACK_SPACE) {
      if (!isMenu && !mIncrementalString.IsEmpty()) {
        mIncrementalString.SetLength(mIncrementalString.Length() - 1);
        return nullptr;
      }
#ifdef XP_WIN
      if (nsCOMPtr<nsISound> sound = do_GetService("@mozilla.org/sound;1")) {
        sound->Beep();
      }
#endif  // #ifdef XP_WIN
    }
    return nullptr;
  }
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

  auto* item =
      PopupElement().FindMenuWithShortcut(incrementalString, aDoAction);
  if (item) {
    return item;
  }

  // If we don't match anything, rollback the last typing
  mIncrementalString.SetLength(mIncrementalString.Length() - 1);

  // didn't find a matching menu item
#ifdef XP_WIN
  // behavior on Windows - this item is in a menu popup off of the
  // menu bar, so beep and do nothing else
  if (isMenu) {
    if (nsCOMPtr<nsISound> sound = do_GetService("@mozilla.org/sound;1")) {
      sound->Beep();
    }
  }
#endif  // #ifdef XP_WIN

  return nullptr;
}

nsIWidget* nsMenuPopupFrame::GetWidget() const {
  return mView ? mView->GetWidget() : nullptr;
}

// helpers /////////////////////////////////////////////////////////////

nsresult nsMenuPopupFrame::AttributeChanged(int32_t aNameSpaceID,
                                            nsAtom* aAttribute,
                                            int32_t aModType)

{
  nsresult rv =
      nsBlockFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);

  if (aAttribute == nsGkAtoms::left || aAttribute == nsGkAtoms::top) {
    MoveToAttributePosition();
  }

  if (aAttribute == nsGkAtoms::remote) {
    // When the remote attribute changes, we need to create a new widget to
    // ensure that it has the correct compositor and transparency settings to
    // match the new value.
    PrepareWidget(true);
  }

  if (aAttribute == nsGkAtoms::followanchor) {
    if (nsXULPopupManager* pm = nsXULPopupManager::GetInstance()) {
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
        mContent->AsElement()->GetAttr(nsGkAtoms::label, title);
        if (!title.IsEmpty()) {
          widget->SetTitle(title);
        }
      }
    }
  } else if (aAttribute == nsGkAtoms::ignorekeys) {
    nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
    if (pm) {
      nsAutoString ignorekeys;
      mContent->AsElement()->GetAttr(nsGkAtoms::ignorekeys, ignorekeys);
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
  mContent->AsElement()->GetAttr(nsGkAtoms::left, left);
  mContent->AsElement()->GetAttr(nsGkAtoms::top, top);
  nsresult err1, err2;
  mozilla::CSSIntPoint pos(left.ToInteger(&err1), top.ToInteger(&err2));

  if (NS_SUCCEEDED(err1) && NS_SUCCEEDED(err2)) MoveTo(pos, false);

  PresShell()->FrameNeedsReflow(
      this, IntrinsicDirty::FrameAncestorsAndDescendants, NS_FRAME_IS_DIRTY);
}

void nsMenuPopupFrame::Destroy(DestroyContext& aContext) {
  // XXX: Currently we don't fire popuphidden for these popups, that seems wrong
  // but alas, also pre-existing.
  HidePopup(/* aDeselectMenu = */ false, ePopupClosed,
            /* aFromFrameDestruction = */ true);

  if (RefPtr<nsXULPopupManager> pm = nsXULPopupManager::GetInstance()) {
    pm->PopupDestroyed(this);
  }

  nsBlockFrame::Destroy(aContext);
}

nsMargin nsMenuPopupFrame::GetMargin() const {
  nsMargin margin;
  StyleMargin()->GetMargin(margin);
  if (mIsTopLevelContextMenu) {
    const CSSIntPoint offset(
        LookAndFeel::GetInt(LookAndFeel::IntID::ContextMenuOffsetHorizontal),
        LookAndFeel::GetInt(LookAndFeel::IntID::ContextMenuOffsetVertical));
    auto auOffset = CSSIntPoint::ToAppUnits(offset);
    margin.top += auOffset.y;
    margin.bottom += auOffset.y;
    margin.left += auOffset.x;
    margin.right += auOffset.x;
  }
  return margin;
}

void nsMenuPopupFrame::MoveTo(const CSSPoint& aPos, bool aUpdateAttrs,
                              bool aByMoveToRect) {
  nsIWidget* widget = GetWidget();
  nsPoint appUnitsPos = CSSPixel::ToAppUnits(aPos);

  // reposition the popup at the specified coordinates. Don't clear the anchor
  // and position, because the popup can be reset to its anchor position by
  // using (-1, -1) as coordinates.
  //
  // Subtract off the margin as it will be added to the position when
  // SetPopupPosition is called.
  {
    nsMargin margin = GetMargin();
    if (mIsContextMenu && IsDirectionRTL()) {
      appUnitsPos.x += margin.right + mRect.Width();
    } else {
      appUnitsPos.x -= margin.left;
    }
    appUnitsPos.y -= margin.top;
  }

  if (mScreenRect.TopLeft() == appUnitsPos &&
      (!widget || widget->GetClientOffset() == mLastClientOffset)) {
    return;
  }

  mPositionedByMoveToRect = aByMoveToRect;
  mScreenRect.MoveTo(appUnitsPos);
  if (mAnchorType == MenuPopupAnchorType_Rect) {
    // This ensures that the anchor width is still honored, to prevent it from
    // changing spuriously.
    mScreenRect.height = 0;
    // But we still need to make sure that our top left position ends up in
    // appUnitsPos.
    mPopupAlignment = POPUPALIGNMENT_TOPLEFT;
    mPopupAnchor = POPUPALIGNMENT_BOTTOMLEFT;
    mXPos = mYPos = 0;
  } else {
    mAnchorType = MenuPopupAnchorType_Point;
  }

  SetPopupPosition(true);

  RefPtr<Element> popup = mContent->AsElement();
  if (aUpdateAttrs &&
      (popup->HasAttr(nsGkAtoms::left) || popup->HasAttr(nsGkAtoms::top))) {
    nsAutoString left, top;
    left.AppendInt(RoundedToInt(aPos).x);
    top.AppendInt(RoundedToInt(aPos).y);
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
  SetPopupPosition(false);
}

int8_t nsMenuPopupFrame::GetAlignmentPosition() const {
  // The code below handles most cases of alignment, anchor and position values.
  // Those that are not handled just return POPUPPOSITION_UNKNOWN.

  if (mPosition == POPUPPOSITION_OVERLAP ||
      mPosition == POPUPPOSITION_AFTERPOINTER ||
      mPosition == POPUPPOSITION_SELECTION) {
    return mPosition;
  }

  int8_t position = mPosition;

  if (position == POPUPPOSITION_UNKNOWN) {
    switch (mPopupAnchor) {
      case POPUPALIGNMENT_BOTTOMRIGHT:
      case POPUPALIGNMENT_BOTTOMLEFT:
      case POPUPALIGNMENT_BOTTOMCENTER:
        position = mPopupAlignment == POPUPALIGNMENT_TOPRIGHT
                       ? POPUPPOSITION_AFTEREND
                       : POPUPPOSITION_AFTERSTART;
        break;
      case POPUPALIGNMENT_TOPRIGHT:
      case POPUPALIGNMENT_TOPLEFT:
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
 * KEEP THIS IN SYNC WITH nsIFrame::CreateView
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
  auto visibility = ViewVisibility::Hide;

  NS_ASSERTION(parentView, "no parent view");

  // Create a view
  nsView* view = viewManager->CreateView(GetRect(), parentView, visibility);
  auto zIndex = ZIndex();
  viewManager->SetViewZIndex(view, zIndex.isNothing(), zIndex.valueOr(0));
  // XXX put view last in document order until we can do better
  viewManager->InsertChild(parentView, view, nullptr, true);

  // Remember our view
  SetView(view);

  NS_FRAME_LOG(
      NS_FRAME_TRACE_CALLS,
      ("nsMenuPopupFrame::CreatePopupView: frame=%p view=%p", this, view));
}

bool nsMenuPopupFrame::ShouldFollowAnchor() const {
  if (mAnchorType != MenuPopupAnchorType_Node || !mAnchorContent) {
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

  return mPopupType == PopupType::Panel &&
         mContent->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                                            nsGkAtoms::arrow, eCaseMatters);
}

bool nsMenuPopupFrame::ShouldFollowAnchor(nsRect& aRect) {
  if (!ShouldFollowAnchor()) {
    return false;
  }

  if (nsIFrame* anchorFrame = GetAnchorFrame()) {
    if (nsPresContext* rootPresContext = PresContext()->GetRootPresContext()) {
      aRect = ComputeAnchorRect(rootPresContext, anchorFrame);
    }
  }

  return true;
}

bool nsMenuPopupFrame::IsDirectionRTL() const {
  const nsIFrame* anchor = GetAnchorFrame();
  const nsIFrame* f = anchor ? anchor : this;
  return f->StyleVisibility()->mDirection == StyleDirection::Rtl;
}

nsIFrame* nsMenuPopupFrame::GetAnchorFrame() const {
  nsIContent* anchor = mAnchorContent;
  if (!anchor) {
    return nullptr;
  }
  return MaybeDelegatedAnchorFrame(anchor->GetPrimaryFrame());
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
  nsIFrame* anchor = GetAnchorFrame();
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
      pm->HidePopup(mContent->AsElement(),
                    {HidePopupOption::DeselectMenu, HidePopupOption::Async});
    }

    return;
  }

  nsRect anchorRect = ComputeAnchorRect(rootPresContext, anchor);

  // If the rectangles are different, move the popup.
  if (!anchorRect.IsEqualEdges(aRect)) {
    aRect = anchorRect;
    SetPopupPosition(true);
  }
}
