/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsXULLeafFrame.h"
#include "nsCOMPtr.h"
#include "nsIDeviceContext.h"
#include "nsIFontMetrics.h"
#include "nsHTMLAtoms.h"
#include "nsXULAtoms.h"
#include "nsIPresContext.h"
#include "nsIRenderingContext.h"
#include "nsIStyleContext.h"
#include "nsIContent.h"
#include "nsINameSpaceManager.h"

//
// NS_NewToolbarFrame
//
// Creates a new Toolbar frame and returns it in |aNewFrame|
//
nsresult
NS_NewXULLeafFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame )
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsXULLeafFrame* it = new (aPresShell) nsXULLeafFrame;
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  // it->SetFlags(aFlags);
  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewTextFrame



NS_IMETHODIMP
nsXULLeafFrame::Reflow(nsIPresContext*   aPresContext,
                     nsHTMLReflowMetrics&     aMetrics,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus)
{

  if (eReflowReason_Incremental == aReflowState.reason) {
    nsIFrame* targetFrame;
    
    // See if it's targeted at us
    aReflowState.reflowCommand->GetTarget(targetFrame);
    if (this == targetFrame) {
      Invalidate(aPresContext, nsRect(0,0,mRect.width,mRect.height), PR_FALSE);
    }
  } else if (eReflowReason_Dirty == aReflowState.reason) {
    Invalidate(aPresContext, nsRect(0,0,mRect.width,mRect.height), PR_FALSE);
  }

 
  nsresult result = nsLeafFrame::Reflow(aPresContext, aMetrics, aReflowState, aStatus);

  if (aReflowState.mComputedWidth != NS_INTRINSICSIZE)
      NS_ASSERTION(aMetrics.width == aReflowState.mComputedWidth + aReflowState.mComputedBorderPadding.left + aReflowState.mComputedBorderPadding.right,
                   "Text width is wrong!!!");

  if (aReflowState.mComputedHeight != NS_INTRINSICSIZE)
      NS_ASSERTION(aMetrics.height == aReflowState.mComputedHeight + aReflowState.mComputedBorderPadding.top + aReflowState.mComputedBorderPadding.bottom,
                   "Text height is wrong!!!");

  return result;
}

void
nsXULLeafFrame::GetDesiredSize(nsIPresContext* aPresContext,
                                    const nsHTMLReflowState& aReflowState,
                                    nsHTMLReflowMetrics& aDesiredSize)
{
  // get our info object.
  nsBoxInfo info;
  info.Clear();

  GetBoxInfo(aPresContext, aReflowState, info);

  // size is our preferred unless calculated.
  aDesiredSize.width = info.prefSize.width;
  aDesiredSize.height = info.prefSize.height;

  // if either the width or the height was not computed use our intrinsic size
  if (aReflowState.mComputedWidth != NS_INTRINSICSIZE)
       aDesiredSize.width = aReflowState.mComputedWidth;

  if (aReflowState.mComputedHeight != NS_INTRINSICSIZE) {
       aDesiredSize.height = aReflowState.mComputedHeight;
       nscoord descent = info.prefSize.height - info.ascent;
       aDesiredSize.ascent = aDesiredSize.height - descent;
       if (aDesiredSize.ascent < 0)
           aDesiredSize.ascent = 0;
  } else {
       aDesiredSize.ascent = info.ascent;
  }


}



NS_IMETHODIMP
nsXULLeafFrame::InvalidateCache(nsIFrame* aChild)
{
    // we don't cache any information
    return NS_OK;
}

/**
 * Ok return our dimensions
 */
NS_IMETHODIMP
nsXULLeafFrame::GetBoxInfo(nsIPresContext* aPresContext, const nsHTMLReflowState& aReflowState, nsBoxInfo& aSize)
{
  aSize.prefSize.width  = 0;
  aSize.prefSize.height = 0;
  aSize.minSize.width   = 0;
  aSize.minSize.height  = 0;
  aSize.maxSize.width   = NS_INTRINSICSIZE;
  aSize.maxSize.height  = NS_INTRINSICSIZE;
  aSize.ascent          = 0;  
  return NS_OK;
}

/**
 * We can be a nsIBox
 */
NS_IMETHODIMP 
nsXULLeafFrame::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
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

  return nsLeafFrame::QueryInterface(aIID, aInstancePtr);                                     
}

/*
 * We are a frame and we do not maintain a ref count
 */
NS_IMETHODIMP_(nsrefcnt) 
nsXULLeafFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt) 
nsXULLeafFrame::Release(void)
{
    return NS_OK;
}


NS_IMETHODIMP
nsXULLeafFrame::GetFrameName(nsString& aResult) const
{
  aResult = "Leaf";
  return NS_OK;
}

NS_IMETHODIMP
nsXULLeafFrame::ContentChanged(nsIPresContext* aPresContext,
                            nsIContent*     aChild,
                            nsISupports*    aSubContent)
{
    nsCOMPtr<nsIPresShell> shell;
    aPresContext->GetShell(getter_AddRefs(shell));
    mState |= NS_FRAME_IS_DIRTY;
    return mParent->ReflowDirtyChild(shell, this);

}

