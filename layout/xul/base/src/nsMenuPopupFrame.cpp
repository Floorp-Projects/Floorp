/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMenuPopupFrame.h"
#include "nsGkAtoms.h"
#include "nsIContent.h"
#include "nsIAtom.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "nsCSSRendering.h"
#include "nsINameSpaceManager.h"
#include "nsViewManager.h"
#include "nsWidgetsCID.h"
#include "nsMenuFrame.h"
#include "nsMenuBarFrame.h"
#include "nsPopupSetFrame.h"
#include "nsEventDispatcher.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMScreen.h"
#include "nsIPresShell.h"
#include "nsFrameManager.h"
#include "nsIDocument.h"
#include "nsRect.h"
#include "nsIComponentManager.h"
#include "nsBoxLayoutState.h"
#include "nsIScrollableFrame.h"
#include "nsGUIEvent.h"
#include "nsIRootBox.h"
#include "nsIDocShellTreeItem.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsLayoutUtils.h"
#include "nsContentUtils.h"
#include "nsCSSFrameConstructor.h"
#include "nsEventStateManager.h"
#include "nsIPopupBoxObject.h"
#include "nsPIWindowRoot.h"
#include "nsIReflowCallback.h"
#include "nsBindingManager.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIBaseWindow.h"
#include "nsISound.h"
#include "nsIScreenManager.h"
#include "nsIServiceManager.h"
#include "nsThemeConstants.h"
#include "nsDisplayList.h"
#include "mozilla/Preferences.h"
#include "mozilla/LookAndFeel.h"
#include <algorithm>

using namespace mozilla;

int8_t nsMenuPopupFrame::sDefaultLevelIsTop = -1;

// NS_NewMenuPopupFrame
//
// Wrapper for creating a new menu popup container
//
nsIFrame*
NS_NewMenuPopupFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMenuPopupFrame (aPresShell, aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsMenuPopupFrame)

NS_QUERYFRAME_HEAD(nsMenuPopupFrame)
  NS_QUERYFRAME_ENTRY(nsMenuPopupFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsBoxFrame)

//
// nsMenuPopupFrame ctor
//
nsMenuPopupFrame::nsMenuPopupFrame(nsIPresShell* aShell, nsStyleContext* aContext)
  :nsBoxFrame(aShell, aContext),
  mCurrentMenu(nullptr),
  mPrefSize(-1, -1),
  mLastClientOffset(0, 0),
  mPopupType(ePopupTypePanel),
  mPopupState(ePopupClosed),
  mPopupAlignment(POPUPALIGNMENT_NONE),
  mPopupAnchor(POPUPALIGNMENT_NONE),
  mPosition(POPUPPOSITION_UNKNOWN),
  mConsumeRollupEvent(nsIPopupBoxObject::ROLLUP_DEFAULT),
  mFlipBoth(false),
  mIsOpenChanged(false),
  mIsContextMenu(false),
  mAdjustOffsetForContextMenu(false),
  mGeneratedChildren(false),
  mMenuCanOverlapOSBar(false),
  mShouldAutoPosition(true),
  mInContentShell(true),
  mIsMenuLocked(false),
  mIsDragPopup(false),
  mHFlip(false),
  mVFlip(false)
{
  // the preference name is backwards here. True means that the 'top' level is
  // the default, and false means that the 'parent' level is the default.
  if (sDefaultLevelIsTop >= 0)
    return;
  sDefaultLevelIsTop =
    Preferences::GetBool("ui.panel.default_level_parent", false);
} // ctor


void
nsMenuPopupFrame::Init(nsIContent*      aContent,
                       nsIFrame*        aParent,
                       nsIFrame*        aPrevInFlow)
{
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
  nsIDocument* doc = aContent->OwnerDoc();
  int32_t namespaceID;
  nsCOMPtr<nsIAtom> tag = doc->BindingManager()->ResolveTag(aContent, &namespaceID);
  if (namespaceID == kNameSpaceID_XUL) {
    if (tag == nsGkAtoms::menupopup || tag == nsGkAtoms::popup)
      mPopupType = ePopupTypeMenu;
    else if (tag == nsGkAtoms::tooltip)
      mPopupType = ePopupTypeTooltip;
  }

  if (mPopupType == ePopupTypePanel &&
      aContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                            nsGkAtoms::drag, eIgnoreCase)) {
    mIsDragPopup = true;
  }

  nsCOMPtr<nsISupports> cont = PresContext()->GetContainer();
  nsCOMPtr<nsIDocShellTreeItem> dsti = do_QueryInterface(cont);
  int32_t type = -1;
  if (dsti && NS_SUCCEEDED(dsti->GetItemType(&type)) &&
      type == nsIDocShellTreeItem::typeChrome)
    mInContentShell = false;

  // To improve performance, create the widget for the popup only if it is not
  // a leaf. Leaf popups such as menus will create their widgets later when
  // the popup opens.
  if (!IsLeaf() && !ourView->HasWidget()) {
    CreateWidgetForView(ourView);
  }

  if (aContent->NodeInfo()->Equals(nsGkAtoms::tooltip, kNameSpaceID_XUL) &&
      aContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::_default,
                            nsGkAtoms::_true, eIgnoreCase)) {
    nsIRootBox* rootBox =
      nsIRootBox::GetRootBox(PresContext()->GetPresShell());
    if (rootBox) {
      rootBox->SetDefaultTooltip(aContent);
    }
  }

  AddStateBits(NS_FRAME_IN_POPUP);
}

bool
nsMenuPopupFrame::IsNoAutoHide() const
{
  // Panels with noautohide="true" don't hide when the mouse is clicked
  // outside of them, or when another application is made active. Non-autohide
  // panels cannot be used in content windows.
  return (!mInContentShell && mPopupType == ePopupTypePanel &&
           mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::noautohide,
                                 nsGkAtoms::_true, eIgnoreCase));
}

nsPopupLevel
nsMenuPopupFrame::PopupLevel(bool aIsNoAutoHide) const
{
  // The popup level is determined as follows, in this order:
  //   1. non-panels (menus and tooltips) are always topmost
  //   2. any specified level attribute
  //   3. if a titlebar attribute is set, use the 'floating' level
  //   4. if this is a noautohide panel, use the 'parent' level
  //   5. use the platform-specific default level

  // If this is not a panel, this is always a top-most popup.
  if (mPopupType != ePopupTypePanel)
    return ePopupLevelTop;

  // If the level attribute has been set, use that.
  static nsIContent::AttrValuesArray strings[] =
    {&nsGkAtoms::top, &nsGkAtoms::parent, &nsGkAtoms::floating, nullptr};
  switch (mContent->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::level,
                                    strings, eCaseMatters)) {
    case 0:
      return ePopupLevelTop;
    case 1:
      return ePopupLevelParent;
    case 2:
      return ePopupLevelFloating;
  }

  // Panels with titlebars most likely want to be floating popups.
  if (mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::titlebar))
    return ePopupLevelFloating;

  // If this panel is a noautohide panel, the default is the parent level.
  if (aIsNoAutoHide)
    return ePopupLevelParent;

  // Otherwise, the result depends on the platform.
  return sDefaultLevelIsTop ? ePopupLevelTop : ePopupLevelParent;
}

void
nsMenuPopupFrame::EnsureWidget()
{
  nsView* ourView = GetView();
  if (!ourView->HasWidget()) {
    NS_ASSERTION(!mGeneratedChildren && !GetFirstPrincipalChild(),
                 "Creating widget for MenuPopupFrame with children");
    CreateWidgetForView(ourView);
  }
}

nsresult
nsMenuPopupFrame::CreateWidgetForView(nsView* aView)
{
  // Create a widget for ourselves.
  nsWidgetInitData widgetData;
  widgetData.mWindowType = eWindowType_popup;
  widgetData.mBorderStyle = eBorderStyle_default;
  widgetData.clipSiblings = true;
  widgetData.mPopupHint = mPopupType;
  widgetData.mNoAutoHide = IsNoAutoHide();
  widgetData.mIsDragPopup = mIsDragPopup;

  nsAutoString title;
  if (mContent && widgetData.mNoAutoHide) {
    if (mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::titlebar,
                              nsGkAtoms::normal, eCaseMatters)) {
      widgetData.mBorderStyle = eBorderStyle_title;

      mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::label, title);

      if (mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::close,
                                nsGkAtoms::_true, eCaseMatters)) {
        widgetData.mBorderStyle =
          static_cast<enum nsBorderStyle>(widgetData.mBorderStyle | eBorderStyle_close);
      }
    }
  }

  nsTransparencyMode mode = nsLayoutUtils::GetFrameTransparency(this, this);
  bool viewHasTransparentContent = !mInContentShell &&
                                     (eTransparencyTransparent ==
                                      mode);
  nsIContent* parentContent = GetContent()->GetParent();
  nsIAtom *tag = nullptr;
  if (parentContent)
    tag = parentContent->Tag();
  widgetData.mSupportTranslucency = mode == eTransparencyTransparent;
  widgetData.mDropShadow = !(viewHasTransparentContent || tag == nsGkAtoms::menulist);
  widgetData.mPopupLevel = PopupLevel(widgetData.mNoAutoHide);

  // panels which have a parent level need a parent widget. This allows them to
  // always appear in front of the parent window but behind other windows that
  // should be in front of it.
  nsCOMPtr<nsIWidget> parentWidget;
  if (widgetData.mPopupLevel != ePopupLevelTop) {
    nsCOMPtr<nsISupports> cont = PresContext()->GetContainer();
    nsCOMPtr<nsIDocShellTreeItem> dsti = do_QueryInterface(cont);
    if (!dsti)
      return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
    dsti->GetTreeOwner(getter_AddRefs(treeOwner));
    if (!treeOwner) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIBaseWindow> baseWindow(do_QueryInterface(treeOwner));
    if (baseWindow)
      baseWindow->GetMainWidget(getter_AddRefs(parentWidget));
  }

  nsresult rv = aView->CreateWidgetForPopup(&widgetData, parentWidget,
                                            true, true);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsIWidget* widget = aView->GetWidget();
  widget->SetTransparencyMode(mode);
  widget->SetWindowShadowStyle(GetShadowStyle());

  // most popups don't have a title so avoid setting the title if there isn't one
  if (!title.IsEmpty()) {
    widget->SetTitle(title);
  }

  return NS_OK;
}

uint8_t
nsMenuPopupFrame::GetShadowStyle()
{
  uint8_t shadow = StyleUIReset()->mWindowShadow;
  if (shadow != NS_STYLE_WINDOW_SHADOW_DEFAULT)
    return shadow;

  switch (StyleDisplay()->mAppearance) {
    case NS_THEME_TOOLTIP:
      return NS_STYLE_WINDOW_SHADOW_TOOLTIP;
    case NS_THEME_MENUPOPUP:
      return NS_STYLE_WINDOW_SHADOW_MENU;
  }
  return NS_STYLE_WINDOW_SHADOW_DEFAULT;
}

// this class is used for dispatching popupshown events asynchronously.
class nsXULPopupShownEvent : public nsRunnable
{
public:
  nsXULPopupShownEvent(nsIContent *aPopup, nsPresContext* aPresContext)
    : mPopup(aPopup), mPresContext(aPresContext)
  {
  }

  NS_IMETHOD Run()
  {
    nsMouseEvent event(true, NS_XUL_POPUP_SHOWN, nullptr, nsMouseEvent::eReal);
    return nsEventDispatcher::Dispatch(mPopup, mPresContext, &event);                 
  }

private:
  nsCOMPtr<nsIContent> mPopup;
  nsRefPtr<nsPresContext> mPresContext;
};

NS_IMETHODIMP
nsMenuPopupFrame::SetInitialChildList(ChildListID  aListID,
                                      nsFrameList& aChildList)
{
  // unless the list is empty, indicate that children have been generated.
  if (aChildList.NotEmpty())
    mGeneratedChildren = true;
  return nsBoxFrame::SetInitialChildList(aListID, aChildList);
}

bool
nsMenuPopupFrame::IsLeaf() const
{
  if (mGeneratedChildren)
    return false;

  if (mPopupType != ePopupTypeMenu) {
    // any panel with a type attribute, such as the autocomplete popup,
    // is always generated right away.
    return !mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::type);
  }

  // menu popups generate their child frames lazily only when opened, so
  // behave like a leaf frame. However, generate child frames normally if
  // the parent menu has a sizetopopup attribute. In this case the size of
  // the parent menu is dependent on the size of the popup, so the frames
  // need to exist in order to calculate this size.
  nsIContent* parentContent = mContent->GetParent();
  return (parentContent &&
          !parentContent->HasAttr(kNameSpaceID_None, nsGkAtoms::sizetopopup));
}

void
nsMenuPopupFrame::LayoutPopup(nsBoxLayoutState& aState, nsIFrame* aParentMenu, bool aSizedToPopup)
{
  if (!mGeneratedChildren)
    return;

  SchedulePaint();

  bool shouldPosition = true;
  bool isOpen = IsOpen();
  if (!isOpen) {
    // if the popup is not open, only do layout while showing or if the menu
    // is sized to the popup
    shouldPosition = (mPopupState == ePopupShowing);
    if (!shouldPosition && !aSizedToPopup)
      return;
  }

  // if the popup has just been opened, make sure the scrolled window is at 0,0
  if (mIsOpenChanged) {
    nsIScrollableFrame *scrollframe = do_QueryFrame(GetChildBox());
    if (scrollframe) {
      nsWeakFrame weakFrame(this);
      scrollframe->ScrollTo(nsPoint(0,0), nsIScrollableFrame::INSTANT);
      if (!weakFrame.IsAlive()) {
        return;
      }
    }
  }

  // get the preferred, minimum and maximum size. If the menu is sized to the
  // popup, then the popup's width is the menu's width.
  nsSize prefSize = GetPrefSize(aState);
  nsSize minSize = GetMinSize(aState); 
  nsSize maxSize = GetMaxSize(aState);

  if (aSizedToPopup) {
    prefSize.width = aParentMenu->GetRect().width;
  }
  prefSize = BoundsCheck(minSize, prefSize, maxSize);

  // if the size changed then set the bounds to be the preferred size
  bool sizeChanged = (mPrefSize != prefSize);
  if (sizeChanged) {
    SetBounds(aState, nsRect(0, 0, prefSize.width, prefSize.height), false);
    mPrefSize = prefSize;
  }

  if (shouldPosition) {
    SetPopupPosition(aParentMenu, false);
  }

  nsRect bounds(GetRect());
  Layout(aState);

  // if the width or height changed, readjust the popup position. This is a
  // special case for tooltips where the preferred height doesn't include the
  // real height for its inline element, but does once it is laid out.
  // This is bug 228673 which doesn't have a simple fix.
  if (!aParentMenu) {
    nsSize newsize = GetSize();
    if (newsize.width > bounds.width || newsize.height > bounds.height) {
      // the size after layout was larger than the preferred size,
      // so set the preferred size accordingly
      mPrefSize = newsize;
      if (isOpen) {
        SetPopupPosition(nullptr, false);
      }
    }
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
    viewManager->ResizeView(view, rect);

    viewManager->SetViewVisibility(view, nsViewVisibility_kShow);
    mPopupState = ePopupOpenAndVisible;
    nsContainerFrame::SyncFrameViewProperties(pc, this, nullptr, view, 0);
  }

  // finally, if the popup just opened, send a popupshown event
  if (mIsOpenChanged) {
    mIsOpenChanged = false;
    nsCOMPtr<nsIRunnable> event = new nsXULPopupShownEvent(GetContent(), pc);
    NS_DispatchToCurrentThread(event);
  }
}

nsIContent*
nsMenuPopupFrame::GetTriggerContent(nsMenuPopupFrame* aMenuPopupFrame)
{
  while (aMenuPopupFrame) {
    if (aMenuPopupFrame->mTriggerContent)
      return aMenuPopupFrame->mTriggerContent;

    // check up the menu hierarchy until a popup with a trigger node is found
    nsMenuFrame* menuFrame = do_QueryFrame(aMenuPopupFrame->GetParent());
    if (!menuFrame)
      break;

    nsMenuParent* parentPopup = menuFrame->GetMenuParent();
    if (!parentPopup || !parentPopup->IsMenu())
      break;

    aMenuPopupFrame = static_cast<nsMenuPopupFrame *>(parentPopup);
  }

  return nullptr;
}

void
nsMenuPopupFrame::InitPositionFromAnchorAlign(const nsAString& aAnchor,
                                              const nsAString& aAlign)
{
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

void
nsMenuPopupFrame::InitializePopup(nsIContent* aAnchorContent,
                                  nsIContent* aTriggerContent,
                                  const nsAString& aPosition,
                                  int32_t aXPos, int32_t aYPos,
                                  bool aAttributesOverride)
{
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

  // if aAttributesOverride is true, then the popupanchor, popupalign and
  // position attributes on the <popup> override those values passed in.
  // If false, those attributes are only used if the values passed in are empty
  if (aAnchorContent) {
    nsAutoString anchor, align, position, flip;
    mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::popupanchor, anchor);
    mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::popupalign, align);
    mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::position, position);
    mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::flip, flip);

    if (aAttributesOverride) {
      // if the attributes are set, clear the offset position. Otherwise,
      // the offset is used to adjust the position from the anchor point
      if (anchor.IsEmpty() && align.IsEmpty() && position.IsEmpty())
        position.Assign(aPosition);
      else
        mXPos = mYPos = 0;
    }
    else if (!aPosition.IsEmpty()) {
      position.Assign(aPosition);
    }

    mFlipBoth = flip.EqualsLiteral("both");
    mSlide = flip.EqualsLiteral("slide");

    position.CompressWhitespace();
    int32_t spaceIdx = position.FindChar(' ');
    // if there is a space in the position, assume it is the anchor and
    // alignment as two separate tokens.
    if (spaceIdx >= 0) {
      InitPositionFromAnchorAlign(Substring(position, 0, spaceIdx), Substring(position, spaceIdx + 1));
    }
    else if (position.EqualsLiteral("before_start")) {
      mPopupAnchor = POPUPALIGNMENT_TOPLEFT;
      mPopupAlignment = POPUPALIGNMENT_BOTTOMLEFT;
      mPosition = POPUPPOSITION_BEFORESTART;
    }
    else if (position.EqualsLiteral("before_end")) {
      mPopupAnchor = POPUPALIGNMENT_TOPRIGHT;
      mPopupAlignment = POPUPALIGNMENT_BOTTOMRIGHT;
      mPosition = POPUPPOSITION_BEFOREEND;
    }
    else if (position.EqualsLiteral("after_start")) {
      mPopupAnchor = POPUPALIGNMENT_BOTTOMLEFT;
      mPopupAlignment = POPUPALIGNMENT_TOPLEFT;
      mPosition = POPUPPOSITION_AFTERSTART;
    }
    else if (position.EqualsLiteral("after_end")) {
      mPopupAnchor = POPUPALIGNMENT_BOTTOMRIGHT;
      mPopupAlignment = POPUPALIGNMENT_TOPRIGHT;
      mPosition = POPUPPOSITION_AFTEREND;
    }
    else if (position.EqualsLiteral("start_before")) {
      mPopupAnchor = POPUPALIGNMENT_TOPLEFT;
      mPopupAlignment = POPUPALIGNMENT_TOPRIGHT;
      mPosition = POPUPPOSITION_STARTBEFORE;
    }
    else if (position.EqualsLiteral("start_after")) {
      mPopupAnchor = POPUPALIGNMENT_BOTTOMLEFT;
      mPopupAlignment = POPUPALIGNMENT_BOTTOMRIGHT;
      mPosition = POPUPPOSITION_STARTAFTER;
    }
    else if (position.EqualsLiteral("end_before")) {
      mPopupAnchor = POPUPALIGNMENT_TOPRIGHT;
      mPopupAlignment = POPUPALIGNMENT_TOPLEFT;
      mPosition = POPUPPOSITION_ENDBEFORE;
    }
    else if (position.EqualsLiteral("end_after")) {
      mPopupAnchor = POPUPALIGNMENT_BOTTOMRIGHT;
      mPopupAlignment = POPUPALIGNMENT_BOTTOMLEFT;
      mPosition = POPUPPOSITION_ENDAFTER;
    }
    else if (position.EqualsLiteral("overlap")) {
      mPopupAnchor = POPUPALIGNMENT_TOPLEFT;
      mPopupAlignment = POPUPALIGNMENT_TOPLEFT;
      mPosition = POPUPPOSITION_OVERLAP;
    }
    else if (position.EqualsLiteral("after_pointer")) {
      mPopupAnchor = POPUPALIGNMENT_TOPLEFT;
      mPopupAlignment = POPUPALIGNMENT_TOPLEFT;
      mPosition = POPUPPOSITION_AFTERPOINTER;
      // XXXndeakin this is supposed to anchor vertically after, but with the
      // horizontal position as the mouse pointer.
      mYPos += 21;
    }
    else {
      InitPositionFromAnchorAlign(anchor, align);
    }
  }

  mScreenXPos = -1;
  mScreenYPos = -1;

  if (aAttributesOverride) {
    // Use |left| and |top| dimension attributes to position the popup if
    // present, as they may have been persisted. 
    nsAutoString left, top;
    mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::left, left);
    mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::top, top);

    nsresult err;
    if (!left.IsEmpty()) {
      int32_t x = left.ToInteger(&err);
      if (NS_SUCCEEDED(err))
        mScreenXPos = x;
    }
    if (!top.IsEmpty()) {
      int32_t y = top.ToInteger(&err);
      if (NS_SUCCEEDED(err))
        mScreenYPos = y;
    }
  }
}

void
nsMenuPopupFrame::InitializePopupAtScreen(nsIContent* aTriggerContent,
                                          int32_t aXPos, int32_t aYPos,
                                          bool aIsContextMenu)
{
  EnsureWidget();

  mPopupState = ePopupShowing;
  mAnchorContent = nullptr;
  mTriggerContent = aTriggerContent;
  mScreenXPos = aXPos;
  mScreenYPos = aYPos;
  mFlipBoth = false;
  mSlide = false;
  mPopupAnchor = POPUPALIGNMENT_NONE;
  mPopupAlignment = POPUPALIGNMENT_NONE;
  mIsContextMenu = aIsContextMenu;
  mAdjustOffsetForContextMenu = aIsContextMenu;
}

void
nsMenuPopupFrame::InitializePopupWithAnchorAlign(nsIContent* aAnchorContent,
                                                 nsAString& aAnchor,
                                                 nsAString& aAlign,
                                                 int32_t aXPos, int32_t aYPos)
{
  EnsureWidget();

  mPopupState = ePopupShowing;
  mAdjustOffsetForContextMenu = false;
  mFlipBoth = false;
  mSlide = false;

  // this popup opening function is provided for backwards compatibility
  // only. It accepts either coordinates or an anchor and alignment value
  // but doesn't use both together.
  if (aXPos == -1 && aYPos == -1) {
    mAnchorContent = aAnchorContent;
    mScreenXPos = -1;
    mScreenYPos = -1;
    mXPos = 0;
    mYPos = 0;
    InitPositionFromAnchorAlign(aAnchor, aAlign);
  }
  else {
    mAnchorContent = nullptr;
    mPopupAnchor = POPUPALIGNMENT_NONE;
    mPopupAlignment = POPUPALIGNMENT_NONE;
    mScreenXPos = aXPos;
    mScreenYPos = aYPos;
    mXPos = aXPos;
    mYPos = aYPos;
  }
}

void
nsMenuPopupFrame::ShowPopup(bool aIsContextMenu, bool aSelectFirstItem)
{
  mIsContextMenu = aIsContextMenu;

  InvalidateFrameSubtree();

  if (mPopupState == ePopupShowing) {
    mPopupState = ePopupOpen;
    mIsOpenChanged = true;

    nsMenuFrame* menuFrame = do_QueryFrame(GetParent());
    if (menuFrame) {
      nsWeakFrame weakFrame(this);
      menuFrame->PopupOpened();
      if (!weakFrame.IsAlive())
        return;
    }

    // do we need an actual reflow here?
    // is SetPopupPosition all that is needed?
    PresContext()->PresShell()->FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                                                 NS_FRAME_HAS_DIRTY_CHILDREN);

    if (mPopupType == ePopupTypeMenu) {
      nsCOMPtr<nsISound> sound(do_CreateInstance("@mozilla.org/sound;1"));
      if (sound)
        sound->PlayEventSound(nsISound::EVENT_MENU_POPUP);
    }
  }

  mShouldAutoPosition = true;
}

void
nsMenuPopupFrame::HidePopup(bool aDeselectMenu, nsPopupState aNewState)
{
  NS_ASSERTION(aNewState == ePopupClosed || aNewState == ePopupInvisible,
               "popup being set to unexpected state");

  // don't hide the popup when it isn't open
  if (mPopupState == ePopupClosed || mPopupState == ePopupShowing)
    return;

  // clear the trigger content if the popup is being closed. But don't clear
  // it if the popup is just being made invisible as a popuphiding or command
  // event may want to retrieve it.
  if (aNewState == ePopupClosed) {
    // if the popup had a trigger node set, clear the global window popup node
    // as well
    if (mTriggerContent) {
      nsIDocument* doc = mContent->GetCurrentDoc();
      if (doc) {
        nsPIDOMWindow* win = doc->GetWindow();
        if (win) {
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
    if (aNewState == ePopupClosed)
      mPopupState = ePopupClosed;
    return;
  }

  mPopupState = aNewState;

  if (IsMenu())
    SetCurrentMenuItem(nullptr);

  mIncrementalString.Truncate();

  LockMenuUntilClosed(false);

  mIsOpenChanged = false;
  mCurrentMenu = nullptr; // make sure no current menu is set
  mHFlip = mVFlip = false;

  nsView* view = GetView();
  nsViewManager* viewManager = view->GetViewManager();
  viewManager->SetViewVisibility(view, nsViewVisibility_kHide);

  FireDOMEvent(NS_LITERAL_STRING("DOMMenuInactive"), mContent);

  // XXX, bug 137033, In Windows, if mouse is outside the window when the menupopup closes, no
  // mouse_enter/mouse_exit event will be fired to clear current hover state, we should clear it manually.
  // This code may not the best solution, but we can leave it here until we find the better approach.
  NS_ASSERTION(mContent->IsElement(), "How do we have a non-element?");
  nsEventStates state = mContent->AsElement()->State();

  if (state.HasState(NS_EVENT_STATE_HOVER)) {
    nsEventStateManager *esm = PresContext()->EventStateManager();
    esm->SetContentState(nullptr, NS_EVENT_STATE_HOVER);
  }

  nsMenuFrame* menuFrame = do_QueryFrame(GetParent());
  if (menuFrame) {
    menuFrame->PopupClosed(aDeselectMenu);
  }
}

void
nsMenuPopupFrame::GetLayoutFlags(uint32_t& aFlags)
{
  aFlags = NS_FRAME_NO_SIZE_VIEW | NS_FRAME_NO_MOVE_VIEW | NS_FRAME_NO_VISIBILITY;
}

///////////////////////////////////////////////////////////////////////////////
// GetRootViewForPopup
//   Retrieves the view for the popup widget that contains the given frame. 
//   If the given frame is not contained by a popup widget, return the
//   root view of the root viewmanager.
nsView*
nsMenuPopupFrame::GetRootViewForPopup(nsIFrame* aStartFrame)
{
  nsView* view = aStartFrame->GetClosestView();
  NS_ASSERTION(view, "frame must have a closest view!");
  while (view) {
    // Walk up the view hierarchy looking for a view whose widget has a 
    // window type of eWindowType_popup - in other words a popup window
    // widget. If we find one, this is the view we want. 
    nsIWidget* widget = view->GetWidget();
    if (widget) {
      nsWindowType wtype;
      widget->GetWindowType(wtype);
      if (wtype == eWindowType_popup) {
        return view;
      }
    }

    nsView* temp = view->GetParent();
    if (!temp) {
      // Otherwise, we've walked all the way up to the root view and not
      // found a view for a popup window widget. Just return the root view.
      return view;
    }
    view = temp;
  }

  return nullptr;
}

nsPoint
nsMenuPopupFrame::AdjustPositionForAnchorAlign(nsRect& anchorRect,
                                               FlipStyle& aHFlip, FlipStyle& aVFlip)
{
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
    default:
    {
      FlipStyle anchorEdge = mFlipBoth ? FlipStyle_Inside : FlipStyle_None;
      aHFlip = (popupAnchor == -popupAlign) ? FlipStyle_Outside : anchorEdge;
      if (((popupAnchor > 0) == (popupAlign > 0)) ||
          (popupAnchor == POPUPALIGNMENT_TOPLEFT && popupAlign == POPUPALIGNMENT_TOPLEFT))
        aVFlip = FlipStyle_Outside;
      else
        aVFlip = anchorEdge;
      break;
    }
  }

  return pnt;
}

nscoord
nsMenuPopupFrame::SlideOrResize(nscoord& aScreenPoint, nscoord aSize,
                               nscoord aScreenBegin, nscoord aScreenEnd,
                               nscoord *aOffset)
{
  // The popup may be positioned such that either the left/top or bottom/right
  // is outside the screen - but never both.
  if (aScreenPoint < aScreenBegin) {
    *aOffset = aScreenBegin - aScreenPoint;
    aScreenPoint = aScreenBegin;
  } else if (aScreenPoint + aSize > aScreenEnd) {
    *aOffset = aScreenPoint + aSize - aScreenEnd;
    aScreenPoint = std::max(aScreenEnd - aSize, 0);
  }
  return std::min(aSize, aScreenEnd - aScreenPoint);
}

nscoord
nsMenuPopupFrame::FlipOrResize(nscoord& aScreenPoint, nscoord aSize, 
                               nscoord aScreenBegin, nscoord aScreenEnd,
                               nscoord aAnchorBegin, nscoord aAnchorEnd,
                               nscoord aMarginBegin, nscoord aMarginEnd,
                               nscoord aOffsetForContextMenu, FlipStyle aFlip,
                               bool* aFlipSide)
{
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
      }
      else {
        // If the newly calculated position is different than the existing
        // position, flip such that the popup is to the right or bottom of the
        // anchor point instead . However, when flipping use the same margin
        // size.
        nscoord newScreenPoint = endpos + aMarginEnd;
        if (newScreenPoint != aScreenPoint) {
          *aFlipSide = true;
          aScreenPoint = newScreenPoint;
          // check if the new position is still off the right or bottom edge of
          // the screen. If so, resize the popup.
          if (aScreenPoint + aSize > aScreenEnd) {
            popupSize = aScreenEnd - aScreenPoint;
          }
        }
      }
    }
    else {
      aScreenPoint = aScreenBegin;
    }
  }
  else if (aScreenPoint + aSize > aScreenEnd) {
    // at its current position, the popup would extend past the right or
    // bottom edge of the screen, so it will have to be moved or resized.
    if (aFlip) {
      // for inside flips, we flip on the opposite side of the anchor
      nscoord startpos = aFlip == FlipStyle_Outside ? aAnchorBegin : aAnchorEnd;
      nscoord endpos = aFlip == FlipStyle_Outside ? aAnchorEnd : aAnchorBegin;

      // check whether there is more room to the left and right (or top and
      // bottom) of the anchor and put the popup on the side with more room.
      if (aScreenEnd - endpos >= startpos - aScreenBegin) {
        if (mIsContextMenu) {
          aScreenPoint = aScreenEnd - aSize;
        }
        else {
          aScreenPoint = endpos + aMarginBegin;
          popupSize = aScreenEnd - aScreenPoint;
        }
      }
      else {
        // if the newly calculated position is different than the existing
        // position, we flip such that the popup is to the left or top of the
        // anchor point instead.
        nscoord newScreenPoint = startpos - aSize - aMarginBegin - aOffsetForContextMenu;
        if (newScreenPoint != aScreenPoint) {
          *aFlipSide = true;
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
    }
    else {
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

nsresult
nsMenuPopupFrame::SetPopupPosition(nsIFrame* aAnchorFrame, bool aIsMove)
{
  if (!mShouldAutoPosition)
    return NS_OK;

  // If this is due to a move, return early if the popup hasn't been laid out
  // yet. On Windows, this can happen when using a drag popup before it opens.
  if (aIsMove && (mPrefSize.width == -1 || mPrefSize.height == -1)) {
    return NS_OK;
  }

  nsPresContext* presContext = PresContext();
  nsIFrame* rootFrame = presContext->PresShell()->FrameManager()->GetRootFrame();
  NS_ASSERTION(rootFrame->GetView() && GetView() &&
               rootFrame->GetView() == GetView()->GetParent(),
               "rootFrame's view is not our view's parent???");

  // if the frame is not specified, use the anchor node passed to OpenPopup. If
  // that wasn't specified either, use the root frame. Note that mAnchorContent
  // might be a different document so its presshell must be used.
  if (!aAnchorFrame) {
    if (mAnchorContent) {
      aAnchorFrame = mAnchorContent->GetPrimaryFrame();
    }

    if (!aAnchorFrame) {
      aAnchorFrame = rootFrame;
      if (!aAnchorFrame)
        return NS_OK;
    }
  }

  bool sizedToPopup = false;
  if (aAnchorFrame->GetContent()) {
    // the popup should be the same size as the anchor menu, for example, a menulist.
    sizedToPopup = nsMenuFrame::IsSizedToPopup(aAnchorFrame->GetContent(), false);
  }

  // the dimensions of the anchor in its app units
  nsRect parentRect = aAnchorFrame->GetScreenRectInAppUnits();

  // the anchor may be in a different document with a different scale,
  // so adjust the size so that it is in the app units of the popup instead
  // of the anchor.
  parentRect = parentRect.ConvertAppUnitsRoundOut(
    aAnchorFrame->PresContext()->AppUnitsPerDevPixel(),
    presContext->AppUnitsPerDevPixel());

  // Set the popup's size to the preferred size. Below, this size will be
  // adjusted to fit on the screen or within the content area. If the anchor
  // is sized to the popup, use the anchor's width instead of the preferred
  // width. The preferred size should already be set by the parent frame.
  NS_ASSERTION(mPrefSize.width >= 0 || mPrefSize.height >= 0,
               "preferred size of popup not set");
  mRect.width = sizedToPopup ? parentRect.width : mPrefSize.width;
  mRect.height = mPrefSize.height;

  // the screen position in app units where the popup should appear
  nsPoint screenPoint;

  // For anchored popups, the anchor rectangle. For non-anchored popups, the
  // size will be 0.
  nsRect anchorRect = parentRect;

  // indicators of whether the popup should be flipped or resized.
  FlipStyle hFlip = FlipStyle_None, vFlip = FlipStyle_None;

  nsMargin margin(0, 0, 0, 0);
  StyleMargin()->GetMargin(margin);

  // the screen rectangle of the root frame, in dev pixels.
  nsRect rootScreenRect = rootFrame->GetScreenRectInAppUnits();

  nsDeviceContext* devContext = presContext->DeviceContext();
  nscoord offsetForContextMenu = 0;

  if (IsAnchored()) {
    // if we are anchored, there are certain things we don't want to do when
    // repositioning the popup to fit on the screen, such as end up positioned
    // over the anchor, for instance a popup appearing over the menu label.
    // When doing this reposition, we want to move the popup to the side with
    // the most room. The combination of anchor and alignment dictate if we 
    // readjust above/below or to the left/right.
    if (mAnchorContent) {
      // move the popup according to the anchor and alignment. This will also
      // tell us which axis the popup is flush against in case we have to move
      // it around later. The AdjustPositionForAnchorAlign method accounts for
      // the popup's margin.
      screenPoint = AdjustPositionForAnchorAlign(anchorRect, hFlip, vFlip);
    }
    else {
      // with no anchor, the popup is positioned relative to the root frame
      anchorRect = rootScreenRect;
      screenPoint = anchorRect.TopLeft() + nsPoint(margin.left, margin.top);
    }

    // mXPos and mYPos specify an additonal offset passed to OpenPopup that
    // should be added to the position.  We also add the offset to the anchor
    // pos so a later flip/resize takes the offset into account.
    nscoord anchorXOffset = presContext->CSSPixelsToAppUnits(mXPos);
    if (IsDirectionRTL()) {
      screenPoint.x -= anchorXOffset;
      anchorRect.x -= anchorXOffset;
    } else {
      screenPoint.x += anchorXOffset;
      anchorRect.x += anchorXOffset;
    }
    nscoord anchorYOffset = presContext->CSSPixelsToAppUnits(mYPos);
    screenPoint.y += anchorYOffset;
    anchorRect.y += anchorYOffset;

    // If this is a noautohide popup, set the screen coordinates of the popup.
    // This way, the popup stays at the location where it was opened even when
    // the window is moved. Popups at the parent level follow the parent
    // window as it is moved and remained anchored, so we want to maintain the
    // anchoring instead.
    if (IsNoAutoHide() && PopupLevel(true) != ePopupLevelParent) {
      // Account for the margin that will end up being added to the screen coordinate
      // the next time SetPopupPosition is called.
      mScreenXPos = presContext->AppUnitsToIntCSSPixels(screenPoint.x - margin.left);
      mScreenYPos = presContext->AppUnitsToIntCSSPixels(screenPoint.y - margin.top);
    }
  }
  else {
    // the popup is positioned at a screen coordinate.
    // first convert the screen position in mScreenXPos and mScreenYPos from
    // CSS pixels into device pixels, ignoring any scaling as mScreenXPos and
    // mScreenYPos are unscaled screen coordinates.
    int32_t factor = devContext->UnscaledAppUnitsPerDevPixel();

    // context menus should be offset by two pixels so that they don't appear
    // directly where the cursor is. Otherwise, it is too easy to have the
    // context menu close up again.
    if (mAdjustOffsetForContextMenu) {
      int32_t offsetForContextMenuDev =
        nsPresContext::CSSPixelsToAppUnits(CONTEXT_MENU_OFFSET_PIXELS) / factor;
      offsetForContextMenu = presContext->DevPixelsToAppUnits(offsetForContextMenuDev);
    }

    // next, convert into app units accounting for the scaling
    screenPoint.x = presContext->DevPixelsToAppUnits(
                      nsPresContext::CSSPixelsToAppUnits(mScreenXPos) / factor);
    screenPoint.y = presContext->DevPixelsToAppUnits(
                      nsPresContext::CSSPixelsToAppUnits(mScreenYPos) / factor);
    anchorRect = nsRect(screenPoint, nsSize(0, 0));

    // add the margins on the popup
    screenPoint.MoveBy(margin.left + offsetForContextMenu,
                       margin.top + offsetForContextMenu);

    // screen positioned popups can be flipped vertically but never horizontally
    vFlip = FlipStyle_Outside;
  }

  // If a panel is being moved, don't constrain or flip it. But always do this for
  // content shells, so that the popup doesn't extend outside the containing frame.
  if (mInContentShell || !aIsMove || mPopupType != ePopupTypePanel) {
    nsRect screenRect = GetConstraintRect(anchorRect, rootScreenRect);

    // ensure that anchorRect is on screen
    if (!anchorRect.IntersectRect(anchorRect, screenRect)) {
      anchorRect.width = anchorRect.height = 0;
      // if the anchor isn't within the screen, move it to the edge of the screen.
      if (anchorRect.x < screenRect.x)
        anchorRect.x = screenRect.x;
      if (anchorRect.XMost() > screenRect.XMost())
        anchorRect.x = screenRect.XMost();
      if (anchorRect.y < screenRect.y)
        anchorRect.y = screenRect.y;
      if (anchorRect.YMost() > screenRect.YMost())
        anchorRect.y = screenRect.YMost();
    }

    // shrink the the popup down if it is larger than the screen size
    if (mRect.width > screenRect.width)
      mRect.width = screenRect.width;
    if (mRect.height > screenRect.height)
      mRect.height = screenRect.height;

    // at this point the anchor (anchorRect) is within the available screen
    // area (screenRect) and the popup is known to be no larger than the screen.

    // We might want to "slide" an arrow if the panel is of the correct type -
    // but we can only slide on one axis - the other axis must be "flipped or
    // resized" as normal.
    bool slideHorizontal = mSlide && mPosition <= POPUPPOSITION_AFTEREND;
    bool slideVertical = mSlide && mPosition >= POPUPPOSITION_STARTBEFORE;

    // Next, check if there is enough space to show the popup at full size when
    // positioned at screenPoint. If not, flip the popups to the opposite side
    // of their anchor point, or resize them as necessary.
    if (slideHorizontal) {
      mRect.width = SlideOrResize(screenPoint.x, mRect.width, screenRect.x,
                                  screenRect.XMost(), &mAlignmentOffset);
    } else {
      mRect.width = FlipOrResize(screenPoint.x, mRect.width, screenRect.x,
                                 screenRect.XMost(), anchorRect.x, anchorRect.XMost(),
                                 margin.left, margin.right, offsetForContextMenu, hFlip,
                                 &mHFlip);
    }
    if (slideVertical) {
      mRect.height = SlideOrResize(screenPoint.y, mRect.height, screenRect.y,
                                  screenRect.YMost(), &mAlignmentOffset);
    } else {
      mRect.height = FlipOrResize(screenPoint.y, mRect.height, screenRect.y,
                                  screenRect.YMost(), anchorRect.y, anchorRect.YMost(),
                                  margin.top, margin.bottom, offsetForContextMenu, vFlip,
                                  &mVFlip);
    }

    NS_ASSERTION(screenPoint.x >= screenRect.x && screenPoint.y >= screenRect.y &&
                 screenPoint.x + mRect.width <= screenRect.XMost() &&
                 screenPoint.y + mRect.height <= screenRect.YMost(),
                 "Popup is offscreen");
  }

  // determine the x and y position of the view by subtracting the desired
  // screen position from the screen position of the root frame.
  nsPoint viewPoint = screenPoint - rootScreenRect.TopLeft();

  // snap the view's position to device pixels, see bug 622507
  viewPoint.x = presContext->RoundAppUnitsToNearestDevPixels(viewPoint.x);
  viewPoint.y = presContext->RoundAppUnitsToNearestDevPixels(viewPoint.y);

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

  presContext->GetPresShell()->GetViewManager()->
    MoveViewTo(view, viewPoint.x, viewPoint.y);

  // Now that we've positioned the view, sync up the frame's origin.
  nsBoxFrame::SetPosition(viewPoint - GetParent()->GetOffsetTo(rootFrame));

  if (sizedToPopup) {
    nsBoxLayoutState state(PresContext());
    // XXXndeakin can parentSize.width still extend outside?
    SetBounds(state, nsRect(mRect.x, mRect.y, parentRect.width, mRect.height));
  }

  return NS_OK;
}

/* virtual */ nsMenuFrame*
nsMenuPopupFrame::GetCurrentMenuItem()
{
  return mCurrentMenu;
}

nsRect
nsMenuPopupFrame::GetConstraintRect(const nsRect& aAnchorRect,
                                    const nsRect& aRootScreenRect)
{
  nsIntRect screenRectPixels;
  nsPresContext* presContext = PresContext();

  // determine the available screen space. It will be reduced by the OS chrome
  // such as menubars. It addition, for content shells, it will be the area of
  // the content rather than the screen.
  nsCOMPtr<nsIScreen> screen;
  nsCOMPtr<nsIScreenManager> sm(do_GetService("@mozilla.org/gfx/screenmanager;1"));
  if (sm) {
    // for content shells, get the screen where the root frame is located.
    // This is because we need to constrain the content to this content area,
    // so we should use the same screen. Otherwise, use the screen where the
    // anchor is located.
    nsRect rect = mInContentShell ? aRootScreenRect : aAnchorRect;
    // nsIScreenManager::ScreenForRect wants the coordinates in CSS pixels
    int32_t width = std::max(1, nsPresContext::AppUnitsToIntCSSPixels(rect.width));
    int32_t height = std::max(1, nsPresContext::AppUnitsToIntCSSPixels(rect.height));
    sm->ScreenForRect(nsPresContext::AppUnitsToIntCSSPixels(rect.x),
                      nsPresContext::AppUnitsToIntCSSPixels(rect.y),
                      width, height, getter_AddRefs(screen));
    if (screen) {
      // get the total screen area if the popup is allowed to overlap it.
      if (mMenuCanOverlapOSBar && !mInContentShell)
        screen->GetRect(&screenRectPixels.x, &screenRectPixels.y,
                        &screenRectPixels.width, &screenRectPixels.height);
      else
        screen->GetAvailRect(&screenRectPixels.x, &screenRectPixels.y,
                             &screenRectPixels.width, &screenRectPixels.height);
    }
  }

  // keep a 3 pixel margin to the right and bottom of the screen for the WinXP dropshadow
  screenRectPixels.SizeTo(screenRectPixels.width - 3, screenRectPixels.height - 3);

  nsRect screenRect = screenRectPixels.ToAppUnits(presContext->AppUnitsPerDevPixel());
  if (mInContentShell) {
    // for content shells, clip to the client area rather than the screen area
    screenRect.IntersectRect(screenRect, aRootScreenRect);
  }

  return screenRect;
}

void nsMenuPopupFrame::CanAdjustEdges(int8_t aHorizontalSide, int8_t aVerticalSide, nsIntPoint& aChange)
{
  int8_t popupAlign(mPopupAlignment);
  if (IsDirectionRTL()) {
    popupAlign = -popupAlign;
  }

  if (aHorizontalSide == (mHFlip ? NS_SIDE_RIGHT : NS_SIDE_LEFT)) {
    if (popupAlign == POPUPALIGNMENT_TOPLEFT || popupAlign == POPUPALIGNMENT_BOTTOMLEFT) {
      aChange.x = 0;
    }
  }
  else if (aHorizontalSide == (mHFlip ? NS_SIDE_LEFT : NS_SIDE_RIGHT)) {
    if (popupAlign == POPUPALIGNMENT_TOPRIGHT || popupAlign == POPUPALIGNMENT_BOTTOMRIGHT) {
      aChange.x = 0;
    }
  }

  if (aVerticalSide == (mVFlip ? NS_SIDE_BOTTOM : NS_SIDE_TOP)) {
    if (popupAlign == POPUPALIGNMENT_TOPLEFT || popupAlign == POPUPALIGNMENT_TOPRIGHT) {
      aChange.y = 0;
    }
  }
  else if (aVerticalSide == (mVFlip ? NS_SIDE_TOP : NS_SIDE_BOTTOM)) {
    if (popupAlign == POPUPALIGNMENT_BOTTOMLEFT || popupAlign == POPUPALIGNMENT_BOTTOMRIGHT) {
      aChange.y = 0;
    }
  }
}

bool nsMenuPopupFrame::ConsumeOutsideClicks()
{
  // If the popup has explicitly set a consume mode, honor that.
  if (mConsumeRollupEvent != nsIPopupBoxObject::ROLLUP_DEFAULT)
    return (mConsumeRollupEvent == nsIPopupBoxObject::ROLLUP_CONSUME);

  if (mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::consumeoutsideclicks,
                            nsGkAtoms::_true, eCaseMatters))
    return true;
  if (mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::consumeoutsideclicks,
                            nsGkAtoms::_false, eCaseMatters))
    return false;

  nsCOMPtr<nsIContent> parentContent = mContent->GetParent();
  if (parentContent) {
    nsINodeInfo *ni = parentContent->NodeInfo();
    if (ni->Equals(nsGkAtoms::menulist, kNameSpaceID_XUL))
      return true;  // Consume outside clicks for combo boxes on all platforms
#if defined(XP_WIN) || defined(XP_OS2)
    // Don't consume outside clicks for menus in Windows
    if (ni->Equals(nsGkAtoms::menu, kNameSpaceID_XUL) ||
        ni->Equals(nsGkAtoms::splitmenu, kNameSpaceID_XUL) ||
        ni->Equals(nsGkAtoms::popupset, kNameSpaceID_XUL) ||
        ((ni->Equals(nsGkAtoms::button, kNameSpaceID_XUL) ||
          ni->Equals(nsGkAtoms::toolbarbutton, kNameSpaceID_XUL)) &&
         (parentContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                                     nsGkAtoms::menu, eCaseMatters) ||
          parentContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                                     nsGkAtoms::menuButton, eCaseMatters)))) {
      return false;
    }
#endif
    if (ni->Equals(nsGkAtoms::textbox, kNameSpaceID_XUL)) {
      // Don't consume outside clicks for autocomplete widget
      if (parentContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                                     nsGkAtoms::autocomplete, eCaseMatters))
        return false;
    }
  }

  return true;
}

// XXXroc this is megalame. Fossicking around for a frame of the right
// type is a recipe for disaster in the long term.
nsIScrollableFrame* nsMenuPopupFrame::GetScrollFrame(nsIFrame* aStart)
{
  if (!aStart)
    return nullptr;  

  // try start frame and siblings
  nsIFrame* currFrame = aStart;
  do {
    nsIScrollableFrame* sf = do_QueryFrame(currFrame);
    if (sf)
      return sf;
    currFrame = currFrame->GetNextSibling();
  } while (currFrame);

  // try children
  currFrame = aStart;
  do {
    nsIFrame* childFrame = currFrame->GetFirstPrincipalChild();
    nsIScrollableFrame* sf = GetScrollFrame(childFrame);
    if (sf)
      return sf;
    currFrame = currFrame->GetNextSibling();
  } while (currFrame);

  return nullptr;
}

void nsMenuPopupFrame::EnsureMenuItemIsVisible(nsMenuFrame* aMenuItem)
{
  if (aMenuItem) {
    aMenuItem->PresContext()->PresShell()->ScrollFrameRectIntoView(
      aMenuItem,
      nsRect(nsPoint(0,0), aMenuItem->GetRect().Size()),
      nsIPresShell::ScrollAxis(),
      nsIPresShell::ScrollAxis(),
      nsIPresShell::SCROLL_OVERFLOW_HIDDEN |
      nsIPresShell::SCROLL_FIRST_ANCESTOR_ONLY);
  }
}

NS_IMETHODIMP nsMenuPopupFrame::SetCurrentMenuItem(nsMenuFrame* aMenuItem)
{
  if (mCurrentMenu == aMenuItem)
    return NS_OK;

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

void
nsMenuPopupFrame::CurrentMenuIsBeingDestroyed()
{
  mCurrentMenu = nullptr;
}

NS_IMETHODIMP
nsMenuPopupFrame::ChangeMenuItem(nsMenuFrame* aMenuItem,
                                 bool aSelectFirstItem)
{
  if (mCurrentMenu == aMenuItem)
    return NS_OK;

  // When a context menu is open, the current menu is locked, and no change
  // to the menu is allowed.
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (!mIsContextMenu && pm && pm->HasContextMenu(this))
    return NS_OK;

  // Unset the current child.
  if (mCurrentMenu) {
    mCurrentMenu->SelectMenu(false);
    nsMenuPopupFrame* popup = mCurrentMenu->GetPopup();
    if (popup) {
      if (mCurrentMenu->IsOpen()) {
        if (pm)
          pm->HidePopupAfterDelay(popup);
      }
    }
  }

  // Set the new child.
  if (aMenuItem) {
    EnsureMenuItemIsVisible(aMenuItem);
    aMenuItem->SelectMenu(true);
  }

  mCurrentMenu = aMenuItem;

  return NS_OK;
}

nsMenuFrame*
nsMenuPopupFrame::Enter(nsGUIEvent* aEvent)
{
  mIncrementalString.Truncate();

  // Give it to the child.
  if (mCurrentMenu)
    return mCurrentMenu->Enter(aEvent);

  return nullptr;
}

nsMenuFrame*
nsMenuPopupFrame::FindMenuWithShortcut(nsIDOMKeyEvent* aKeyEvent, bool& doAction)
{
  uint32_t charCode, keyCode;
  aKeyEvent->GetCharCode(&charCode);
  aKeyEvent->GetKeyCode(&keyCode);

  doAction = false;

  // Enumerate over our list of frames.
  nsIFrame* immediateParent = nullptr;
  PresContext()->PresShell()->
    FrameConstructor()->GetInsertionPoint(this, nullptr, &immediateParent);
  if (!immediateParent)
    immediateParent = this;

  uint32_t matchCount = 0, matchShortcutCount = 0;
  bool foundActive = false;
  bool isShortcut;
  nsMenuFrame* frameBefore = nullptr;
  nsMenuFrame* frameAfter = nullptr;
  nsMenuFrame* frameShortcut = nullptr;

  nsIContent* parentContent = mContent->GetParent();

  bool isMenu = parentContent &&
                  !parentContent->NodeInfo()->Equals(nsGkAtoms::menulist, kNameSpaceID_XUL);

  static DOMTimeStamp lastKeyTime = 0;
  DOMTimeStamp keyTime;
  aKeyEvent->GetTimeStamp(&keyTime);

  if (charCode == 0) {
    if (keyCode == NS_VK_BACK) {
      if (!isMenu && !mIncrementalString.IsEmpty()) {
        mIncrementalString.SetLength(mIncrementalString.Length() - 1);
        return nullptr;
      }
      else {
#ifdef XP_WIN
        nsCOMPtr<nsISound> soundInterface = do_CreateInstance("@mozilla.org/sound;1");
        if (soundInterface)
          soundInterface->Beep();
#endif  // #ifdef XP_WIN
      }
    }
    return nullptr;
  }
  else {
    PRUnichar uniChar = ToLowerCase(static_cast<PRUnichar>(charCode));
    if (isMenu || // Menu supports only first-letter navigation
        keyTime - lastKeyTime > INC_TYP_INTERVAL) // Interval too long, treat as new typing
      mIncrementalString = uniChar;
    else {
      mIncrementalString.Append(uniChar);
    }
  }

  // See bug 188199 & 192346, if all letters in incremental string are same, just try to match the first one
  nsAutoString incrementalString(mIncrementalString);
  uint32_t charIndex = 1, stringLength = incrementalString.Length();
  while (charIndex < stringLength && incrementalString[charIndex] == incrementalString[charIndex - 1]) {
    charIndex++;
  }
  if (charIndex == stringLength) {
    incrementalString.Truncate(1);
    stringLength = 1;
  }

  lastKeyTime = keyTime;

  nsIFrame* currFrame;
  // NOTE: If you crashed here due to a bogus |immediateParent| it is 
  //       possible that the menu whose shortcut is being looked up has 
  //       been destroyed already.  One strategy would be to 
  //       setTimeout(<func>,0) as detailed in:
  //       <http://bugzilla.mozilla.org/show_bug.cgi?id=126675#c32>
  currFrame = immediateParent->GetFirstPrincipalChild();

  int32_t menuAccessKey = -1;
  nsMenuBarListener::GetMenuAccessKey(&menuAccessKey);

  // We start searching from first child. This process is divided into two parts
  //   -- before current and after current -- by the current item
  while (currFrame) {
    nsIContent* current = currFrame->GetContent();
    
    // See if it's a menu item.
    if (nsXULPopupManager::IsValidMenuItem(PresContext(), current, true)) {
      nsAutoString textKey;
      if (menuAccessKey >= 0) {
        // Get the shortcut attribute.
        current->GetAttr(kNameSpaceID_None, nsGkAtoms::accesskey, textKey);
      }
      if (textKey.IsEmpty()) { // No shortcut, try first letter
        isShortcut = false;
        current->GetAttr(kNameSpaceID_None, nsGkAtoms::label, textKey);
        if (textKey.IsEmpty()) // No label, try another attribute (value)
          current->GetAttr(kNameSpaceID_None, nsGkAtoms::value, textKey);
      }
      else
        isShortcut = true;

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
            // Record the matched item. If there is only one matched shortcut item, do it
            frameShortcut = menu;
          }
          if (!foundActive) {
            // It's a first candidate item located before/on the current item
            if (!frameBefore)
              frameBefore = menu;
          }
          else {
            // It's a first candidate item located after the current item
            if (!frameAfter)
              frameAfter = menu;
          }
        }
        else
          return nullptr;
      }

      // Get the active status
      if (current->AttrValueIs(kNameSpaceID_None, nsGkAtoms::menuactive,
                               nsGkAtoms::_true, eCaseMatters)) {
        foundActive = true;
        if (stringLength > 1) {
          // If there is more than one char typed, the current item has highest priority,
          //   otherwise the item next to current has highest priority
          if (currFrame == frameBefore)
            return frameBefore;
        }
      }
    }
    currFrame = currFrame->GetNextSibling();
  }

  doAction = (isMenu && (matchCount == 1 || matchShortcutCount == 1));

  if (matchShortcutCount == 1) // We have one matched shortcut item
    return frameShortcut;
  if (frameAfter) // If we have matched item after the current, use it
    return frameAfter;
  else if (frameBefore) // If we haven't, use the item before the current
    return frameBefore;

  // If we don't match anything, rollback the last typing
  mIncrementalString.SetLength(mIncrementalString.Length() - 1);

  // didn't find a matching menu item
#ifdef XP_WIN
  // behavior on Windows - this item is in a menu popup off of the
  // menu bar, so beep and do nothing else
  if (isMenu) {
    nsCOMPtr<nsISound> soundInterface = do_CreateInstance("@mozilla.org/sound;1");
    if (soundInterface)
      soundInterface->Beep();
  }
#endif  // #ifdef XP_WIN

  return nullptr;
}

void
nsMenuPopupFrame::LockMenuUntilClosed(bool aLock)
{
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

nsIWidget*
nsMenuPopupFrame::GetWidget()
{
  nsView * view = GetRootViewForPopup(this);
  if (!view)
    return nullptr;

  return view->GetWidget();
}

void
nsMenuPopupFrame::AttachedDismissalListener()
{
  mConsumeRollupEvent = nsIPopupBoxObject::ROLLUP_DEFAULT;
}

void
nsMenuPopupFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                   const nsRect&           aDirtyRect,
                                   const nsDisplayListSet& aLists)
{
  // don't pass events to drag popups
  if (aBuilder->IsForEventDelivery() && mIsDragPopup) {
    return;
  }

  nsBoxFrame::BuildDisplayList(aBuilder, aDirtyRect, aLists);
}

// helpers /////////////////////////////////////////////////////////////

NS_IMETHODIMP 
nsMenuPopupFrame::AttributeChanged(int32_t aNameSpaceID,
                                   nsIAtom* aAttribute,
                                   int32_t aModType)

{
  nsresult rv = nsBoxFrame::AttributeChanged(aNameSpaceID, aAttribute,
                                             aModType);
  
  if (aAttribute == nsGkAtoms::left || aAttribute == nsGkAtoms::top)
    MoveToAttributePosition();

  if (aAttribute == nsGkAtoms::label) {
    // set the label for the titlebar
    nsView* view = GetView();
    if (view) {
      nsIWidget* widget = view->GetWidget();
      if (widget) {
        nsAutoString title;
        mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::label, title);
        if (!title.IsEmpty()) {
          widget->SetTitle(title);
        }
      }
    }
  }

  return rv;
}

void
nsMenuPopupFrame::MoveToAttributePosition()
{
  // Move the widget around when the user sets the |left| and |top| attributes. 
  // Note that this is not the best way to move the widget, as it results in lots
  // of FE notifications and is likely to be slow as molasses. Use |moveTo| on
  // nsIPopupBoxObject if possible. 
  nsAutoString left, top;
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::left, left);
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::top, top);
  nsresult err1, err2;
  int32_t xpos = left.ToInteger(&err1);
  int32_t ypos = top.ToInteger(&err2);

  if (NS_SUCCEEDED(err1) && NS_SUCCEEDED(err2))
    MoveTo(xpos, ypos, false);
}

void
nsMenuPopupFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  nsMenuFrame* menu = do_QueryFrame(GetParent());
  if (menu) {
    // clear the open attribute on the parent menu
    nsContentUtils::AddScriptRunner(
      new nsUnsetAttrRunnable(menu->GetContent(), nsGkAtoms::open));
  }

  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm)
    pm->PopupDestroyed(this);

  nsIRootBox* rootBox =
    nsIRootBox::GetRootBox(PresContext()->GetPresShell());
  if (rootBox && rootBox->GetDefaultTooltip() == mContent) {
    rootBox->SetDefaultTooltip(nullptr);
  }

  nsBoxFrame::DestroyFrom(aDestructRoot);
}


void
nsMenuPopupFrame::MoveTo(int32_t aLeft, int32_t aTop, bool aUpdateAttrs)
{
  nsIWidget* widget = GetWidget();
  if ((mScreenXPos == aLeft && mScreenYPos == aTop) &&
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
    nscoord offsetForContextMenu =
      nsPresContext::CSSPixelsToAppUnits(CONTEXT_MENU_OFFSET_PIXELS);
    margin.left += offsetForContextMenu;
    margin.top += offsetForContextMenu;
  }

  nsPresContext* presContext = PresContext();
  mScreenXPos = aLeft - presContext->AppUnitsToIntCSSPixels(margin.left);
  mScreenYPos = aTop - presContext->AppUnitsToIntCSSPixels(margin.top);

  SetPopupPosition(nullptr, true);

  nsCOMPtr<nsIContent> popup = mContent;
  if (aUpdateAttrs && (popup->HasAttr(kNameSpaceID_None, nsGkAtoms::left) ||
                       popup->HasAttr(kNameSpaceID_None, nsGkAtoms::top)))
  {
    nsAutoString left, top;
    left.AppendInt(aLeft);
    top.AppendInt(aTop);
    popup->SetAttr(kNameSpaceID_None, nsGkAtoms::left, left, false);
    popup->SetAttr(kNameSpaceID_None, nsGkAtoms::top, top, false);
  }
}

void
nsMenuPopupFrame::MoveToAnchor(nsIContent* aAnchorContent,
                               const nsAString& aPosition,
                               int32_t aXPos, int32_t aYPos,
                               bool aAttributesOverride)
{
  NS_ASSERTION(mPopupState == ePopupOpenAndVisible, "popup must be open to move it");

  InitializePopup(aAnchorContent, mTriggerContent, aPosition,
                  aXPos, aYPos, aAttributesOverride);
  // InitializePopup changed the state so reset it.
  mPopupState = ePopupOpenAndVisible;

  // Pass false here so that flipping and adjusting to fit on the screen happen.
  SetPopupPosition(nullptr, false);
}

bool
nsMenuPopupFrame::GetAutoPosition()
{
  return mShouldAutoPosition;
}

void
nsMenuPopupFrame::SetAutoPosition(bool aShouldAutoPosition)
{
  mShouldAutoPosition = aShouldAutoPosition;
}

void
nsMenuPopupFrame::SetConsumeRollupEvent(uint32_t aConsumeMode)
{
  mConsumeRollupEvent = aConsumeMode;
}

int8_t
nsMenuPopupFrame::GetAlignmentPosition() const
{
  // The code below handles most cases of alignment, anchor and position values. Those that are
  // not handled just return POPUPPOSITION_UNKNOWN.

  if (mPosition == POPUPPOSITION_OVERLAP || mPosition == POPUPPOSITION_AFTERPOINTER)
    return mPosition;

  int8_t position = mPosition;

  if (position == POPUPPOSITION_UNKNOWN) {
    switch (mPopupAnchor) {
      case POPUPALIGNMENT_BOTTOMCENTER:
        position = mPopupAlignment == POPUPALIGNMENT_TOPRIGHT ?
                     POPUPPOSITION_AFTEREND : POPUPPOSITION_AFTERSTART;
        break;
      case POPUPALIGNMENT_TOPCENTER:
        position = mPopupAlignment == POPUPALIGNMENT_BOTTOMRIGHT ?
                     POPUPPOSITION_BEFOREEND : POPUPPOSITION_BEFORESTART;
        break;
      case POPUPALIGNMENT_LEFTCENTER:
        position = mPopupAlignment == POPUPALIGNMENT_BOTTOMRIGHT ?
                     POPUPPOSITION_STARTAFTER : POPUPPOSITION_STARTBEFORE;
        break;
      case POPUPALIGNMENT_RIGHTCENTER:
        position = mPopupAlignment == POPUPALIGNMENT_BOTTOMLEFT ?
                     POPUPPOSITION_ENDAFTER : POPUPPOSITION_ENDBEFORE;
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
 * KEEP THIS IN SYNC WITH nsContainerFrame::CreateViewForFrame
 * as much as possible. Until we get rid of views finally...
 */
void
nsMenuPopupFrame::CreatePopupView()
{
  if (HasView()) {
    return;
  }

  nsViewManager* viewManager = PresContext()->GetPresShell()->GetViewManager();
  NS_ASSERTION(nullptr != viewManager, "null view manager");

  // Create a view
  nsView* parentView = viewManager->GetRootView();
  nsViewVisibility visibility = nsViewVisibility_kHide;
  int32_t zIndex = INT32_MAX;
  bool    autoZIndex = false;

  NS_ASSERTION(parentView, "no parent view");

  // Create a view
  nsView *view = viewManager->CreateView(GetRect(), parentView, visibility);
  viewManager->SetViewZIndex(view, autoZIndex, zIndex);
  // XXX put view last in document order until we can do better
  viewManager->InsertChild(parentView, view, nullptr, true);

  // Remember our view
  SetView(view);

  NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
    ("nsMenuPopupFrame::CreatePopupView: frame=%p view=%p", this, view));
}
