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
#include "nsHTMLFrame.h"
#include "nsFrame.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsViewsCID.h"
#include "nsIView.h"
#include "nsIViewManager.h"

nsresult
nsHTMLFrame::CreateViewForFrame(nsIPresContext*  aPresContext,
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
        nsStyleColor* myColor = (nsStyleColor*)
          aStyleContext->GetData(eStyleStruct_Color);

        // Get parent's nsStyleColor
        nsIStyleContext* parentSC;
        parent->GetStyleContext(aPresContext, parentSC);
        nsStyleColor* parentColor = (nsStyleColor*)
          parentSC->GetData(eStyleStruct_Color);

        // If the opacities are different then I need a view
        if (myColor->mOpacity != parentColor->mOpacity) {
          NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
            ("nsHTMLFrame::CreateViewForFrame: frame=%p opacity=%g parentOpacity=%g",
             aFrame, myColor->mOpacity, parentColor->mOpacity));
          aForce = PR_TRUE;
        }
        NS_RELEASE(parentSC);
      }
    }

    // See if the frame is being relatively positioned
    if (!aForce) {
      nsStylePosition* position = (nsStylePosition*)
        aStyleContext->GetData(eStyleStruct_Position);
      if (NS_STYLE_POSITION_RELATIVE == position->mPosition) {
        NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
          ("nsHTMLFrame::CreateViewForFrame: frame=%p relatively positioned",
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
        ("nsHTMLFrame::CreateViewForFrame: frame=%p view=%p",
         aFrame));
      return result;
    }
  }
  return NS_OK;
}
