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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *    Travis Bogard <travis@netscape.com> 
 */
#include "nsCOMPtr.h"
#include "nsLeafFrame.h"
#include "nsHTMLContainerFrame.h"
#include "nsIHTMLContent.h"
#include "nsIWebShell.h"
#include "nsIDocShell.h"
#include "nsIDocShellLoadInfo.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIWebNavigation.h"
#include "nsIBaseWindow.h"
#include "nsIContentViewer.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsHTMLIIDs.h"
#include "nsIComponentManager.h"
#include "nsIStreamListener.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIDocument.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsWidgetsCID.h"
#include "nsViewsCID.h"
#include "nsHTMLAtoms.h"
#include "nsIScrollableView.h"
#include "nsStyleCoord.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIDocumentLoader.h"
#include "nsIPref.h"
#include "nsFrameSetFrame.h"
#include "nsIDOMHTMLFrameElement.h"
#include "nsIDOMHTMLIFrameElement.h"
#include "nsGenericHTMLElement.h"
#include "nsLayoutAtoms.h"
#include "nsIChromeEventHandler.h"
#include "nsIScriptSecurityManager.h"
#include "nsICodebasePrincipal.h"
#include "nsXPIDLString.h"
#include "nsIScrollable.h"

#ifdef INCLUDE_XUL
#include "nsIDOMXULElement.h"
#include "nsIBoxObject.h"
#include "nsIBrowserBoxObject.h"
#include "nsISHistory.h"
#endif

class nsHTMLFrame;

static NS_DEFINE_CID(kWebShellCID, NS_WEB_SHELL_CID);
static NS_DEFINE_CID(kCViewCID, NS_VIEW_CID);
static NS_DEFINE_CID(kCChildCID, NS_CHILD_CID);

/*******************************************************************************
 * FrameLoadingInfo 
 ******************************************************************************/
class FrameLoadingInfo : public nsISupports
{
public:
  FrameLoadingInfo(const nsSize& aSize);

  // nsISupports interface...
  NS_DECL_ISUPPORTS

protected:
  virtual ~FrameLoadingInfo() {}

public:
  nsSize mFrameSize;
};


/*******************************************************************************
 * nsHTMLFrameOuterFrame
 ******************************************************************************/
#define nsHTMLFrameOuterFrameSuper nsHTMLContainerFrame

class nsHTMLFrameOuterFrame : public nsHTMLFrameOuterFrameSuper {

public:
  nsHTMLFrameOuterFrame();

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const;
#endif

  NS_IMETHOD GetFrameType(nsIAtom** aType) const;

  NS_IMETHOD Paint(nsIPresContext* aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsFramePaintLayer aWhichLayer);

  NS_IMETHOD Init(nsIPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext,
                  nsIFrame*        aPrevInFlow);

  NS_IMETHOD Reflow(nsIPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent* aChild,
                              PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aHint);
  NS_IMETHOD  VerifyTree() const;
  PRBool IsInline();

protected:
  virtual ~nsHTMLFrameOuterFrame();
  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredSize);
  virtual PRIntn GetSkipSides() const;
  PRBool mIsInline;
};

/*******************************************************************************
 * nsHTMLFrameInnerFrame
 ******************************************************************************/
class nsHTMLFrameInnerFrame : public nsLeafFrame {

public:

  nsHTMLFrameInnerFrame();

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const;
#endif

  NS_IMETHOD GetFrameType(nsIAtom** aType) const;

  /**
    * @see nsIFrame::Paint
    */
  NS_IMETHOD Paint(nsIPresContext* aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsFramePaintLayer aWhichLayer);

  /**
    * @see nsIFrame::Reflow
    */
  NS_IMETHOD Reflow(nsIPresContext*          aCX,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD DidReflow(nsIPresContext* aPresContext,
                       nsDidReflowStatus aStatus);

  NS_IMETHOD GetParentContent(nsIContent*& aContent);
  PRBool GetURL(nsIContent* aContent, nsString& aResult);
  PRBool GetName(nsIContent* aContent, nsString& aResult);
  PRInt32 GetScrolling(nsIContent* aContent, PRBool aStandardMode);
  nsFrameborder GetFrameBorder(PRBool aStandardMode);
  PRInt32 GetMarginWidth(nsIPresContext* aPresContext, nsIContent* aContent);
  PRInt32 GetMarginHeight(nsIPresContext* aPresContext, nsIContent* aContent);

  nsresult ReloadURL(nsIPresContext* aPresContext);

protected:
  nsresult CreateDocShell(nsIPresContext* aPresContext, const nsSize& aSize);
  nsresult DoLoadURL(nsIPresContext* aPresContext);

  virtual ~nsHTMLFrameInnerFrame();

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredSize);

  nsCOMPtr<nsIBaseWindow> mSubShell;
  PRBool mCreatingViewer;
};


/*******************************************************************************
 * nsHTMLFrameOuterFrame
 ******************************************************************************/
nsHTMLFrameOuterFrame::nsHTMLFrameOuterFrame()
  : nsHTMLContainerFrame()
{
  mIsInline = PR_FALSE;
}

nsHTMLFrameOuterFrame::~nsHTMLFrameOuterFrame()
{
  //printf("nsHTMLFrameOuterFrame destructor %X \n", this);
}

NS_IMETHODIMP
nsHTMLFrameOuterFrame::Init(nsIPresContext*  aPresContext,
                            nsIContent*      aContent,
                            nsIFrame*        aParent,
                            nsIStyleContext* aContext,
                            nsIFrame*        aPrevInFlow)
{
  // determine if we are a <frame> or <iframe>
  if (aContent) {
    nsCOMPtr<nsIDOMHTMLFrameElement> frameElem = do_QueryInterface(aContent);
    mIsInline = frameElem ? PR_FALSE : PR_TRUE;
  }
  return nsHTMLFrameOuterFrameSuper::Init(aPresContext, aContent, aParent,
                                          aContext, aPrevInFlow);
}

PRIntn
nsHTMLFrameOuterFrame::GetSkipSides() const
{
  return 0;
}

void 
nsHTMLFrameOuterFrame::GetDesiredSize(nsIPresContext* aPresContext,
                                      const nsHTMLReflowState& aReflowState,
                                      nsHTMLReflowMetrics& aDesiredSize)
{
  // <frame> processing does not use this routine, only <iframe>
  float p2t;
  aPresContext->GetScaledPixelsToTwips(&p2t);

  // If no width/height was specified, use 300/150.
  // This is for compatability with IE.
  if (NS_UNCONSTRAINEDSIZE != aReflowState.mComputedWidth) {
    aDesiredSize.width = aReflowState.mComputedWidth;
  }
  else {
    aDesiredSize.width = NSIntPixelsToTwips(300, p2t);
  }
  if (NS_UNCONSTRAINEDSIZE != aReflowState.mComputedHeight) {
    aDesiredSize.height = aReflowState.mComputedHeight;
  }
  else {
    aDesiredSize.height = NSIntPixelsToTwips(150, p2t);
  }
  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;

  // For unknown reasons, the maxElementSize for the InnerFrame is used, but the
  // maxElementSize for the OuterFrame is ignored, the following is not used!
  if (aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = aDesiredSize.width;
    aDesiredSize.maxElementSize->height = aDesiredSize.height;
  }
}

PRBool nsHTMLFrameOuterFrame::IsInline()
{ 
  return mIsInline;
}

NS_IMETHODIMP
nsHTMLFrameOuterFrame::Paint(nsIPresContext* aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect& aDirtyRect,
                             nsFramePaintLayer aWhichLayer)
{
  //printf("outer paint %X (%d,%d,%d,%d) \n", this, aDirtyRect.x, aDirtyRect.y, aDirtyRect.width, aDirtyRect.height);
  nsIFrame* firstChild = mFrames.FirstChild();
  if (nsnull != firstChild) {
    firstChild->Paint(aPresContext, aRenderingContext, aDirtyRect,
                      aWhichLayer);
  }
  if (IsInline()) {
    return nsHTMLContainerFrame::Paint(aPresContext, aRenderingContext,
                                       aDirtyRect, aWhichLayer);
  } else {
    return NS_OK;
  }
}

#ifdef DEBUG
NS_IMETHODIMP nsHTMLFrameOuterFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("FrameOuter", aResult);
}
#endif

NS_IMETHODIMP
nsHTMLFrameOuterFrame::GetFrameType(nsIAtom** aType) const
{
  NS_PRECONDITION(nsnull != aType, "null OUT parameter pointer");
  *aType = nsLayoutAtoms::htmlFrameOuterFrame; 
  NS_ADDREF(*aType);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFrameOuterFrame::Reflow(nsIPresContext*          aPresContext,
                              nsHTMLReflowMetrics&     aDesiredSize,
                              const nsHTMLReflowState& aReflowState,
                              nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsHTMLFrameOuterFrame", aReflowState.reason);
  //printf("OuterFrame::Reflow %X (%d,%d) \n", this, aReflowState.availableWidth, aReflowState.availableHeight); 
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("enter nsHTMLFrameOuterFrame::Reflow: maxSize=%d,%d reason=%d",
      aReflowState.availableWidth, aReflowState.availableHeight, aReflowState.reason));

  if (IsInline()) {
    GetDesiredSize(aPresContext, aReflowState, aDesiredSize); // IFRAME
  } else {
    aDesiredSize.width  = aReflowState.availableWidth; // FRAME
    aDesiredSize.height = aReflowState.availableHeight;
    if (aDesiredSize.maxElementSize) { // Probably not used...
      aDesiredSize.maxElementSize->width = aDesiredSize.width;
      aDesiredSize.maxElementSize->height = aDesiredSize.height;
    }
  }

  nsIFrame* firstChild = mFrames.FirstChild();
  if (nsnull == firstChild) {
    nsCOMPtr<nsIPresShell> shell;
    aPresContext->GetShell(getter_AddRefs(shell));
    firstChild = new (shell.get()) nsHTMLFrameInnerFrame;
    if (firstChild) {
      mFrames.SetFrames(firstChild);
      // Resolve the style context for the inner frame
      nsresult rv = NS_OK;
      nsIStyleContext *innerStyleContext = nsnull;
      rv = aPresContext->ResolveStyleContextFor(mContent, mStyleContext,
                                                PR_FALSE,
                                                &innerStyleContext);
      if (NS_SUCCEEDED(rv)) {
        rv = firstChild->Init(aPresContext, mContent, this, innerStyleContext, nsnull);
        // have to release the context: Init does its own AddRef...
        NS_RELEASE(innerStyleContext);
      } else {
        NS_WARNING( "Error resolving style for InnerFrame in nsHTMLFrameOuterFrame");
      }
      if (NS_FAILED(rv)){
        NS_WARNING( "Error initializing InnerFrame in nsHTMLFrameOuterFrame");
        return rv;
      }
    } else {
      NS_WARNING("no memory allocating inner frame in nsHTMLFrameOuterFrame");
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
 
  nsSize innerSize(aDesiredSize.width, aDesiredSize.height);
  nsPoint offset(0,0);
  nsMargin border = aReflowState.mComputedBorderPadding;
  if (IsInline()) {
    offset.x = border.left;
    offset.y = border.top;
    // XXX Don't subtract the border!!! The size we are given does not include our
    // border! -EDV
    //innerSize.width  -= border.left + border.right;
    //innerSize.height -= border.top  + border.bottom;

    // we now need to add our border in. -EDV
    aDesiredSize.width += border.left + border.right;
    aDesiredSize.height += border.top + border.bottom;
  }

  // Reflow the child and get its desired size
  nsHTMLReflowMetrics kidMetrics(aDesiredSize.maxElementSize);
  nsHTMLReflowState   kidReflowState(aPresContext, aReflowState, firstChild,
                                     innerSize);
  ReflowChild(firstChild, aPresContext, kidMetrics, kidReflowState,
              offset.x, offset.y, 0, aStatus);
  NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");

  // For unknown reasons, the maxElementSize for the InnerFrame is used, but the
  // maxElementSize for the OuterFrame is ignored, add in border here to prevent
  // a table from shrinking inside the iframe's border when resized.
  if (IsInline()) {
    if (kidMetrics.maxElementSize) {
      kidMetrics.maxElementSize->width += border.left + border.right;
      kidMetrics.maxElementSize->height += border.top + border.bottom;
    }
  }

  // Place and size the child
  FinishReflowChild(firstChild, aPresContext, kidMetrics, offset.x, offset.y, 0);

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("exit nsHTMLFrameOuterFrame::Reflow: size=%d,%d status=%x",
      aDesiredSize.width, aDesiredSize.height, aStatus));

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFrameOuterFrame::VerifyTree() const
{
  // XXX Completely disabled for now; once pseud-frames are reworked
  // then we can turn it back on.
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFrameOuterFrame::AttributeChanged(nsIPresContext* aPresContext,
                                        nsIContent* aChild,
                                        PRInt32 aNameSpaceID,
                                        nsIAtom* aAttribute,
                                        PRInt32 aHint)
{
  if (nsHTMLAtoms::src == aAttribute) {
    printf("got a request\n");
    nsIFrame* firstChild = mFrames.FirstChild();
    if (nsnull != firstChild) {
      return ((nsHTMLFrameInnerFrame*)firstChild)->ReloadURL(aPresContext);
    }
  }
  return NS_OK;
}

nsresult
NS_NewHTMLFrameOuterFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsHTMLFrameOuterFrame* it = new (aPresShell) nsHTMLFrameOuterFrame;
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

/*******************************************************************************
 * nsHTMLFrameInnerFrame
 ******************************************************************************/
nsHTMLFrameInnerFrame::nsHTMLFrameInnerFrame()
  : nsLeafFrame()
{
  mCreatingViewer = PR_FALSE;
}

nsHTMLFrameInnerFrame::~nsHTMLFrameInnerFrame()
{
   //printf("nsHTMLFrameInnerFrame destructor %X \n", this);

#ifdef INCLUDE_XUL
  nsCOMPtr<nsIDOMXULElement> xulElt(do_QueryInterface(mContent));
  if (xulElt && mSubShell) {
    // We might be a XUL browser and may need to store the current URL in our box object.
    nsCOMPtr<nsIBoxObject> boxObject;
    xulElt->GetBoxObject(getter_AddRefs(boxObject));
    if (boxObject) {
      nsCOMPtr<nsIBrowserBoxObject> browser(do_QueryInterface(boxObject));
      if (browser) {
        nsCOMPtr<nsIWebNavigation> webShell(do_QueryInterface(mSubShell));
        nsCOMPtr<nsISHistory> hist;
        webShell->GetSessionHistory(getter_AddRefs(hist));
        if (hist)
          boxObject->SetPropertyAsSupports(NS_LITERAL_STRING("history"), hist);
      }
    }
  }
#endif
 
  if(mSubShell)
    mSubShell->Destroy();
  mSubShell = nsnull; // This is the location it was released before...
                      // Not sure if there is ordering depending on this.
}

PRBool nsHTMLFrameInnerFrame::GetURL(nsIContent* aContent, nsString& aResult)
{
  aResult.SetLength(0);    
  if (NS_CONTENT_ATTR_HAS_VALUE == (aContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::src, aResult))) {
    if (aResult.Length() > 0) {
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}

PRBool nsHTMLFrameInnerFrame::GetName(nsIContent* aContent, nsString& aResult)
{
  aResult.SetLength(0);     

  if (NS_CONTENT_ATTR_HAS_VALUE == (aContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::name, aResult))) {
    if (aResult.Length() > 0) {
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}

PRInt32 nsHTMLFrameInnerFrame::GetScrolling(nsIContent* aContent, PRBool aStandardMode)
{
  PRInt32 returnValue = -1;
  nsresult rv = NS_OK;
  nsCOMPtr<nsIHTMLContent> content = do_QueryInterface(mContent, &rv);
  if (NS_SUCCEEDED(rv) && content) {
    nsHTMLValue value;
    if (NS_CONTENT_ATTR_HAS_VALUE == (content->GetHTMLAttribute(nsHTMLAtoms::scrolling, value))) {
      if (eHTMLUnit_Enumerated == value.GetUnit()) {
        PRInt32 intValue;
        intValue = value.GetIntValue();
        if (!aStandardMode) {
          if ((NS_STYLE_FRAME_ON == intValue) || (NS_STYLE_FRAME_SCROLL == intValue)) {
            returnValue = NS_STYLE_OVERFLOW_SCROLL;
          } else if ((NS_STYLE_FRAME_OFF == intValue) || (NS_STYLE_FRAME_NOSCROLL == intValue)) {
            returnValue = NS_STYLE_OVERFLOW_HIDDEN;
          }
        } else {
          if (NS_STYLE_FRAME_YES == intValue) {
            returnValue = NS_STYLE_OVERFLOW_SCROLL;
          } else if (NS_STYLE_FRAME_NO == intValue) {
            returnValue = NS_STYLE_OVERFLOW_HIDDEN;
          } else if (NS_STYLE_FRAME_AUTO == intValue) {
            returnValue = NS_STYLE_OVERFLOW_AUTO;
          }
        }
      }      
    }

    // Check style for overflow
    const nsStyleDisplay* display;
    GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)display));
    if (display->mOverflow)
      returnValue = display->mOverflow;

  }
  return returnValue;
}

nsFrameborder nsHTMLFrameInnerFrame::GetFrameBorder(PRBool aStandardMode)
{
  nsFrameborder rv = eFrameborder_Notset;
  nsresult res = NS_OK;
  nsCOMPtr<nsIHTMLContent> content = do_QueryInterface(mContent, &res);
  if (NS_SUCCEEDED(res) && content) {
    nsHTMLValue value;
    if (NS_CONTENT_ATTR_HAS_VALUE == (content->GetHTMLAttribute(nsHTMLAtoms::frameborder, value))) {
      if (eHTMLUnit_Enumerated == value.GetUnit()) {
        PRInt32 intValue;
        intValue = value.GetIntValue();
        if (!aStandardMode) {
          if (NS_STYLE_FRAME_YES == intValue) {
            rv = eFrameborder_Yes;
          } 
          else if (NS_STYLE_FRAME_NO == intValue) {
            rv = eFrameborder_No;
          }
        } else {
          if (NS_STYLE_FRAME_0 == intValue) {
            rv = eFrameborder_No;
          } 
          else if (NS_STYLE_FRAME_1 == intValue) {
            rv = eFrameborder_Yes;
          }
        }
      }
    }
  }
  // XXX if we get here, check for nsIDOMFRAMESETElement interface
  return rv;
}


PRInt32 nsHTMLFrameInnerFrame::GetMarginWidth(nsIPresContext* aPresContext, nsIContent* aContent)
{
  PRInt32 marginWidth = -1;
  nsresult rv = NS_OK;
  nsCOMPtr<nsIHTMLContent> content = do_QueryInterface(mContent, &rv);
  if (NS_SUCCEEDED(rv) && content) {
    float p2t;
    aPresContext->GetScaledPixelsToTwips(&p2t);
    nsHTMLValue value;
    content->GetHTMLAttribute(nsHTMLAtoms::marginwidth, value);
    if (eHTMLUnit_Pixel == value.GetUnit()) { 
      marginWidth = NSIntPixelsToTwips(value.GetPixelValue(), p2t);
      if (marginWidth < 0) {
        marginWidth = 0;
      }
    }
  }
  return marginWidth;
}

PRInt32 nsHTMLFrameInnerFrame::GetMarginHeight(nsIPresContext* aPresContext, nsIContent* aContent)
{
  PRInt32 marginHeight = -1;
  nsresult rv = NS_OK;
  nsCOMPtr<nsIHTMLContent> content = do_QueryInterface(mContent, &rv);
  if (NS_SUCCEEDED(rv) && content) {
    float p2t;
    aPresContext->GetScaledPixelsToTwips(&p2t);
    nsHTMLValue value;
    content->GetHTMLAttribute(nsHTMLAtoms::marginheight, value);
    if (eHTMLUnit_Pixel == value.GetUnit()) { 
      marginHeight = NSIntPixelsToTwips(value.GetPixelValue(), p2t);
      if (marginHeight < 0) {
        marginHeight = 0;
      }
    }
  }
  return marginHeight;
}

#ifdef DEBUG
NS_IMETHODIMP nsHTMLFrameInnerFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("FrameInner", aResult);
}
#endif

NS_IMETHODIMP
nsHTMLFrameInnerFrame::GetFrameType(nsIAtom** aType) const
{
  NS_PRECONDITION(nsnull != aType, "null OUT parameter pointer");
  *aType = nsLayoutAtoms::htmlFrameInnerFrame; 
  NS_ADDREF(*aType);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFrameInnerFrame::Paint(nsIPresContext*      aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect&        aDirtyRect,
                             nsFramePaintLayer    aWhichLayer)
{
  //printf("inner paint %X (%d,%d,%d,%d) \n", this, aDirtyRect.x, aDirtyRect.y, aDirtyRect.width, aDirtyRect.height);
  // if there is not web shell paint based on our background color, 
  // otherwise let the web shell paint the sub document 
  if (!mSubShell) {
    const nsStyleColor* color =
      (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
    aRenderingContext.SetColor(color->mBackgroundColor);
    aRenderingContext.FillRect(mRect);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFrameInnerFrame::GetParentContent(nsIContent*& aContent)
{
  nsHTMLFrameOuterFrame* parent;
  nsresult rv = GetParent((nsIFrame**)&parent);
  if (NS_SUCCEEDED(rv) && parent) {
    rv = parent->GetContent(&aContent);
  }
  return rv;
}

NS_IMETHODIMP
nsHTMLFrameInnerFrame::DidReflow(nsIPresContext* aPresContext,
                        nsDidReflowStatus aStatus)
{
  nsresult rv = nsLeafFrame::DidReflow(aPresContext, aStatus);


  // The view is created hidden; once we have reflowed it and it has been
  // positioned then we show it.
  if (NS_FRAME_REFLOW_FINISHED == aStatus) {
    nsIView* view = nsnull;
    GetView(aPresContext, &view);
    if (view) {
      const nsStyleDisplay* display;
      GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)display));
      nsViewVisibility newVis = display->IsVisible() ? nsViewVisibility_kShow : nsViewVisibility_kHide;
      nsViewVisibility oldVis;
      // only change if different.
      view->GetVisibility(oldVis);
      if (newVis != oldVis) 
        view->SetVisibility(newVis);
    }
  }
  
  return rv;
}

nsresult
nsHTMLFrameInnerFrame::CreateDocShell(nsIPresContext* aPresContext,
                                      const nsSize& aSize)
{
  nsresult rv;
  nsCOMPtr<nsIContent> parentContent;
  GetParentContent(*getter_AddRefs(parentContent));

  mSubShell = do_CreateInstance(kWebShellCID);
  NS_ENSURE_TRUE(mSubShell, NS_ERROR_FAILURE);

  // notify the pres shell that a docshell has been created
  nsCOMPtr<nsIPresShell> presShell;
  aPresContext->GetShell(getter_AddRefs(presShell));
  if (presShell)
  {
    nsCOMPtr<nsISupports> subShellAsSupports(do_QueryInterface(mSubShell));
    NS_ENSURE_TRUE(subShellAsSupports, NS_ERROR_FAILURE);
    presShell->SetSubShellFor(mContent, subShellAsSupports);
  }
  
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(mSubShell));
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);
  // pass along marginwidth, marginheight, scrolling so sub document can use it
  docShell->SetMarginWidth(GetMarginWidth(aPresContext, parentContent));
  docShell->SetMarginHeight(GetMarginHeight(aPresContext, parentContent));
  nsCompatibility mode;
  aPresContext->GetCompatibilityMode(&mode);
  // Current and initial scrolling is set so that all succeeding docs
  // will use the scrolling value set here, regardless if scrolling is
  // set by viewing a particular document (e.g. XUL turns off scrolling)
  nsCOMPtr<nsIScrollable> scrollableContainer(do_QueryInterface(mSubShell));
  if (scrollableContainer) {
    scrollableContainer->SetDefaultScrollbarPreferences(nsIScrollable::ScrollOrientation_Y,
      GetScrolling(parentContent, mode));
    scrollableContainer->SetDefaultScrollbarPreferences(nsIScrollable::ScrollOrientation_X,
      GetScrolling(parentContent, mode));
  }

  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mSubShell));
  NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);
  nsString frameName;
  if (GetName(parentContent, frameName)) {
    docShellAsItem->SetName(frameName.GetUnicode());
  }

  // If our container is a web-shell, inform it that it has a new
  // child. If it's not a web-shell then some things will not operate
  // properly.
  nsCOMPtr<nsISupports> container;
  aPresContext->GetContainer(getter_AddRefs(container));
  if (container) {
    nsCOMPtr<nsIDocShellTreeNode> parentAsNode(do_QueryInterface(container));
    if (parentAsNode) {
      nsCOMPtr<nsIDocShellTreeItem> parentAsItem(do_QueryInterface(parentAsNode));
      PRInt32 parentType;
      parentAsItem->GetItemType(&parentType);

      nsIAtom* typeAtom = NS_NewAtom("type");
      nsAutoString value, valuePiece;
      PRBool isContent;

      isContent = PR_FALSE;
      if (NS_SUCCEEDED(parentContent->GetAttribute(kNameSpaceID_None,
         typeAtom, value))) {

        // we accept "content" and "content-xxx" values.
        // at time of writing, we expect "xxx" to be "primary", but
        // someday it might be an integer expressing priority
        value.Left(valuePiece, 7);
        if (valuePiece.EqualsIgnoreCase("content") &&
           (value.Length() == 7 ||
              value.Mid(valuePiece, 7, 1) == 1 && valuePiece.EqualsWithConversion("-")))
            isContent = PR_TRUE;
      }
      NS_IF_RELEASE(typeAtom);
      if (isContent) {
        // The web shell's type is content.
        docShellAsItem->SetItemType(nsIDocShellTreeItem::typeContent);
      } else {
        // Inherit our type from our parent webshell.  If it is
        // chrome, we'll be chrome.  If it is content, we'll be
        // content.
        docShellAsItem->SetItemType(parentType);
      }
      
      parentAsNode->AddChild(docShellAsItem);

      if (isContent) {
        nsCOMPtr<nsIDocShellTreeOwner> parentTreeOwner;
        parentAsItem->GetTreeOwner(getter_AddRefs(parentTreeOwner));
        if(parentTreeOwner)
          parentTreeOwner->ContentShellAdded(docShellAsItem, 
            value.EqualsIgnoreCase("content-primary") ? PR_TRUE : PR_FALSE, 
            value.GetUnicode());
      }
      // connect the container...
      nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(mSubShell));
      nsCOMPtr<nsIWebShellContainer> outerContainer(do_QueryInterface(container));
      if (outerContainer)
        webShell->SetContainer(outerContainer);


      // Make sure all shells have links back to the content element in the
      // nearest enclosing chrome shell.
      nsCOMPtr<nsIDocShell> parentShell(do_QueryInterface(parentAsNode));
      nsCOMPtr<nsIChromeEventHandler> chromeEventHandler;
      if (parentType == nsIDocShellTreeItem::typeChrome) {
        // Our parent shell is a chrome shell. It is therefore our nearest
        // enclosing chrome shell.
        chromeEventHandler = do_QueryInterface(mContent);
        NS_WARN_IF_FALSE(chromeEventHandler, "This mContent should implement this.");
      }
      else {
        // Our parent shell is a content shell. Get the chrome info from
        // it and use that for our shell as well.
        parentShell->GetChromeEventHandler(getter_AddRefs(chromeEventHandler));
      }

      docShell->SetChromeEventHandler(chromeEventHandler);
    }
  }

  float t2p;
  aPresContext->GetTwipsToPixels(&t2p);

  // create, init, set the parent of the view
  nsIView* view;
  rv = nsComponentManager::CreateInstance(kCViewCID, nsnull, NS_GET_IID(nsIView),
                                        (void **)&view);
  if (NS_OK != rv) {
    NS_ASSERTION(0, "Could not create view for nsHTMLFrame");
    return rv;
  }

  nsIView* parView;
  nsPoint origin;
  GetOffsetFromView(aPresContext, origin, &parView);  
  nsRect viewBounds(origin.x, origin.y, aSize.width, aSize.height);

  nsCOMPtr<nsIViewManager> viewMan;
  presShell->GetViewManager(getter_AddRefs(viewMan));  
  rv = view->Init(viewMan, viewBounds, parView);
  viewMan->InsertChild(parView, view, 0);
  rv = view->CreateWidget(kCChildCID);
  SetView(aPresContext, view);

  // if the visibility is hidden, reflect that in the view
  const nsStyleDisplay* display;
  GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)display));
  if (!display->IsVisible()) {
    view->SetVisibility(nsViewVisibility_kHide);
  }

  nsCOMPtr<nsIWidget> widget;
  view->GetWidget(*getter_AddRefs(widget));

  mSubShell->InitWindow(nsnull, widget, 0, 0, NSToCoordRound(aSize.width * t2p),
    NSToCoordRound(aSize.height * t2p));
  mSubShell->Create();

  mSubShell->SetVisibility(PR_TRUE);

  return NS_OK;
}

static PRBool CheckForBrowser(nsIContent* aContent, nsIBaseWindow* aShell)
{
#ifdef INCLUDE_XUL
  nsCOMPtr<nsIDOMXULElement> xulElt(do_QueryInterface(aContent));
  if (xulElt) {
    // We might be a XUL browser and may have stored the URL in our box object.
    nsCOMPtr<nsIBoxObject> boxObject;
    xulElt->GetBoxObject(getter_AddRefs(boxObject));
    if (boxObject) {
      nsCOMPtr<nsIBrowserBoxObject> browser(do_QueryInterface(boxObject));
      if (browser) {
        nsCOMPtr<nsISupports> supp;
        boxObject->GetPropertyAsSupports(NS_LITERAL_STRING("history"), getter_AddRefs(supp));
        if (supp) {
          nsCOMPtr<nsISHistory> hist(do_QueryInterface(supp));
          if (hist) {
            nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(aShell));
            webNav->SetSessionHistory(hist);
            nsCOMPtr<nsIWebNavigation> histNav(do_QueryInterface(hist));
            histNav->Reload(0);
            boxObject->RemoveProperty(NS_LITERAL_STRING("history"));
            return PR_FALSE;
          }
        }
      }
    }
  }
#endif
  return PR_TRUE;
}

nsresult
nsHTMLFrameInnerFrame::DoLoadURL(nsIPresContext* aPresContext)
{
  nsresult rv = NS_OK;

  NS_ENSURE_TRUE (mSubShell, NS_ERROR_UNEXPECTED);

  // Prevent recursion
  mCreatingViewer=PR_TRUE;

  // Get the URL to load
  nsCOMPtr<nsIContent> parentContent;
  rv = GetParentContent(*getter_AddRefs(parentContent));
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && parentContent, rv);

  PRBool load = CheckForBrowser(parentContent, mSubShell);

  if (load) {
    nsAutoString url;
    GetURL(parentContent, url);
    url.Trim(" \t\n\r");
    if (url.IsEmpty())  // Load about:blank into a frame if not URL is specified (bug 35986)
      url = NS_ConvertASCIItoUCS2("about:blank");

    // Make an absolute URL
    nsCOMPtr<nsIURI> baseURL;
    nsCOMPtr<nsIHTMLContent> htmlContent = do_QueryInterface(parentContent, &rv);
    if (NS_SUCCEEDED(rv) && htmlContent) {
      htmlContent->GetBaseURL(*getter_AddRefs(baseURL));
    }
    else {
      nsCOMPtr<nsIDocument> doc;
      rv = parentContent->GetDocument(*getter_AddRefs(doc));
      if (NS_SUCCEEDED(rv) && doc) {
        doc->GetBaseURL(*getter_AddRefs(baseURL));
      }
    }
    nsAutoString absURL;
    rv = NS_MakeAbsoluteURI(absURL, url, baseURL);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIURI> uri;
    NS_NewURI(getter_AddRefs(uri), absURL, nsnull);

    // Check for security
    NS_WITH_SERVICE(nsIScriptSecurityManager, secMan,
                    NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // Get base URL
    nsCOMPtr<nsIURI> baseURI;
    rv = aPresContext->GetBaseURL(getter_AddRefs(baseURI));

    // Get docshell and create load info
    nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(mSubShell));
    NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);
    nsCOMPtr<nsIDocShellLoadInfo> loadInfo;
    docShell->CreateLoadInfo(getter_AddRefs(loadInfo));
    NS_ENSURE_TRUE(loadInfo, NS_ERROR_FAILURE);

    // Get referring URL
    nsCOMPtr<nsIURI> referrer;
    nsCOMPtr<nsIPrincipal> principal;
    rv = secMan->GetSubjectPrincipal(getter_AddRefs(principal));
    NS_ENSURE_SUCCESS(rv, rv);
    // If we were called from script, get the referring URL from the script
    if (principal) {
      nsCOMPtr<nsICodebasePrincipal> codebase = do_QueryInterface(principal);
      if (codebase) {
        rv = codebase->GetURI(getter_AddRefs(referrer));
        NS_ENSURE_SUCCESS(rv, rv);
      }
      // Pass the script principal to the docshell
      nsCOMPtr<nsISupports> owner = do_QueryInterface(principal);
      loadInfo->SetOwner(owner);
    }
    if (!referrer) { // We're not being called form script, tell the docshell
                     // to inherit an owner from the current document.
      loadInfo->SetInheritOwner(PR_TRUE);
      referrer = baseURI;
    }

    // Check if we are allowed to load absURL
    nsCOMPtr<nsIURI> newURI;
    rv = NS_NewURI(getter_AddRefs(newURI), absURL, baseURI);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = secMan->CheckLoadURI(referrer, newURI, nsIScriptSecurityManager::STANDARD);
    if (NS_FAILED(rv))
      return rv; // We're not

    rv = docShell->LoadURI(uri, loadInfo, nsIWebNavigation::LOAD_FLAGS_NONE);
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to load URL");
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLFrameInnerFrame::Reflow(nsIPresContext*          aPresContext,
                              nsHTMLReflowMetrics&     aDesiredSize,
                              const nsHTMLReflowState& aReflowState,
                              nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsHTMLFrameInnerFrame", aReflowState.reason);
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("enter nsHTMLFrameInnerFrame::Reflow: maxSize=%d,%d reason=%d",
      aReflowState.availableWidth,
      aReflowState.availableHeight,
      aReflowState.reason));

  nsresult rv = NS_OK;

  // use the max size set in aReflowState by the nsHTMLFrameOuterFrame as our size
  if (!mCreatingViewer) {
    // create the web shell
    // we do this even if the size is not positive (bug 11762)
    // we do this even if there is no src (bug 16218)
    if (!mSubShell) {
      nsSize  maxSize(aReflowState.availableWidth, aReflowState.availableHeight);
      rv = CreateDocShell(aPresContext, maxSize);
    }
    // Whether or not we had to create a webshell, load the document
    if (NS_SUCCEEDED(rv)) {
      rv = DoLoadURL(aPresContext);
    }
  }

  GetDesiredSize(aPresContext, aReflowState, aDesiredSize);

  aStatus = NS_FRAME_COMPLETE;

  // resize the sub document
  if(mSubShell) {
    float t2p;
    aPresContext->GetTwipsToPixels(&t2p);

    PRInt32 x = 0;
    PRInt32 y = 0;

    mSubShell->GetPositionAndSize(&x, &y, nsnull, nsnull);
    PRInt32 cx  = NSToCoordRound(aDesiredSize.width * t2p);
    PRInt32 cy = NSToCoordRound(aDesiredSize.height * t2p);
    mSubShell->SetPositionAndSize(x, y, cx, cy, PR_FALSE);

    NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
      ("exit nsHTMLFrameInnerFrame::Reflow: size=%d,%d rv=%x",
      aDesiredSize.width, aDesiredSize.height, aStatus));
  }
    
  return rv;
}

// load a new url
nsresult
nsHTMLFrameInnerFrame::ReloadURL(nsIPresContext* aPresContext)
{
  return DoLoadURL(aPresContext);
}

void 
nsHTMLFrameInnerFrame::GetDesiredSize(nsIPresContext* aPresContext,
                                      const nsHTMLReflowState& aReflowState,
                                      nsHTMLReflowMetrics& aDesiredSize)
{
  aDesiredSize.width  = aReflowState.availableWidth;
  aDesiredSize.height = aReflowState.availableHeight;
  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;

  // For unknown reasons, the maxElementSize for the InnerFrame is used, but the
  // maxElementSize for the OuterFrame is ignored, make sure to get it right here!
  if (aDesiredSize.maxElementSize) {
    if ((NS_UNCONSTRAINEDSIZE == aReflowState.availableWidth) ||
        (eStyleUnit_Percent == aReflowState.mStylePosition->mWidth.GetUnit())) {
      aDesiredSize.maxElementSize->width = 0; // percent width springy down to 0 px
    }
    else {
      aDesiredSize.maxElementSize->width = aDesiredSize.width;
    }
    if ((NS_UNCONSTRAINEDSIZE == aReflowState.availableHeight) ||
        (eStyleUnit_Percent == aReflowState.mStylePosition->mHeight.GetUnit())) {
      aDesiredSize.maxElementSize->height = 0; // percent height springy down to 0px
    }
    else {
      aDesiredSize.maxElementSize->height = aDesiredSize.height;
    }
  }
}

/*******************************************************************************
 * FrameLoadingInfo
 ******************************************************************************/
FrameLoadingInfo::FrameLoadingInfo(const nsSize& aSize)
{
  NS_INIT_REFCNT();

  mFrameSize = aSize;
}

/*
 * Implementation of ISupports methods...
 */
NS_IMPL_ISUPPORTS(FrameLoadingInfo, NS_GET_IID(nsISupports));

