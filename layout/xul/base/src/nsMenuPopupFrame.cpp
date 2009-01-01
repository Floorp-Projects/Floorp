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
#include "nsIScrollableView.h"
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
#include "nsIReflowCallback.h"
#include "nsBindingManager.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIBaseWindow.h"
#include "nsISound.h"
#include "nsIRootBox.h"

PRInt8 nsMenuPopupFrame::sDefaultLevelParent = -1;

// NS_NewMenuPopupFrame
//
// Wrapper for creating a new menu popup container
//
nsIFrame*
NS_NewMenuPopupFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMenuPopupFrame (aPresShell, aContext);
}

//
// nsMenuPopupFrame ctor
//
nsMenuPopupFrame::nsMenuPopupFrame(nsIPresShell* aShell, nsStyleContext* aContext)
  :nsBoxFrame(aShell, aContext),
  mCurrentMenu(nsnull),
  mPopupAlignment(POPUPALIGNMENT_NONE),
  mPopupAnchor(POPUPALIGNMENT_NONE),
  mPopupType(ePopupTypePanel),
  mPopupState(ePopupClosed),
  mIsOpenChanged(PR_FALSE),
  mIsContextMenu(PR_FALSE),
  mAdjustOffsetForContextMenu(PR_FALSE),
  mGeneratedChildren(PR_FALSE),
  mMenuCanOverlapOSBar(PR_FALSE),
  mShouldAutoPosition(PR_TRUE),
  mConsumeRollupEvent(nsIPopupBoxObject::ROLLUP_DEFAULT),
  mInContentShell(PR_TRUE),
  mPrefSize(-1, -1)
{
  if (sDefaultLevelParent >= 0)
    return;
  sDefaultLevelParent =
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

  return rv;
}

PRBool
nsMenuPopupFrame::IsNoAutoHide()
{
  // Panels with noautohide="true" don't hide when the mouse is clicked
  // outside of them, or when another application is made active. Non-autohide
  // panels cannot be used in content windows.
  return (!mInContentShell && mPopupType == ePopupTypePanel &&
           mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::noautohide,
                                 nsGkAtoms::_true, eIgnoreCase));
}

PRBool
nsMenuPopupFrame::IsTopMost()
{
  // If this panel is not a panel, this is always a top-most popup
  if (mPopupType != ePopupTypePanel)
    return PR_TRUE;

  // If this panel is a noautohide panel, it should appear just above the parent
  // window.
  if (IsNoAutoHide())
    return PR_FALSE;

  // Otherwise, check the topmost attribute.
  if (mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::level,
                            nsGkAtoms::top, eIgnoreCase))
    return PR_TRUE;

  if (mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::level,
                            nsGkAtoms::parent, eIgnoreCase))
    return PR_FALSE;

  // Otherwise, the result depends on the platform.
  return sDefaultLevelParent ? PR_TRUE : PR_FALSE;
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

  nsTransparencyMode mode = nsLayoutUtils::GetFrameTransparency(this);
  PRBool viewHasTransparentContent = !mInContentShell &&
                                     (eTransparencyTransparent ==
                                      mode);
  nsIContent* parentContent = GetContent()->GetParent();
  nsIAtom *tag = nsnull;
  if (parentContent)
    tag = parentContent->Tag();
  widgetData.mDropShadow = !(viewHasTransparentContent || tag == nsGkAtoms::menulist);

  // panels which are not topmost need a parent widget. This allows them to
  // always appear in front of the parent window but behind other windows that
  // should be in front of it.
  nsCOMPtr<nsIWidget> parentWidget;
  if (!IsTopMost()) {
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

#if defined(XP_MACOSX) || defined(XP_BEOS)
  static NS_DEFINE_IID(kCPopupCID,  NS_POPUP_CID);
  aView->CreateWidget(kCPopupCID, &widgetData, nsnull, PR_TRUE, PR_TRUE, 
                      eContentTypeUI, parentWidget);
#else
  static NS_DEFINE_IID(kCChildCID,  NS_CHILD_CID);
  aView->CreateWidget(kCChildCID, &widgetData, nsnull, PR_TRUE, PR_TRUE,
                      eContentTypeInherit, parentWidget);
#endif
  nsIWidget* widget = aView->GetWidget();
  widget->SetTransparencyMode(mode);
  widget->SetWindowShadowStyle(GetStyleUIReset()->mWindowShadow);
  return NS_OK;
}

// this class is used for dispatching popupshowing events asynchronously.
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
                                      nsIFrame* aChildList)
{
  // unless the list is empty, indicate that children have been generated.
  if (aChildList)
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
  // the parent menu is dependant on the size of the popup, so the frames
  // need to exist in order to calculate this size.
  nsIContent* parentContent = mContent->GetParent();
  return (parentContent &&
          !parentContent->HasAttr(kNameSpaceID_None, nsGkAtoms::sizetopopup));
}

void
nsMenuPopupFrame::SetPreferredBounds(nsBoxLayoutState& aState,
                                     const nsRect& aRect)
{
  nsBox::SetBounds(aState, aRect, PR_FALSE);
  mPrefSize = aRect.Size();
}

void
nsMenuPopupFrame::AdjustView()
{
  if ((mPopupState == ePopupOpen || mPopupState == ePopupOpenAndVisible) &&
      mGeneratedChildren) {
    // if the popup has just opened, make sure the scrolled window is at 0,0
    if (mIsOpenChanged) {
      nsIBox* child = GetChildBox();
      nsCOMPtr<nsIScrollableFrame> scrollframe(do_QueryInterface(child));
      if (scrollframe)
        scrollframe->ScrollTo(nsPoint(0,0));
    }

    nsIView* view = GetView();
    nsIViewManager* viewManager = view->GetViewManager();
    nsRect rect = GetRect();
    rect.x = rect.y = 0;
    viewManager->ResizeView(view, rect);
    viewManager->SetViewVisibility(view, nsViewVisibility_kShow);
    mPopupState = ePopupOpenAndVisible;

    nsPresContext* pc = PresContext();
    nsContainerFrame::SyncFrameViewProperties(pc, this, nsnull, view, 0);

    // fire popupshown event when the state has changed
    if (mIsOpenChanged) {
      mIsOpenChanged = PR_FALSE;
      nsCOMPtr<nsIRunnable> event = new nsXULPopupShownEvent(GetContent(), pc);
      NS_DispatchToCurrentThread(event);
    }
  }
}

void
nsMenuPopupFrame::InitPositionFromAnchorAlign(const nsAString& aAnchor,
                                              const nsAString& aAlign)
{
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
                                  const nsAString& aPosition,
                                  PRInt32 aXPos, PRInt32 aYPos,
                                  PRBool aAttributesOverride)
{
  EnsureWidget();

  mPopupState = ePopupShowing;
  mAnchorContent = aAnchorContent;
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
      mPopupAnchor = POPUPALIGNMENT_NONE;
      mPopupAlignment = POPUPALIGNMENT_NONE;
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
nsMenuPopupFrame::InitializePopupAtScreen(PRInt32 aXPos, PRInt32 aYPos,
                                          PRBool aIsContextMenu)
{
  EnsureWidget();

  mPopupState = ePopupShowing;
  mAnchorContent = nsnull;
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
LazyGeneratePopupDone(nsIContent* aPopup, nsIFrame* aFrame, void* aArg)
{
  // be safe and check the frame type
  if (aFrame->GetType() == nsGkAtoms::menuPopupFrame) {
    nsWeakFrame weakFrame(aFrame);
    nsMenuPopupFrame* popupFrame = static_cast<nsMenuPopupFrame*>(aFrame);

    nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
    if (pm && popupFrame->IsMenu()) {
      nsCOMPtr<nsIContent> popup = aPopup;
      pm->UpdateMenuItems(popup);

      if (!weakFrame.IsAlive())
        return;

      PRBool selectFirstItem = (PRBool)NS_PTR_TO_INT32(aArg);
      if (selectFirstItem) {
        nsMenuFrame* next = pm->GetNextMenuItem(popupFrame, nsnull, PR_TRUE);
        popupFrame->SetCurrentMenuItem(next);
      }
    }

    if (weakFrame.IsAlive()) {
      popupFrame->PresContext()->PresShell()->
        FrameNeedsReflow(popupFrame, nsIPresShell::eTreeChange,
                         NS_FRAME_HAS_DIRTY_CHILDREN);
    }
  }
}


PRBool
nsMenuPopupFrame::ShowPopup(PRBool aIsContextMenu, PRBool aSelectFirstItem)
{
  mIsContextMenu = aIsContextMenu;

  PRBool hasChildren = PR_FALSE;

  if (mPopupState == ePopupShowing) {
    mPopupState = ePopupOpen;
    mIsOpenChanged = PR_TRUE;

    nsIFrame* parent = GetParent();
    if (parent && parent->GetType() == nsGkAtoms::menuFrame) {
      nsWeakFrame weakFrame(this);
      (static_cast<nsMenuFrame*>(parent))->PopupOpened();
      if (!weakFrame.IsAlive())
        return PR_FALSE;
    }

    // the frames for the child menus have not been created yet, so tell the
    // frame constructor to build them
    if (mFrames.IsEmpty() && !mGeneratedChildren) {
      PresContext()->PresShell()->FrameConstructor()->
        AddLazyChildren(mContent, LazyGeneratePopupDone, NS_INT32_TO_PTR(aSelectFirstItem));
    }
    else {
      hasChildren = PR_TRUE;
      PresContext()->PresShell()->
        FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                         NS_FRAME_HAS_DIRTY_CHILDREN);
    }
    if (mPopupType == ePopupTypeMenu) {
      nsCOMPtr<nsISound> sound(do_CreateInstance("@mozilla.org/sound;1"));
      if (sound)
        sound->PlaySystemSound(NS_SYSSOUND_MENU_POPUP);
    }
  }

  mShouldAutoPosition = PR_TRUE;
  return hasChildren;
}

void
nsMenuPopupFrame::HidePopup(PRBool aDeselectMenu, nsPopupState aNewState)
{
  NS_ASSERTION(aNewState == ePopupClosed || aNewState == ePopupInvisible,
               "popup being set to unexpected state");

  // don't hide the popup when it isn't open
  if (mPopupState == ePopupClosed || mPopupState == ePopupShowing)
    return;

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

  mIsOpenChanged = PR_FALSE;
  mCurrentMenu = nsnull; // make sure no current menu is set
 
  nsIView* view = GetView();
  nsIViewManager* viewManager = view->GetViewManager();
  viewManager->SetViewVisibility(view, nsViewVisibility_kHide);
  viewManager->ResizeView(view, nsRect(0, 0, 0, 0));

  FireDOMEvent(NS_LITERAL_STRING("DOMMenuInactive"), mContent);

  // XXX, bug 137033, In Windows, if mouse is outside the window when the menupopup closes, no
  // mouse_enter/mouse_exit event will be fired to clear current hover state, we should clear it manually.
  // This code may not the best solution, but we can leave it here until we find the better approach.
  nsIEventStateManager *esm = PresContext()->EventStateManager();

  PRInt32 state;
  esm->GetContentState(mContent, state);

  if (state & NS_EVENT_STATE_HOVER)
    esm->SetContentState(nsnull, NS_EVENT_STATE_HOVER);

  nsIFrame* parent = GetParent();
  if (parent && parent->GetType() == nsGkAtoms::menuFrame) {
    (static_cast<nsMenuFrame*>(parent))->PopupClosed(aDeselectMenu);
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
    case POPUPALIGNMENT_TOPLEFT:
      pnt = anchorRect.TopLeft();
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
  }

  // If the alignment is on the right edge of the popup, move the popup left
  // by the width. Similarly, if the alignment is on the bottom edge of the
  // popup, move the popup up by the height. In addition, account for the
  // margins of the popup on the edge on which it is aligned.
  nsMargin margin;
  GetStyleMargin()->GetMargin(margin);
  switch (popupAlign) {
    case POPUPALIGNMENT_TOPLEFT:
      pnt.MoveBy(margin.left, margin.top);
      break;
    case POPUPALIGNMENT_TOPRIGHT:
      pnt.MoveBy(-mRect.width - margin.right, margin.top);
      break;
    case POPUPALIGNMENT_BOTTOMLEFT:
      pnt.MoveBy(margin.left, -mRect.height - margin.bottom);
      break;
    case POPUPALIGNMENT_BOTTOMRIGHT:
      pnt.MoveBy(-mRect.width - margin.right, -mRect.height - margin.bottom);
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
                               nscoord aOffsetForContextMenu, PRBool aFlip)
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
        aScreenPoint = aAnchorEnd + aMarginEnd;
        // check if the new position is still off the right or bottom edge of
        // the screen. If so, resize the popup.
        if (aScreenPoint + aSize > aScreenEnd)
          popupSize = aScreenEnd - aScreenPoint;
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
        popupSize = aScreenEnd - aScreenPoint;
      }
      else {
        // flip such that the popup is to the left or top of the anchor point
        // instead.
        aScreenPoint = aAnchorBegin - aSize - aMarginBegin - aOffsetForContextMenu;
        // check if the new position is still off the left or top edge of the
        // screen. If so, resize the popup.
        if (aScreenPoint < aScreenBegin) {
          aScreenPoint = aScreenBegin;
          popupSize = aAnchorBegin - aScreenPoint - aMarginBegin - aOffsetForContextMenu;
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
nsMenuPopupFrame::SetPopupPosition(nsIFrame* aAnchorFrame)
{
  if (!mShouldAutoPosition && !mInContentShell) 
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
      nsCOMPtr<nsIDocument> document = mAnchorContent->GetDocument();
      if (document) {
        nsIPresShell *shell = document->GetPrimaryShell();
        if (!shell)
          return NS_ERROR_FAILURE;

        aAnchorFrame = shell->GetPrimaryFrameFor(mAnchorContent);
      }
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
  nsSize parentSize = aAnchorFrame->GetSize();

  // the anchor may be in a different document with a different scale,
  // so adjust the size so that it is in the app units of the popup instead
  // of the anchor. This is done by converting to device pixels by dividing
  // by the anchor's app units per device pixel and then converting back to
  // app units by multiplying by the popup's app units per device pixel.
  float adj = float(presContext->AppUnitsPerDevPixel()) /
              aAnchorFrame->PresContext()->AppUnitsPerDevPixel();
  parentSize.width = NSToCoordCeil(parentSize.width * adj);
  parentSize.height = NSToCoordCeil(parentSize.height * adj);

  // Set the popup's size to the preferred size. Below, this size will be
  // adjusted to fit on the screen or within the content area. If the anchor
  // is sized to the popup, use the anchor's width instead of the preferred
  // width. The preferred size should already be set by the parent frame.
  NS_ASSERTION(mPrefSize.width >= 0 || mPrefSize.height >= 0,
               "preferred size of popup not set");
  mRect.width = sizedToPopup ? parentSize.width : mPrefSize.width;
  mRect.height = mPrefSize.height;

  // the screen position in app units where the popup should appear
  nsPoint screenPoint;

  // For anchored popups, the anchor rectangle. For non-anchored popups, the
  // size will be 0.
  nsRect anchorRect;

  // indicators of whether the popup should be flipped or resized.
  PRBool hFlip = PR_FALSE, vFlip = PR_FALSE;
  
  nsMargin margin;
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
      anchorRect = aAnchorFrame->GetScreenRectInAppUnits();
      anchorRect.ScaleRoundOut(adj);

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
    screenPoint.x += presContext->CSSPixelsToAppUnits(mXPos);
    screenPoint.y += presContext->CSSPixelsToAppUnits(mYPos);
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
    anchorRect = nsRect(screenPoint, nsSize());

    // add the margins on the popup
    screenPoint.MoveBy(margin.left + offsetForContextMenu,
                       margin.top + offsetForContextMenu);

    // screen positioned popups can be flipped vertically but never horizontally
    vFlip = PR_TRUE;
  }

  // screenRect will hold the rectangle of the available screen space. It
  // will be reduced by the OS chrome such as menubars. It addition, for
  // content shells, it will be the area of the content rather than the
  // screen.
  nsRect screenRect;
  if (mMenuCanOverlapOSBar)
    devContext->GetRect(screenRect);
  else
    devContext->GetClientRect(screenRect);

  // keep a 3 pixel margin to the right and bottom of the screen for the WinXP dropshadow
  screenRect.SizeBy(-nsPresContext::CSSPixelsToAppUnits(3),
                    -nsPresContext::CSSPixelsToAppUnits(3));

  // for content shells, clip to the client area rather than the screen area
  if (mInContentShell)
    screenRect.IntersectRect(screenRect, rootScreenRect);

  // ensure that anchorRect is on screen
  if (!anchorRect.IntersectRect(anchorRect, screenRect)) {
    // if the anchor isn't within the screen, move it to the edge of the screen.
    // IntersectRect will have set both the width and height of anchorRect to 0.
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
                             margin.left, margin.right, offsetForContextMenu, hFlip);
  mRect.height = FlipOrResize(screenPoint.y, mRect.height, screenRect.y,
                              screenRect.YMost(), anchorRect.y, anchorRect.YMost(),
                              margin.top, margin.bottom, offsetForContextMenu, vFlip);

  NS_ASSERTION(screenPoint.x >= screenRect.x && screenPoint.y >= screenRect.y &&
               screenPoint.x + mRect.width <= screenRect.XMost() &&
               screenPoint.y + mRect.height <= screenRect.YMost(),
               "Popup is offscreen");

  // determine the x and y position of the view by subtracting the desired
  // screen position from the screen position of the root frame.
  nsPoint viewPoint = screenPoint - rootScreenRect.TopLeft();
  presContext->GetViewManager()->MoveViewTo(GetView(), viewPoint.x, viewPoint.y); 

  // Now that we've positioned the view, sync up the frame's origin.
  nsBoxFrame::SetPosition(viewPoint - GetParent()->GetOffsetTo(rootFrame));

  if (sizedToPopup) {
    nsBoxLayoutState state(PresContext());
    // XXXndeakin can parentSize.width still extend outside?
    SetBounds(state, nsRect(mRect.x, mRect.y, parentSize.width, mRect.height));
  }

  return NS_OK;
}

/* virtual */ nsMenuFrame*
nsMenuPopupFrame::GetCurrentMenuItem()
{
  return mCurrentMenu;
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

static nsIScrollableView* GetScrollableViewForFrame(nsIFrame* aFrame)
{
  nsIScrollableFrame* sf;
  nsresult rv = CallQueryInterface(aFrame, &sf);
  if (NS_FAILED(rv))
    return nsnull;
  return sf->GetScrollableView();
}

// XXXroc this is megalame. Fossicking around for a view of the right
// type is a recipe for disaster in the long term.
nsIScrollableView* nsMenuPopupFrame::GetScrollableView(nsIFrame* aStart)
{
  if ( ! aStart )
    return nsnull;  

  nsIFrame* currFrame;
  nsIScrollableView* scrollableView=nsnull;

  // try start frame and siblings
  currFrame=aStart;
  do {
    scrollableView = GetScrollableViewForFrame(currFrame);
    if ( scrollableView )
      return scrollableView;
    currFrame = currFrame->GetNextSibling();
  } while ( currFrame );

  // try children
  nsIFrame* childFrame;
  currFrame=aStart;
  do {
    childFrame = currFrame->GetFirstChild(nsnull);
    scrollableView=GetScrollableView(childFrame);
    if ( scrollableView )
      return scrollableView;
    currFrame = currFrame->GetNextSibling();
  } while ( currFrame );

  return nsnull;
}

void nsMenuPopupFrame::EnsureMenuItemIsVisible(nsMenuFrame* aMenuItem)
{
  if (aMenuItem) {
    nsIFrame* childFrame = GetFirstChild(nsnull);
    nsIScrollableView *scrollableView;
    scrollableView = GetScrollableView(childFrame);
    if (scrollableView) {
      nscoord scrollX, scrollY;

      nsRect viewRect = scrollableView->View()->GetBounds();
      nsRect itemRect = aMenuItem->GetRect();
      scrollableView->GetScrollPosition(scrollX, scrollY);
  
      // scroll down
      if ( itemRect.y + itemRect.height > scrollY + viewRect.height )
        scrollableView->ScrollTo(scrollX, itemRect.y + itemRect.height - viewRect.height, 0);
      
      // scroll up
      else if ( itemRect.y < scrollY )
        scrollableView->ScrollTo(scrollX, itemRect.y, 0);
    }
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

  // accessibility needs this to ensure the frames get constructed when the
  // menugenerated attribute is set, see bug 279703 comment 42 for discussion
  if (aAttribute == nsGkAtoms::menugenerated &&
      mFrames.IsEmpty() && !mGeneratedChildren) {
    EnsureWidget();
    PresContext()->PresShell()->FrameConstructor()->
      AddLazyChildren(mContent, LazyGeneratePopupDone, nsnull, PR_TRUE);
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
  mScreenXPos = left.ToInteger(&err1);
  mScreenYPos = top.ToInteger(&err2);

  if (NS_SUCCEEDED(err1) && NS_SUCCEEDED(err2))
    MoveToInternal(mScreenXPos, mScreenYPos);
}

void
nsMenuPopupFrame::Destroy()
{
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm)
    pm->PopupDestroyed(this);

  nsIRootBox* rootBox =
    nsIRootBox::GetRootBox(PresContext()->GetPresShell());
  if (rootBox && rootBox->GetDefaultTooltip() == mContent) {
    rootBox->SetDefaultTooltip(nsnull);
  }

  nsBoxFrame::Destroy();
}

void
nsMenuPopupFrame::MoveTo(PRInt32 aLeft, PRInt32 aTop)
{
  // Set the 'left' and 'top' attributes
  nsAutoString left, top;
  left.AppendInt(aLeft);
  top.AppendInt(aTop);

  nsWeakFrame weakFrame(this);
  mContent->SetAttr(kNameSpaceID_None, nsGkAtoms::left, left, PR_FALSE);
  if (!weakFrame.IsAlive()) {
    return;
  }
  mContent->SetAttr(kNameSpaceID_None, nsGkAtoms::top, top, PR_FALSE);
  if (!weakFrame.IsAlive()) {
    return;
  }

  MoveToInternal(aLeft, aTop);
}

void
nsMenuPopupFrame::MoveToInternal(PRInt32 aLeft, PRInt32 aTop)
{
  // just don't support moving popups for content shells
  if (mInContentShell)
    return;

  nsIView* view = GetView();
  NS_ASSERTION(view->GetParent(), "Must have parent!");

  // Retrieve screen position of parent view
  nsIntPoint screenPos = view->GetParent()->GetScreenPosition();

  nsPresContext* context = PresContext();
  aLeft = context->AppUnitsToDevPixels(nsPresContext::CSSPixelsToAppUnits(aLeft));
  aTop = context->AppUnitsToDevPixels(nsPresContext::CSSPixelsToAppUnits(aTop));

  // Move the widget. The widget will be null if it hasn't been created yet,
  // but that's OK as the popup won't be open in this case.
  // XXXbz don't we want screenPos to be the parent _widget_'s position, then?
  nsIWidget* widget = view->GetWidget();
  if (widget) 
    widget->Move(aLeft - screenPos.x, aTop - screenPos.y);
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
