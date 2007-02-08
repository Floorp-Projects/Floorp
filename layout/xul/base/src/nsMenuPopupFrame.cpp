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
#include "nsIPopupSetFrame.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMScreen.h"
#include "nsIPresShell.h"
#include "nsFrameManager.h"
#include "nsIDocument.h"
#include "nsIDeviceContext.h"
#include "nsRect.h"
#include "nsIDOMXULDocument.h"
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
#include "nsCSSFrameConstructor.h"
#include "nsIBoxLayout.h"
#include "nsIPopupBoxObject.h"
#ifdef XP_WIN
#include "nsISound.h"
#endif

const PRInt32 kMaxZ = 0x7fffffff; //XXX: Shouldn't there be a define somewhere for MaxInt for PRInt32


static nsIPopupSetFrame*
GetPopupSetFrame(nsPresContext* aPresContext)
{
  nsIRootBox* rootBox = nsIRootBox::GetRootBox(aPresContext->PresShell());
  if (!rootBox)
    return nsnull;

  nsIFrame* popupSetFrame = rootBox->GetPopupSetFrame();
  if (!popupSetFrame)
    return nsnull;

  nsIPopupSetFrame* popupSet = nsnull;
  CallQueryInterface(popupSetFrame, &popupSet);
  return popupSet;
}


// NS_NewMenuPopupFrame
//
// Wrapper for creating a new menu popup container
//
nsIFrame*
NS_NewMenuPopupFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMenuPopupFrame (aPresShell, aContext);
}

NS_IMETHODIMP_(nsrefcnt) 
nsMenuPopupFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt) 
nsMenuPopupFrame::Release(void)
{
    return NS_OK;
}


//
// QueryInterface
//
NS_INTERFACE_MAP_BEGIN(nsMenuPopupFrame)
  NS_INTERFACE_MAP_ENTRY(nsIMenuParent)
NS_INTERFACE_MAP_END_INHERITING(nsBoxFrame)


//
// nsMenuPopupFrame ctor
//
nsMenuPopupFrame::nsMenuPopupFrame(nsIPresShell* aShell, nsStyleContext* aContext)
  :nsBoxFrame(aShell, aContext),
  mCurrentMenu(nsnull),
  mTimerMenu(nsnull),
  mCloseTimer(nsnull),
  mMenuCanOverlapOSBar(PR_FALSE),
  mShouldAutoPosition(PR_TRUE),
  mShouldRollup(PR_TRUE),
  mConsumeRollupEvent(nsIPopupBoxObject::ROLLUP_DEFAULT)
{
  SetIsContextMenu(PR_FALSE);   // we're not a context menu by default
} // ctor


NS_IMETHODIMP
nsMenuPopupFrame::Init(nsIContent*      aContent,
                       nsIFrame*        aParent,
                       nsIFrame*        aPrevInFlow)
{
  nsresult rv = nsBoxFrame::Init(aContent, aParent, aPrevInFlow);

  // Set up a mediator which can be used for callbacks on this frame.
  mTimerMediator = new nsMenuPopupTimerMediator(this);
  if (NS_UNLIKELY(!mTimerMediator))
    return NS_ERROR_OUT_OF_MEMORY;

  nsPresContext* presContext = GetPresContext();

  // lookup if we're allowed to overlap the OS bar (menubar/taskbar) from the
  // look&feel object
  PRBool tempBool;
  presContext->LookAndFeel()->
    GetMetric(nsILookAndFeel::eMetric_MenusCanOverlapOSBar, tempBool);
  mMenuCanOverlapOSBar = tempBool;

  CreateViewForFrame(presContext, this, GetStyleContext(), PR_TRUE);

  // Now that we've made a view, remove it and insert it at the correct
  // position in the view hierarchy (as the root view).  We do this so that we
  // can draw the menus outside the confines of the window.
  nsIView* ourView = GetView();
  nsIViewManager* viewManager = ourView->GetViewManager();

  // Remove the view from its old position.
  viewManager->RemoveChild(ourView);

  // Reinsert ourselves as the root view with a maximum z-index.
  nsIView* rootView;
  viewManager->GetRootView(rootView);
  viewManager->SetViewZIndex(ourView, PR_FALSE, kMaxZ);
  viewManager->InsertChild(rootView, ourView, nsnull, PR_TRUE);

  // XXX Hack. The menu's view should float above all other views,
  // so we use the nsIView::SetFloating() to tell the view manager
  // about that constraint.
  viewManager->SetViewFloating(ourView, PR_TRUE);

  // XXX Hack. Change our transparency to be non-transparent
  // until the bug related to update of transparency on show/hide
  // is fixed.
  viewManager->SetViewContentTransparency(ourView, PR_FALSE);

  // XXX make sure we are hidden (shouldn't this be done automatically?)
  viewManager->SetViewVisibility(ourView, nsViewVisibility_kHide);
  if (!ourView->HasWidget()) {
    CreateWidgetForView(ourView);
  }

  MoveToAttributePosition();

  return rv;
}

nsresult
nsMenuPopupFrame::CreateWidgetForView(nsIView* aView)
{
  // Create a widget for ourselves.
  nsWidgetInitData widgetData;
  widgetData.mWindowType = eWindowType_popup;
  widgetData.mBorderStyle = eBorderStyle_default;
  widgetData.clipSiblings = PR_TRUE;

  PRBool isCanvas;
  const nsStyleBackground* bg;
  PRBool hasBG =
    nsCSSRendering::FindBackground(GetPresContext(), this, &bg, &isCanvas);
  PRBool viewHasTransparentContent = hasBG &&
    (bg->mBackgroundFlags & NS_STYLE_BG_COLOR_TRANSPARENT) &&
    !GetStyleDisplay()->mAppearance;
  if (viewHasTransparentContent) {
    nsCOMPtr<nsISupports> cont = GetPresContext()->GetContainer();
    nsCOMPtr<nsIDocShellTreeItem> dsti = do_QueryInterface(cont);
    PRInt32 type = -1;
    if (!dsti || NS_FAILED(dsti->GetItemType(&type)) ||
        type != nsIDocShellTreeItem::typeChrome)
      viewHasTransparentContent = PR_FALSE;
  }

  nsIContent* parentContent = GetContent()->GetParent();
  nsIAtom *tag = nsnull;
  if (parentContent)
    tag = parentContent->Tag();
  widgetData.mDropShadow = !(viewHasTransparentContent || tag == nsGkAtoms::menulist);
  
#if defined(XP_MACOSX) || defined(XP_BEOS)
  static NS_DEFINE_IID(kCPopupCID,  NS_POPUP_CID);
  aView->CreateWidget(kCPopupCID, &widgetData, nsnull, PR_TRUE, PR_TRUE, 
                      eContentTypeUI);
#else
  static NS_DEFINE_IID(kCChildCID,  NS_CHILD_CID);
  aView->CreateWidget(kCChildCID, &widgetData, nsnull, PR_TRUE, PR_TRUE);
#endif
  aView->GetWidget()->SetWindowTranslucency(viewHasTransparentContent);
  return NS_OK;
}

void
nsMenuPopupFrame::InvalidateInternal(const nsRect& aDamageRect,
                                     nscoord aX, nscoord aY, nsIFrame* aForChild,
                                     PRBool aImmediate)
{
  InvalidateRoot(aDamageRect, aX, aY, aImmediate);
}

void
nsMenuPopupFrame::GetLayoutFlags(PRUint32& aFlags)
{
  aFlags = NS_FRAME_NO_SIZE_VIEW | NS_FRAME_NO_MOVE_VIEW | NS_FRAME_NO_VISIBILITY;
}

///////////////////////////////////////////////////////////////////////////////
// GetViewOffset
//   Retrieves the offset of the given view with the root view, in the 
//   coordinate system of the root view. 
void
nsMenuPopupFrame::GetViewOffset(nsIView* aView, nsPoint& aPoint)
{
  // Notes:
  //   1) The root view is the client area of the toplevel window that
  //      this popup is anchored to. 
  //   2) Each menupopup is a child of the root view (see 
  //      nsMenuPopupFrame::Init())
  //   3) The coordinates that we return are the total distance between 
  //      the top left of the start view and the origin of the root view.
  
  // Keep track of the root view so that we know to stop there
  nsIView* rootView;
  aView->GetViewManager()->GetRootView(rootView);
  aPoint = aView->GetOffsetTo(rootView);
}

///////////////////////////////////////////////////////////////////////////////
// GetRootViewForPopup
//   Retrieves the view for the popup widget that contains the given frame. 
//   If the given frame is not contained by a popup widget, return the
//   root view.  This is the root view of the pres context's
//   viewmanager if aStopAtViewManagerRoot is true; otherwise it's the
//   root view of the root viewmanager.
void
nsMenuPopupFrame::GetRootViewForPopup(nsIFrame* aStartFrame,
                                      PRBool    aStopAtViewManagerRoot,
                                      nsIView** aResult)
{
  *aResult = nsnull;

  nsIView* view = aStartFrame->GetClosestView();
  NS_ASSERTION(view, "frame must have a closest view!");
  if (view) {
    nsIView* rootView = nsnull;
    if (aStopAtViewManagerRoot) {
      view->GetViewManager()->GetRootView(rootView);
    }
    
    while (view) {
      // Walk up the view hierarchy looking for a view whose widget has a 
      // window type of eWindowType_popup - in other words a popup window
      // widget. If we find one, this is the view we want. 
      nsIWidget* widget = view->GetWidget();
      if (widget) {
        nsWindowType wtype;
        widget->GetWindowType(wtype);
        if (wtype == eWindowType_popup) {
          *aResult = view;
          return;
        }
      }

      if (aStopAtViewManagerRoot && view == rootView) {
        *aResult = view;
        return;
      }

      nsIView* temp = view->GetParent();
      if (!temp) {
        // Otherwise, we've walked all the way up to the root view and not
        // found a view for a popup window widget. Just return the root view.
        *aResult = view;
      }
      view = temp;
    }
  }
}


//
// AdjustClientXYForNestedDocuments
// 
// almost certainly, the document where the mouse was clicked is not
// the document that contains the popup, especially if we're viewing a page
// with frames. Thus we need to make adjustments to the client coordinates to
// take this into account and get them back into the relative coordinates of
// this document.
//
void
nsMenuPopupFrame::AdjustClientXYForNestedDocuments ( nsIDOMXULDocument* inPopupDoc, nsIPresShell* inPopupShell, 
                                                         PRInt32 inClientX, PRInt32 inClientY, 
                                                         PRInt32* outAdjX, PRInt32* outAdjY )
{
  if ( !inPopupDoc || !outAdjX || !outAdjY )
    return;

  // Find the widget associated with the popup's document
  nsIWidget* popupDocumentWidget = nsnull;
  nsIViewManager* viewManager = inPopupShell->GetViewManager();
  if ( viewManager ) {  
    nsIView* rootView;
    viewManager->GetRootView(rootView);
    if ( rootView )
      popupDocumentWidget = rootView->GetNearestWidget(nsnull);
  }
  NS_ASSERTION(popupDocumentWidget, "ACK, BAD WIDGET");
  
  // Find the widget associated with the target's document.
  // For tooltips, we check the document's tooltipNode (which is set by
  // nsXULTooltipListener).  For regular popups, use popupNode (set by
  // nsXULPopupListener).

  nsCOMPtr<nsIDOMNode> targetNode;
  if (mContent->Tag() == nsGkAtoms::tooltip)
    inPopupDoc->TrustedGetTooltipNode(getter_AddRefs(targetNode));
  else
    inPopupDoc->TrustedGetPopupNode(getter_AddRefs(targetNode));

  //NS_ASSERTION(targetNode, "no popup/tooltip node on document!");
  nsCOMPtr<nsIContent> targetAsContent ( do_QueryInterface(targetNode) );
  nsIWidget* targetDocumentWidget = nsnull;
  if ( targetAsContent ) {
    nsCOMPtr<nsIDocument> targetDocument = targetAsContent->GetDocument();
    if (targetDocument) {
      nsIPresShell *shell = targetDocument->GetShellAt(0);
      if ( shell ) {
        // We might be inside a popup widget. If so, we need to use that widget and
        // not the root view's widget.
        nsIFrame* targetFrame = shell->GetPrimaryFrameFor(targetAsContent);
        nsIView* parentView = nsnull;
        if (targetFrame) {
          GetRootViewForPopup(targetFrame, PR_TRUE, &parentView);
          if (parentView) {
            targetDocumentWidget = parentView->GetNearestWidget(nsnull);
          }
        }
        if (!targetDocumentWidget) {
          // We aren't inside a popup. This means we should use the root view's
          // widget.
          nsIViewManager* viewManagerTarget = shell->GetViewManager();
          if ( viewManagerTarget ) {
            nsIView* rootViewTarget;
            viewManagerTarget->GetRootView(rootViewTarget);
            if ( rootViewTarget ) {
              targetDocumentWidget = rootViewTarget->GetNearestWidget(nsnull);
            }
          }
        }
      }
    }
  }
  //NS_ASSERTION(targetDocumentWidget, "ACK, BAD TARGET");

  // the offset we need is the difference between the upper left corner of the two widgets. Use
  // screen coordinates to find the global offset between them.
  nsRect popupDocTopLeft;
  if ( popupDocumentWidget ) {
    nsRect topLeftClient ( 0, 0, 10, 10 );
    popupDocumentWidget->WidgetToScreen ( topLeftClient, popupDocTopLeft );
  }
  nsRect targetDocTopLeft;
  if ( targetDocumentWidget ) {
    nsRect topLeftClient ( 0, 0, 10, 10 );
    targetDocumentWidget->WidgetToScreen ( topLeftClient, targetDocTopLeft );
  }
  nsPoint pixelOffset ( targetDocTopLeft.x - popupDocTopLeft.x, targetDocTopLeft.y - popupDocTopLeft.y );

  nsPresContext* context = GetPresContext();
  *outAdjX = nsPresContext::CSSPixelsToAppUnits(inClientX) +
             context->DevPixelsToAppUnits(pixelOffset.x);
  *outAdjY = nsPresContext::CSSPixelsToAppUnits(inClientY) +
             context->DevPixelsToAppUnits(pixelOffset.y);
  
} // AdjustClientXYForNestedDocuments


//
// AdjustPositionForAnchorAlign
// 
// Uses the |popupanchor| and |popupalign| attributes on the popup to move the popup around and
// anchor it to its parent. |outFlushWithTopBottom| will be TRUE if the popup is flush with either
// the top or bottom edge of its parent, and FALSE if it is flush with the left or right edge of
// the parent.
// 
void
nsMenuPopupFrame::AdjustPositionForAnchorAlign ( PRInt32* ioXPos, PRInt32* ioYPos, const nsRect & inParentRect,
                                                    const nsString& aPopupAnchor, const nsString& aPopupAlign,
                                                    PRBool* outFlushWithTopBottom )
{
  nsAutoString popupAnchor(aPopupAnchor);
  nsAutoString popupAlign(aPopupAlign);

  if (GetStyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL) {
    if (popupAnchor.EqualsLiteral("topright"))
      popupAnchor.AssignLiteral("topleft");
    else if (popupAnchor.EqualsLiteral("topleft"))
      popupAnchor.AssignLiteral("topright");
    else if (popupAnchor.EqualsLiteral("bottomleft"))
      popupAnchor.AssignLiteral("bottomright");
    else if (popupAnchor.EqualsLiteral("bottomright"))
      popupAnchor.AssignLiteral("bottomleft");

    if (popupAlign.EqualsLiteral("topright"))
      popupAlign.AssignLiteral("topleft");
    else if (popupAlign.EqualsLiteral("topleft"))
      popupAlign.AssignLiteral("topright");
    else if (popupAlign.EqualsLiteral("bottomleft"))
      popupAlign.AssignLiteral("bottomright");
    else if (popupAnchor.EqualsLiteral("bottomright"))
      popupAlign.AssignLiteral("bottomleft");
  }

  // Adjust position for margins at the aligned corner
  nsMargin margin;
  GetStyleMargin()->GetMargin(margin);
  if (popupAlign.EqualsLiteral("topleft")) {
    *ioXPos += margin.left;
    *ioYPos += margin.top;
  } else if (popupAlign.EqualsLiteral("topright")) {
    *ioXPos += margin.right;
    *ioYPos += margin.top;
  } else if (popupAlign.EqualsLiteral("bottomleft")) {
    *ioXPos += margin.left;
    *ioYPos += margin.bottom;
  } else if (popupAlign.EqualsLiteral("bottomright")) {
    *ioXPos += margin.right;
    *ioYPos += margin.bottom;
  }
  
  if (popupAnchor.EqualsLiteral("topright") && popupAlign.EqualsLiteral("topleft")) {
    *ioXPos += inParentRect.width;
  }
  else if (popupAnchor.EqualsLiteral("topleft") && popupAlign.EqualsLiteral("topleft")) {
    *outFlushWithTopBottom = PR_TRUE;
  }
  else if (popupAnchor.EqualsLiteral("topright") && popupAlign.EqualsLiteral("bottomright")) {
    *ioXPos -= (mRect.width - inParentRect.width);
    *ioYPos -= mRect.height;
    *outFlushWithTopBottom = PR_TRUE;
  }
  else if (popupAnchor.EqualsLiteral("bottomright") && popupAlign.EqualsLiteral("bottomleft")) {
    *ioXPos += inParentRect.width;
    *ioYPos -= (mRect.height - inParentRect.height);
  }
  else if (popupAnchor.EqualsLiteral("bottomright") && popupAlign.EqualsLiteral("topright")) {
    *ioXPos -= (mRect.width - inParentRect.width);
    *ioYPos += inParentRect.height;
    *outFlushWithTopBottom = PR_TRUE;
  }
  else if (popupAnchor.EqualsLiteral("topleft") && popupAlign.EqualsLiteral("topright")) {
    *ioXPos -= mRect.width;
  }
  else if (popupAnchor.EqualsLiteral("topleft") && popupAlign.EqualsLiteral("bottomleft")) {
    *ioYPos -= mRect.height;
    *outFlushWithTopBottom = PR_TRUE;
  }
  else if (popupAnchor.EqualsLiteral("bottomleft") && popupAlign.EqualsLiteral("bottomright")) {
    *ioXPos -= mRect.width;
    *ioYPos -= (mRect.height - inParentRect.height);
  }
  else if (popupAnchor.EqualsLiteral("bottomleft") && popupAlign.EqualsLiteral("topleft")) {
    *ioYPos += inParentRect.height;
    *outFlushWithTopBottom = PR_TRUE;
  }
  else
    NS_WARNING ( "Hmmm, looks like you've hit a anchor/align case we weren't setup for." );

} // AdjustPositionForAnchorAlign


//
// IsMoreRoomOnOtherSideOfParent
//
// Determine if there is more room on the screen for the popup to live if it was positioned
// on the flip side of the parent from the side it is flush against (ie, if it's top edge was
// flush against the bottom, is there more room if its bottom edge were flush against the top)
//
PRBool
nsMenuPopupFrame::IsMoreRoomOnOtherSideOfParent ( PRBool inFlushAboveBelow, PRInt32 inScreenViewLocX, PRInt32 inScreenViewLocY,
                                                     const nsRect & inScreenParentFrameRect, PRInt32 inScreenTopTwips, PRInt32 inScreenLeftTwips,
                                                     PRInt32 inScreenBottomTwips, PRInt32 inScreenRightTwips )
{
  PRBool switchSides = PR_FALSE;
  if ( inFlushAboveBelow ) {
    PRInt32 availAbove = inScreenParentFrameRect.y - inScreenTopTwips;
    PRInt32 availBelow = inScreenBottomTwips - (inScreenParentFrameRect.y + inScreenParentFrameRect.height) ;
    if ( inScreenViewLocY > inScreenParentFrameRect.y )       // view is now below parent
      switchSides = availAbove > availBelow;
    else
      switchSides = availBelow > availAbove;
  }
  else {
    PRInt32 availLeft = inScreenParentFrameRect.x - inScreenLeftTwips;
    PRInt32 availRight = inScreenRightTwips - (inScreenParentFrameRect.x + inScreenParentFrameRect.width) ;
    if ( inScreenViewLocX > inScreenParentFrameRect.x )       // view is now to the right of parent
      switchSides = availLeft > availRight;
    else
      switchSides = availRight > availLeft;           
  }

  return switchSides;
  
} // IsMoreRoomOnOtherSideOfParent


//
// MovePopupToOtherSideOfParent
//
// Move the popup to the other side of the parent (ie, if it the popup's top edge is flush against the
// bottom of its parent, move the popup so that its bottom edge is now flush against the top of its
// parent...same idea for left/right).
//
// NOTE: In moving the popup, it may need to change size in order to stay on the screen. This will
//       have the side effect of touching |mRect|.
//
void
nsMenuPopupFrame::MovePopupToOtherSideOfParent ( PRBool inFlushAboveBelow, PRInt32* ioXPos, PRInt32* ioYPos, 
                                                 PRInt32* ioScreenViewLocX, PRInt32* ioScreenViewLocY,
                                                 const nsRect & inScreenParentFrameRect, PRInt32 inScreenTopTwips, PRInt32 inScreenLeftTwips,
                                                 PRInt32 inScreenBottomTwips, PRInt32 inScreenRightTwips )
{
  if ( inFlushAboveBelow ) {
    if ( *ioScreenViewLocY > inScreenParentFrameRect.y ) {     // view is currently below parent
      // move it above.
      PRInt32 shiftDistY = inScreenParentFrameRect.height + mRect.height;
      *ioYPos -= shiftDistY;
      *ioScreenViewLocY -= shiftDistY;
      // trim it to fit.
      if ( *ioScreenViewLocY < inScreenTopTwips ) {
        PRInt32 trimY = inScreenTopTwips - *ioScreenViewLocY;
        *ioYPos += trimY;
        *ioScreenViewLocY += trimY;
        mRect.height -= trimY;
      }
    }
    else {                                               // view is currently above parent
      // move it below
      PRInt32 shiftDistY = inScreenParentFrameRect.height + mRect.height;
      *ioYPos += shiftDistY;
      *ioScreenViewLocY += shiftDistY;
    }
  }
  else {
    if ( *ioScreenViewLocX > inScreenParentFrameRect.x ) {     // view is currently to the right of the parent
      // move it to the left.
      PRInt32 shiftDistX = inScreenParentFrameRect.width + mRect.width;
      *ioXPos -= shiftDistX;
      *ioScreenViewLocX -= shiftDistX;
      // trim it to fit.
      if ( *ioScreenViewLocX < inScreenLeftTwips ) {
        PRInt32 trimX = inScreenLeftTwips - *ioScreenViewLocX;
        *ioXPos += trimX;
        *ioScreenViewLocX += trimX;
        mRect.width -= trimX;
      }
    }
    else {                                               // view is currently to the right of the parent
      // move it to the right
      PRInt32 shiftDistX = inScreenParentFrameRect.width + mRect.width;
      *ioXPos += shiftDistX;
      *ioScreenViewLocX += shiftDistX;
    }               
  }

} // MovePopupToOtherSideOfParent

class nsASyncMenuActivation : public nsRunnable
{
public:
  nsASyncMenuActivation(nsIContent* aContent)
    : mContent(aContent)
  {
  }

  NS_IMETHOD Run() {
    if (mContent &&
        !mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::menuactive,
                               nsGkAtoms::_true, eCaseMatters) &&
        mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::menutobedisplayed,
                              nsGkAtoms::_true, eCaseMatters)) {
      mContent->SetAttr(kNameSpaceID_None, nsGkAtoms::menuactive,
                        NS_LITERAL_STRING("true"), PR_TRUE);
    }
    return NS_OK;
  }

  nsCOMPtr<nsIContent> mContent;
};

nsresult 
nsMenuPopupFrame::SyncViewWithFrame(nsPresContext* aPresContext,
                                    const nsString& aPopupAnchor,
                                    const nsString& aPopupAlign,
                                    nsIFrame* aFrame, 
                                    PRInt32 aXPos, PRInt32 aYPos)
{
  NS_ENSURE_ARG(aPresContext);
  NS_ENSURE_ARG(aFrame);

  if (!mShouldAutoPosition) 
    return NS_OK;

  // |containingView|
  //   The view that contains the frame that is invoking this popup. This is 
  //   the canvas view inside the scrollport view. It can have negative bounds
  //   if the canvas is scrolled so that part is off screen.
  nsIView* containingView = nsnull;
  nsPoint offset;
  nsMargin margin;
  containingView = aFrame->GetClosestView(&offset);
  if (!containingView)
    return NS_OK;

  // |view|
  //   The root view for the popup window widget associated with this frame,
  //   or, the view associated with this frame. 
  nsIView* view = GetView();

  // |parentPos|
  //   The distance between the containingView and the root view. This provides
  //   a hint as to where to position the menu relative to the window. 
  nsPoint parentPos;
  GetViewOffset(containingView, parentPos);

  // |parentRect|
  //   The dimensions of the frame invoking the popup. 
  nsRect parentRect = aFrame->GetRect();

  // get the document and the global script object
  nsIPresShell *presShell = aPresContext->PresShell();
  nsIDocument *document = presShell->GetDocument();

  PRBool sizedToPopup = (mContent->Tag() != nsGkAtoms::tooltip) &&
    (nsMenuFrame::IsSizedToPopup(aFrame->GetContent(), PR_FALSE));

  // If we stick to our parent's width, set it here before we move the
  // window around, because moving is done with respect to the width...
  if (sizedToPopup) {
    mRect.width = parentRect.width;
  }

  // |xpos| and |ypos| hold the x and y positions of where the popup will be moved to,
  // in _twips_, in the coordinate system of the _parent view_.
  PRInt32 xpos = 0, ypos = 0;

  // if we are anchored to our parent, there are certain things we don't want to do
  // when repositioning the view to fit on the screen, such as end up positioned over
  // the parent. When doing this reposition, we want to move the popup to the side with
  // the most room. The combination of anchor and alignment dictate if we readjst 
  // above/below or to the left/right.
  PRBool anchoredToParent = PR_FALSE;
  PRBool readjustAboveBelow = PR_FALSE;

  if ( aXPos != -1 || aYPos != -1 ) {
  
    // for this case, we've been handed a specific x/y location (in client coordinates) for
    // the popup. However, we may be deeply nested in a frameset, etc and so the client coordinates
    // need some adjusting. 
    nsCOMPtr<nsIDOMXULDocument> xulDoc ( do_QueryInterface(document) );
    AdjustClientXYForNestedDocuments ( xulDoc, presShell, aXPos, aYPos, &xpos, &ypos );

    // Add in the top and left margins
    GetStyleMargin()->GetMargin(margin);

    xpos += margin.left;
    ypos += margin.top;
  } 
  else {
    anchoredToParent = PR_TRUE;

    xpos = parentPos.x + offset.x;
    ypos = parentPos.y + offset.y;
    
    // move the popup according to the anchor/alignment attributes. This will also tell us
    // which axis the popup is flush against in case we have to move it around later.
    AdjustPositionForAnchorAlign ( &xpos, &ypos, parentRect, aPopupAnchor, aPopupAlign, &readjustAboveBelow );    
  }
  
  // Compute info about the screen dimensions. Because of multiple monitor systems,
  // the left or top sides of the screen may be in negative space (main monitor is on the
  // right, etc). We need to be sure to do the right thing.
  nsPIDOMWindow *window = document->GetWindow();
  if (!window)
    return NS_OK;

  nsIDeviceContext* devContext = GetPresContext()->DeviceContext();
  nsRect rect;
  if ( mMenuCanOverlapOSBar ) {
    devContext->GetRect(rect);
  }
  else {
    devContext->GetClientRect(rect);
  }

  // keep 3px margin to the right and bottom of the screen for WinXP dropshadow
  rect.width  -= nsPresContext::CSSPixelsToAppUnits(3);
  rect.height -= nsPresContext::CSSPixelsToAppUnits(3);

  PRInt32 screenLeftTwips   = rect.x;
  PRInt32 screenTopTwips    = rect.y;
  PRInt32 screenWidthTwips  = rect.width;
  PRInt32 screenHeightTwips = rect.height;
  PRInt32 screenRightTwips  = rect.XMost();
  PRInt32 screenBottomTwips = rect.YMost();
  
  // Recall that |xpos| and |ypos| are in the coordinate system of the parent view. In
  // order to determine the screen coordinates of where our view will end up, we
  // need to find the x/y position of the parent view in screen coords. That is done
  // by getting the widget associated with the parent view and determining the offset 
  // based on converting (0,0) in its coordinate space to screen coords. We then
  // offset that point by (|xpos|,|ypos|) to get the true screen coordinates of
  // the view. *whew*

  // |parentView|
  //   The root view for the window that contains the frame, for frames inside 
  //   menupopups this is the first view inside the popup window widget, for 
  //   frames inside a toplevel window, this is the root view of the toplevel
  //   window.
  nsIView* parentView = nsnull;
  GetRootViewForPopup(aFrame, PR_FALSE, &parentView);
  if (!parentView)
    return NS_OK;

  // Use containingView instead of parentView, to account for the scrollarrows
  // that a parent menu might have.

  nsPoint parentViewWidgetOffset;
  nsIWidget* parentViewWidget = containingView->GetNearestWidget(&parentViewWidgetOffset);
  nsRect localParentWidgetRect(0,0,0,0), screenParentWidgetRect;
  parentViewWidget->WidgetToScreen ( localParentWidgetRect, screenParentWidgetRect );
  PRInt32 screenViewLocX = aPresContext->DevPixelsToAppUnits(screenParentWidgetRect.x) +
    (xpos - parentPos.x) + parentViewWidgetOffset.x;
  PRInt32 screenViewLocY = aPresContext->DevPixelsToAppUnits(screenParentWidgetRect.y) +
    (ypos - parentPos.y) + parentViewWidgetOffset.y;

  if ( anchoredToParent ) {
    
    //
    // Popup is anchored to the parent, guarantee that it does not cover the parent. We
    // shouldn't do anything funky if it will already fit on the screen as is.
    //

    ///////////////////////////////////////////////////////////////////////////////
    //
    //                +------------------------+          
    //                |           /\           |
    // parentPos -> - +------------------------+
    //              | |                        |
    //       offset | |                        |
    //              | |                        |
    //              | |                        | (screenViewLocX,screenViewLocY)
    //              - |========================|+--------------
    //                | parentRect           > ||
    //                |========================||
    //                |                        || Submenu 
    //                +------------------------+|  ( = mRect )
    //                |           \/           ||
    //                +------------------------+



    // compute screen coordinates of parent frame so we can play with it. Make sure we put it
    // into twips as everything else is as well.
    nsRect screenParentFrameRect (aPresContext->AppUnitsToDevPixels(offset.x), aPresContext->AppUnitsToDevPixels(offset.y),
                                    parentRect.width, parentRect.height );
    parentViewWidget->WidgetToScreen ( screenParentFrameRect, screenParentFrameRect );
    screenParentFrameRect.x = aPresContext->DevPixelsToAppUnits(screenParentFrameRect.x);
    screenParentFrameRect.y = aPresContext->DevPixelsToAppUnits(screenParentFrameRect.y);

    // Don't let it spill off the screen to the top
    if (screenViewLocY < screenTopTwips) {
      PRInt32 moveDist = screenTopTwips - screenViewLocY;
      screenViewLocY = screenTopTwips;
      ypos += moveDist;
    }
    
    // if it doesn't fit on the screen, do our magic.
    if ( (screenViewLocX + mRect.width) > screenRightTwips ||
           screenViewLocX < screenLeftTwips ||
          (screenViewLocY + mRect.height) > screenBottomTwips ) {
      
      // figure out which side of the parent has the most free space so we can move/resize
      // the popup there. This should still work if the parent frame is partially screen.
      PRBool switchSides = IsMoreRoomOnOtherSideOfParent ( readjustAboveBelow, screenViewLocX, screenViewLocY,
                                                            screenParentFrameRect, screenTopTwips, screenLeftTwips,
                                                            screenBottomTwips, screenRightTwips );
      
      // move the popup to the correct side, if necessary. Note that MovePopupToOtherSideOfParent() 
      // can change width/height of |mRect|.
      if ( switchSides )
        MovePopupToOtherSideOfParent ( readjustAboveBelow, &xpos, &ypos, &screenViewLocX, &screenViewLocY, 
                                        screenParentFrameRect, screenTopTwips, screenLeftTwips,
                                        screenBottomTwips, screenRightTwips );
                                        
      // We are allowed to move the popup along the axis to which we're not anchored to the parent
      // in order to get it to not spill off the screen.
      if ( readjustAboveBelow ) {
        // move left to be on screen, but don't let it go off the screen at the left
        if ( (screenViewLocX + mRect.width) > screenRightTwips ) {
          PRInt32 moveDistX = (screenViewLocX + mRect.width) - screenRightTwips;
          if ( screenViewLocX - moveDistX < screenLeftTwips )
            moveDistX = screenViewLocX - screenLeftTwips;          
          screenViewLocX -= moveDistX;
          xpos -= moveDistX;
        } else if (screenViewLocX < screenLeftTwips) {
          // move right to be on screen, but don't let it go off the screen at the right
          PRInt32 moveDistX = screenLeftTwips - screenViewLocX;
          if ( (screenViewLocX + mRect.width + moveDistX) > screenRightTwips )
            moveDistX = screenRightTwips - screenViewLocX - mRect.width;
          screenViewLocX += moveDistX;
          xpos += moveDistX;
        }
      }
      else {
        // move it up to be on screen, but don't let it go off the screen at the top
        /*
         *  |
         *  |
         *  |+----  screenViewLocY
         *  ||
         *  ||  Submenu ( = mRect )
         * -+|
         *   |
         *   |
         * - - - - - - - - - - screenBottomTwips (bottom of the screen)
         *   |    \ 
         *   |     }  moveDistY
         *   |    / 
         *   +----  screenViewLocY + mRect.height
         */

        if ( (screenViewLocY + mRect.height) > screenBottomTwips ) {
          // XXX Bug 84121 comment 48 says the next line has to use screenHeightTwips, why not screenBottomTwips?
          PRInt32 moveDistY = (screenViewLocY + mRect.height) - screenHeightTwips;
          if ( screenViewLocY - moveDistY < screenTopTwips )
            moveDistY = screenViewLocY - screenTopTwips;          
          screenViewLocY -= moveDistY;
          ypos -= moveDistY; 
        } 
      }
      
      // Resize it to fit on the screen. By this point, we've given the popup as much
      // room as we can w/out covering the parent. If it still can't be as big
      // as it wants to be, well, it just has to suck up and deal. 
      //
      // ySpillage is calculated the same way as moveDistY above. see picture there.

      PRInt32 xSpillage = (screenViewLocX + mRect.width) - screenRightTwips;
      if ( xSpillage > 0 )
        mRect.width -= xSpillage;
      PRInt32 ySpillage = (screenViewLocY + mRect.height) - screenBottomTwips;
      if ( ySpillage > 0 )
        mRect.height -= ySpillage;

      // shrink to fit onto the screen, vertically and horizontally
      if(mRect.width > screenWidthTwips) 
          mRect.width = screenWidthTwips;    
      if(mRect.height > screenHeightTwips)
          mRect.height = screenHeightTwips;   

    } // if it doesn't fit on screen
  } // if anchored to parent
  else {
  
    //
    // Popup not anchored to anything, just make sure it's on the screen by any
    // means necessary
    //

    // If you decide to mess with this code in some way other than just
    // converting it to be just like the anchored codepath, please make sure to
    // not regress bug 120226, bug 172530, bug 245163.

    // XXXbz this is really silly.  We should be able to anchor popups to a
    // point or rect, not a frame, and we should be doing so with context
    // menus.  Furthermore, we should not be adding in pixels manually to
    // adjust position (in XULPopupListenerImpl::LaunchPopup comes to mind,
    // though ConvertPosition in the same file has some 21-px bogosity in the
    // y-direction too).

    // shrink to fit onto the screen, vertically and horizontally
    if(mRect.width > screenWidthTwips) 
        mRect.width = screenWidthTwips;    
    if(mRect.height > screenHeightTwips)
        mRect.height = screenHeightTwips;   

    // First, adjust the X position.  For the X position, we slide the popup
    // left or right as needed to get it on screen.
    if ( screenViewLocX < screenLeftTwips ) {
      PRInt32 moveDistX = screenLeftTwips - screenViewLocX;
      xpos += moveDistX;
      screenViewLocX += moveDistX;
    }
    if ( (screenViewLocX + mRect.width) > screenRightTwips )
      xpos -= (screenViewLocX + mRect.width) - screenRightTwips;

    // Now the Y position.  If the popup is up too high, slide it down so it's
    // on screen.
    if ( screenViewLocY < screenTopTwips ) {
      PRInt32 moveDistY = screenTopTwips - screenViewLocY;
      ypos += moveDistY;
      screenViewLocY += moveDistY;
    }

    // Now if the popup extends down too far, either resize it or flip it to be
    // above the anchor point and resize it to fit above, depending on where we
    // have more room.
    if ( (screenViewLocY + mRect.height) > screenBottomTwips ) {
      // XXXbz it'd be good to make use of IsMoreRoomOnOtherSideOfParent and
      // such here, but that's really focused on having a nonempty parent
      // rect...
      if (screenBottomTwips - screenViewLocY >
          screenViewLocY - screenTopTwips) {
        // More space below our desired point.  Resize to fit in this space.
        // Note that this is making mRect smaller; othewise we would not have
        // reached this code.
        mRect.height = screenBottomTwips - screenViewLocY;
      } else {
        // More space above our desired point.  Flip and resize to fit in this
        // space.
        if (mRect.height > screenViewLocY - screenTopTwips) {
          // We wouldn't fit.  Shorten before flipping.
          mRect.height = screenViewLocY - screenTopTwips;
        }
        ypos -= (mRect.height + margin.top + margin.bottom);
      }
    }
  }  

  aPresContext->GetViewManager()->MoveViewTo(view, xpos, ypos); 

  // Now that we've positioned the view, sync up the frame's origin.
  nsPoint frameOrigin = GetPosition();
  nsPoint offsetToView;
  GetOriginToViewOffset(offsetToView, nsnull);
  frameOrigin -= offsetToView;
  nsBoxFrame::SetPosition(frameOrigin);

  if (sizedToPopup) {
      nsBoxLayoutState state(GetPresContext());
      SetBounds(state, nsRect(mRect.x, mRect.y, parentRect.width, mRect.height));
  }
    
  if (!mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::menuactive,
                             nsGkAtoms::_true, eCaseMatters) &&
      mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::menutobedisplayed,
                            nsGkAtoms::_true, eCaseMatters)) {
    nsCOMPtr<nsIRunnable> ev =
      new nsASyncMenuActivation(mContent);
    NS_DispatchToCurrentThread(ev);
  }

  return NS_OK;
}

static void GetInsertionPoint(nsIPresShell* aShell, nsIFrame* aFrame, nsIFrame* aChild,
                              nsIFrame** aResult)
{
  nsIContent* child = nsnull;
  if (aChild)
    child = aChild->GetContent();
  aShell->FrameConstructor()->GetInsertionPoint(aFrame, child, aResult);
}

/* virtual */ nsIMenuFrame*
nsMenuPopupFrame::GetNextMenuItem(nsIMenuFrame* aStart)
{
  nsIFrame* immediateParent = nsnull;
  GetInsertionPoint(GetPresContext()->PresShell(), this, nsnull,
                    &immediateParent);
  if (!immediateParent)
    immediateParent = this;

  nsIFrame* currFrame = nsnull;
  nsIFrame* startFrame = nsnull;
  if (aStart) {
    aStart->QueryInterface(NS_GET_IID(nsIFrame), (void**)&currFrame); 
    if (currFrame) {
      startFrame = currFrame;
      currFrame = currFrame->GetNextSibling();
    }
  }
  else 
    currFrame = immediateParent->GetFirstChild(nsnull);
  
  while (currFrame) {
    // See if it's a menu item.
    if (IsValidItem(currFrame->GetContent())) {
      nsIMenuFrame *menuFrame;
      if (NS_FAILED(CallQueryInterface(currFrame, &menuFrame)))
        menuFrame = nsnull;
      return menuFrame;
    }
    currFrame = currFrame->GetNextSibling();
  }

  currFrame = immediateParent->GetFirstChild(nsnull);

  // Still don't have anything. Try cycling from the beginning.
  while (currFrame && currFrame != startFrame) {
    // See if it's a menu item.
    if (IsValidItem(currFrame->GetContent())) {
      nsIMenuFrame *menuFrame;
      if (NS_FAILED(CallQueryInterface(currFrame, &menuFrame)))
        menuFrame = nsnull;
      return menuFrame;
    }

    currFrame = currFrame->GetNextSibling();
  }

  // No luck. Just return our start value.
  return aStart;
}

/* virtual */ nsIMenuFrame*
nsMenuPopupFrame::GetPreviousMenuItem(nsIMenuFrame* aStart)
{
  nsIFrame* immediateParent = nsnull;
  GetInsertionPoint(GetPresContext()->PresShell(), this, nsnull,
                    &immediateParent);
  if (!immediateParent)
    immediateParent = this;

  nsFrameList frames(immediateParent->GetFirstChild(nsnull));
                              
  nsIFrame* currFrame = nsnull;
  nsIFrame* startFrame = nsnull;
  if (aStart) {
    aStart->QueryInterface(NS_GET_IID(nsIFrame), (void**)&currFrame);
    if (currFrame) {
      startFrame = currFrame;
      currFrame = frames.GetPrevSiblingFor(currFrame);
    }
  }
  else currFrame = frames.LastChild();

  while (currFrame) {
    // See if it's a menu item.
    if (IsValidItem(currFrame->GetContent())) {
      nsIMenuFrame *menuFrame;
      if (NS_FAILED(CallQueryInterface(currFrame, &menuFrame)))
        menuFrame = nsnull;
      return menuFrame;
    }
    currFrame = frames.GetPrevSiblingFor(currFrame);
  }

  currFrame = frames.LastChild();

  // Still don't have anything. Try cycling from the end.
  while (currFrame && currFrame != startFrame) {
    // See if it's a menu item.
    if (IsValidItem(currFrame->GetContent())) {
      nsIMenuFrame *menuFrame;
      if (NS_FAILED(CallQueryInterface(currFrame, &menuFrame)))
        menuFrame = nsnull;
      return menuFrame;
    }

    currFrame = frames.GetPrevSiblingFor(currFrame);
  }

  // No luck. Just return our start value.
  return aStart;
}

/* virtual */ nsIMenuFrame*
nsMenuPopupFrame::GetCurrentMenuItem()
{
  return mCurrentMenu;
}

NS_IMETHODIMP nsMenuPopupFrame::ConsumeOutsideClicks(PRBool& aConsumeOutsideClicks)
{
  /*
   * When this popup is open, should clicks outside of it be consumed?
   * Return PR_TRUE if the popup hould rollup on an outside click, 
   * but consume that click so it can't be used for anything else.
   * Return PR_FALSE to allow clicks outside the popup to activate content 
   * even when the popup is open.
   * ---------------------------------------------------------------------
   * 
   * Should clicks outside of a popup be eaten?
   *
   *       Menus     Autocomplete     Comboboxes
   * Mac     Eat           No              Eat
   * Win     No            No              Eat     
   * Unix    Eat           No              Eat
   *
   */

  // If the popup has explicitly set a consume mode, honor that.
  if (mConsumeRollupEvent != nsIPopupBoxObject::ROLLUP_DEFAULT) {
    aConsumeOutsideClicks = mConsumeRollupEvent == nsIPopupBoxObject::ROLLUP_CONSUME;
    return NS_OK;
  }

  aConsumeOutsideClicks = PR_TRUE;

  nsCOMPtr<nsIContent> parentContent = mContent->GetParent();

  if (parentContent) {
    nsIAtom *parentTag = parentContent->Tag();
    if (parentTag == nsGkAtoms::menulist)
      return NS_OK;  // Consume outside clicks for combo boxes on all platforms
    if (parentTag == nsGkAtoms::menu || parentTag == nsGkAtoms::popupset) {
#if defined(XP_WIN) || defined(XP_OS2)
      // Don't consume outside clicks for menus in Windows
      aConsumeOutsideClicks = PR_FALSE;
#endif
      return NS_OK;
    }
    if (parentTag == nsGkAtoms::textbox) {
      // Don't consume outside clicks for autocomplete widget
      if (parentContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                                     nsGkAtoms::autocomplete, eCaseMatters))
        aConsumeOutsideClicks = PR_FALSE;
    }
  }

  return NS_OK;
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

void nsMenuPopupFrame::EnsureMenuItemIsVisible(nsIMenuFrame* aMenuItem)
{
  nsIFrame* frame=nsnull;
  aMenuItem->QueryInterface(NS_GET_IID(nsIFrame), (void**)&frame);
  if ( frame ) {
    nsIFrame* childFrame=nsnull;
    childFrame = GetFirstChild(nsnull);
    nsIScrollableView *scrollableView;
    scrollableView=GetScrollableView(childFrame);
    if ( scrollableView ) {
      nscoord scrollX, scrollY;

      nsRect viewRect = scrollableView->View()->GetBounds();
      nsRect itemRect = frame->GetRect();
      scrollableView->GetScrollPosition(scrollX, scrollY);
  
      // scroll down
      if ( itemRect.y + itemRect.height > scrollY + viewRect.height )
        scrollableView->ScrollTo(scrollX, itemRect.y + itemRect.height - viewRect.height, NS_SCROLL_PROPERTY_ALWAYS_BLIT);
      
      // scroll up
      else if ( itemRect.y < scrollY )
        scrollableView->ScrollTo(scrollX, itemRect.y, NS_SCROLL_PROPERTY_ALWAYS_BLIT);
    }
  }
}

NS_IMETHODIMP nsMenuPopupFrame::SetCurrentMenuItem(nsIMenuFrame* aMenuItem)
{
  // When a context menu is open, the current menu is locked, and no change
  // to the menu is allowed.
  nsIMenuParent *contextMenu = GetContextMenu();
  if (contextMenu)
    return NS_OK;

  if (mCurrentMenu == aMenuItem)
    return NS_OK;
  
  // Unset the current child.
  if (mCurrentMenu) {
    PRBool isOpen = PR_FALSE;
    mCurrentMenu->MenuIsOpen(isOpen);
    mCurrentMenu->SelectMenu(PR_FALSE);
    // XXX bug 294183 sometimes mCurrentMenu gets cleared
    if (mCurrentMenu && isOpen) {
      // Don't close up immediately.
      // Kick off a close timer.
      KillCloseTimer(); // Ensure we don't have another stray waiting closure.
      PRInt32 menuDelay = 300;   // ms

      GetPresContext()->LookAndFeel()->
        GetMetric(nsILookAndFeel::eMetric_SubmenuDelay, menuDelay);

      // Kick off the timer.
      mCloseTimer = do_CreateInstance("@mozilla.org/timer;1");
      mCloseTimer->InitWithCallback(mTimerMediator, menuDelay, nsITimer::TYPE_ONE_SHOT);
      mTimerMenu = mCurrentMenu;
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


NS_IMETHODIMP
nsMenuPopupFrame::Escape(PRBool& aHandledFlag)
{
  mIncrementalString.Truncate();

  // See if we have a context menu open.
  nsIMenuParent* contextMenu = GetContextMenu();
  if (contextMenu) {
    // Get the context menu parent.
    nsIFrame* childFrame;
    CallQueryInterface(contextMenu, &childFrame);
    nsIPopupSetFrame* popupSetFrame = GetPopupSetFrame(GetPresContext());
    if (popupSetFrame)
      // Destroy the popup.
      popupSetFrame->DestroyPopup(childFrame, PR_FALSE);
    aHandledFlag = PR_TRUE;
    return NS_OK;
  }

  if (!mCurrentMenu)
    return NS_OK;

  // See if our menu is open.
  PRBool isOpen = PR_FALSE;
  mCurrentMenu->MenuIsOpen(isOpen);
  if (isOpen) {
    // Let the child menu handle this.
    mCurrentMenu->Escape(aHandledFlag);
    if (!aHandledFlag) {
      // We should close up.
      mCurrentMenu->OpenMenu(PR_FALSE);
      // SelectMenu() so DOMMenuItemActive is fired for accessibility
      mCurrentMenu->SelectMenu(PR_TRUE);
      aHandledFlag = PR_TRUE;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMenuPopupFrame::Enter()
{
  mIncrementalString.Truncate();

  // See if we have a context menu open.
  nsIMenuParent *contextMenu = GetContextMenu();
  if (contextMenu)
    return contextMenu->Enter();

  // Give it to the child.
  if (mCurrentMenu)
    mCurrentMenu->Enter();

  return NS_OK;
}

nsIMenuParent*
nsMenuPopupFrame::GetContextMenu()
{
  if (mIsContextMenu)
    return nsnull;

  return nsMenuFrame::GetContextMenu();
}

nsIMenuFrame*
nsMenuPopupFrame::FindMenuWithShortcut(nsIDOMKeyEvent* aKeyEvent, PRBool& doAction)
{
  PRUint32 charCode, keyCode;
  aKeyEvent->GetCharCode(&charCode);
  aKeyEvent->GetKeyCode(&keyCode);

  doAction = PR_FALSE;

  // Enumerate over our list of frames.
  nsIFrame* immediateParent = nsnull;
  GetInsertionPoint(GetPresContext()->PresShell(), this, nsnull,
                    &immediateParent);
  if (!immediateParent)
    immediateParent = this;

  PRUint32 matchCount = 0, matchShortcutCount = 0;
  PRBool foundActive = PR_FALSE;
  PRBool isShortcut;
  nsIMenuFrame* frameBefore = nsnull;
  nsIMenuFrame* frameAfter = nsnull;
  nsIMenuFrame* frameShortcut = nsnull;

  nsIContent* parentContent = mContent->GetParent();

  PRBool isMenu =
    parentContent && parentContent->Tag() != nsGkAtoms::menulist;

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
    PRUnichar uniChar = ToLowerCase(NS_STATIC_CAST(PRUnichar, charCode));
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

  // We start searching from first child. This process is divided into two parts
  //   -- before current and after current -- by the current item
  while (currFrame) {
    nsIContent* current = currFrame->GetContent();
    
    // See if it's a menu item.
    if (IsValidItem(current)) {
      nsAutoString textKey;
      // Get the shortcut attribute.
      current->GetAttr(kNameSpaceID_None, nsGkAtoms::accesskey, textKey);
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
        nsIMenuFrame* menuFrame;
        if (NS_SUCCEEDED(CallQueryInterface(currFrame, &menuFrame))) {
          // There is one match
          matchCount++;
          if (isShortcut) {
            // There is one shortcut-key match
            matchShortcutCount++;
            // Record the matched item. If there is only one matched shortcut item, do it
            frameShortcut = menuFrame;
          }
          if (!foundActive) {
            // It's a first candidate item located before/on the current item
            if (!frameBefore)
              frameBefore = menuFrame;
          }
          else {
            // It's a first candidate item located after the current item
            if (!frameAfter)
              frameAfter = menuFrame;
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
          nsIMenuFrame* menuFrame;
          if (NS_SUCCEEDED(CallQueryInterface(currFrame, &menuFrame)) &&
              menuFrame == frameBefore) {
            return frameBefore;
          }
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
nsMenuPopupFrame::ShortcutNavigation(nsIDOMKeyEvent* aKeyEvent, PRBool& aHandledFlag)
{
  // See if we have a context menu open.
  nsIMenuParent *contextMenu = GetContextMenu();
  if (contextMenu)
    return contextMenu->ShortcutNavigation(aKeyEvent, aHandledFlag);

  if (mCurrentMenu) {
    PRBool isOpen = PR_FALSE;
    mCurrentMenu->MenuIsOpen(isOpen);
    if (isOpen) {
      // No way this applies to us. Give it to our child.
      mCurrentMenu->ShortcutNavigation(aKeyEvent, aHandledFlag);
      return NS_OK;
    }
  }

  // This applies to us. Let's see if one of the shortcuts applies
  PRBool action;
  nsIMenuFrame* result = FindMenuWithShortcut(aKeyEvent, action);
  if (result) {
    // We got one!
    nsIFrame* frame = nsnull;
    CallQueryInterface(result, &frame);
    nsWeakFrame weakResult(frame);
    aHandledFlag = PR_TRUE;
    SetCurrentMenuItem(result);
    if (action && weakResult.IsAlive()) {
      result->Enter();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMenuPopupFrame::KeyboardNavigation(PRUint32 aKeyCode, PRBool& aHandledFlag)
{
  // See if we have a context menu open.
  nsIMenuParent *contextMenu = GetContextMenu();
  if (contextMenu)
    return contextMenu->KeyboardNavigation(aKeyCode, aHandledFlag);

  nsNavigationDirection theDirection;
  NS_DIRECTION_FROM_KEY_CODE(theDirection, aKeyCode);

  mIncrementalString.Truncate();

  // This method only gets called if we're open.
  if (!mCurrentMenu && NS_DIRECTION_IS_INLINE(theDirection)) {
    // We've been opened, but we haven't had anything selected.
    // We can handle End, but our parent handles Start.
    if (theDirection == eNavigationDirection_End) {
      nsIMenuFrame* nextItem = GetNextMenuItem(nsnull);
      if (nextItem) {
        aHandledFlag = PR_TRUE;
        SetCurrentMenuItem(nextItem);
      }
    }
    return NS_OK;
  }

  PRBool isContainer = PR_FALSE;
  PRBool isOpen = PR_FALSE;
  PRBool isDisabled = PR_FALSE;
  nsWeakFrame weakFrame(this);
  if (mCurrentMenu) {
    mCurrentMenu->MenuIsContainer(isContainer);
    mCurrentMenu->MenuIsOpen(isOpen);
    mCurrentMenu->MenuIsDisabled(isDisabled);

    if (isOpen) {
      // Give our child a shot.
      mCurrentMenu->KeyboardNavigation(aKeyCode, aHandledFlag);
      NS_ENSURE_TRUE(weakFrame.IsAlive(), NS_OK);
    }
    else if (theDirection == eNavigationDirection_End &&
             isContainer && !isDisabled) {
      // The menu is not yet open. Open it and select the first item.
      aHandledFlag = PR_TRUE;
      nsIFrame* frame = nsnull;
      CallQueryInterface(mCurrentMenu, &frame);
      nsWeakFrame weakCurrentFrame(frame);
      mCurrentMenu->OpenMenu(PR_TRUE);
      NS_ENSURE_TRUE(weakCurrentFrame.IsAlive(), NS_OK);
      mCurrentMenu->SelectFirstItem();
      NS_ENSURE_TRUE(weakFrame.IsAlive(), NS_OK);
    }
  }

  if (aHandledFlag)
    return NS_OK; // The child menu took it for us.

  // For block progression, we can move in either direction
  if (NS_DIRECTION_IS_BLOCK(theDirection) ||
      NS_DIRECTION_IS_BLOCK_TO_EDGE(theDirection)) {

    nsIMenuFrame* nextItem;
    
    if (theDirection == eNavigationDirection_Before)
      nextItem = GetPreviousMenuItem(mCurrentMenu);
    else if (theDirection == eNavigationDirection_After)
      nextItem = GetNextMenuItem(mCurrentMenu);
    else if (theDirection == eNavigationDirection_First)
      nextItem = GetNextMenuItem(nsnull);
    else
      nextItem = GetPreviousMenuItem(nsnull);

    if (nextItem) {
      aHandledFlag = PR_TRUE;
      SetCurrentMenuItem(nextItem);
    }
  }
  else if (mCurrentMenu && isContainer && isOpen) {
    if (theDirection == eNavigationDirection_Start) {
      // Close it up.
      mCurrentMenu->OpenMenu(PR_FALSE);
      NS_ENSURE_TRUE(weakFrame.IsAlive(), NS_OK);
      // SelectMenu() so DOMMenuItemActive is fired for accessibility
      mCurrentMenu->SelectMenu(PR_TRUE);
      aHandledFlag = PR_TRUE;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMenuPopupFrame::GetParentPopup(nsIMenuParent** aMenuParent)
{
  *aMenuParent = nsnull;
  nsIFrame* parent = GetParent();
  while (parent) {
    nsCOMPtr<nsIMenuParent> menuParent = do_QueryInterface(parent);
    if (menuParent) {
      *aMenuParent = menuParent.get();
      NS_ADDREF(*aMenuParent);
      return NS_OK;
    }
    parent = parent->GetParent();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMenuPopupFrame::HideChain()
{
  if (!mShouldRollup)
    return NS_OK;

  // Stop capturing rollups
  // (must do this during Hide, which happens before the menu item is executed,
  // since this reinstates normal event handling.)
  nsMenuDismissalListener::Shutdown();
  
  nsIFrame* frame = GetParent();
  if (frame) {
    nsWeakFrame weakMenu(frame);
    nsIMenuFrame* menuFrame;
    if (NS_FAILED(CallQueryInterface(frame, &menuFrame))) {
      nsIPopupSetFrame* popupSetFrame = GetPopupSetFrame(GetPresContext());
      if (popupSetFrame)
        // Hide the popup.
        popupSetFrame->HidePopup(this);
      return NS_OK;
    }
   
    menuFrame->ActivateMenu(PR_FALSE);
    NS_ENSURE_TRUE(weakMenu.IsAlive(), NS_OK);
    menuFrame->SelectMenu(PR_FALSE);
    NS_ENSURE_TRUE(weakMenu.IsAlive(), NS_OK);

    // Get the parent.
    nsIMenuParent *menuParent = menuFrame->GetMenuParent();
    if (menuParent)
      menuParent->HideChain();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMenuPopupFrame::DismissChain()
{
  if (!mShouldRollup)
    return NS_OK;

  // Stop capturing rollups
  nsMenuDismissalListener::Shutdown();
  
  // Get our menu parent.
  nsIFrame* frame = GetParent();
  if (frame) {
    nsIMenuFrame *menuFrame = nsnull;
    CallQueryInterface(frame, &menuFrame);
    if (!menuFrame) {
      nsIPopupSetFrame* popupSetFrame = GetPopupSetFrame(GetPresContext());
      if (popupSetFrame) {
        // make sure the menu is not highlighted
        if (mCurrentMenu) {
          PRBool wasOpen;
          mCurrentMenu->MenuIsOpen(wasOpen);
          if (wasOpen)
            mCurrentMenu->OpenMenu(PR_FALSE);
          mCurrentMenu->SelectMenu(PR_FALSE);
        }
        // Destroy the popup.
        popupSetFrame->DestroyPopup(this, PR_TRUE);
      }
      return NS_OK;
    }
  
    menuFrame->OpenMenu(PR_FALSE);

    // Get the parent.
    nsIMenuParent* menuParent = menuFrame->GetMenuParent();
    if (menuParent)
      menuParent->DismissChain();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMenuPopupFrame::GetWidget(nsIWidget **aWidget)
{
  // Get parent view
  nsIView * view = nsnull;
  // XXX should this be passing PR_FALSE or PR_TRUE for aStopAtViewManagerRoot?
  nsMenuPopupFrame::GetRootViewForPopup(this, PR_FALSE, &view);
  if (!view)
    return NS_OK;

  *aWidget = view->GetWidget();
  NS_IF_ADDREF(*aWidget);
  return NS_OK;
}

NS_IMETHODIMP
nsMenuPopupFrame::AttachedDismissalListener()
{
  mConsumeRollupEvent = nsIPopupBoxObject::ROLLUP_DEFAULT;
  return NS_OK;
}

NS_IMETHODIMP
nsMenuPopupFrame::InstallKeyboardNavigator()
{
  if (mKeyboardNavigator)
    return NS_OK;

  nsCOMPtr<nsIDOMEventReceiver> target = do_QueryInterface(mContent->GetDocument());
  
  mTarget = target;
  mKeyboardNavigator = new nsMenuListener(this);
  NS_IF_ADDREF(mKeyboardNavigator);

  target->AddEventListener(NS_LITERAL_STRING("keypress"), (nsIDOMKeyListener*)mKeyboardNavigator, PR_TRUE); 
  target->AddEventListener(NS_LITERAL_STRING("keydown"), (nsIDOMKeyListener*)mKeyboardNavigator, PR_TRUE);  
  target->AddEventListener(NS_LITERAL_STRING("keyup"), (nsIDOMKeyListener*)mKeyboardNavigator, PR_TRUE);   
  
  return NS_OK;
}

NS_IMETHODIMP
nsMenuPopupFrame::RemoveKeyboardNavigator()
{
  if (!mKeyboardNavigator)
    return NS_OK;

  mTarget->RemoveEventListener(NS_LITERAL_STRING("keypress"), (nsIDOMKeyListener*)mKeyboardNavigator, PR_TRUE);
  mTarget->RemoveEventListener(NS_LITERAL_STRING("keydown"), (nsIDOMKeyListener*)mKeyboardNavigator, PR_TRUE);
  mTarget->RemoveEventListener(NS_LITERAL_STRING("keyup"), (nsIDOMKeyListener*)mKeyboardNavigator, PR_TRUE);
  
  NS_IF_RELEASE(mKeyboardNavigator);

  return NS_OK;
}

// helpers /////////////////////////////////////////////////////////////

PRBool 
nsMenuPopupFrame::IsValidItem(nsIContent* aContent)
{
  nsIAtom *tag = aContent->Tag();
  
  PRBool skipNavigatingDisabledMenuItem;
  GetPresContext()->LookAndFeel()->
    GetMetric(nsILookAndFeel::eMetric_SkipNavigatingDisabledMenuItem,
              skipNavigatingDisabledMenuItem);

  PRBool result = (tag == nsGkAtoms::menu ||
                   tag == nsGkAtoms::menuitem ||
                   tag == nsGkAtoms::option);
  if (skipNavigatingDisabledMenuItem)
    result = result && !IsDisabled(aContent);

  return result;
}

PRBool 
nsMenuPopupFrame::IsDisabled(nsIContent* aContent)
{
  return aContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::disabled,
                               nsGkAtoms::_true, eCaseMatters);
}

NS_IMETHODIMP 
nsMenuPopupFrame::AttributeChanged(PRInt32 aNameSpaceID,
                                   nsIAtom* aAttribute,
                                   PRInt32 aModType)

{
  nsresult rv = nsBoxFrame::AttributeChanged(aNameSpaceID, aAttribute,
                                             aModType);
  
  if (aAttribute == nsGkAtoms::left || aAttribute == nsGkAtoms::top)
    MoveToAttributePosition();
  
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
  PRInt32 err1, err2, xPos, yPos;
  xPos = left.ToInteger(&err1);
  yPos = top.ToInteger(&err2);

  if (NS_SUCCEEDED(err1) && NS_SUCCEEDED(err2)) {
    MoveToInternal(xPos, yPos);
  }
}


NS_IMETHODIMP 
nsMenuPopupFrame::HandleEvent(nsPresContext* aPresContext, 
                              nsGUIEvent*     aEvent,
                              nsEventStatus*  aEventStatus)
{
  return nsBoxFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
}

void
nsMenuPopupFrame::Destroy()
{
  // Null out the pointer to this frame in the mediator wrapper so that it 
  // doesn't try to interact with a deallocated frame.
  mTimerMediator->ClearFrame();

  if (mCloseTimer)
    mCloseTimer->Cancel();

  nsPresContext* rootPresContext = GetPresContext()->RootPresContext();
  if (rootPresContext->ContainsActivePopup(this)) {
    rootPresContext->NotifyRemovedActivePopup(this);
  }

  RemoveKeyboardNavigator();
  nsBoxFrame::Destroy();
}

// REVIEW: The override here was doing nothing at all since nsBoxFrame is our
// parent class
//
// Notify
//
// The item selection timer has fired, we might have to readjust the 
// selected item. There are two cases here that we are trying to deal with:
//   (1) diagonal movement from a parent menu to a submenu passing briefly over
//       other items, and
//   (2) moving out from a submenu to a parent or grandparent menu.
// In both cases, |mTimerMenu| is the menu item that might have an open submenu and
// |mCurrentMenu| is the item the mouse is currently over, which could be none of them.
//
// case (1):
//  As the mouse moves from the parent item of a submenu (we'll call 'A') diagonally into the
//  submenu, it probably passes through one or more sibilings (B). As the mouse passes
//  through B, it becomes the current menu item and the timer is set and mTimerMenu is 
//  set to A. Before the timer fires, the mouse leaves the menu containing A and B and
//  enters the submenus. Now when the timer fires, |mCurrentMenu| is null (!= |mTimerMenu|)
//  so we have to see if anything in A's children is selected (recall that even disabled
//  items are selected, the style just doesn't show it). If that is the case, we need to
//  set the selected item back to A.
//
// case (2);
//  Item A has an open submenu, and in it there is an item (B) which also has an open
//  submenu (so there are 3 menus displayed right now). The mouse then leaves B's child
//  submenu and selects an item that is a sibling of A, call it C. When the mouse enters C,
//  the timer is set and |mTimerMenu| is A and |mCurrentMenu| is C. As the timer fires,
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
nsMenuPopupFrame::Notify(nsITimer* aTimer)
{
  // Our timer has fired. 
  if (aTimer == mCloseTimer.get()) {
    PRBool menuOpen = PR_FALSE;
    mTimerMenu->MenuIsOpen(menuOpen);
    if (menuOpen)
      mTimerMenu->OpenMenu(PR_FALSE);

    if (mCloseTimer)
      mCloseTimer->Cancel();
  }
  
  mCloseTimer = nsnull;
  mTimerMenu = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsMenuPopupFrame::KillCloseTimer()
{
  if (mCloseTimer && mTimerMenu) {
    PRBool menuOpen = PR_FALSE;
    mTimerMenu->MenuIsOpen(menuOpen);
    if (menuOpen) {
      mTimerMenu->OpenMenu(PR_FALSE);
    }
    mCloseTimer->Cancel();
    mCloseTimer = nsnull;
    mTimerMenu = nsnull;
  }
  return NS_OK;
}



NS_IMETHODIMP
nsMenuPopupFrame::KillPendingTimers ( )
{
  return KillCloseTimer();

} // KillPendingTimers

NS_IMETHODIMP
nsMenuPopupFrame::CancelPendingTimers()
{
  if (mCloseTimer && mTimerMenu) {
    if (mTimerMenu != mCurrentMenu) {
      SetCurrentMenuItem(mTimerMenu);
    }
    mCloseTimer->Cancel();
    mCloseTimer = nsnull;
    mTimerMenu = nsnull;
  }
  return NS_OK;
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
  nsIView* view = GetView();
  NS_ASSERTION(view->GetParent(), "Must have parent!");
  
  // Retrieve screen position of parent view
  nsIntPoint screenPos = view->GetParent()->GetScreenPosition();

  // Move the widget
  // XXXbz don't we want screenPos to be the parent _widget_'s position, then?
  view->GetWidget()->Move(aLeft - screenPos.x, aTop - screenPos.y);
}

void 
nsMenuPopupFrame::GetAutoPosition(PRBool* aShouldAutoPosition)
{
  *aShouldAutoPosition = mShouldAutoPosition;
}

void
nsMenuPopupFrame::SetAutoPosition(PRBool aShouldAutoPosition)
{
  mShouldAutoPosition = aShouldAutoPosition;
}

void
nsMenuPopupFrame::EnableRollup(PRBool aShouldRollup)
{
  if (!nsMenuDismissalListener::sInstance ||
       nsMenuDismissalListener::sInstance->GetCurrentMenuParent() != this)
    return;

  if (aShouldRollup)
    nsMenuDismissalListener::sInstance->Register();
  else
    nsMenuDismissalListener::sInstance->Unregister();
}

void
nsMenuPopupFrame::SetConsumeRollupEvent(PRUint32 aConsumeMode)
{
  mConsumeRollupEvent = aConsumeMode;
}

// nsMenuPopupTimerMediator implementation.
NS_IMPL_ISUPPORTS1(nsMenuPopupTimerMediator, nsITimerCallback)

/**
 * Constructs a wrapper around an nsMenuFrame.
 * @param aFrame nsMenuFrame to create a wrapper around.
 */
nsMenuPopupTimerMediator::nsMenuPopupTimerMediator(nsMenuPopupFrame *aFrame) :
  mFrame(aFrame)
{
  NS_ASSERTION(mFrame, "Must have frame");
}

nsMenuPopupTimerMediator::~nsMenuPopupTimerMediator()
{
}

/**
 * Delegates the notification to the contained frame if it has not been destroyed.
 * @param aTimer Timer which initiated the callback.
 * @return NS_ERROR_FAILURE if the frame has been destroyed.
 */
NS_IMETHODIMP nsMenuPopupTimerMediator::Notify(nsITimer* aTimer)
{
  if (!mFrame)
    return NS_ERROR_FAILURE;

  return mFrame->Notify(aTimer);
}

/**
 * Clear the pointer to the contained nsMenuFrame. This should be called
 * when the contained nsMenuFrame is destroyed.
 */
void nsMenuPopupTimerMediator::ClearFrame()
{
  mFrame = nsnull;
}
