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
#include "nsAbsoluteFrame.h"
#include "nsFrame.h"
#include "nsHTMLBase.h"
#include "nsPlaceholderFrame.h"
#include "nsScrollFrame.h"
#include "nsStyleConsts.h"
#include "nsViewsCID.h"

#include "nsIContentDelegate.h"
#include "nsIPresContext.h"
#include "nsIStyleContext.h"
#include "nsIView.h"
#include "nsIViewManager.h"

nsresult
nsHTMLBase::CreateViewForFrame(nsIPresContext*  aPresContext,
                                nsIFrame*        aFrame,
                                nsIStyleContext* aStyleContext,
                                PRBool           aForce)
{
  nsIView* view;
  aFrame->GetView(view);
  if (nsnull == view) {
    // We don't yet have a view; see if we need a view

    PRBool scrollView = PR_FALSE;

    // See if the opacity is not the same as the geometric parent
    // frames opacity.
    if (!aForce) {
      nsIFrame* parent;
      aFrame->GetGeometricParent(parent);
      if (nsnull != parent) {
        // Get my nsStyleColor
        const nsStyleColor* myColor = (const nsStyleColor*)
          aStyleContext->GetStyleData(eStyleStruct_Color);

        // Get parent's nsStyleColor
        const nsStyleColor* parentColor;
        parent->GetStyleData(eStyleStruct_Color, (const nsStyleStruct*&)parentColor);

        // If the opacities are different then I need a view
        if (myColor->mOpacity != parentColor->mOpacity) {
          NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
            ("nsHTMLBase::CreateViewForFrame: frame=%p opacity=%g parentOpacity=%g",
             aFrame, myColor->mOpacity, parentColor->mOpacity));
          aForce = PR_TRUE;
        }
      }
    }

    // See if the frame is being relatively positioned
    if (!aForce) {
      const nsStylePosition* position = (const nsStylePosition*)
        aStyleContext->GetStyleData(eStyleStruct_Position);
      if (NS_STYLE_POSITION_RELATIVE == position->mPosition) {
        NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
          ("nsHTMLBase::CreateViewForFrame: frame=%p relatively positioned",
           aFrame));
        aForce = PR_TRUE;
      }
    }

    if (aForce) {
      // Create a view
      nsIFrame* parent;
      nsIView*  view;

      aFrame->GetParentWithView(parent);
      NS_ASSERTION(parent, "GetParentWithView failed");
      nsIView* parView;
   
      parent->GetView(parView);
      NS_ASSERTION(parView, "no parent with view");

      // Create a view
      static NS_DEFINE_IID(kViewCID, NS_VIEW_CID);
      static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);

      nsresult result = NSRepository::CreateInstance(kViewCID, 
                                                     nsnull, 
                                                     kIViewIID, 
                                                     (void **)&view);
      if (NS_OK == result) {
        nsIView*        rootView = parView;
        nsIViewManager* viewManager = rootView->GetViewManager();

        // Initialize the view
        NS_ASSERTION(nsnull != viewManager, "null view manager");

        nsRect bounds;
        aFrame->GetRect(bounds);
        view->Init(viewManager, bounds, rootView);
        viewManager->InsertChild(rootView, view, 0);

        NS_RELEASE(viewManager);
      }

      // Remember our view
      aFrame->SetView(view);
      NS_RELEASE(view);
      NS_RELEASE(parView);

      NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
        ("nsHTMLBase::CreateViewForFrame: frame=%p view=%p",
         aFrame));
      return result;
    }
  }
  return NS_OK;
}

nsresult
nsHTMLBase::CreateFrame(nsIPresContext* aPresContext,
                        nsIFrame*       aParentFrame,
                        nsIContent*     aKid,
                        nsIFrame*       aKidPrevInFlow,
                        nsIFrame*&      aResult)
{
  nsIStyleContext* kidSC =
    aPresContext->ResolveStyleContextFor(aKid, aParentFrame);
  if (nsnull == kidSC) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  const nsStylePosition* kidPosition = (const nsStylePosition*)
    kidSC->GetStyleData(eStyleStruct_Position);
  const nsStyleDisplay* kidDisplay = (const nsStyleDisplay*)
    kidSC->GetStyleData(eStyleStruct_Display);

  // Check whether it wants to floated or absolutely positioned
  nsIFrame* kidFrame = nsnull;
  nsresult rv;
  if (NS_STYLE_POSITION_ABSOLUTE == kidPosition->mPosition) {
    rv = nsAbsoluteFrame::NewFrame(&kidFrame, aKid, aParentFrame);
    if (NS_OK == rv) {
      kidFrame->SetStyleContext(aPresContext, kidSC);
    }
  }
  else if (NS_STYLE_FLOAT_NONE != kidDisplay->mFloats) {
    rv = nsPlaceholderFrame::NewFrame(&kidFrame, aKid, aParentFrame);
    if (NS_OK == rv) {
      kidFrame->SetStyleContext(aPresContext, kidSC);
    }
  }
  else if ((NS_STYLE_OVERFLOW_SCROLL == kidDisplay->mOverflow) ||
           (NS_STYLE_OVERFLOW_AUTO == kidDisplay->mOverflow)) {
    rv = NS_NewScrollFrame(&kidFrame, aKid, aParentFrame);
    if (NS_OK == rv) {
      kidFrame->SetStyleContext(aPresContext, kidSC);
    }
  }
  else if (nsnull == aKidPrevInFlow) {
    // Create initial frame for the child
    nsIContentDelegate* kidDel;
    switch (kidDisplay->mDisplay) {
    case NS_STYLE_DISPLAY_NONE:
      rv = nsFrame::NewFrame(&kidFrame, aKid, aParentFrame);
      if (NS_OK == rv) {
        kidFrame->SetStyleContext(aPresContext, kidSC);
      }
      break;

    default:
      kidDel = aKid->GetDelegate(aPresContext);
      rv = kidDel->CreateFrame(aPresContext, aKid, aParentFrame,
                               kidSC, kidFrame);
      NS_RELEASE(kidDel);
      break;
    }
  } else {
    // Since kid has a prev-in-flow, use that to create the next
    // frame.
    rv = aKidPrevInFlow->CreateContinuingFrame(aPresContext, aParentFrame,
                                               kidSC, kidFrame);
  }

  if (NS_OK == rv) {
    // Wrap the frame in a view if necessary
    rv = CreateViewForFrame(aPresContext, kidFrame, kidSC, PR_FALSE);
  }

  aResult = kidFrame;
  NS_RELEASE(kidSC);
  return rv;
}
