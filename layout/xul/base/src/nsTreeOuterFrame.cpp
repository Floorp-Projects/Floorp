/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 */

#include "nsCOMPtr.h"
#include "nsTreeOuterFrame.h"
#include "nsIStyleContext.h"
#include "nsIContent.h"
#include "nsIView.h"
#include "nsCSSRendering.h"
#include "nsTreeCellFrame.h"
#include "nsCellMap.h"
#include "nsIDOMXULTreeElement.h"
#include "nsINameSpaceManager.h"
#include "nsXULAtoms.h"
#include "nsBoxFrame.h"
#include "nsTreeFrame.h"

//
// NS_NewTreeOuterFrame
//
// Creates a new tree frame
//
nsresult
NS_NewTreeOuterFrame (nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsTreeOuterFrame* it = new (aPresShell) nsTreeOuterFrame;
  if (!it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewTreeOuterFrame


// Constructor
nsTreeOuterFrame::nsTreeOuterFrame()
:nsTableOuterFrame() { }

// Destructor
nsTreeOuterFrame::~nsTreeOuterFrame()
{
}

NS_IMETHODIMP
nsTreeOuterFrame::Init(nsIPresContext*  aPresContext,
              nsIContent*      aContent,
              nsIFrame*        aParent,
              nsIStyleContext* aContext,
              nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsTableOuterFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
  CreateViewForFrame(aPresContext,this,aContext,PR_TRUE);
  nsIView* view;
  GetView(aPresContext, &view);
  view->SetContentTransparency(PR_TRUE);
  return rv;
}

NS_IMETHODIMP
nsTreeOuterFrame::AdjustZeroWidth()
{
  // don't do anything, tables change 0 width into auto
  return NS_OK;
}

NS_IMETHODIMP 
nsTreeOuterFrame::HandleEvent(nsIPresContext* aPresContext, 
                             nsGUIEvent*     aEvent,
                             nsEventStatus*  aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);
  *aEventStatus = nsEventStatus_eConsumeDoDefault;
  if (aEvent->message == NS_KEY_PRESS) {
    nsITreeFrame* tree = FindTreeFrame(aPresContext);
    nsTreeFrame* treeFrame = (nsTreeFrame*)tree;
    return treeFrame->HandleEvent(aPresContext, aEvent, aEventStatus);
  } 
  return NS_OK;
}

NS_IMETHODIMP
nsTreeOuterFrame::Reflow(nsIPresContext*          aPresContext,
							      nsHTMLReflowMetrics&     aDesiredSize,
							      const nsHTMLReflowState& aReflowState,
							      nsReflowStatus&          aStatus)
{
    
    // XXX at the moment we don't handle non incremental dirty reflow commands. So just convert them
    // to style changes for now.
    if (aReflowState.reason == eReflowReason_Dirty) {
        NS_WARNING("XXX Fix me!! Converting Dirty to Resize!! Table need to implement reflow Dirty!!");
        nsHTMLReflowState goodState(aReflowState);
        goodState.reason = eReflowReason_Resize;
        return Reflow(aPresContext, aDesiredSize, goodState, aStatus);
    }
    

    if (aReflowState.mComputedWidth == NS_UNCONSTRAINEDSIZE) {
        NS_WARNING("Inefficient XUL: Reflowing outer tree frame with unconstrained width, try giving it a width in CSS!");
        nsHTMLReflowState goodState(aReflowState);
        goodState.mComputedWidth = 1000;
        return Reflow(aPresContext, aDesiredSize, goodState, aStatus);
    }

    if (aReflowState.mComputedHeight == NS_UNCONSTRAINEDSIZE) {
        NS_WARNING("Inefficient XUL: Reflowing outer tree frame with unconstrained height, try giving it a height in CSS!");
    }
  //printf("TOF Width: %d, TOF Height: %d\n", aReflowState.mComputedWidth, aReflowState.mComputedHeight);

  nsresult rv = nsTableOuterFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);

  nsITreeFrame* tree = FindTreeFrame(aPresContext);
  nsTreeFrame* treeFrame = (nsTreeFrame*)tree;
  if (treeFrame->GetFixedRowSize() != -1) {
    // The tree had a rows attribute that altered its dimensions.  We
    // do not want to use the computed width and height passed in in this
    // case.
    nsRect rect;
    treeFrame->GetRect(rect);

    // XXX This is not sufficient. Will eventually have to deal with margins and padding
    // and also with the presence of captions.  For now, though, punt on those and
    // hope nobody tries to do this with a row-based tree.
    aDesiredSize.width = rect.width;
    aDesiredSize.height = rect.height;
  }

  treeFrame->HaltReflow(PR_FALSE);
  return rv;
}

NS_IMETHODIMP 
nsTreeOuterFrame::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{           
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
                                                                         
  *aInstancePtr = NULL;                                                  
                                                                                        
  if (aIID.Equals(NS_GET_IID(nsISelfScrollingFrame))) {
    *aInstancePtr = (void*)(nsISelfScrollingFrame*) this;
    return NS_OK;
  }

  return nsTableOuterFrame::QueryInterface(aIID, aInstancePtr);                                     
}

/*
 * We are a frame and we do not maintain a ref count
 */
NS_IMETHODIMP_(nsrefcnt) 
nsTreeOuterFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt) 
nsTreeOuterFrame::Release(void)
{
    return NS_OK;
}

NS_IMETHODIMP 
nsTreeOuterFrame::FixBadReflowState(const nsHTMLReflowState& aParentReflowState,
                                    nsHTMLReflowState& aChildReflowState)
{
  if (aParentReflowState.mComputedWidth != NS_UNCONSTRAINEDSIZE) {
    aChildReflowState.mComputedWidth = aParentReflowState.mComputedWidth;
  }

  if (aParentReflowState.mComputedHeight != NS_UNCONSTRAINEDSIZE) {
    aChildReflowState.mComputedHeight = aParentReflowState.mComputedHeight;
  }
  
  return NS_OK;
}

nsITreeFrame*
nsTreeOuterFrame::FindTreeFrame(nsIPresContext* aPresContext)
{
  nsITreeFrame* treeframe;
  nsIFrame* child;
  FirstChild(aPresContext, nsnull, &child);

  while (child != nsnull) {
    if (NS_OK == child->QueryInterface(NS_GET_IID(nsITreeFrame),
                                       (void**)&treeframe)) {
      return treeframe;
    }
    child->GetNextSibling(&child);
  }

  return nsnull;
}


NS_IMETHODIMP
nsTreeOuterFrame::ScrollByLines(nsIPresContext* aPresContext, PRInt32 lines)
{
  // What we need to do is call the corresponding method on our TreeFrame
  // In most cases the TreeFrame will be the only child, but just to make
  // sure we'll check for the right interface

  nsITreeFrame* treeframe = FindTreeFrame(aPresContext);
  nsISelfScrollingFrame* ssf;

  if (treeframe) {
    if (NS_OK == treeframe->QueryInterface(NS_GET_IID(nsISelfScrollingFrame),
                                           (void**)&ssf)) {
      return ssf->ScrollByLines(aPresContext, lines);
    }
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsTreeOuterFrame::ScrollByPages(nsIPresContext* aPresContext, PRInt32 pages)
{
  // What we need to do is call the corresponding method on our TreeFrame
  // In most cases the TreeFrame will be the only child, but just to make
  // sure we'll check for the right interface

  nsITreeFrame* treeframe = FindTreeFrame(aPresContext);
  nsISelfScrollingFrame* ssf;

  if (treeframe) {
    if (NS_OK == treeframe->QueryInterface(NS_GET_IID(nsISelfScrollingFrame),
                                           (void**)&ssf)) {
      return ssf->ScrollByPages(aPresContext, pages);
    }
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsTreeOuterFrame::CollapseScrollbar(nsIPresContext* aPresContext, PRBool aHide)
{
  // What we need to do is call the corresponding method on our TreeFrame
  // In most cases the TreeFrame will be the only child, but just to make
  // sure we'll check for the right interface

  nsITreeFrame* treeframe = FindTreeFrame(aPresContext);
  nsISelfScrollingFrame* ssf;

  if (treeframe) {
    if (NS_OK == treeframe->QueryInterface(NS_GET_IID(nsISelfScrollingFrame),
                                       (void**)&ssf)) {
      return ssf->CollapseScrollbar(aPresContext, aHide);
    }
  }

  return NS_ERROR_FAILURE;
}

