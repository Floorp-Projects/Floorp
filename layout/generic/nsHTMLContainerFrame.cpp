/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "nsHTMLContainerFrame.h"
#include "nsIRenderingContext.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIContent.h"
#include "nsLayoutAtoms.h"
#include "nsLayoutUtils.h"
#include "nsCSSAnonBoxes.h"
#include "nsIWidget.h"
#include "nsILinkHandler.h"
#include "nsHTMLValue.h"
#include "nsGUIEvent.h"
#include "nsIDocument.h"
#include "nsIURL.h"
#include "nsPlaceholderFrame.h"
#include "nsHTMLParts.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsViewsCID.h"
#include "nsIDOMEvent.h"
#include "nsIScrollableView.h"
#include "nsWidgetsCID.h"
#include "nsCOMPtr.h"
#include "nsIDeviceContext.h"
#include "nsIFontMetrics.h"
#include "nsReflowPath.h"
#include "nsCSSFrameConstructor.h"

static NS_DEFINE_CID(kCChildCID, NS_CHILD_CID);

NS_IMETHODIMP
nsHTMLContainerFrame::Paint(nsPresContext*      aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect&        aDirtyRect,
                            nsFramePaintLayer    aWhichLayer,
                            PRUint32             aFlags)
{
  if (NS_FRAME_IS_UNFLOWABLE & mState) {
    return NS_OK;
  }

  // Paint inline element backgrounds in the foreground layer, but
  // others in the background (bug 36710).  (nsInlineFrame::Paint does
  // this differently.)
  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
    PaintSelf(aPresContext, aRenderingContext, aDirtyRect);
  }
    
  PaintChildren(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer, aFlags);
  return NS_OK;
}

void
nsHTMLContainerFrame::PaintDecorationsAndChildren(
                                       nsPresContext*      aPresContext,
                                       nsIRenderingContext& aRenderingContext,
                                       const nsRect&        aDirtyRect,
                                       nsFramePaintLayer    aWhichLayer,
                                       PRBool               aIsBlock,
                                       PRUint32             aFlags)
{
  // Do standards mode painting of 'text-decoration's: under+overline
  // behind children, line-through in front.  For Quirks mode, see
  // nsTextFrame::PaintTextDecorations.  (See bug 1777.)
  nscolor underColor, overColor, strikeColor;
  PRUint8 decorations = NS_STYLE_TEXT_DECORATION_NONE;
  nsCOMPtr<nsIFontMetrics> fm;
  PRBool isVisible;

  if (eCompatibility_NavQuirks != aPresContext->CompatibilityMode() && 
      NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer &&
      NS_SUCCEEDED(IsVisibleForPainting(aPresContext, aRenderingContext,
                                        PR_TRUE, &isVisible)) &&
      isVisible) {
    GetTextDecorations(aPresContext, aIsBlock, decorations, underColor, 
                       overColor, strikeColor);
    if (decorations & (NS_STYLE_TEXT_DECORATION_UNDERLINE |
                       NS_STYLE_TEXT_DECORATION_OVERLINE |
                       NS_STYLE_TEXT_DECORATION_LINE_THROUGH)) {
      const nsStyleFont* font = GetStyleFont();
      NS_ASSERTION(font->mFont.decorations == NS_FONT_DECORATION_NONE,
                   "fonts on style structs shouldn't have decorations");

      // XXX This is relatively slow and shouldn't need to be used here.
      nsCOMPtr<nsIDeviceContext> deviceContext;
      aRenderingContext.GetDeviceContext(*getter_AddRefs(deviceContext));
      nsCOMPtr<nsIFontMetrics> normalFont;
      deviceContext->GetMetricsFor(font->mFont, *getter_AddRefs(fm));
    }
    if (decorations & NS_STYLE_TEXT_DECORATION_UNDERLINE) {
      PaintTextDecorations(aRenderingContext, fm,
                           NS_STYLE_TEXT_DECORATION_UNDERLINE, underColor);
    }
    if (decorations & NS_STYLE_TEXT_DECORATION_OVERLINE) {
      PaintTextDecorations(aRenderingContext, fm,
                           NS_STYLE_TEXT_DECORATION_OVERLINE, overColor);
    }
  }

  PaintChildren(aPresContext, aRenderingContext, aDirtyRect,
                aWhichLayer, aFlags);

  if (decorations & NS_STYLE_TEXT_DECORATION_LINE_THROUGH) {
    PaintTextDecorations(aRenderingContext, fm,
                         NS_STYLE_TEXT_DECORATION_LINE_THROUGH, strikeColor);
  }
}

static PRBool 
HasTextFrameDescendantOrInFlow(nsPresContext* aPresContext, nsIFrame* aFrame);

void
nsHTMLContainerFrame::GetTextDecorations(nsPresContext* aPresContext, 
                                         PRBool aIsBlock,
                                         PRUint8& aDecorations,
                                         nscolor& aUnderColor, 
                                         nscolor& aOverColor, 
                                         nscolor& aStrikeColor)
{
  aDecorations = NS_STYLE_TEXT_DECORATION_NONE;
  if (!mStyleContext->HasTextDecorations()) {
    // This is a neccessary, but not sufficient, condition for text
    // decorations.
    return; 
  }

  // A mask of all possible decorations.
  PRUint8 decorMask = NS_STYLE_TEXT_DECORATION_UNDERLINE |
                      NS_STYLE_TEXT_DECORATION_OVERLINE |
                      NS_STYLE_TEXT_DECORATION_LINE_THROUGH; 

  if (!aIsBlock) {
    aDecorations = GetStyleTextReset()->mTextDecoration  & decorMask;
    if (aDecorations) {
      const nsStyleColor* styleColor = GetStyleColor();
      aUnderColor = styleColor->mColor;
      aOverColor = styleColor->mColor;
      aStrikeColor = styleColor->mColor;
    }
  }
  else {
    // walk tree
    for (nsIFrame *frame = this; frame && decorMask; frame = frame->GetParent()) {
      // find text-decorations.  "Inherit" from parent *block* frames

      nsStyleContext* styleContext = frame->GetStyleContext();
      const nsStyleDisplay* styleDisplay = styleContext->GetStyleDisplay();
      if (!styleDisplay->IsBlockLevel() &&
          styleDisplay->mDisplay != NS_STYLE_DISPLAY_TABLE_CELL) {
        // If an inline frame is discovered while walking up the tree,
        // we should stop according to CSS3 draft. CSS2 is rather vague
        // about this.
        break;
      }

      const nsStyleTextReset* styleText = styleContext->GetStyleTextReset();
      PRUint8 decors = decorMask & styleText->mTextDecoration;
      if (decors) {
        // A *new* text-decoration is found.
        nscolor color = styleContext->GetStyleColor()->mColor;

        if (NS_STYLE_TEXT_DECORATION_UNDERLINE & decors) {
          aUnderColor = color;
          decorMask &= ~NS_STYLE_TEXT_DECORATION_UNDERLINE;
          aDecorations |= NS_STYLE_TEXT_DECORATION_UNDERLINE;
        }
        if (NS_STYLE_TEXT_DECORATION_OVERLINE & decors) {
          aOverColor = color;
          decorMask &= ~NS_STYLE_TEXT_DECORATION_OVERLINE;
          aDecorations |= NS_STYLE_TEXT_DECORATION_OVERLINE;
        }
        if (NS_STYLE_TEXT_DECORATION_LINE_THROUGH & decors) {
          aStrikeColor = color;
          decorMask &= ~NS_STYLE_TEXT_DECORATION_LINE_THROUGH;
          aDecorations |= NS_STYLE_TEXT_DECORATION_LINE_THROUGH;
        }
      }
    }
  }
  
  if (aDecorations) {
    // If this frame contains no text, we're required to ignore this property
    if (!HasTextFrameDescendantOrInFlow(aPresContext, this)) {
      aDecorations = NS_STYLE_TEXT_DECORATION_NONE;
    }
  }
}

static PRBool 
HasTextFrameDescendant(nsPresContext* aPresContext, nsIFrame* aParent)
{
  for (nsIFrame* kid = aParent->GetFirstChild(nsnull); kid;
       kid = kid->GetNextSibling())
  {
    if (kid->GetType() == nsLayoutAtoms::textFrame) {
      // This is only a candidate. We need to determine if this text
      // frame is empty, as in containing only (non-pre) whitespace.
      // See bug 20163.
      if (!kid->IsEmpty()) {
        return PR_TRUE;
      }
    }
    if (HasTextFrameDescendant(aPresContext, kid)) {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

static PRBool 
HasTextFrameDescendantOrInFlow(nsPresContext* aPresContext, nsIFrame* aFrame)
{
  for (nsIFrame *f = aFrame->GetFirstInFlow(); f; f = f->GetNextInFlow()) {
    if (HasTextFrameDescendant(aPresContext, f))
      return PR_TRUE;
  }
  return PR_FALSE;
}


/*virtual*/ void
nsHTMLContainerFrame::PaintTextDecorationLines(
                   nsIRenderingContext& aRenderingContext, 
                   nscolor aColor, 
                   nscoord aOffset, 
                   nscoord aAscent, 
                   nscoord aSize) 
{
  nsMargin bp;
  CalcBorderPadding(bp);
  aRenderingContext.SetColor(aColor);
  nscoord innerWidth = mRect.width - bp.left - bp.right;
  aRenderingContext.FillRect(bp.left, 
                             bp.top + aAscent - aOffset, innerWidth, aSize);
}

void
nsHTMLContainerFrame::PaintTextDecorations(
                            nsIRenderingContext& aRenderingContext,
                            nsIFontMetrics* aFontMetrics,
                            PRUint8 aDecoration,
                            nscolor aColor)
{
  nscoord ascent, offset, size;
  aFontMetrics->GetMaxAscent(ascent);
  if (aDecoration & 
      (NS_STYLE_TEXT_DECORATION_UNDERLINE 
       | NS_STYLE_TEXT_DECORATION_OVERLINE)) {
    aFontMetrics->GetUnderline(offset, size);
    if (NS_STYLE_TEXT_DECORATION_UNDERLINE & aDecoration) {
      PaintTextDecorationLines(aRenderingContext, aColor, offset, ascent, size);
    }
    else if (NS_STYLE_TEXT_DECORATION_OVERLINE & aDecoration) {
      PaintTextDecorationLines(aRenderingContext, aColor, ascent, ascent, size);
    }
  }
  else if (NS_STYLE_TEXT_DECORATION_LINE_THROUGH & aDecoration) {
    aFontMetrics->GetStrikeout(offset, size);
    PaintTextDecorationLines(aRenderingContext, aColor, offset, ascent, size);
  }
}

/**
 * Create a next-in-flow for aFrame. Will return the newly created
 * frame in aNextInFlowResult <b>if and only if</b> a new frame is
 * created; otherwise nsnull is returned in aNextInFlowResult.
 */
nsresult
nsHTMLContainerFrame::CreateNextInFlow(nsPresContext* aPresContext,
                                       nsIFrame*       aOuterFrame,
                                       nsIFrame*       aFrame,
                                       nsIFrame*&      aNextInFlowResult)
{
  aNextInFlowResult = nsnull;

  nsIFrame* nextInFlow = aFrame->GetNextInFlow();
  if (nsnull == nextInFlow) {
    // Create a continuation frame for the child frame and insert it
    // into our lines child list.
    nsIFrame* nextFrame = aFrame->GetNextSibling();

    aPresContext->PresShell()->FrameConstructor()->
      CreateContinuingFrame(aPresContext, aFrame, aOuterFrame, &nextInFlow);

    if (nsnull == nextInFlow) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    aFrame->SetNextSibling(nextInFlow);
    nextInFlow->SetNextSibling(nextFrame);

    NS_FRAME_LOG(NS_FRAME_TRACE_NEW_FRAMES,
       ("nsHTMLContainerFrame::MaybeCreateNextInFlow: frame=%p nextInFlow=%p",
        aFrame, nextInFlow));

    aNextInFlowResult = nextInFlow;
  }
  return NS_OK;
}

static nsresult
ReparentFrameViewTo(nsIFrame*       aFrame,
                    nsIViewManager* aViewManager,
                    nsIView*        aNewParentView,
                    nsIView*        aOldParentView)
{

  // XXX What to do about placeholder views for "position: fixed" elements?
  // They should be reparented too.

  // Does aFrame have a view?
  if (aFrame->HasView()) {
    nsIView* view = aFrame->GetView();
    // Verify that the current parent view is what we think it is
    //nsIView*  parentView;
    //NS_ASSERTION(parentView == aOldParentView, "unexpected parent view");

    aViewManager->RemoveChild(view);
    
    // The view will remember the Z-order and other attributes that have been set on it.
    nsIView* insertBefore = nsLayoutUtils::FindSiblingViewFor(aNewParentView, aFrame);
    aViewManager->InsertChild(aNewParentView, view, insertBefore, insertBefore != nsnull);
  } else {
    PRInt32 listIndex = 0;
    nsIAtom* listName = nsnull;
    // This loop iterates through every child list name, and also
    // executes once with listName == nsnull.
    do {
      // Iterate the child frames, and check each child frame to see if it has
      // a view
      nsIFrame* childFrame = aFrame->GetFirstChild(listName);
      for (; childFrame; childFrame = childFrame->GetNextSibling()) {
        ReparentFrameViewTo(childFrame, aViewManager,
                            aNewParentView, aOldParentView);
      }
      listName = aFrame->GetAdditionalChildListName(listIndex++);
    } while (listName);
  }

  return NS_OK;
}

nsresult
nsHTMLContainerFrame::ReparentFrameView(nsPresContext* aPresContext,
                                        nsIFrame*       aChildFrame,
                                        nsIFrame*       aOldParentFrame,
                                        nsIFrame*       aNewParentFrame)
{
  NS_PRECONDITION(aChildFrame, "null child frame pointer");
  NS_PRECONDITION(aOldParentFrame, "null old parent frame pointer");
  NS_PRECONDITION(aNewParentFrame, "null new parent frame pointer");
  NS_PRECONDITION(aOldParentFrame != aNewParentFrame, "same old and new parent frame");

  // This code is called often and we need it to be as fast as possible, so
  // see if we can trivially detect that no work needs to be done
  if (!aChildFrame->HasView()) {
    // Child frame doesn't have a view. See if it has any child frames
    if (!aChildFrame->GetFirstChild(nsnull)) {
      return NS_OK;
    }
  }

  // See if either the old parent frame or the new parent frame have a view

  while (!aOldParentFrame->HasView() && !aNewParentFrame->HasView()) {
    // Walk up both the old parent frame and the new parent frame nodes
    // stopping when we either find a common parent or views for one
    // or both of the frames.
    //
    // This works well in the common case where we push/pull and the old parent
    // frame and the new parent frame are part of the same flow. They will
    // typically be the same distance (height wise) from the
    aOldParentFrame = aOldParentFrame->GetParent();
    aNewParentFrame = aNewParentFrame->GetParent();
    
    // We should never walk all the way to the root frame without finding
    // a view
    NS_ASSERTION(aOldParentFrame && aNewParentFrame, "didn't find view");

    // See if we reached a common ancestor
    if (aOldParentFrame == aNewParentFrame) {
      break;
    }
  }

  // See if we found a common parent frame
  if (aOldParentFrame == aNewParentFrame) {
    // We found a common parent and there are no views between the old parent
    // and the common parent or the new parent frame and the common parent.
    // Because neither the old parent frame nor the new parent frame have views,
    // then any child views don't need reparenting
    return NS_OK;
  }

  // We found views for one or both of the ancestor frames before we
  // found a common ancestor.
  nsIView* oldParentView = aOldParentFrame->GetClosestView();
  nsIView* newParentView = aNewParentFrame->GetClosestView();
  
  // See if the old parent frame and the new parent frame are in the
  // same view sub-hierarchy. If they are then we don't have to do
  // anything
  if (oldParentView != newParentView) {
    // They're not so we need to reparent any child views
    return ReparentFrameViewTo(aChildFrame, oldParentView->GetViewManager(), newParentView,
                               oldParentView);
  }

  return NS_OK;
}

nsresult
nsHTMLContainerFrame::ReparentFrameViewList(nsPresContext* aPresContext,
                                            nsIFrame*       aChildFrameList,
                                            nsIFrame*       aOldParentFrame,
                                            nsIFrame*       aNewParentFrame)
{
  NS_PRECONDITION(aChildFrameList, "null child frame list");
  NS_PRECONDITION(aOldParentFrame, "null old parent frame pointer");
  NS_PRECONDITION(aNewParentFrame, "null new parent frame pointer");
  NS_PRECONDITION(aOldParentFrame != aNewParentFrame, "same old and new parent frame");

  // See if either the old parent frame or the new parent frame have a view
  while (!aOldParentFrame->HasView() && !aNewParentFrame->HasView()) {
    // Walk up both the old parent frame and the new parent frame nodes
    // stopping when we either find a common parent or views for one
    // or both of the frames.
    //
    // This works well in the common case where we push/pull and the old parent
    // frame and the new parent frame are part of the same flow. They will
    // typically be the same distance (height wise) from the
    aOldParentFrame = aOldParentFrame->GetParent();
    aNewParentFrame = aNewParentFrame->GetParent();
    
    // We should never walk all the way to the root frame without finding
    // a view
    NS_ASSERTION(aOldParentFrame && aNewParentFrame, "didn't find view");

    // See if we reached a common ancestor
    if (aOldParentFrame == aNewParentFrame) {
      break;
    }
  }


  // See if we found a common parent frame
  if (aOldParentFrame == aNewParentFrame) {
    // We found a common parent and there are no views between the old parent
    // and the common parent or the new parent frame and the common parent.
    // Because neither the old parent frame nor the new parent frame have views,
    // then any child views don't need reparenting
    return NS_OK;
  }

  // We found views for one or both of the ancestor frames before we
  // found a common ancestor.
  nsIView* oldParentView = aOldParentFrame->GetClosestView();
  nsIView* newParentView = aNewParentFrame->GetClosestView();
  
  // See if the old parent frame and the new parent frame are in the
  // same view sub-hierarchy. If they are then we don't have to do
  // anything
  if (oldParentView != newParentView) {
    nsIViewManager* viewManager = oldParentView->GetViewManager();

    // They're not so we need to reparent any child views
    for (nsIFrame* f = aChildFrameList; f; f = f->GetNextSibling()) {
      ReparentFrameViewTo(f, viewManager, newParentView,
                          oldParentView);
    }
  }

  return NS_OK;
}

nsresult
nsHTMLContainerFrame::CreateViewForFrame(nsIFrame* aFrame,
                                         nsIFrame* aContentParentFrame,
                                         PRBool aForce)
{
  if (aFrame->HasView()) {
    return NS_OK;
  }

  // If we don't yet have a view, see if we need a view
  if (!(aForce || FrameNeedsView(aFrame))) {
    // don't need a view
    return NS_OK;
  }

  // Create a view
  nsIFrame* parent = aFrame->GetAncestorWithView();
  NS_ASSERTION(parent, "GetParentWithView failed");

  nsIView* parentView = parent->GetView();
  NS_ASSERTION(parentView, "no parent with view");

  // Create a view
  static NS_DEFINE_CID(kViewCID, NS_VIEW_CID);
  nsIView* view;
  nsresult result = CallCreateInstance(kViewCID, &view);
  if (NS_FAILED(result)) {
    return result;
  }
    
  nsIViewManager* viewManager = parentView->GetViewManager();
  NS_ASSERTION(viewManager, "null view manager");
    
  // Initialize the view
  view->Init(viewManager, aFrame->GetRect(), parentView);

  SyncFrameViewProperties(aFrame->GetPresContext(), aFrame, nsnull, view);

  // Insert the view into the view hierarchy. If the parent view is a
  // scrolling view we need to do this differently
  nsIScrollableView*  scrollingView;
  if (NS_SUCCEEDED(CallQueryInterface(parentView, &scrollingView))) {
    scrollingView->SetScrolledView(view);
  } else {
    nsIView* insertBefore = nsLayoutUtils::FindSiblingViewFor(parentView, aFrame);
    // we insert this view 'above' the insertBefore view, unless insertBefore is null,
    // in which case we want to call with aAbove == PR_FALSE to insert at the beginning
    // in document order
    viewManager->InsertChild(parentView, view, insertBefore, insertBefore != nsnull);

    if (nsnull != aContentParentFrame) {
      nsIView* zParentView = aContentParentFrame->GetClosestView();
      if (zParentView != parentView) {
        insertBefore = nsLayoutUtils::FindSiblingViewFor(zParentView, aFrame);
        viewManager->InsertZPlaceholder(zParentView, view, insertBefore, insertBefore != nsnull);
      }
    }
  }

  // XXX If it's fixed positioned, then create a widget so it floats
  // above the scrolling area
  const nsStyleDisplay* display = aFrame->GetStyleContext()->GetStyleDisplay();
  if (NS_STYLE_POSITION_FIXED == display->mPosition) {
    view->CreateWidget(kCChildCID);
  }

  // Reparent views on any child frames (or their descendants) to this
  // view. We can just call ReparentFrameViewTo on this frame because
  // we know this frame has no view, so it will crawl the children. Also,
  // we know that any descendants with views must have 'parentView' as their
  // parent view.
  ReparentFrameViewTo(aFrame, viewManager, view, parentView);

  // Remember our view
  aFrame->SetView(view);

  NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
               ("nsHTMLContainerFrame::CreateViewForFrame: frame=%p view=%p",
                aFrame));
  return NS_OK;
}
