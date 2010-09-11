/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Original Author: David W. Hyatt (hyatt@netscape.com)
 *   Mike Pinkerton (pinkerton@netscape.com)
 *   Dean Tessman <dean_tessman@hotmail.com>
 *   Ben Goodger <ben@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsMenuPopupFrame.h"
#include "nsGkAtoms.h"
#include "nsIContent.h"
#include "prtypes.h"
#include "nsIAtom.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "nsCSSRendering.h"
#include "nsINameSpaceManager.h"
#include "nsIViewManager.h"
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
#include "nsIDeviceContext.h"
#include "nsRect.h"
#include "nsILookAndFeel.h"
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
#include "nsIEventStateManager.h"
#include "nsIBoxLayout.h"
#include "nsIPopupBoxObject.h"
#include "nsPIWindowRoot.h"
#include "nsIReflowCallback.h"
#include "nsBindingManager.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIBaseWindow.h"
#include "nsISound.h"
#include "nsIRootBox.h"
#include "nsIScreenManager.h"
#include "nsIServiceManager.h"
#include "nsThemeConstants.h"

PRInt8 nsMenuPopupFrame::sDefaultLevelIsTop = -1;

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

//
// nsMenuPopupFrame ctor
//
nsMenuPopupFrame::nsMenuPopupFrame(nsIPresShell* aShell, nsStyleContext* aContext)
  :nsBoxFrame(aShell, aContext),
  mCurrentMenu(nsnull),
  mPrefSize(-1, -1),
  mPopupType(ePopupTypePanel),
  mPopupState(ePopupClosed),
  mPopupAlignment(POPUPALIGNMENT_NONE),
  mPopupAnchor(POPUPALIGNMENT_NONE),
  mIsOpenChanged(PR_FALSE),
  mIsContextMenu(PR_FALSE),
  mAdjustOffsetForContextMenu(PR_FALSE),
  mGeneratedChildren(PR_FALSE),
  mMenuCanOverlapOSBar(PR_FALSE),
  mShouldAutoPosition(PR_TRUE),
  mConsumeRollupEvent(nsIPopupBoxObject::ROLLUP_DEFAULT),
  mInContentShell(PR_TRUE),
  mIsMenuLocked(PR_FALSE),
  mHFlip(PR_FALSE),
  mVFlip(PR_FALSE)
{
  // the preference name is backwards here. True means that the 'top' level is
  // the default, and false means that the 'parent' level is the default.
  if (sDefaultLevelIsTop >= 0)
    return;
  sDefaultLevelIsTop =
    nsContentUtils::GetBoolPref("ui.panel.default_level_parent", PR_FALSE);
} // ctor


NS_IMETHODIMP
nsMenuPopupFrame::Init(nsIContent*      aContent,
                       nsIFrame*        aParent,
                       nsIFrame*        aPrevInFlow)
{
  nsresult rv = nsBoxFrame::Init(aContent, aParent, aPrevInFlow);
  NS_ENSURE_SUCCESS(rv, rv);

  nsPresContext* presContext = PresContext();

  // lookup if we're allowed to overlap the OS bar (menubar/taskbar) from the
  // look&feel object
  PRBool tempBool;
  presContext->LookAndFeel()->
    GetMetric(nsILookAndFeel::eMetric_MenusCanOverlapOSBar, tempBool);
  mMenuCanOverlapOSBar = tempBool;

  rv = CreateViewForFrame(presContext, this, GetStyleContext(), PR_TRUE, PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  // XXX Hack. The popup's view should float above all other views,
  // so we use the nsIView::SetFloating() to tell the view manager
  // about that constraint.
  nsIView* ourView = GetView();
  nsIViewManager* viewManager = ourView->GetViewManager();
  viewManager->SetViewFloating(ourView, PR_TRUE);

  mPopupType = ePopupTypePanel;
  nsIDocument* doc = aContent->GetOwnerDoc();
  if (doc) {
    PRInt32 namespaceID;
    nsCOMPtr<nsIAtom> tag = doc->BindingManager()->ResolveTag(aContent, &namespaceID);
    if (namespaceID == kNameSpaceID_XUL) {
      if (tag == nsGkAtoms::menupopup || tag == nsGkAtoms::popup)
        mPopupType = ePopupTypeMenu;
      else if (tag == nsGkAtoms::tooltip)
        mPopupType = ePopupTypeTooltip;
    }
  }

  nsCOMPtr<nsISupports> cont = PresContext()->GetContainer();
  nsCOMPtr<nsIDocShellTreeItem> dsti = do_QueryInterface(cont);
  PRInt32 type = -1;
  if (dsti && NS_SUCCEEDED(dsti->GetItemType(&type)) &&
      type == nsIDocShellTreeItem::typeChrome)
    mInContentShell = PR_FALSE;

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

  return rv;
}

PRBool
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
nsMenuPopupFrame::PopupLevel(PRBool aIsNoAutoHide) const
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
    {&nsGkAtoms::top, &nsGkAtoms::parent, &nsGkAtoms::floating, nsnull};
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
  nsIView* ourView = GetView();
  if (!ourView->HasWidget()) {
    NS_ASSERTION(!mGeneratedChildren && !GetFirstChild(nsnull),
                 "Creating widget for MenuPopupFrame with children");
    CreateWidgetForView(ourView);
  }
}

nsresult
nsMenuPopupFrame::CreateWidgetForView(nsIView* aView)
{
  // Create a widget for ourselves.
  nsWidgetInitData widgetData;
  widgetData.mWindowType = eWindowType_popup;
  widgetData.mBorderStyle = eBorderStyle_default;
  widgetData.clipSiblings = PR_TRUE;
  widgetData.mPopupHint = mPopupType;
  widgetData.mNoAutoHide = IsNoAutoHide();

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
  PRBool viewHasTransparentContent = !mInContentShell &&
                                     (eTransparencyTransparent ==
                                      mode);
  nsIContent* parentContent = GetContent()->GetParent();
  nsIAtom *tag = nsnull;
  if (parentContent)
    tag = parentContent->Tag();
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

  aView->CreateWidgetForPopup(&widgetData, parentWidget,
                              PR_TRUE, PR_TRUE, eContentTypeUI);

  nsIWidget* widget = aView->GetWidget();
  widget->SetTransparencyMode(mode);
  widget->SetWindowShadowStyle(GetShadowStyle());

  // most popups don't have a title so avoid setting the title if there isn't one
  if (!title.IsEmpty()) {
    widget->SetTitle(title);
  }

  return NS_OK;
}

PRUint8
nsMenuPopupFrame::GetShadowStyle()
{
  PRUint8 shadow = GetStyleUIReset()->mWindowShadow;
  if (shadow != NS_STYLE_WINDOW_SHADOW_DEFAULT)
    return shadow;

  switch (GetStyleDisplay()->mAppearance) {
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
    nsMouseEvent event(PR_TRUE, NS_XUL_POPUP_SHOWN, nsnull, nsMouseEvent::eReal);
    return nsEventDispatcher::Dispatch(mPopup, mPresContext, &event);                 
  }

private:
  nsCOMPtr<nsIContent> mPopup;
  nsRefPtr<nsPresContext> mPresContext;
};

NS_IMETHODIMP
nsMenuPopupFrame::SetInitialChildList(nsIAtom* aListName,
                                      nsFrameList& aChildList)
{
  // unless the list is empty, indicate that children have been generated.
  if (aChildList.NotEmpty())
    mGeneratedChildren = PR_TRUE;
  return nsBoxFrame::SetInitialChildList(aListName, aChildList);
}

PRBool
nsMenuPopupFrame::IsLeaf() const
{
  if (mGeneratedChildren)
    return PR_FALSE;

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
nsMenuPopupFrame::LayoutPopup(nsBoxLayoutState& aState, nsIFrame* aParentMenu, PRBool aSizedToPopup)
{
  if (!mGeneratedChildren)
    return;

  PRBool shouldPosition = PR_TRUE;
  PRBool isOpen = IsOpen();
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
      scrollframe->ScrollTo(nsPoint(0,0), nsIScrollableFrame::INSTANT);
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
  PRBool sizeChanged = (mPrefSize != prefSize);
  if (sizeChanged) {
    SetBounds(aState, nsRect(0, 0, prefSize.width, prefSize.height), PR_FALSE);
    mPrefSize = prefSize;
  }

  if (shouldPosition) {
    SetPopupPosition(aParentMenu, PR_FALSE);
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
        SetPopupPosition(nsnull, PR_FALSE);
      }
    }
  }

  nsPresContext* pc = PresContext();
  if (isOpen) {
    nsIView* view = GetView();
    nsIViewManager* viewManager = view->GetViewManager();
    nsRect rect = GetRect();
    rect.x = rect.y = 0;

    // Increase the popup's view size to account for any titlebar or borders.
    // XXXndeakin this should really be accounted for earlier in
    // SetPopupPosition so that this extra size is accounted for when flipping
    // or resizing the popup due to it being too large, but that can be a
    // followup bug.
    if (mPopupType == ePopupTypePanel && view) {
      nsIWidget* widget = view->GetWidget();
      if (widget) {
        nsIntSize popupSize = nsIntSize(pc->AppUnitsToDevPixels(rect.width),
                                        pc->AppUnitsToDevPixels(rect.height));
        popupSize = widget->ClientToWindowSize(popupSize);
        rect.width = pc->DevPixelsToAppUnits(popupSize.width);
        rect.height = pc->DevPixelsToAppUnits(popupSize.height);
      }
    }
    viewManager->ResizeView(view, rect);

    viewManager->SetViewVisibility(view, nsViewVisibility_kShow);
    mPopupState = ePopupOpenAndVisible;
    nsContainerFrame::SyncFrameViewProperties(pc, this, nsnull, view, 0);
  }

  // finally, if the popup just opened, send a popupshown event
  if (mIsOpenChanged) {
    mIsOpenChanged = PR_FALSE;
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
    nsMenuFrame* menuFrame = aMenuPopupFrame->GetParentMenu();
    if (!menuFrame)
      break;

    nsMenuParent* parentPopup = menuFrame->GetMenuParent();
    if (!parentPopup || !parentPopup->IsMenu())
      break;

    aMenuPopupFrame = static_cast<nsMenuPopupFrame *>(parentPopup);
  }

  return nsnull;
}

void
nsMenuPopupFrame::InitPositionFromAnchorAlign(const nsAString& aAnchor,
                                              const nsAString& aAlign)
{
  mTriggerContent = nsnull;

  if (aAnchor.EqualsLiteral("topleft"))
    mPopupAnchor = POPUPALIGNMENT_TOPLEFT;
  else if (aAnchor.EqualsLiteral("topright"))
    mPopupAnchor = POPUPALIGNMENT_TOPRIGHT;
  else if (aAnchor.EqualsLiteral("bottomleft"))
    mPopupAnchor = POPUPALIGNMENT_BOTTOMLEFT;
  else if (aAnchor.EqualsLiteral("bottomright"))
    mPopupAnchor = POPUPALIGNMENT_BOTTOMRIGHT;
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
}

void
nsMenuPopupFrame::InitializePopup(nsIContent* aAnchorContent,
                                  nsIContent* aTriggerContent,
                                  const nsAString& aPosition,
                                  PRInt32 aXPos, PRInt32 aYPos,
                                  PRBool aAttributesOverride)
{
  EnsureWidget();

  mPopupState = ePopupShowing;
  mAnchorContent = aAnchorContent;
  mTriggerContent = aTriggerContent;
  mXPos = aXPos;
  mYPos = aYPos;
  mAdjustOffsetForContextMenu = PR_FALSE;

  // if aAttributesOverride is true, then the popupanchor, popupalign and
  // position attributes on the <popup> override those values passed in.
  // If false, those attributes are only used if the values passed in are empty
  if (aAnchorContent) {
    nsAutoString anchor, align, position;
    mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::popupanchor, anchor);
    mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::popupalign, align);
    mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::position, position);

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

    if (position.EqualsLiteral("before_start")) {
      mPopupAnchor = POPUPALIGNMENT_TOPLEFT;
      mPopupAlignment = POPUPALIGNMENT_BOTTOMLEFT;
    }
    else if (position.EqualsLiteral("before_end")) {
      mPopupAnchor = POPUPALIGNMENT_TOPRIGHT;
      mPopupAlignment = POPUPALIGNMENT_BOTTOMRIGHT;
    }
    else if (position.EqualsLiteral("after_start")) {
      mPopupAnchor = POPUPALIGNMENT_BOTTOMLEFT;
      mPopupAlignment = POPUPALIGNMENT_TOPLEFT;
    }
    else if (position.EqualsLiteral("after_end")) {
      mPopupAnchor = POPUPALIGNMENT_BOTTOMRIGHT;
      mPopupAlignment = POPUPALIGNMENT_TOPRIGHT;
    }
    else if (position.EqualsLiteral("start_before")) {
      mPopupAnchor = POPUPALIGNMENT_TOPLEFT;
      mPopupAlignment = POPUPALIGNMENT_TOPRIGHT;
    }
    else if (position.EqualsLiteral("start_after")) {
      mPopupAnchor = POPUPALIGNMENT_BOTTOMLEFT;
      mPopupAlignment = POPUPALIGNMENT_BOTTOMRIGHT;
    }
    else if (position.EqualsLiteral("end_before")) {
      mPopupAnchor = POPUPALIGNMENT_TOPRIGHT;
      mPopupAlignment = POPUPALIGNMENT_TOPLEFT;
    }
    else if (position.EqualsLiteral("end_after")) {
      mPopupAnchor = POPUPALIGNMENT_BOTTOMRIGHT;
      mPopupAlignment = POPUPALIGNMENT_BOTTOMLEFT;
    }
    else if (position.EqualsLiteral("overlap")) {
      mPopupAnchor = POPUPALIGNMENT_TOPLEFT;
      mPopupAlignment = POPUPALIGNMENT_TOPLEFT;
    }
    else if (position.EqualsLiteral("after_pointer")) {
      mPopupAnchor = POPUPALIGNMENT_TOPLEFT;
      mPopupAlignment = POPUPALIGNMENT_TOPLEFT;
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

    PRInt32 err;
    if (!left.IsEmpty()) {
      PRInt32 x = left.ToInteger(&err);
      if (NS_SUCCEEDED(err))
        mScreenXPos = x;
    }
    if (!top.IsEmpty()) {
      PRInt32 y = top.ToInteger(&err);
      if (NS_SUCCEEDED(err))
        mScreenYPos = y;
    }
  }
}

void
nsMenuPopupFrame::InitializePopupAtScreen(nsIContent* aTriggerContent,
                                          PRInt32 aXPos, PRInt32 aYPos,
                                          PRBool aIsContextMenu)
{
  EnsureWidget();

  mPopupState = ePopupShowing;
  mAnchorContent = nsnull;
  mTriggerContent = aTriggerContent;
  mScreenXPos = aXPos;
  mScreenYPos = aYPos;
  mPopupAnchor = POPUPALIGNMENT_NONE;
  mPopupAlignment = POPUPALIGNMENT_NONE;
  mIsContextMenu = aIsContextMenu;
  mAdjustOffsetForContextMenu = aIsContextMenu;
}

void
nsMenuPopupFrame::InitializePopupWithAnchorAlign(nsIContent* aAnchorContent,
                                                 nsAString& aAnchor,
                                                 nsAString& aAlign,
                                                 PRInt32 aXPos, PRInt32 aYPos)
{
  EnsureWidget();

  mPopupState = ePopupShowing;
  mAdjustOffsetForContextMenu = PR_FALSE;

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
    mAnchorContent = nsnull;
    mPopupAnchor = POPUPALIGNMENT_NONE;
    mPopupAlignment = POPUPALIGNMENT_NONE;
    mScreenXPos = aXPos;
    mScreenYPos = aYPos;
    mXPos = aXPos;
    mYPos = aYPos;
  }
}

void
nsMenuPopupFrame::ShowPopup(PRBool aIsContextMenu, PRBool aSelectFirstItem)
{
  mIsContextMenu = aIsContextMenu;

  if (mPopupState == ePopupShowing) {
    mPopupState = ePopupOpen;
    mIsOpenChanged = PR_TRUE;

    nsMenuFrame* menuFrame = GetParentMenu();
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

  mShouldAutoPosition = PR_TRUE;
}

void
nsMenuPopupFrame::HidePopup(PRBool aDeselectMenu, nsPopupState aNewState)
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
            root->SetPopupNode(nsnull);
          }
        }
      }
    }
    mTriggerContent = nsnull;
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
    SetCurrentMenuItem(nsnull);

  mIncrementalString.Truncate();

  LockMenuUntilClosed(PR_FALSE);

  mIsOpenChanged = PR_FALSE;
  mCurrentMenu = nsnull; // make sure no current menu is set
  mHFlip = mVFlip = PR_FALSE;

  nsIView* view = GetView();
  nsIViewManager* viewManager = view->GetViewManager();
  viewManager->SetViewVisibility(view, nsViewVisibility_kHide);
  viewManager->ResizeView(view, nsRect(0, 0, 0, 0));

  FireDOMEvent(NS_LITERAL_STRING("DOMMenuInactive"), mContent);

  // XXX, bug 137033, In Windows, if mouse is outside the window when the menupopup closes, no
  // mouse_enter/mouse_exit event will be fired to clear current hover state, we should clear it manually.
  // This code may not the best solution, but we can leave it here until we find the better approach.
  nsIEventStateManager *esm = PresContext()->EventStateManager();

  PRInt32 state = esm->GetContentState(mContent);

  if (state & NS_EVENT_STATE_HOVER)
    esm->SetContentState(nsnull, NS_EVENT_STATE_HOVER);

  nsMenuFrame* menuFrame = GetParentMenu();
  if (menuFrame) {
    menuFrame->PopupClosed(aDeselectMenu);
  }
}

void
nsMenuPopupFrame::InvalidateInternal(const nsRect& aDamageRect,
                                     nscoord aX, nscoord aY, nsIFrame* aForChild,
                                     PRUint32 aFlags)
{
  InvalidateRoot(aDamageRect + nsPoint(aX, aY), aFlags);
}

void
nsMenuPopupFrame::GetLayoutFlags(PRUint32& aFlags)
{
  aFlags = NS_FRAME_NO_SIZE_VIEW | NS_FRAME_NO_MOVE_VIEW | NS_FRAME_NO_VISIBILITY;
}

///////////////////////////////////////////////////////////////////////////////
// GetRootViewForPopup
//   Retrieves the view for the popup widget that contains the given frame. 
//   If the given frame is not contained by a popup widget, return the
//   root view of the root viewmanager.
nsIView*
nsMenuPopupFrame::GetRootViewForPopup(nsIFrame* aStartFrame)
{
  nsIView* view = aStartFrame->GetClosestView();
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

    nsIView* temp = view->GetParent();
    if (!temp) {
      // Otherwise, we've walked all the way up to the root view and not
      // found a view for a popup window widget. Just return the root view.
      return view;
    }
    view = temp;
  }

  return nsnull;
}

nsPoint
nsMenuPopupFrame::AdjustPositionForAnchorAlign(const nsRect& anchorRect,
                                               PRBool& aHFlip, PRBool& aVFlip)
{
  // flip the anchor and alignment for right-to-left
  PRInt8 popupAnchor(mPopupAnchor);
  PRInt8 popupAlign(mPopupAlignment);
  if (GetStyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL) {
    popupAnchor = -popupAnchor;
    popupAlign = -popupAlign;
  }

  // first, determine at which corner of the anchor the popup should appear
  nsPoint pnt;
  switch (popupAnchor) {
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
  GetStyleMargin()->GetMargin(margin);
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
  aHFlip = (popupAnchor == -popupAlign);
  aVFlip = ((popupAnchor > 0) == (popupAlign > 0)) ||
            (popupAnchor == POPUPALIGNMENT_TOPLEFT && popupAlign == POPUPALIGNMENT_TOPLEFT);

  return pnt;
}

nscoord
nsMenuPopupFrame::FlipOrResize(nscoord& aScreenPoint, nscoord aSize, 
                               nscoord aScreenBegin, nscoord aScreenEnd,
                               nscoord aAnchorBegin, nscoord aAnchorEnd,
                               nscoord aMarginBegin, nscoord aMarginEnd,
                               nscoord aOffsetForContextMenu, PRBool aFlip,
                               PRPackedBool* aFlipSide)
{
  // all of the coordinates used here are in app units relative to the screen
  nscoord popupSize = aSize;
  if (aScreenPoint < aScreenBegin) {
    // at its current position, the popup would extend past the left or top
    // edge of the screen, so it will have to be moved or resized.
    if (aFlip) {
      // check whether there is more room to the left and right (or top and
      // bottom) of the anchor and put the popup on the side with more room.
      if (aAnchorBegin - aScreenBegin >= aScreenEnd - aAnchorEnd) {
        aScreenPoint = aScreenBegin;
        popupSize = aAnchorBegin - aScreenPoint - aMarginEnd;
      }
      else {
        // flip such that the popup is to the right or bottom of the anchor
        // point instead. However, when flipping use the same margin size.
        *aFlipSide = PR_TRUE;
        aScreenPoint = aAnchorEnd + aMarginEnd;
        // check if the new position is still off the right or bottom edge of
        // the screen. If so, resize the popup.
        if (aScreenPoint + aSize > aScreenEnd) {
          popupSize = aScreenEnd - aScreenPoint;
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
      // check whether there is more room to the left and right (or top and
      // bottom) of the anchor and put the popup on the side with more room.
      if (aScreenEnd - aAnchorEnd >= aAnchorBegin - aScreenBegin) {
        if (mIsContextMenu) {
          aScreenPoint = aScreenEnd - aSize;
        }
        else {
          popupSize = aScreenEnd - aScreenPoint;
        }
      }
      else {
        // flip such that the popup is to the left or top of the anchor point
        // instead.
        *aFlipSide = PR_TRUE;
        aScreenPoint = aAnchorBegin - aSize - aMarginBegin - aOffsetForContextMenu;
        // check if the new position is still off the left or top edge of the
        // screen. If so, resize the popup.
        if (aScreenPoint < aScreenBegin) {
          aScreenPoint = aScreenBegin;
          if (!mIsContextMenu) {
            popupSize = aAnchorBegin - aScreenPoint - aMarginBegin;
          }
        }
      }
    }
    else {
      aScreenPoint = aScreenEnd - aSize;
    }
  }

  return popupSize;
}

nsresult
nsMenuPopupFrame::SetPopupPosition(nsIFrame* aAnchorFrame, PRBool aIsMove)
{
  if (!mShouldAutoPosition)
    return NS_OK;

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

  PRBool sizedToPopup = PR_FALSE;
  if (aAnchorFrame->GetContent()) {
    // the popup should be the same size as the anchor menu, for example, a menulist.
    sizedToPopup = nsMenuFrame::IsSizedToPopup(aAnchorFrame->GetContent(), PR_FALSE);
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
  PRBool hFlip = PR_FALSE, vFlip = PR_FALSE;
  
  nsMargin margin(0, 0, 0, 0);
  GetStyleMargin()->GetMargin(margin);

  // the screen rectangle of the root frame, in dev pixels.
  nsRect rootScreenRect = rootFrame->GetScreenRectInAppUnits();

  nsIDeviceContext* devContext = presContext->DeviceContext();
  nscoord offsetForContextMenu = 0;
  // if mScreenXPos and mScreenYPos are -1, then we are anchored. If they
  // have other values, then the popup appears unanchored at that screen
  // coordinate.
  if (mScreenXPos == -1 && mScreenYPos == -1) {
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
    // should be added to the position
    if (GetStyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL)
      screenPoint.x -= presContext->CSSPixelsToAppUnits(mXPos);
    else
      screenPoint.x += presContext->CSSPixelsToAppUnits(mXPos);
    screenPoint.y += presContext->CSSPixelsToAppUnits(mYPos);

    // If this is a noautohide popup, set the screen coordinates of the popup.
    // This way, the popup stays at the location where it was opened even when
    // the window is moved. Popups at the parent level follow the parent
    // window as it is moved and remained anchored, so we want to maintain the
    // anchoring instead.
    if (IsNoAutoHide() && PopupLevel(PR_TRUE) != ePopupLevelParent) {
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
    PRInt32 factor = devContext->UnscaledAppUnitsPerDevPixel();

    // context menus should be offset by two pixels so that they don't appear
    // directly where the cursor is. Otherwise, it is too easy to have the
    // context menu close up again.
    if (mAdjustOffsetForContextMenu) {
      PRInt32 offsetForContextMenuDev =
        nsPresContext::CSSPixelsToAppUnits(2) / factor;
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
    vFlip = PR_TRUE;
  }

  // if a panel is being moved, don't flip it. But always do this for content
  // shells, so that the popup doesn't extend outside the containing frame.
  if (aIsMove && mPopupType == ePopupTypePanel && !mInContentShell) {
    hFlip = vFlip = PR_FALSE;
  }

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
  // Next, check if there is enough space to show the popup at full size when
  // positioned at screenPoint. If not, flip the popups to the opposite side
  // of their anchor point, or resize them as necessary.
  mRect.width = FlipOrResize(screenPoint.x, mRect.width, screenRect.x,
                             screenRect.XMost(), anchorRect.x, anchorRect.XMost(),
                             margin.left, margin.right, offsetForContextMenu, hFlip, &mHFlip);

  mRect.height = FlipOrResize(screenPoint.y, mRect.height, screenRect.y,
                              screenRect.YMost(), anchorRect.y, anchorRect.YMost(),
                              margin.top, margin.bottom, offsetForContextMenu, vFlip, &mVFlip);

  NS_ASSERTION(screenPoint.x >= screenRect.x && screenPoint.y >= screenRect.y &&
               screenPoint.x + mRect.width <= screenRect.XMost() &&
               screenPoint.y + mRect.height <= screenRect.YMost(),
               "Popup is offscreen");

  // determine the x and y position of the view by subtracting the desired
  // screen position from the screen position of the root frame.
  nsPoint viewPoint = screenPoint - rootScreenRect.TopLeft();
  nsIView* view = GetView();
  NS_ASSERTION(view, "popup with no view");
  presContext->GetPresShell()->GetViewManager()->
    MoveViewTo(view, viewPoint.x, viewPoint.y);

  // Offset the position by the width and height of the borders and titlebar.
  // Even though GetClientOffset should return (0, 0) when there is no
  // titlebar or borders, we skip these calculations anyway for non-panels
  // to save time since they will never have a titlebar.
  nsIWidget* widget = view->GetWidget();
  if (mPopupType == ePopupTypePanel && widget) {
    nsIntPoint offset = widget->GetClientOffset();
    viewPoint.x += presContext->DevPixelsToAppUnits(offset.x);
    viewPoint.y += presContext->DevPixelsToAppUnits(offset.y);
  }

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
    PRInt32 width = rect.width > 0 ? presContext->AppUnitsToDevPixels(rect.width) : 1;
    PRInt32 height = rect.height > 0 ? presContext->AppUnitsToDevPixels(rect.height) : 1;
    sm->ScreenForRect(presContext->AppUnitsToDevPixels(rect.x),
                      presContext->AppUnitsToDevPixels(rect.y),
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
  screenRectPixels.SizeBy(-3, -3);

  nsRect screenRect = screenRectPixels.ToAppUnits(presContext->AppUnitsPerDevPixel());
  if (mInContentShell) {
    // for content shells, clip to the client area rather than the screen area
    screenRect.IntersectRect(screenRect, aRootScreenRect);
  }

  return screenRect;
}

void nsMenuPopupFrame::CanAdjustEdges(PRInt8 aHorizontalSide, PRInt8 aVerticalSide, nsIntPoint& aChange)
{
  PRInt8 popupAlign(mPopupAlignment);
  if (GetStyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL) {
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

PRBool nsMenuPopupFrame::ConsumeOutsideClicks()
{
  // If the popup has explicitly set a consume mode, honor that.
  if (mConsumeRollupEvent != nsIPopupBoxObject::ROLLUP_DEFAULT)
    return (mConsumeRollupEvent == nsIPopupBoxObject::ROLLUP_CONSUME);

  nsCOMPtr<nsIContent> parentContent = mContent->GetParent();
  if (parentContent) {
    nsINodeInfo *ni = parentContent->NodeInfo();
    if (ni->Equals(nsGkAtoms::menulist, kNameSpaceID_XUL))
      return PR_TRUE;  // Consume outside clicks for combo boxes on all platforms
#if defined(XP_WIN) || defined(XP_OS2)
    // Don't consume outside clicks for menus in Windows
    if (ni->Equals(nsGkAtoms::menu, kNameSpaceID_XUL) ||
       (ni->Equals(nsGkAtoms::popupset, kNameSpaceID_XUL)))
      return PR_FALSE;
#endif
    if (ni->Equals(nsGkAtoms::textbox, kNameSpaceID_XUL)) {
      // Don't consume outside clicks for autocomplete widget
      if (parentContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                                     nsGkAtoms::autocomplete, eCaseMatters))
        return PR_FALSE;
    }
  }

  return PR_TRUE;
}

// XXXroc this is megalame. Fossicking around for a frame of the right
// type is a recipe for disaster in the long term.
nsIScrollableFrame* nsMenuPopupFrame::GetScrollFrame(nsIFrame* aStart)
{
  if (!aStart)
    return nsnull;  

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
    nsIFrame* childFrame = currFrame->GetFirstChild(nsnull);
    nsIScrollableFrame* sf = GetScrollFrame(childFrame);
    if (sf)
      return sf;
    currFrame = currFrame->GetNextSibling();
  } while (currFrame);

  return nsnull;
}

void nsMenuPopupFrame::EnsureMenuItemIsVisible(nsMenuFrame* aMenuItem)
{
  if (aMenuItem) {
    aMenuItem->PresContext()->PresShell()->
      ScrollFrameRectIntoView(aMenuItem,
                              nsRect(nsPoint(0,0), aMenuItem->GetRect().Size()),
                              NS_PRESSHELL_SCROLL_ANYWHERE,
                              NS_PRESSHELL_SCROLL_ANYWHERE,
                              nsIPresShell::SCROLL_OVERFLOW_HIDDEN |
                              nsIPresShell::SCROLL_FIRST_ANCESTOR_ONLY);
  }
}

NS_IMETHODIMP nsMenuPopupFrame::SetCurrentMenuItem(nsMenuFrame* aMenuItem)
{
  if (mCurrentMenu == aMenuItem)
    return NS_OK;

  if (mCurrentMenu) {
    mCurrentMenu->SelectMenu(PR_FALSE);
  }

  if (aMenuItem) {
    EnsureMenuItemIsVisible(aMenuItem);
    aMenuItem->SelectMenu(PR_TRUE);
  }

  mCurrentMenu = aMenuItem;

  return NS_OK;
}

void
nsMenuPopupFrame::CurrentMenuIsBeingDestroyed()
{
  mCurrentMenu = nsnull;
}

NS_IMETHODIMP
nsMenuPopupFrame::ChangeMenuItem(nsMenuFrame* aMenuItem,
                                 PRBool aSelectFirstItem)
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
    mCurrentMenu->SelectMenu(PR_FALSE);
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
    aMenuItem->SelectMenu(PR_TRUE);
  }

  mCurrentMenu = aMenuItem;

  return NS_OK;
}

nsMenuFrame*
nsMenuPopupFrame::Enter()
{
  mIncrementalString.Truncate();

  // Give it to the child.
  if (mCurrentMenu)
    return mCurrentMenu->Enter();

  return nsnull;
}

nsMenuFrame*
nsMenuPopupFrame::FindMenuWithShortcut(nsIDOMKeyEvent* aKeyEvent, PRBool& doAction)
{
  PRUint32 charCode, keyCode;
  aKeyEvent->GetCharCode(&charCode);
  aKeyEvent->GetKeyCode(&keyCode);

  doAction = PR_FALSE;

  // Enumerate over our list of frames.
  nsIFrame* immediateParent = nsnull;
  PresContext()->PresShell()->
    FrameConstructor()->GetInsertionPoint(this, nsnull, &immediateParent);
  if (!immediateParent)
    immediateParent = this;

  PRUint32 matchCount = 0, matchShortcutCount = 0;
  PRBool foundActive = PR_FALSE;
  PRBool isShortcut;
  nsMenuFrame* frameBefore = nsnull;
  nsMenuFrame* frameAfter = nsnull;
  nsMenuFrame* frameShortcut = nsnull;

  nsIContent* parentContent = mContent->GetParent();

  PRBool isMenu = parentContent &&
                  !parentContent->NodeInfo()->Equals(nsGkAtoms::menulist, kNameSpaceID_XUL);

  static DOMTimeStamp lastKeyTime = 0;
  DOMTimeStamp keyTime;
  aKeyEvent->GetTimeStamp(&keyTime);

  if (charCode == 0) {
    if (keyCode == NS_VK_BACK) {
      if (!isMenu && !mIncrementalString.IsEmpty()) {
        mIncrementalString.SetLength(mIncrementalString.Length() - 1);
        return nsnull;
      }
      else {
#ifdef XP_WIN
        nsCOMPtr<nsISound> soundInterface = do_CreateInstance("@mozilla.org/sound;1");
        if (soundInterface)
          soundInterface->Beep();
#endif  // #ifdef XP_WIN
      }
    }
    return nsnull;
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
  PRUint32 charIndex = 1, stringLength = incrementalString.Length();
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
  currFrame = immediateParent->GetFirstChild(nsnull);

  PRInt32 menuAccessKey = -1;
  nsMenuBarListener::GetMenuAccessKey(&menuAccessKey);

  // We start searching from first child. This process is divided into two parts
  //   -- before current and after current -- by the current item
  while (currFrame) {
    nsIContent* current = currFrame->GetContent();
    
    // See if it's a menu item.
    if (nsXULPopupManager::IsValidMenuItem(PresContext(), current, PR_TRUE)) {
      nsAutoString textKey;
      if (menuAccessKey >= 0) {
        // Get the shortcut attribute.
        current->GetAttr(kNameSpaceID_None, nsGkAtoms::accesskey, textKey);
      }
      if (textKey.IsEmpty()) { // No shortcut, try first letter
        isShortcut = PR_FALSE;
        current->GetAttr(kNameSpaceID_None, nsGkAtoms::label, textKey);
        if (textKey.IsEmpty()) // No label, try another attribute (value)
          current->GetAttr(kNameSpaceID_None, nsGkAtoms::value, textKey);
      }
      else
        isShortcut = PR_TRUE;

      if (StringBeginsWith(textKey, incrementalString,
                           nsCaseInsensitiveStringComparator())) {
        // mIncrementalString is a prefix of textKey
        if (currFrame->GetType() == nsGkAtoms::menuFrame) {
          // There is one match
          matchCount++;
          if (isShortcut) {
            // There is one shortcut-key match
            matchShortcutCount++;
            // Record the matched item. If there is only one matched shortcut item, do it
            frameShortcut = static_cast<nsMenuFrame *>(currFrame);
          }
          if (!foundActive) {
            // It's a first candidate item located before/on the current item
            if (!frameBefore)
              frameBefore = static_cast<nsMenuFrame *>(currFrame);
          }
          else {
            // It's a first candidate item located after the current item
            if (!frameAfter)
              frameAfter = static_cast<nsMenuFrame *>(currFrame);
          }
        }
        else
          return nsnull;
      }

      // Get the active status
      if (current->AttrValueIs(kNameSpaceID_None, nsGkAtoms::menuactive,
                               nsGkAtoms::_true, eCaseMatters)) {
        foundActive = PR_TRUE;
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

  return nsnull;
}

void
nsMenuPopupFrame::LockMenuUntilClosed(PRBool aLock)
{
  mIsMenuLocked = aLock;

  // Lock / unlock the parent, too.
  nsIFrame* parent = GetParent();
  if (parent && parent->GetType() == nsGkAtoms::menuFrame) {
    nsMenuParent* parentParent = static_cast<nsMenuFrame*>(parent)->GetMenuParent();
    if (parentParent) {
      parentParent->LockMenuUntilClosed(aLock);
    }
  }
}

NS_IMETHODIMP
nsMenuPopupFrame::GetWidget(nsIWidget **aWidget)
{
  nsIView * view = GetRootViewForPopup(this);
  if (!view)
    return NS_OK;

  *aWidget = view->GetWidget();
  NS_IF_ADDREF(*aWidget);
  return NS_OK;
}

void
nsMenuPopupFrame::AttachedDismissalListener()
{
  mConsumeRollupEvent = nsIPopupBoxObject::ROLLUP_DEFAULT;
}

// helpers /////////////////////////////////////////////////////////////

NS_IMETHODIMP 
nsMenuPopupFrame::AttributeChanged(PRInt32 aNameSpaceID,
                                   nsIAtom* aAttribute,
                                   PRInt32 aModType)

{
  nsresult rv = nsBoxFrame::AttributeChanged(aNameSpaceID, aAttribute,
                                             aModType);
  
  if (aAttribute == nsGkAtoms::left || aAttribute == nsGkAtoms::top)
    MoveToAttributePosition();

  if (aAttribute == nsGkAtoms::label) {
    // set the label for the titlebar
    nsIView* view = GetView();
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

  // accessibility needs this to ensure the frames get constructed when the
  // menugenerated attribute is set, see bug 279703 comment 42 for discussion
  if (aAttribute == nsGkAtoms::menugenerated &&
      mFrames.IsEmpty() && !mGeneratedChildren) {
    EnsureWidget();

    // indicate that the children have been generated and then generate them
    mGeneratedChildren = PR_TRUE;
    PresContext()->PresShell()->FrameConstructor()->GenerateChildFrames(this);
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
  PRInt32 err1, err2;
  PRInt32 xpos = left.ToInteger(&err1);
  PRInt32 ypos = top.ToInteger(&err2);

  if (NS_SUCCEEDED(err1) && NS_SUCCEEDED(err2))
    MoveTo(xpos, ypos, PR_FALSE);
}

void
nsMenuPopupFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  nsIFrame* parent = GetParent();
  if (parent && parent->GetType() == nsGkAtoms::menuFrame) {
    // clear the open attribute on the parent menu
    nsContentUtils::AddScriptRunner(
      new nsUnsetAttrRunnable(parent->GetContent(), nsGkAtoms::open));
  }

  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm)
    pm->PopupDestroyed(this);

  nsIRootBox* rootBox =
    nsIRootBox::GetRootBox(PresContext()->GetPresShell());
  if (rootBox && rootBox->GetDefaultTooltip() == mContent) {
    rootBox->SetDefaultTooltip(nsnull);
  }

  nsBoxFrame::DestroyFrom(aDestructRoot);
}


void
nsMenuPopupFrame::MoveTo(PRInt32 aLeft, PRInt32 aTop, PRBool aUpdateAttrs)
{
  if (mScreenXPos == aLeft && mScreenYPos == aTop)
    return;

  // reposition the popup at the specified coordinates. Don't clear the anchor
  // and position, because the popup can be reset to its anchor position by
  // using (-1, -1) as coordinates. Subtract off the margin as it will be
  // added to the position when SetPopupPosition is called.
  nsMargin margin(0, 0, 0, 0);
  GetStyleMargin()->GetMargin(margin);
  nsPresContext* presContext = PresContext();
  mScreenXPos = aLeft - presContext->AppUnitsToIntCSSPixels(margin.left);
  mScreenYPos = aTop - presContext->AppUnitsToIntCSSPixels(margin.top);

  SetPopupPosition(nsnull, PR_TRUE);

  nsCOMPtr<nsIContent> popup = mContent;
  if (aUpdateAttrs && (popup->HasAttr(kNameSpaceID_None, nsGkAtoms::left) ||
                       popup->HasAttr(kNameSpaceID_None, nsGkAtoms::top)))
  {
    nsAutoString left, top;
    left.AppendInt(aLeft);
    top.AppendInt(aTop);
    popup->SetAttr(kNameSpaceID_None, nsGkAtoms::left, left, PR_FALSE);
    popup->SetAttr(kNameSpaceID_None, nsGkAtoms::top, top, PR_FALSE);
  }
}

PRBool
nsMenuPopupFrame::GetAutoPosition()
{
  return mShouldAutoPosition;
}

void
nsMenuPopupFrame::SetAutoPosition(PRBool aShouldAutoPosition)
{
  mShouldAutoPosition = aShouldAutoPosition;
}

void
nsMenuPopupFrame::SetConsumeRollupEvent(PRUint32 aConsumeMode)
{
  mConsumeRollupEvent = aConsumeMode;
}
