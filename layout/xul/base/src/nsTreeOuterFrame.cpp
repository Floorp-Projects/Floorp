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
nsTreeOuterFrame::HandleEvent(nsIPresContext* aPresContext, 
                             nsGUIEvent*     aEvent,
                             nsEventStatus*  aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);
  *aEventStatus = nsEventStatus_eConsumeDoDefault;
  if (aEvent->message == NS_KEY_DOWN) {
    // Retrieve the tree frame.
    nsIFrame* curr = mFrames.FirstChild();
    while (curr) {
      nsCOMPtr<nsIContent> content;
      curr->GetContent(getter_AddRefs(content));
      if (content) {
        nsCOMPtr<nsIAtom> tag;
        content->GetTag(*getter_AddRefs(tag));
        if (tag && tag.get() == nsXULAtoms::tree) {
          // This is our actual tree frame.
          return curr->HandleEvent(aPresContext, aEvent, aEventStatus);
        }
      }

      curr->GetNextSibling(&curr);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsTreeOuterFrame::Reflow(nsIPresContext*          aPresContext,
							      nsHTMLReflowMetrics&     aDesiredSize,
							      const nsHTMLReflowState& aReflowState,
							      nsReflowStatus&          aStatus)
{
  NS_ASSERTION(aReflowState.mComputedWidth != NS_UNCONSTRAINEDSIZE, 
               "Reflowing outer tree frame with unconstrained width!!!!");
  
  NS_ASSERTION(aReflowState.mComputedHeight != NS_UNCONSTRAINEDSIZE, 
               "Reflowing outer tree frame with unconstrained height!!!!");

  //printf("TOF Width: %d, TOF Height: %d\n", aReflowState.mComputedWidth, aReflowState.mComputedHeight);

  return nsTableOuterFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);
}

/**
 * Ok return our dimensions
 */
NS_IMETHODIMP
nsTreeOuterFrame::GetBoxInfo(nsIPresContext* aPresContext, const nsHTMLReflowState& aReflowState, nsBoxInfo& aSize)
{
  aSize.minSize.width = 0;
  aSize.minSize.height = 0;
  aSize.prefSize.width = 100;
  aSize.prefSize.height = 100;

  ((nsCalculatedBoxInfo&)aSize).prefWidthIntrinsic = PR_FALSE;
  ((nsCalculatedBoxInfo&)aSize).prefHeightIntrinsic = PR_FALSE;

  return NS_OK;
}

/**
 * We can be a nsIBox
 */
NS_IMETHODIMP 
nsTreeOuterFrame::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{           
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
                                                                         
  *aInstancePtr = NULL;                                                  
                                                                                        
  if (aIID.Equals(NS_GET_IID(nsIBox))) {                                         
    *aInstancePtr = (void*)(nsIBox*) this;                                        
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }   
  else if (aIID.Equals(NS_GET_IID(nsISelfScrollingFrame))) {
    *aInstancePtr = (void*)(nsISelfScrollingFrame*) this;
    return NS_OK;
  }

  return nsTableOuterFrame::QueryInterface(aIID, aInstancePtr);                                     
}

NS_IMETHODIMP
nsTreeOuterFrame::Dirty(nsIPresContext* aPresContext, const nsHTMLReflowState& aReflowState, nsIFrame*& incrementalChild)
{
  incrementalChild = this;
  return NS_OK;
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
nsTreeOuterFrame::FindTreeFrame()
{
  nsITreeFrame* treeframe;
  nsIFrame* child;
  FirstChild(nsnull, &child);

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

  nsITreeFrame* treeframe = FindTreeFrame();
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
  printf("nsTreeOuterFrame::ScrollByPages\n");
  // What we need to do is call the corresponding method on our TreeFrame
  // In most cases the TreeFrame will be the only child, but just to make
  // sure we'll check for the right interface

  nsITreeFrame* treeframe = FindTreeFrame();
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

  nsITreeFrame* treeframe = FindTreeFrame();
  nsISelfScrollingFrame* ssf;

  if (treeframe) {
    if (NS_OK == treeframe->QueryInterface(NS_GET_IID(nsISelfScrollingFrame),
                                       (void**)&ssf)) {
      return ssf->CollapseScrollbar(aPresContext, aHide);
    }
  }

  return NS_ERROR_FAILURE;
}

