/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsHTMLContainerFrame.h"
#include "nsIRenderingContext.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsCSSRendering.h"
#include "nsIContent.h"
#include "nsHTMLAtoms.h"
#include "nsIWidget.h"
#include "nsILinkHandler.h"
#include "nsHTMLValue.h"
#include "nsGUIEvent.h"
#include "nsIDocument.h"
#include "nsIURL.h"
#include "nsIPtr.h"
#include "nsPlaceholderFrame.h"
#include "nsIHTMLContent.h"
#include "nsHTMLParts.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsViewsCID.h"
#include "nsIReflowCommand.h"
#include "nsHTMLIIDs.h"
#include "nsDOMEvent.h"
#include "nsIScrollableView.h"
#include "nsWidgetsCID.h"
#include "nsIStyleSet.h"

static NS_DEFINE_IID(kScrollViewIID, NS_ISCROLLABLEVIEW_IID);
static NS_DEFINE_IID(kCChildCID, NS_CHILD_CID);

NS_IMETHODIMP
nsHTMLContainerFrame::Paint(nsIPresContext& aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect& aDirtyRect,
                            nsFramePaintLayer aWhichLayer)
{
  if (eFramePaintLayer_Underlay == aWhichLayer) {
    const nsStyleDisplay* disp = (const nsStyleDisplay*)
      mStyleContext->GetStyleData(eStyleStruct_Display);
    if (disp->mVisible && mRect.width && mRect.height) {
      // Paint our background and border
      PRIntn skipSides = GetSkipSides();
      const nsStyleColor* color = (const nsStyleColor*)
        mStyleContext->GetStyleData(eStyleStruct_Color);
      const nsStyleSpacing* spacing = (const nsStyleSpacing*)
        mStyleContext->GetStyleData(eStyleStruct_Spacing);

      nsRect  rect(0, 0, mRect.width, mRect.height);
      nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                      aDirtyRect, rect, *color, *spacing, 0, 0);
      nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                  aDirtyRect, rect, *spacing, mStyleContext, skipSides);
    }
  }

  // Now paint the kids. Note that child elements have the opportunity to
  // override the visibility property and display even if their parent is
  // hidden
  PaintChildren(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
  return NS_OK;
}

/**
 * Create a next-in-flow for aFrame. Will return the newly created
 * frame in aNextInFlowResult <b>if and only if</b> a new frame is
 * created; otherwise nsnull is returned in aNextInFlowResult.
 */
nsresult
nsHTMLContainerFrame::CreateNextInFlow(nsIPresContext& aPresContext,
                                       nsIFrame* aOuterFrame,
                                       nsIFrame* aFrame,
                                       nsIFrame*& aNextInFlowResult)
{
  aNextInFlowResult = nsnull;

  nsIFrame* nextInFlow;
  aFrame->GetNextInFlow(&nextInFlow);
  if (nsnull == nextInFlow) {
    // Create a continuation frame for the child frame and insert it
    // into our lines child list.
    nsIFrame* nextFrame;
    aFrame->GetNextSibling(&nextFrame);

    nsIPresShell* presShell;
    nsIStyleSet*  styleSet;
    aPresContext.GetShell(&presShell);
    presShell->GetStyleSet(&styleSet);
    NS_RELEASE(presShell);
    styleSet->CreateContinuingFrame(&aPresContext, aFrame, aOuterFrame, &nextInFlow);
    NS_RELEASE(styleSet);

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

nsresult
nsHTMLContainerFrame::CreateViewForFrame(nsIPresContext& aPresContext,
                                         nsIFrame* aFrame,
                                         nsIStyleContext* aStyleContext,
                                         PRBool aForce)
{
  nsIView* view;
  aFrame->GetView(&view);
  // If we don't yet have a view, see if we need a view
  if (nsnull == view) {
    PRInt32 zIndex = 0;

    // Get nsStyleColor and nsStyleDisplay
    const nsStyleColor* color = (const nsStyleColor*)
      aStyleContext->GetStyleData(eStyleStruct_Color);
    const nsStyleDisplay* display = (const nsStyleDisplay*)
      aStyleContext->GetStyleData(eStyleStruct_Display);

    if (color->mOpacity != 1.0f) {
      NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
        ("nsHTMLContainerFrame::CreateViewForFrame: frame=%p opacity=%g",
         aFrame, color->mOpacity));
      aForce = PR_TRUE;
    }

    // See if the frame is being relatively positioned or absolutely
    // positioned
    if (!aForce) {
      const nsStylePosition* position = (const nsStylePosition*)
        aStyleContext->GetStyleData(eStyleStruct_Position);

      if (NS_STYLE_POSITION_RELATIVE == position->mPosition) {
        NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
          ("nsHTMLContainerFrame::CreateViewForFrame: frame=%p relatively positioned",
           aFrame));
        aForce = PR_TRUE;
      
      } else if (position->IsAbsolutelyPositioned()) {
        NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
          ("nsHTMLContainerFrame::CreateViewForFrame: frame=%p relatively positioned",
           aFrame));
        aForce = PR_TRUE;

        // Get the z-index to use. This only applies to positioned elements
        if (position->mZIndex.GetUnit() == eStyleUnit_Integer) {
          zIndex = position->mZIndex.GetIntValue();
        }

        // XXX CSS2 says clip applies to block-level and replaced elements with
        // overflow set to other than 'visible'. Where should this go?
#if 0
        // Is there a clip rect specified?
        nsViewClip    clip = {0, 0, 0, 0};
        PRUint8       clipType = (display->mClipFlags & NS_STYLE_CLIP_TYPE_MASK);
        nsViewClip*   pClip = nsnull;

        if (NS_STYLE_CLIP_RECT == clipType) {
          if ((NS_STYLE_CLIP_LEFT_AUTO & display->mClipFlags) == 0) {
            clip.mLeft = display->mClip.left;
          }
          if ((NS_STYLE_CLIP_RIGHT_AUTO & display->mClipFlags) == 0) {
            clip.mRight = display->mClip.right;
          }
          if ((NS_STYLE_CLIP_TOP_AUTO & display->mClipFlags) == 0) {
            clip.mTop = display->mClip.top;
          }
          if ((NS_STYLE_CLIP_BOTTOM_AUTO & display->mClipFlags) == 0) {
            clip.mBottom = display->mClip.bottom;
          }
          pClip = &clip;
        }
        else if (NS_STYLE_CLIP_INHERIT == clipType) {
          // XXX need to handle clip inherit (get from parent style context)
          NS_NOTYETIMPLEMENTED("clip inherit");
        }
#endif
      }
    }

    // See if the frame is a scrolled frame
    if (!aForce) {
      nsIAtom*  pseudoTag;
      aStyleContext->GetPseudoType(pseudoTag);
      if (pseudoTag == nsHTMLAtoms::scrolledContentPseudo) {
        NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
          ("nsHTMLContainerFrame::CreateViewForFrame: scrolled frame=%p", aFrame));
        aForce = PR_TRUE;
      }
      NS_IF_RELEASE(pseudoTag);
    }

    if (aForce) {
      // Create a view
      nsIFrame* parent;
      nsIView*  view;

      aFrame->GetParentWithView(&parent);
      NS_ASSERTION(parent, "GetParentWithView failed");
      nsIView* parentView;
   
      parent->GetView(&parentView);
      NS_ASSERTION(parentView, "no parent with view");

      // Create a view
      static NS_DEFINE_IID(kViewCID, NS_VIEW_CID);
      static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);

      nsresult result = nsComponentManager::CreateInstance(kViewCID, 
                                                     nsnull, 
                                                     kIViewIID, 
                                                     (void **)&view);
      if (NS_OK == result) {
        nsIViewManager* viewManager;
        parentView->GetViewManager(viewManager);
        NS_ASSERTION(nsnull != viewManager, "null view manager");

        // Initialize the view
        nsRect bounds;
        aFrame->GetRect(bounds);
        view->Init(viewManager, bounds, parentView);

        // Insert the view into the view hierarchy. If the parent view is a
        // scrolling view we need to do this differently
        nsIScrollableView*  scrollingView;
        if (NS_SUCCEEDED(parentView->QueryInterface(kScrollViewIID, (void**)&scrollingView))) {
          scrollingView->SetScrolledView(view);
        } else {
          viewManager->InsertChild(parentView, view, zIndex);
        }

        // If the background color is transparent or the visibility is hidden
        // then mark the view as having transparent content.
        // XXX We could try and be smarter about this and check whether there's
        // a background image. If there is a background image and the image is
        // fully opaque then we don't need to mark the view as having transparent
        // content...
        if ((NS_STYLE_BG_COLOR_TRANSPARENT & color->mBackgroundFlags) ||
            !display->mVisible) {
          viewManager->SetViewContentTransparency(view, PR_TRUE);
        }
        // XXX If it's relatively positioned or absolutely positioned then we
        // need to mark it as having transparent content, too. See bug #2502
        const nsStylePosition* position = (const nsStylePosition*)
          aStyleContext->GetStyleData(eStyleStruct_Position);

        if ((NS_STYLE_POSITION_RELATIVE == position->mPosition) ||
            (NS_STYLE_POSITION_ABSOLUTE == position->mPosition)) {
          viewManager->SetViewContentTransparency(view, PR_TRUE);
        }

        // XXX If it's fixed positioned, then create a widget so it floats
        // above the scrolling area
        if (NS_STYLE_POSITION_FIXED == position->mPosition) {
          view->CreateWidget(kCChildCID);
        }

        viewManager->SetViewOpacity(view, color->mOpacity);
        NS_RELEASE(viewManager);
      }

      // Remember our view
      aFrame->SetView(view);

      NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
        ("nsHTMLContainerFrame::CreateViewForFrame: frame=%p view=%p",
         aFrame));
      return result;
    }
  }
  return NS_OK;
}

void
nsHTMLContainerFrame::UpdateStyleContexts(nsIPresContext& aPresContext,
                                          nsIFrame* aFrame,
                                          nsIFrame* aOldParent,
                                          nsIFrame* aNewParent)
{
  nsIStyleContext* oldParentSC;
  aOldParent->GetStyleContext(&oldParentSC);
  nsIStyleContext* newParentSC;
  aNewParent->GetStyleContext(&newParentSC);
  if (oldParentSC != newParentSC) {
    aFrame->ReResolveStyleContext(&aPresContext, newParentSC);
  }
  NS_RELEASE(oldParentSC);
  NS_RELEASE(newParentSC);
}
