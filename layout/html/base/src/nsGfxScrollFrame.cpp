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
 */
#include "nsCOMPtr.h"
#include "nsHTMLParts.h"
#include "nsIPresContext.h"
#include "nsIStyleContext.h"
#include "nsIReflowCommand.h"
#include "nsIDeviceContext.h"
#include "nsPageFrame.h"
#include "nsViewsCID.h"
#include "nsIServiceManager.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsHTMLContainerFrame.h"
#include "nsHTMLIIDs.h"
#include "nsCSSRendering.h"
#include "nsIScrollableView.h"
#include "nsWidgetsCID.h"
#include "nsGfxScrollFrame.h"
#include "nsLayoutAtoms.h"
#include "nsIXMLContent.h"
#include "nsXULAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsINameSpaceManager.h"
#include "nsISupportsArray.h"
#include "nsIDocument.h"
#include "nsIFontMetrics.h"
#include "nsIDocumentObserver.h"
#include "nsIDocument.h"
#include "nsIScrollPositionListener.h"
//#include "nsBoxFrame.h"
#include "nsIElementFactory.h"

static NS_DEFINE_IID(kWidgetCID, NS_CHILD_CID);
static NS_DEFINE_IID(kScrollingViewCID, NS_SCROLLING_VIEW_CID);
static NS_DEFINE_IID(kViewCID, NS_VIEW_CID);

static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);
static NS_DEFINE_IID(kScrollViewIID, NS_ISCROLLABLEVIEW_IID);

static NS_DEFINE_IID(kIAnonymousContentCreatorIID,     NS_IANONYMOUS_CONTENT_CREATOR_IID);
static NS_DEFINE_IID(kIScrollableFrameIID,             NS_ISCROLLABLE_FRAME_IID);

//----------------------------------------------------------------------

class nsGfxScrollFrameInner : public nsIDocumentObserver, 
                              public nsIScrollPositionListener {

  NS_DECL_ISUPPORTS

public:

  nsGfxScrollFrameInner(nsGfxScrollFrame* aOuter);
  virtual ~nsGfxScrollFrameInner();

  // nsIScrollPositionListener

  NS_IMETHOD ScrollPositionWillChange(nsIScrollableView* aScrollable, nscoord aX, nscoord aY);
  NS_IMETHOD ScrollPositionDidChange(nsIScrollableView* aScrollable, nscoord aX, nscoord aY);

 // nsIDocumentObserver
  NS_IMETHOD BeginUpdate(nsIDocument *aDocument) { return NS_OK; }
  NS_IMETHOD EndUpdate(nsIDocument *aDocument) { return NS_OK; }
  NS_IMETHOD BeginLoad(nsIDocument *aDocument) { return NS_OK; }
  NS_IMETHOD EndLoad(nsIDocument *aDocument) { return NS_OK; }
  NS_IMETHOD BeginReflow(nsIDocument *aDocument,
			                   nsIPresShell* aShell) { return NS_OK; }
  NS_IMETHOD EndReflow(nsIDocument *aDocument,
		                   nsIPresShell* aShell) { return NS_OK; } 
  NS_IMETHOD ContentChanged(nsIDocument* aDoc, 
                            nsIContent* aContent,
                            nsISupports* aSubContent) { return NS_OK; }
  NS_IMETHOD ContentStatesChanged(nsIDocument* aDocument,
                                  nsIContent* aContent1,
                                  nsIContent* aContent2) { return NS_OK; }
  NS_IMETHOD AttributeChanged(nsIDocument *aDocument,
                              nsIContent*  aContent,
                              PRInt32      aNameSpaceID,
                              nsIAtom*     aAttribute,
                              PRInt32      aHint);
  NS_IMETHOD ContentAppended(nsIDocument *aDocument,
			                       nsIContent* aContainer,
                             PRInt32     aNewIndexInContainer) { return NS_OK; } 
  NS_IMETHOD ContentInserted(nsIDocument *aDocument,
			                       nsIContent* aContainer,
                             nsIContent* aChild,
                             PRInt32 aIndexInContainer) { return NS_OK; } 
  NS_IMETHOD ContentReplaced(nsIDocument *aDocument,
			                       nsIContent* aContainer,
                             nsIContent* aOldChild,
                             nsIContent* aNewChild,
                             PRInt32 aIndexInContainer) { return NS_OK; }
  NS_IMETHOD ContentRemoved(nsIDocument *aDocument,
                            nsIContent* aContainer,
                            nsIContent* aChild,
                            PRInt32 aIndexInContainer) { return NS_OK; }
  NS_IMETHOD StyleSheetAdded(nsIDocument *aDocument,
                             nsIStyleSheet* aStyleSheet) { return NS_OK; }
  NS_IMETHOD StyleSheetRemoved(nsIDocument *aDocument,
                               nsIStyleSheet* aStyleSheet) { return NS_OK; }
  NS_IMETHOD StyleSheetDisabledStateChanged(nsIDocument *aDocument,
                                            nsIStyleSheet* aStyleSheet,
                                            PRBool aDisabled) { return NS_OK; }
  NS_IMETHOD StyleRuleChanged(nsIDocument *aDocument,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule,
                              PRInt32 aHint) { return NS_OK; }
  NS_IMETHOD StyleRuleAdded(nsIDocument *aDocument,
                            nsIStyleSheet* aStyleSheet,
                            nsIStyleRule* aStyleRule) { return NS_OK; }
  NS_IMETHOD StyleRuleRemoved(nsIDocument *aDocument,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule) { return NS_OK; }
  NS_IMETHOD DocumentWillBeDestroyed(nsIDocument *aDocument) { mDocument = nsnull; return NS_OK; }


  PRBool SetAttribute(nsIFrame* aFrame, nsIAtom* aAtom, nscoord aSize, PRBool aReflow=PR_TRUE);
  PRInt32 GetIntegerAttribute(nsIFrame* aFrame, nsIAtom* atom, PRInt32 defaultValue);

  nsresult CalculateChildTotalSize(nsIFrame*            aKidFrame,
                                   nsHTMLReflowMetrics& aKidReflowMetrics);

  nsresult GetFrameSize(   nsIFrame* aFrame,
                           nsSize& aSize);
                        
  nsresult SetFrameSize(   nsIPresContext* aPresContext,
                           nsIFrame* aFrame,
                           nsSize aSize);
 
  nsresult CalculateScrollAreaSize(nsIPresContext*          aPresContext,
                                       const nsHTMLReflowState& aReflowState,
                                       const nsSize&            aSbSize,
                                       nsSize&                  aScrollAreaSize);
  
  void SetScrollbarVisibility(nsIFrame* aScrollbar, PRBool aVisible);



  void ReflowScrollbars(   nsIPresContext*         aPresContext,
                                   const nsHTMLReflowState& aReflowState,
                                   nsReflowStatus&          aStatus,
                                   nsIFrame*&               aIncrementalChild);

  void ReflowScrollbar(    nsIPresContext*                  aPresContext,
                                   const nsHTMLReflowState& aReflowState,
                                   nsReflowStatus&          aStatus,
                                   PRBool&                  aScrollBarResized, 
                                   nsIFrame*                aScrollBarFrame,
                                   nsIFrame*&               aIncrementalChild);


  nsresult ReflowFrame(        nsIPresContext*              aPresContext,
                               nsHTMLReflowMetrics&     aDesiredSize,
                               const nsHTMLReflowState& aReflowState,
                               nsReflowStatus&          aStatus,
                               nsIFrame*                aFrame,
                               const nsSize&            aAvailable,
                               const nsSize&            aComputed,
                               PRBool&                  aResized,
                               nsIFrame*&               aIncrementalChild);

   void ReflowScrollArea(        nsIPresContext*          aPresContext,
                                 nsHTMLReflowMetrics&     aDesiredSize,
                                   const nsHTMLReflowState& aReflowState,
                                   nsReflowStatus&          aStatus,
                                   nsIFrame*&               aIncrementalChild);

   void LayoutChildren(nsIPresContext*          aPresContext,
                              nsHTMLReflowMetrics&     aDesiredSize,
                              const nsHTMLReflowState& aReflowState);

   void GetScrollbarDimensions(nsIPresContext* aPresContext,
                               nsSize& aSbSize);

   void AddRemoveScrollbar       (PRBool& aHasScrollbar, nscoord& aSize, nscoord aSbSize, PRBool aAdd);
   void AddHorizontalScrollbar   (const nsSize& aSbSize, nsSize& aScrollAreaSize);
   void AddVerticalScrollbar     (const nsSize& aSbSize, nsSize& aScrollAreaSize);
   void RemoveHorizontalScrollbar(const nsSize& aSbSize, nsSize& aScrollAreaSize);
   void RemoveVerticalScrollbar  (const nsSize& aSbSize, nsSize& aScrollAreaSize);

   /*
   PRBool IsDirty(nsIFrame* aFrame) {
      nsFrameState  frameState;
      aFrame->GetFrameState(&frameState);
      if (frameState & NS_FRAME_IS_DIRTY) 
          return PR_TRUE;
      else
          return PR_FALSE;
   }

   void MarkDirty(nsIFrame* aFrame) {
      nsFrameState  frameState;
      aFrame->GetFrameState(&frameState);
      frameState |= NS_FRAME_IS_DIRTY;
      aFrame->SetFrameState(frameState);
   }
   */

   nsIScrollableView* GetScrollableView(nsIPresContext* aPresContext);

   void GetScrolledContentSize(nsIPresContext* aPresContext, nsSize& aSize);

  void ScrollbarChanged(nsIPresContext* aPresContext, nscoord aX, nscoord aY);
  nsresult GetContentOf(nsIFrame* aFrame, nsIContent** aContent);

  nsIFrame* mHScrollbarFrame;
  nsIFrame* mVScrollbarFrame;
  nsIFrame* mScrollAreaFrame;
  nscoord mOnePixel;
  nsCOMPtr<nsIDocument> mDocument;
  nsGfxScrollFrame* mOuter;
  nsIScrollableView* mScrollableView;
  nsSize mMaxElementSize;

  PRBool mHasVerticalScrollbar;
  PRBool mHasHorizontalScrollbar;
  PRBool mFirstPass;
  PRBool mIsRoot;
  PRBool mNeverReflowed;
};

NS_IMPL_ISUPPORTS2(nsGfxScrollFrameInner, nsIDocumentObserver, nsIScrollPositionListener)

nsresult
NS_NewGfxScrollFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame, nsIDocument* aDocument)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsGfxScrollFrame* it = new (aPresShell) nsGfxScrollFrame(aDocument);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsGfxScrollFrame::nsGfxScrollFrame(nsIDocument* aDocument)
{
    mInner = new nsGfxScrollFrameInner(this);
    mInner->AddRef();
    mInner->mDocument = aDocument;
    mPresContext = nsnull;
    mInner->mIsRoot = PR_FALSE;
    mInner->mNeverReflowed = PR_TRUE;
}

nsGfxScrollFrame::~nsGfxScrollFrame()
{
    mInner->mOuter = nsnull;
    mInner->Release();
    mPresContext = nsnull;
}

/**
* Set the view that we are scrolling within the scrolling view. 
*/
NS_IMETHODIMP
nsGfxScrollFrame::SetScrolledFrame(nsIPresContext* aPresContext, nsIFrame *aScrolledFrame)
{
   mFrames.DestroyFrame(aPresContext, mInner->mScrollAreaFrame);
   mInner->mScrollAreaFrame = aScrolledFrame;
   mFrames.InsertFrame(nsnull, nsnull, mInner->mScrollAreaFrame);
   return NS_OK;
}

/**
* Get the view that we are scrolling within the scrolling view. 
* @result child view
*/
NS_IMETHODIMP
nsGfxScrollFrame::GetScrolledFrame(nsIPresContext* aPresContext, nsIFrame *&aScrolledFrame) const
{
   return mInner->mScrollAreaFrame->FirstChild(aPresContext, nsnull, &aScrolledFrame);
}

/**
 * Gets the size of the area that lies inside the scrollbars but clips the scrolled frame
 */
NS_IMETHODIMP
nsGfxScrollFrame::GetClipSize(nsIPresContext* aPresContext, 
                              nscoord *aWidth, 
                              nscoord *aHeight) const
{
   nsSize size;
   mInner->mScrollAreaFrame->GetSize(size);
   *aWidth = size.width;
   *aHeight = size.height;
   return NS_OK;
}

/**
* Get information about whether the vertical and horizontal scrollbars
* are currently visible
*/
NS_IMETHODIMP
nsGfxScrollFrame::GetScrollbarVisibility(nsIPresContext* aPresContext,
                                         PRBool *aVerticalVisible,
                                         PRBool *aHorizontalVisible) const
{
   *aVerticalVisible   = mInner->mHasVerticalScrollbar;
   *aHorizontalVisible = mInner->mHasHorizontalScrollbar;
   return NS_OK;
}

nsresult NS_CreateAnonymousNode(nsIContent* aParent, nsIAtom* aTag, PRInt32 aNameSpaceId, nsCOMPtr<nsIContent>& aNewNode);

NS_IMETHODIMP
nsGfxScrollFrame::CreateAnonymousContent(nsIPresContext* aPresContext,
                                         nsISupportsArray& aAnonymousChildren)
{
  // create horzontal scrollbar
  nsCAutoString progID = NS_ELEMENT_FACTORY_PROGID_PREFIX;
  progID += "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
  nsresult rv;
  NS_WITH_SERVICE(nsIElementFactory, elementFactory, progID, &rv);
  if (!elementFactory)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIContent> content;
  elementFactory->CreateInstanceByTag(nsAutoString("scrollbar"), getter_AddRefs(content));
  content->SetAttribute(kNameSpaceID_None, nsHTMLAtoms::align, nsAutoString("horizontal"), PR_FALSE);
  aAnonymousChildren.AppendElement(content);

  // create vertical scrollbar
  content = nsnull;
  elementFactory->CreateInstanceByTag(nsAutoString("scrollbar"), getter_AddRefs(content));
  content->SetAttribute(kNameSpaceID_None, nsHTMLAtoms::align, nsAutoString("vertical"), PR_FALSE);
  aAnonymousChildren.AppendElement(content);

      // XXX For GFX never have scrollbars
 // mScrollableView->SetScrollPreference(nsScrollPreference_kNeverScroll);
  
  return NS_OK;
}

NS_IMETHODIMP
nsGfxScrollFrame::Destroy(nsIPresContext* aPresContext)

{
  return nsHTMLContainerFrame::Destroy(aPresContext);
}

NS_IMETHODIMP
nsGfxScrollFrame::Init(nsIPresContext*  aPresContext,
                    nsIContent*      aContent,
                    nsIFrame*        aParent,
                    nsIStyleContext* aStyleContext,
                    nsIFrame*        aPrevInFlow)
{
  mPresContext = aPresContext;
  nsresult  rv = nsHTMLContainerFrame::Init(aPresContext, aContent,
                                            aParent, aStyleContext,
                                            aPrevInFlow);

//  nsCOMPtr<nsIContent> content;
//  mInner->GetContentOf(this, getter_AddRefs(content));

//  content->GetDocument(*getter_AddRefs(mInner->mDocument));
  mInner->mDocument->AddObserver(mInner);
  return rv;
}
  
NS_IMETHODIMP
nsGfxScrollFrame::SetInitialChildList(nsIPresContext* aPresContext,
                                   nsIAtom*        aListName,
                                   nsIFrame*       aChildList)
{
  nsresult  rv = nsHTMLContainerFrame::SetInitialChildList(aPresContext, aListName,
                                                           aChildList);
  // get scroll area
  mInner->mScrollAreaFrame = mFrames.FirstChild();

  // horizontal scrollbar
  mInner->mScrollAreaFrame->GetNextSibling(&mInner->mHScrollbarFrame);

  // vertical scrollbar
  mInner->mHScrollbarFrame->GetNextSibling(&mInner->mVScrollbarFrame);

  // listen for scroll events.
  mInner->GetScrollableView(aPresContext)->AddScrollPositionListener(mInner);

  return rv;
}

NS_IMETHODIMP
nsGfxScrollFrame::AppendFrames(nsIPresContext* aPresContext,
                            nsIPresShell&   aPresShell,
                            nsIAtom*        aListName,
                            nsIFrame*       aFrameList)
{
  // Only one child frame allowed
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGfxScrollFrame::InsertFrames(nsIPresContext* aPresContext,
                            nsIPresShell&   aPresShell,
                            nsIAtom*        aListName,
                            nsIFrame*       aPrevFrame,
                            nsIFrame*       aFrameList)
{
  // Only one child frame allowed
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGfxScrollFrame::RemoveFrame(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aOldFrame)
{
  // Scroll frame doesn't support incremental changes
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsGfxScrollFrame::DidReflow(nsIPresContext*   aPresContext,
                         nsDidReflowStatus aStatus)
{
    // Let the default nsFrame implementation clear the state flags
    // and size and position our view
  nsresult rv = nsHTMLContainerFrame::DidReflow(aPresContext, aStatus);
    
  return rv;
}


NS_IMETHODIMP
nsGfxScrollFrame::Reflow(nsIPresContext*          aPresContext,
                      nsHTMLReflowMetrics&     aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus)
{
  NS_FRAME_TRACE_MSG(NS_FRAME_TRACE_CALLS,
                     ("enter nsGfxScrollFrame::Reflow: maxSize=%d,%d",
                      aReflowState.availableWidth,
                      aReflowState.availableHeight));

   // if we have any padding then ignore it. It is inherited down to our scrolled frame
  if (aReflowState.mComputedPadding.left != 0 ||
      aReflowState.mComputedPadding.right != 0 ||
      aReflowState.mComputedPadding.top != 0 ||
      aReflowState.mComputedPadding.bottom != 0) 
  {
      nsHTMLReflowState newState(aReflowState);

      // get the padding to remove
      nscoord pwidth = (newState.mComputedPadding.left + newState.mComputedPadding.right);
      nscoord pheight = (newState.mComputedPadding.top + newState.mComputedPadding.bottom);

      // adjust our computed size to add in the padding we are removing. Make sure
      // the computed size doesn't go negative or anything.
      if (newState.mComputedWidth != NS_INTRINSICSIZE)
        newState.mComputedWidth += pwidth;

      if (newState.mComputedHeight != NS_INTRINSICSIZE)
        newState.mComputedHeight += pheight;

      // fix up the borderpadding
      ((nsMargin&)newState.mComputedBorderPadding) -= newState.mComputedPadding;

      // remove the padding
      ((nsMargin&)newState.mComputedPadding) = nsMargin(0,0,0,0);

      // reflow us again with the correct values.
      return Reflow(aPresContext, aDesiredSize, newState, aStatus);
  }

  mInner->mFirstPass = PR_TRUE;


  // Handle Incremental Reflow
  nsIFrame* incrementalChild = nsnull;

  /*
  if (mInner->mNeverReflowed) {
       // on the initial reflow see if we are the root box.
       // the root box.
       mInner->mIsRoot = PR_TRUE;
       mInner->mNeverReflowed = PR_FALSE;

       // see if we are the root box
       nsIFrame* parent = mParent;
       while (parent) {
            nsIBox* ibox = nsnull;
            if (NS_SUCCEEDED(parent->QueryInterface(nsIBox::GetIID(), (void**)&ibox)) && ibox) {
               mInner->mIsRoot = PR_FALSE;
               break;
            }

            parent->GetParent(&parent);
       }
   }
   */
  
   if ( aReflowState.reason == eReflowReason_Incremental ) {

    nsIReflowCommand::ReflowType  reflowType;
    aReflowState.reflowCommand->GetType(reflowType);

    // See if it's targeted at us
    nsIFrame* targetFrame;    
    aReflowState.reflowCommand->GetTarget(targetFrame);

    if (this == targetFrame) {
      // if we are the target see what the type was
      // and generate a normal non incremental reflow.
      switch (reflowType) {

          case nsIReflowCommand::StyleChanged: 
          {
            nsHTMLReflowState newState(aReflowState);
            newState.reason = eReflowReason_StyleChange;
            newState.reflowCommand = nsnull;
            return Reflow(aPresContext, aDesiredSize, newState, aStatus);
          }
          break;

          // if its a dirty type then reflow us with a dirty reflow
          case nsIReflowCommand::ReflowDirty: 
          {
            nsHTMLReflowState newState(aReflowState);
            newState.reason = eReflowReason_Dirty;
            newState.reflowCommand = nsnull;
            return Reflow(aPresContext, aDesiredSize, newState, aStatus);
          }
          break;

          default:
            NS_ASSERTION(PR_FALSE, "unexpected reflow command type");
      } 
    }

    /*
    if (mInner->mIsRoot) 
      nsIBox::HandleRootBoxReflow(aPresContext, this, aReflowState);

    // then get the child we need to flow incrementally
    aReflowState.reflowCommand->GetNext(incrementalChild);
    */
  }



 
  // next reflow the scrollbars so we will at least know their size. If they are bigger or smaller than
  // we thought we will have to reflow the scroll area as well.
  mInner->ReflowScrollbars(   aPresContext,
                      aReflowState,
                      aStatus,
                      incrementalChild);
  
  // then reflow the scroll area. If it gets bigger it could signal the scrollbars need
  // to be reflowed.
  mInner->ReflowScrollArea(   aPresContext,
                      aDesiredSize,
                      aReflowState,
                      aStatus,
                      incrementalChild);

  mInner->mFirstPass = PR_FALSE;

  // reflow the scrollbars again but only the ones that need it.
  mInner->ReflowScrollbars(   aPresContext,
                      aReflowState,
                      aStatus,
                      incrementalChild);

  // layout all the children.
  mInner->LayoutChildren( aPresContext,
                  aDesiredSize,
                  aReflowState);


  /*
  if (aReflowState.mComputedWidth != NS_INTRINSICSIZE)
     ((nscoord&)aReflowState.mComputedWidth) -= (padding.left + padding.right);

  if (aReflowState.mComputedHeight != NS_INTRINSICSIZE)
     ((nscoord&)aReflowState.mComputedHeight) -= (padding.top + padding.bottom);

  ((nsMargin&)aReflowState.mComputedPadding) = padding;
  ((nsMargin&)aReflowState.mComputedBorderPadding) += padding;
*/

  NS_FRAME_TRACE_MSG(NS_FRAME_TRACE_CALLS,
    ("exit nsGfxScrollFrame::Reflow: status=%d width=%d height=%d",
     aStatus, aDesiredSize.width, aDesiredSize.height));
  return NS_OK;
}


/*
NS_IMETHODIMP
nsGfxScrollFrame::Paint(nsIPresContext*      aPresContext,
                     nsIRenderingContext& aRenderingContext,
                     const nsRect&        aDirtyRect,
                     nsFramePaintLayer    aWhichLayer)
{
   
  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
    // Only paint the border and background if we're visible
    const nsStyleDisplay* display = (const nsStyleDisplay*)
      mStyleContext->GetStyleData(eStyleStruct_Display);

    if (display->mVisible) {
      // Paint our border only (no background)
      const nsStyleSpacing* spacing = (const nsStyleSpacing*)
        mStyleContext->GetStyleData(eStyleStruct_Spacing);

      nsRect  rect(0, 0, mRect.width, mRect.height);
      nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                  aDirtyRect, rect, *spacing, mStyleContext, 0);

   
      // paint the little square between the scrollbars
      nsRect vbar;
      nsRect hbar;

      // get the child's rect
      mVScrollbarFrame->GetRect(vbar);
      mVScrollbarFrame->GetRect(hbar);

      // get the margin
      const nsStyleSpacing* s;
      nsresult rv = mVScrollbarFrame->GetStyleData(eStyleStruct_Spacing,
                     (const nsStyleStruct*&) s);

      nsMargin margin;
      if (!s->GetMargin(margin)) 
        margin.SizeTo(0,0,0,0);

      vbar.Inflate(margin);

      
      // get the margin
      rv = mHScrollbarFrame->GetStyleData(eStyleStruct_Spacing,
                     (const nsStyleStruct*&) s);

      if (!s->GetMargin(margin)) 
        margin.SizeTo(0,0,0,0);

      hbar.Inflate(margin);

      rect.SetRect(hbar.x + hbar.width, vbar.y + vbar.height, vbar.width, hbar.height);
      
      nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                      aDirtyRect, rect, *myColor, *mySpacing, 0, 0);

    }
  } else {

      // Paint our children
      nsresult rv = nsHTMLContainerFrame::Paint(aPresContext, aRenderingContext, aDirtyRect,
                                     aWhichLayer);
            return rv;
  } 
}
*/

NS_IMETHODIMP
nsGfxScrollFrame::Paint(nsIPresContext*      aPresContext,
                     nsIRenderingContext& aRenderingContext,
                     const nsRect&        aDirtyRect,
                     nsFramePaintLayer    aWhichLayer)
{
    /*
  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
    // Only paint the border and background if we're visible
    const nsStyleDisplay* display = (const nsStyleDisplay*)
      mStyleContext->GetStyleData(eStyleStruct_Display);

    if (display->mVisible) {
      // Paint our border only (no background)
      const nsStyleSpacing* spacing = (const nsStyleSpacing*)
        mStyleContext->GetStyleData(eStyleStruct_Spacing);

      nsRect  rect(0, 0, mRect.width, mRect.height);
      nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                  aDirtyRect, rect, *spacing, mStyleContext, 0);
      nsCSSRendering::PaintOutline(aPresContext, aRenderingContext, this,
                                  aDirtyRect, rect, *spacing, mStyleContext, 0);
    }
  }
  */

  // Paint our children
  return nsHTMLContainerFrame::Paint(aPresContext, aRenderingContext, aDirtyRect,
                                 aWhichLayer);
}

NS_IMETHODIMP
nsGfxScrollFrame::GetContentAndOffsetsFromPoint(nsIPresContext* aCX,
                                                const nsPoint&  aPoint,
                                                nsIContent **   aNewContent,
                                                PRInt32&        aContentOffset,
                                                PRInt32&        aContentOffsetEnd,
                                                PRBool&         aBeginFrameContent)
{
  if (! mInner)
    return NS_ERROR_NULL_POINTER;

  return mInner->mScrollAreaFrame->GetContentAndOffsetsFromPoint(aCX, aPoint, aNewContent, aContentOffset, aContentOffsetEnd, aBeginFrameContent);
}

PRIntn
nsGfxScrollFrame::GetSkipSides() const
{
  return 0;
}

NS_IMETHODIMP
nsGfxScrollFrame::GetFrameType(nsIAtom** aType) const
{
  NS_PRECONDITION(nsnull != aType, "null OUT parameter pointer");
  *aType = nsLayoutAtoms::scrollFrame; 
  NS_ADDREF(*aType);
  return NS_OK;
}

#ifdef NS_DEBUG
NS_IMETHODIMP
nsGfxScrollFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("GfxScroll", aResult);
}
#endif

NS_IMETHODIMP 
nsGfxScrollFrame::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{           
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
                                                                         
  *aInstancePtr = NULL;                                                  
                                                                                        
  if (aIID.Equals(kIAnonymousContentCreatorIID)) {                                         
    *aInstancePtr = (void*)(nsIAnonymousContentCreator*) this;                                        
    return NS_OK;                                                        
  } else if (aIID.Equals(NS_GET_IID(nsIBox))) {                                         
    *aInstancePtr = (void*)(nsIBox*) this;                                        
    return NS_OK;                                                        
  } else if (aIID.Equals(kIScrollableFrameIID)) {                                         
    *aInstancePtr = (void*)(nsIScrollableFrame*) this;                                        
    return NS_OK;                                                        
  }   


  return nsHTMLContainerFrame::QueryInterface(aIID, aInstancePtr);                                  
}


//-------------------- Inner ----------------------

nsGfxScrollFrameInner::nsGfxScrollFrameInner(nsGfxScrollFrame* aOuter):mHScrollbarFrame(nsnull),
                                               mVScrollbarFrame(nsnull),
                                               mScrollAreaFrame(nsnull),
                                               mHasVerticalScrollbar(PR_FALSE), 
                                               mHasHorizontalScrollbar(PR_FALSE),
                                               mOnePixel(20)
{
   NS_INIT_REFCNT();
   mOuter = aOuter;
   mMaxElementSize.width = 0;
   mMaxElementSize.height = 0;
   mFirstPass = PR_FALSE;
}

NS_IMETHODIMP
nsGfxScrollFrameInner::ScrollPositionWillChange(nsIScrollableView* aScrollable, nscoord aX, nscoord aY)
{
   // Do nothing.
   return NS_OK;
}

/**
 * Called if something externally moves the scroll area
 * This can happen if the user pages up down or uses arrow keys
 * So what we need to do up adjust the scrollbars to match.
 */
NS_IMETHODIMP
nsGfxScrollFrameInner::ScrollPositionDidChange(nsIScrollableView* aScrollable, nscoord aX, nscoord aY)
{
   SetAttribute(mVScrollbarFrame, nsXULAtoms::curpos, aY);
   SetAttribute(mHScrollbarFrame, nsXULAtoms::curpos, aX);
   return NS_OK;
}

NS_IMETHODIMP
nsGfxScrollFrameInner::AttributeChanged(nsIDocument *aDocument,
                              nsIContent*     aContent,
                              PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aHint) 
{
   if (mHScrollbarFrame && mVScrollbarFrame)
   {
     nsCOMPtr<nsIContent> vcontent;
     nsCOMPtr<nsIContent> hcontent;

     mHScrollbarFrame->GetContent(getter_AddRefs(hcontent));
     mVScrollbarFrame->GetContent(getter_AddRefs(vcontent));

     if (hcontent.get() == aContent  || vcontent.get() == aContent)
     {
        nscoord x = 0;
        nscoord y = 0;

        nsAutoString value;
        if (NS_CONTENT_ATTR_HAS_VALUE == hcontent->GetAttribute(kNameSpaceID_None, nsXULAtoms::curpos, value))
        {
           PRInt32 error;

           // convert it to an integer
           x = value.ToInteger(&error);
        }

        if (NS_CONTENT_ATTR_HAS_VALUE == vcontent->GetAttribute(kNameSpaceID_None, nsXULAtoms::curpos, value))
        {
           PRInt32 error;

           // convert it to an integer
           y = value.ToInteger(&error);
        }

        ScrollbarChanged(mOuter->mPresContext, x*mOnePixel, y*mOnePixel);
     }
   }

   return NS_OK;
}


nsIScrollableView*
nsGfxScrollFrameInner::GetScrollableView(nsIPresContext* aPresContext)
{
  nsIScrollableView* scrollingView;
  nsIView*           view;
  mScrollAreaFrame->GetView(aPresContext, &view);
  nsresult result = view->QueryInterface(kScrollViewIID, (void**)&scrollingView);
  NS_ASSERTION(NS_SUCCEEDED(result), "assertion gfx scrollframe does not contain a scrollframe");          
  return scrollingView;
}

void
nsGfxScrollFrameInner::GetScrolledContentSize(nsIPresContext* aPresContext, nsSize& aSize)
{
    // get the ara frame is the scrollarea
    nsIFrame* child = nsnull;
    mScrollAreaFrame->FirstChild(aPresContext, nsnull, &child);

    nsRect rect(0,0,0,0);
    child->GetRect(rect);
    aSize.width = rect.width;
    aSize.height = rect.height;
}


nsresult
nsGfxScrollFrameInner::SetFrameSize(nsIPresContext* aPresContext,
                                    nsIFrame*       aFrame,
                                    nsSize          aSize)
                               
{  
    // get the margin
    const nsStyleSpacing* spacing;
    nsresult rv = aFrame->GetStyleData(eStyleStruct_Spacing,
                   (const nsStyleStruct*&) spacing);

    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to get spacing");
    if (NS_FAILED(rv))
        return rv;

    nsMargin margin;
    if (!spacing->GetMargin(margin)) 
      margin.SizeTo(0,0,0,0);

    // add in the margin
    aSize.width -= margin.left + margin.right;
    aSize.height -= margin.top + margin.bottom;

    aFrame->SizeTo(aPresContext, aSize.width, aSize.height);

    return NS_OK;
}


nsresult
nsGfxScrollFrameInner::GetFrameSize(   nsIFrame* aFrame,
                               nsSize& aSize)
                               
{  
    // get the child's rect
    aFrame->GetSize(aSize);

    // get the margin
    const nsStyleSpacing* spacing;
    nsresult rv = aFrame->GetStyleData(eStyleStruct_Spacing,
                   (const nsStyleStruct*&) spacing);

    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to get spacing");
    if (NS_FAILED(rv))
        return rv;

    nsMargin margin;
    if (!spacing->GetMargin(margin)) 
      margin.SizeTo(0,0,0,0);

    // add in the margin
    aSize.width += margin.left + margin.right;
    aSize.height += margin.top + margin.bottom;

    return NS_OK;
}


// Returns the width of the vertical scrollbar and the height of
// the horizontal scrollbar in twips
void
nsGfxScrollFrameInner::GetScrollbarDimensions(nsIPresContext* aPresContext,
                                      nsSize&        aSbSize)
{
                       
  // if we are using 
    nsRect rect(0,0,0,0);
    mHScrollbarFrame->GetRect(rect);
    aSbSize.height = rect.height;

    rect.SetRect(0,0,0,0);
    mVScrollbarFrame->GetRect(rect);
    aSbSize.width = rect.width;
    return;
}


// Calculates the size of the scroll area. This is the area inside of the
// border edge and inside of any vertical and horizontal scrollbar
// Also returns whether space was reserved for the vertical scrollbar.
nsresult
nsGfxScrollFrameInner::CalculateScrollAreaSize(nsIPresContext*          aPresContext,
                                       const nsHTMLReflowState& aReflowState,
                                       const nsSize&            aSbSize,
                                       nsSize&                  aScrollAreaSize)
{
  aScrollAreaSize.width = aReflowState.mComputedWidth;
  aScrollAreaSize.height = aReflowState.mComputedHeight;

  if (aScrollAreaSize.width != NS_INTRINSICSIZE) {
      aScrollAreaSize.width -= aReflowState.mComputedPadding.left + aReflowState.mComputedPadding.right;
      if (aScrollAreaSize.width < 0)
         aScrollAreaSize.width = 0;
  }

  if (aScrollAreaSize.height != NS_INTRINSICSIZE) {
      aScrollAreaSize.height -= aReflowState.mComputedPadding.top + aReflowState.mComputedPadding.bottom;
      if (aScrollAreaSize.height < 0)
         aScrollAreaSize.height = 0;

  }
  
  // get the current state
  PRBool vert = mHasVerticalScrollbar;
  PRBool horiz = mHasHorizontalScrollbar;

  // reset everyting
  mHasVerticalScrollbar = PR_FALSE;
  mHasHorizontalScrollbar = PR_FALSE;

  if (aReflowState.mStyleDisplay->mOverflow == NS_STYLE_OVERFLOW_SCROLL) 
  {
    vert = PR_TRUE;
    horiz = PR_TRUE;
  } 
  else if (aReflowState.mStyleDisplay->mOverflow == NS_STYLE_OVERFLOW_SCROLLBARS_NONE) 
  {
    vert = PR_FALSE;
    horiz = PR_FALSE;
  } 
  else if (aReflowState.mStyleDisplay->mOverflow == NS_STYLE_OVERFLOW_SCROLLBARS_VERTICAL) 
  {
    vert = PR_TRUE;
    horiz = PR_FALSE;
  }
  else if (aReflowState.mStyleDisplay->mOverflow == NS_STYLE_OVERFLOW_SCROLLBARS_HORIZONTAL)
  {
    vert = PR_FALSE;
    horiz = PR_TRUE;
  }

  // add in the scrollbars.
  if (horiz)
    AddHorizontalScrollbar(aSbSize, aScrollAreaSize);
  
  if (vert)
    AddVerticalScrollbar(aSbSize, aScrollAreaSize);



  return NS_OK;
}

// Calculate the total amount of space needed for the child frame,
// including its child frames that stick outside its bounds and any
// absolutely positioned child frames.
// Updates the width/height members of the reflow metrics
nsresult
nsGfxScrollFrameInner::CalculateChildTotalSize(nsIFrame*            aKidFrame,
                                       nsHTMLReflowMetrics& aKidReflowMetrics)
{
  // If the frame has child frames that stick outside its bounds, then take
  // them into account, too
  nsFrameState  kidState;
  aKidFrame->GetFrameState(&kidState);
  if (NS_FRAME_OUTSIDE_CHILDREN & kidState) {
    aKidReflowMetrics.width = aKidReflowMetrics.mOverflowArea.width;
    aKidReflowMetrics.height = aKidReflowMetrics.mOverflowArea.height;
  }

  return NS_OK;
}

void
nsGfxScrollFrameInner::SetScrollbarVisibility(nsIFrame* aScrollbar, PRBool aVisible)
{
        nsAutoString oldStyle = "";
        nsCOMPtr<nsIContent> child;
        aScrollbar->GetContent(getter_AddRefs(child));
        child->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::value, oldStyle);

        nsAutoString newStyle;
        if (aVisible)
          newStyle = "";
        else
          newStyle = "hidden";

        if (oldStyle != newStyle)
            child->SetAttribute(kNameSpaceID_None, nsHTMLAtoms::value, newStyle, PR_TRUE);
}


/**
 * Reflows the visible scrollbars to fit in our content area. If the scrollbars change size
 * Signal that the content area needs to be reflowed to accomidate the new size. This can
 * happend if a css rule like :hover causes the scrollbar to get bigger. 
 */
void
nsGfxScrollFrameInner::ReflowScrollbars(   nsIPresContext*         aPresContext,
                                   const nsHTMLReflowState& aReflowState,
                                   nsReflowStatus&          aStatus,
                                   nsIFrame*&               aIncrementalChild)
{
  // if we have gfx scrollbars reflow them. If they changed size then signal that 
  // we need to reflow the content area. This could happen if there is a style rule set
  // up on them that changed there size. Like an image on the thumb gets big.
    
      PRBool resized = PR_TRUE;
      if (mHasHorizontalScrollbar || aReflowState.reason == eReflowReason_Initial) {
          ReflowScrollbar(aPresContext, 
                          aReflowState, 
                          aStatus, 
                          resized,
                          mHScrollbarFrame, 
                          aIncrementalChild);
      }    

      if (mHasVerticalScrollbar || aReflowState.reason == eReflowReason_Initial) {
          ReflowScrollbar(aPresContext, 
                          aReflowState, 
                          aStatus, 
                          resized,
                          mVScrollbarFrame, 
                          aIncrementalChild);
      }

    SetScrollbarVisibility(mHScrollbarFrame, mHasHorizontalScrollbar);
    SetScrollbarVisibility(mVScrollbarFrame, mHasVerticalScrollbar);
}

void
nsGfxScrollFrameInner::ReflowScrollbar(    nsIPresContext*          aPresContext,
                                   const nsHTMLReflowState& aReflowState,
                                   nsReflowStatus&          aStatus,
                                   PRBool&                  aScrollbarResized, 
                                   nsIFrame*                aScrollbarFrame,
                                   nsIFrame*&               aIncrementalChild)
{
  nsSize scrollArea;
  GetFrameSize(mScrollAreaFrame, scrollArea);

  // add the padding to it.
  scrollArea.width += aReflowState.mComputedPadding.left + aReflowState.mComputedPadding.right;
  scrollArea.height += aReflowState.mComputedPadding.top + aReflowState.mComputedPadding.bottom;
  
  // is it the horizontal scrollbar?
  if (aScrollbarFrame == mHScrollbarFrame) {

      // get the width to layout in
      nscoord width = scrollArea.width;

      nsHTMLReflowMetrics desiredSize(nsnull);

      // reflow the frame
      ReflowFrame(aPresContext, desiredSize, aReflowState, aStatus, mHScrollbarFrame, 
                      nsSize(NS_INTRINSICSIZE, NS_INTRINSICSIZE),
                      nsSize(width, NS_INTRINSICSIZE),
                      aScrollbarResized,
                      aIncrementalChild);

  } else if (aScrollbarFrame == mVScrollbarFrame) {
      // is it the vertical scrollbar?

      // get the height to layout in
      nscoord height = scrollArea.height;

      nsHTMLReflowMetrics desiredSize(nsnull);

      // reflow the frame
      ReflowFrame(aPresContext, desiredSize, aReflowState, aStatus, mVScrollbarFrame, 
                      nsSize(NS_INTRINSICSIZE, NS_INTRINSICSIZE),
                      nsSize(NS_INTRINSICSIZE, height),
                      aScrollbarResized,
                      aIncrementalChild);

  }
}

void
nsGfxScrollFrameInner::AddRemoveScrollbar(PRBool& aHasScrollbar, nscoord& aSize, nscoord aSbSize, PRBool aAdd)
{
   if ((aAdd && aHasScrollbar) || (!aAdd && !aHasScrollbar))
      return;
 
   nscoord size = aSize;

   if (size != NS_INTRINSICSIZE) {
     if (aAdd)
        size -= aSbSize;
     else
        size += aSbSize;
   }

   // not enough room? If not don't do anything.
   if (size >= aSbSize) {
       aHasScrollbar = aAdd;
       aSize = size;
   }
}

void
nsGfxScrollFrameInner::AddHorizontalScrollbar(const nsSize& aSbSize, nsSize& aScrollAreaSize)
{
   AddRemoveScrollbar(mHasHorizontalScrollbar, aScrollAreaSize.height, aSbSize.height, PR_TRUE);
}

void
nsGfxScrollFrameInner::AddVerticalScrollbar(const nsSize& aSbSize, nsSize& aScrollAreaSize)
{
   AddRemoveScrollbar(mHasVerticalScrollbar, aScrollAreaSize.width, aSbSize.width, PR_TRUE);
}

void
nsGfxScrollFrameInner::RemoveHorizontalScrollbar(const nsSize& aSbSize, nsSize& aScrollAreaSize)
{
   AddRemoveScrollbar(mHasHorizontalScrollbar, aScrollAreaSize.height, aSbSize.height, PR_FALSE);
}

void
nsGfxScrollFrameInner::RemoveVerticalScrollbar(const nsSize& aSbSize, nsSize& aScrollAreaSize)
{
   AddRemoveScrollbar(mHasVerticalScrollbar, aScrollAreaSize.width, aSbSize.width, PR_FALSE);
}

/**
 * Give a computed and available sizes flows the child at that size. 
 * This is special because it takes into account margin and borderpadding.
 * It also tells us if the child changed size or not.
 * And finally it will handle the incremental reflow for us. If it reflowed the 
 * incrementalChild it will set it to nsnull
 */
nsresult
nsGfxScrollFrameInner::ReflowFrame(    nsIPresContext*          aPresContext,
                               nsHTMLReflowMetrics&     aDesiredSize,
                               const nsHTMLReflowState& aReflowState,
                               nsReflowStatus&          aStatus,
                               nsIFrame*                aFrame,
                               const nsSize&            aAvailable,
                               const nsSize&            aComputed,
                               PRBool&                  aResized,
                               nsIFrame*&               aIncrementalChild)
{
      PRBool needsReflow = PR_FALSE;
      nsReflowReason reason = aReflowState.reason;

      nsFrameState childState;
      aFrame->GetFrameState(&childState);

      if (childState & NS_FRAME_FIRST_REFLOW) {
          if (reason != eReflowReason_Initial)
          {
              // if incremental then make sure we send a initial reflow first.
              if (reason == eReflowReason_Incremental) {
                  nsHTMLReflowState state(aReflowState);
                  state.reason = eReflowReason_Initial;
                  state.reflowCommand = nsnull;
                  ReflowFrame( aPresContext, aDesiredSize, state, aStatus, aFrame, aAvailable, aComputed, aResized, aIncrementalChild);
              } else {
                  // convert to initial if not incremental.
                  reason = eReflowReason_Initial;
              }

          }
      } else if (reason == eReflowReason_Initial) {
          reason = eReflowReason_Resize;
      }


      switch(reason)
      {
      case eReflowReason_Incremental: {
            // if incremental see if the next child in the chain is the child. If so then
            // we will just let it go down. If not then convert it to a dirty. It will get picked 
            // up later.
            nsIFrame* incrementalChild = nsnull;
            aReflowState.reflowCommand->GetNext(incrementalChild, PR_FALSE);
            if (incrementalChild == aFrame) {
                needsReflow = PR_TRUE;
                aReflowState.reflowCommand->GetNext(incrementalChild);
            } else {
                nsHTMLReflowState state(aReflowState);
                state.reason = eReflowReason_Dirty;
                return ReflowFrame( aPresContext, aDesiredSize, state, aStatus, aFrame, aAvailable, aComputed, aResized, aIncrementalChild);
            }

             /*
             if (aIncrementalChild == aFrame) {
               needsReflow = PR_TRUE;
               aIncrementalChild = nsnull;
             } else {
               reason = eReflowReason_Resize;
               needsReflow = PR_FALSE;
             }
             */
         } break;
         
         // if its dirty then see if the child we want to reflow is dirty. If it is then
         // mark it as needing to be reflowed.
         case eReflowReason_Dirty: {
             
             // so look at the next child. If it is use convert back to incremental.
             if (aReflowState.reflowCommand) {
                nsIFrame* incrementalChild = nsnull;
                aReflowState.reflowCommand->GetNext(incrementalChild, PR_FALSE);
                if (incrementalChild == aFrame) {
                     nsHTMLReflowState state(aReflowState);
                     state.reason = eReflowReason_Incremental;
                     return ReflowFrame( aPresContext, aDesiredSize, state, aStatus, aFrame, aAvailable, aComputed, aResized, aIncrementalChild);
                } 
              }
 
              // get the frame state to see if it needs reflow
              needsReflow = (childState & NS_FRAME_IS_DIRTY) || (childState & NS_FRAME_HAS_DIRTY_CHILDREN);
             
         } break;

         // if the a resize reflow then it doesn't need to be reflowed. Only if the size is different
         // from the new size would we actually do a reflow
         case eReflowReason_Resize:
             needsReflow = PR_FALSE;
         break;

         case eReflowReason_Initial:
             needsReflow = PR_TRUE;
         break;



         default:
             needsReflow = PR_TRUE;
             
      }

    nsSize currentSize;
    GetFrameSize(aFrame, currentSize);

    // if we do not need reflow then
    // see if the size it what we already want. If it is then we are done.
    if (!needsReflow) {
        // get the current size

        // if the size is not changing and the frame is not dirty then there
        // is nothing to do.
        if (currentSize.width == aComputed.width && currentSize.height == aComputed.height) {
            aDesiredSize.width = currentSize.width;
            aDesiredSize.height = currentSize.height;
            return NS_OK;
        }
    }

    aDesiredSize.width = 0;
    aDesiredSize.height = 0;

    nsHTMLReflowState childReflowState(aPresContext, aReflowState,
                                           aFrame, aAvailable);

    childReflowState.reason = reason;
    childReflowState.mComputedWidth =  aComputed.width;
    childReflowState.mComputedHeight = aComputed.height;

    // subtract out the childs margin and border if computed
    const nsStyleSpacing* spacing;
    nsresult rv = aFrame->GetStyleData(eStyleStruct_Spacing,
                   (const nsStyleStruct*&) spacing);

    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to get spacing");
    if (NS_FAILED(rv))
        return rv;

    // get the margin
    nsMargin margin(0,0,0,0);
    spacing->GetMargin(margin);
    
    // subtract out the border and padding
    if (childReflowState.mComputedWidth != NS_INTRINSICSIZE) {
       childReflowState.mComputedWidth -= childReflowState.mComputedBorderPadding.left + childReflowState.mComputedBorderPadding.right;
       childReflowState.mComputedWidth -= margin.left + margin.right;
       if (childReflowState.mComputedWidth < 0)
           childReflowState.mComputedWidth = 0;
    }

    if (childReflowState.mComputedHeight != NS_INTRINSICSIZE) {
       childReflowState.mComputedHeight -= childReflowState.mComputedBorderPadding.top + childReflowState.mComputedBorderPadding.bottom;
       childReflowState.mComputedWidth -= margin.top + margin.bottom;
       if (childReflowState.mComputedHeight < 0)
           childReflowState.mComputedHeight = 0;
    }
     
    mOuter->ReflowChild(aFrame, aPresContext, aDesiredSize, childReflowState,
                        0, 0, NS_FRAME_NO_MOVE_FRAME, aStatus);
    NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");

    NS_ASSERTION(aDesiredSize.width >= 0 && aDesiredSize.height >= 0,"Error child's size is less than 0!");

    // if the frame size change then mark the flag
    if (currentSize.width != aDesiredSize.width || currentSize.height != aDesiredSize.height) {
       aFrame->SizeTo(aPresContext, aDesiredSize.width, aDesiredSize.height);
       aResized = PR_TRUE;
    }
    aFrame->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);

    // add the margin back in
    aDesiredSize.width += margin.left + margin.right;
    aDesiredSize.height += margin.top + margin.bottom;

    return NS_OK;
}
/**
 * Reflow the scroll area if it needs it and return its size. Also determine if the reflow will
 * cause any of the scrollbars to need to be reflowed.
 */
void
nsGfxScrollFrameInner::ReflowScrollArea(   nsIPresContext*  aPresContext,
                                   nsHTMLReflowMetrics&     aDesiredSize,
                                   const nsHTMLReflowState& aReflowState,
                                   nsReflowStatus&          aStatus,
                                   nsIFrame*&               aIncrementalChild)
{
      nsSize scrollAreaSize;

      // get the current scrollarea size
      nsSize oldScrollAreaSize;

      GetFrameSize(mScrollAreaFrame, oldScrollAreaSize);

      /*
      // add our padding
      oldScrollAreaSize.width += aReflowState.mComputedPadding.left + aReflowState.mComputedPadding.right;
      oldScrollAreaSize.height += aReflowState.mComputedPadding.top + aReflowState.mComputedPadding.bottom;
      */

      // get the width of the vertical scrollbar and the height of the horizontal scrollbar
      nsSize sbSize;
      GetScrollbarDimensions(aPresContext, sbSize);

      // Compute the scroll area size (area inside of the border edge and inside
      // of any vertical and horizontal scrollbars)
      nsHTMLReflowMetrics scrollAreaDesiredSize(aDesiredSize.maxElementSize);

      CalculateScrollAreaSize(aPresContext, aReflowState, sbSize,
                              scrollAreaSize);

      // if the frame is not changing size and its not dirty then do nothing
      //if (oldScrollAreaSize == scrollAreaSize && !IsDirty(mScrollAreaFrame))
      //    return;

      // -------- flow the scroll area -----------
      nsSize      scrollAreaReflowSize(scrollAreaSize.width, scrollAreaSize.height);

      PRBool resized = PR_FALSE;
      ReflowFrame(aPresContext, scrollAreaDesiredSize, aReflowState, aStatus, mScrollAreaFrame, 
                  scrollAreaReflowSize,
                  scrollAreaReflowSize,
                  resized,
                  aIncrementalChild);

        // ------- set up max size -------
      if (nsnull != aDesiredSize.maxElementSize) {
        *aDesiredSize.maxElementSize = *scrollAreaDesiredSize.maxElementSize;
        mMaxElementSize = *aDesiredSize.maxElementSize;
      }

      // make sure we take into account any children outside our bounds
      CalculateChildTotalSize(mScrollAreaFrame, scrollAreaDesiredSize);


      // its possible that we will have to do this twice. Its a special case
      // when a the scrollarea is exactly the same height as its content but
      // the contents width is greater. We will need a horizontal scrollbar
      // but the size of the scrollbar will cause the scrollarea to be shorter
      // than the content. So we will need a vertical scrollbar. But that could
      // require us to squeeze the content. So we would have to reflow. Basically
      // we have to start over. Thats why this loop is here.
      for (int i=0; i < 1; i++) {

        // if we are shrink wrapping the height. 
        if (NS_INTRINSICSIZE == scrollAreaSize.height) {
              // the view port become the size that child requested.
              scrollAreaSize.height = scrollAreaDesiredSize.height;

              // if we were auto and have a vertical scrollbar remove it because we
              // can have whatever size we wanted.
              if (aReflowState.mStyleDisplay->mOverflow != NS_STYLE_OVERFLOW_SCROLL
                  && aReflowState.mStyleDisplay->mOverflow != NS_STYLE_OVERFLOW_SCROLLBARS_VERTICAL) {
                RemoveVerticalScrollbar(sbSize, scrollAreaSize);
              }
        } else {
          // if we are not shrink wrapping
          PRBool mustReflow = PR_FALSE;

          // if we have 'auto' scrollbars
          if (aReflowState.mStyleDisplay->mOverflow != NS_STYLE_OVERFLOW_SCROLL
              && aReflowState.mStyleDisplay->mOverflow != NS_STYLE_OVERFLOW_SCROLLBARS_VERTICAL) {
            // get the ara frame is the scrollarea
            nsSize size;
            GetScrolledContentSize(aPresContext, size);

            // There are two cases to consider
              if (size.height <= scrollAreaSize.height
                  || aReflowState.mStyleDisplay->mOverflow == NS_STYLE_OVERFLOW_SCROLLBARS_HORIZONTAL
                  || aReflowState.mStyleDisplay->mOverflow == NS_STYLE_OVERFLOW_SCROLLBARS_NONE) {
                if (mHasVerticalScrollbar) {
                  // We left room for the vertical scrollbar, but it's not needed;
                  // remove it.
                  RemoveVerticalScrollbar(sbSize, scrollAreaSize);
                  mustReflow = PR_TRUE;
                }
              } else {
                if (!mHasVerticalScrollbar) {
                  // We didn't leave room for the vertical scrollbar, but it turns
                  // out we needed it
                  AddVerticalScrollbar(sbSize, scrollAreaSize);
                  mustReflow = PR_TRUE;
                }
            }

            // Go ahead and reflow the child a second time if we added or removed the scrollbar
            if (mustReflow) {

              // make sure we mark it as dirty so it will reflow again.
              nsFrameState childState;
              mScrollAreaFrame->GetFrameState(&childState);
              childState |= NS_FRAME_IS_DIRTY;
              mScrollAreaFrame->SetFrameState(childState);

              scrollAreaReflowSize.SizeTo(scrollAreaSize.width, scrollAreaSize.height);

              resized = PR_FALSE;

              ReflowFrame(aPresContext, scrollAreaDesiredSize, aReflowState, aStatus, mScrollAreaFrame, 
                              scrollAreaReflowSize,
                              scrollAreaReflowSize,
                              resized,
                              aIncrementalChild);
          
              CalculateChildTotalSize(mScrollAreaFrame, scrollAreaDesiredSize);

            }
          }
        }
      

        // if we are shrink wrapping the scroll area height becomes whatever the child wanted
          if (NS_INTRINSICSIZE == scrollAreaSize.width) {
             scrollAreaSize.width = scrollAreaDesiredSize.width;

             // if we have auto scrollbars the remove the horizontal scrollbar
             if (aReflowState.mStyleDisplay->mOverflow != NS_STYLE_OVERFLOW_SCROLL
                 && aReflowState.mStyleDisplay->mOverflow != NS_STYLE_OVERFLOW_SCROLLBARS_HORIZONTAL) {
                 RemoveHorizontalScrollbar(sbSize, scrollAreaSize);
             }
          } else {
            // if scrollbars are auto
            if ((NS_STYLE_OVERFLOW_SCROLL != aReflowState.mStyleDisplay->mOverflow)
                && (NS_STYLE_OVERFLOW_SCROLLBARS_HORIZONTAL != aReflowState.mStyleDisplay->mOverflow))
            {
              // get the ara frame is the scrollarea
              nsSize size;
              GetScrolledContentSize(aPresContext, size);

              // if the child is wider that the scroll area
              // and we don't have a scrollbar add one.
              if (size.width > scrollAreaSize.width
                  && aReflowState.mStyleDisplay->mOverflow != NS_STYLE_OVERFLOW_SCROLLBARS_VERTICAL
                  && aReflowState.mStyleDisplay->mOverflow != NS_STYLE_OVERFLOW_SCROLLBARS_NONE) { 

                if (!mHasHorizontalScrollbar) {
                     AddHorizontalScrollbar(sbSize, scrollAreaSize);

                     // if we added a horizonal scrollbar and we did not have a vertical
                     // there is a chance that by adding the horizonal scrollbar we will
                     // suddenly need a vertical scrollbar. Is a special case but its 
                     // important.
                     if (!mHasVerticalScrollbar && size.height > scrollAreaSize.height - sbSize.height)
                        continue;
                }
              } else {
                  // if the area is smaller or equal to and we have a scrollbar then
                  // remove it.
                  RemoveHorizontalScrollbar(sbSize, scrollAreaSize);
              }
            }
        }

        // always break out of the loop. Only continue if another scrollbar was added.
        break;
      }

      /*
      // the old scroll area includes our margin. If our margin changed it will cause the
      // scrollbars to be recalculated as well.
      nsSize marginedScrollAreaSize = scrollAreaSize;
      marginedScrollAreaSize.width += aReflowState.mComputedPadding.left + aReflowState.mComputedPadding.right;
      marginedScrollAreaSize.height += aReflowState.mComputedPadding.top + aReflowState.mComputedPadding.bottom;
      */

      nsSize size;
      GetScrolledContentSize(aPresContext, size);

      float p2t;
      aPresContext->GetScaledPixelsToTwips(&p2t);
      mOnePixel = NSIntPixelsToTwips(1, p2t);
      const nsStyleFont* font;
      mOuter->GetStyleData(eStyleStruct_Font, (const nsStyleStruct*&) font);
      const nsFont& f = font->mFont;
      nsCOMPtr<nsIFontMetrics> fm;
      aPresContext->GetMetricsFor(f, getter_AddRefs(fm));
      nscoord fontHeight = 1;
      fm->GetHeight(fontHeight);

      nscoord maxX = size.width - scrollAreaSize.width;
      nscoord maxY = size.height - scrollAreaSize.height;

      nsIScrollableView* scrollable = GetScrollableView(aPresContext);
      scrollable->SetLineHeight(fontHeight);

      // set the scrollbars properties. Mark the scrollbars for reflow if there values change.
      if (mHasVerticalScrollbar) {
          SetAttribute(mVScrollbarFrame, nsXULAtoms::maxpos, maxY);
          SetAttribute(mVScrollbarFrame, nsXULAtoms::pageincrement, nscoord(scrollAreaSize.height - fontHeight));
          SetAttribute(mVScrollbarFrame, nsXULAtoms::increment, fontHeight, PR_FALSE);
      }

      if (mHasHorizontalScrollbar) {
          SetAttribute(mHScrollbarFrame, nsXULAtoms::maxpos, maxX);
          SetAttribute(mHScrollbarFrame, nsXULAtoms::pageincrement, nscoord(float(scrollAreaSize.width)*0.8));
          SetAttribute(mHScrollbarFrame, nsXULAtoms::increment, 10*mOnePixel, PR_FALSE);
      }      
      // size the scroll area
      // even if it is different from the old size. This is because ReflowFrame set the child's
      // frame so we have to make sure our final size is scrollAreaSize

      SetFrameSize(aPresContext, mScrollAreaFrame, scrollAreaSize);  
}  



/**
 * Places the scrollbars and scrollarea and returns the overall size
 */
void
nsGfxScrollFrameInner::LayoutChildren(nsIPresContext*          aPresContext,
                              nsHTMLReflowMetrics&     aDesiredSize,
                              const nsHTMLReflowState& aReflowState)
{
  nsSize scrollAreaSize;
  GetFrameSize(mScrollAreaFrame, scrollAreaSize);

  // add the padding to it.
  scrollAreaSize.width += aReflowState.mComputedPadding.left + aReflowState.mComputedPadding.right;
  scrollAreaSize.height += aReflowState.mComputedPadding.top + aReflowState.mComputedPadding.bottom;

  nsSize sbSize;
  GetScrollbarDimensions(aPresContext, sbSize);

  const nsMargin& borderPadding = aReflowState.mComputedBorderPadding;
  nsMargin border = borderPadding - aReflowState.mComputedPadding;

  // Place scroll area
  nsIView*  view;
  mScrollAreaFrame->MoveTo(aPresContext, borderPadding.left, borderPadding.top);
  mScrollAreaFrame->GetView(aPresContext, &view);
  if (view) {
    nsContainerFrame::SyncFrameViewAfterReflow(aPresContext, mScrollAreaFrame,
                                               view, nsnull);
  }

  // place vertical scrollbar move by the border because the scrollbars are outside our padding
  mVScrollbarFrame->MoveTo(aPresContext, border.left + scrollAreaSize.width, border.top);
  mVScrollbarFrame->GetView(aPresContext, &view);
  if (view) {
    nsContainerFrame::SyncFrameViewAfterReflow(aPresContext, mVScrollbarFrame,
                                               view, nsnull);
  }

  // place horizontal scrollbar
  mHScrollbarFrame->MoveTo(aPresContext, border.left, border.top + scrollAreaSize.height);
  mHScrollbarFrame->GetView(aPresContext, &view);
  if (view) {
    nsContainerFrame::SyncFrameViewAfterReflow(aPresContext, mHScrollbarFrame,
                                               view, nsnull);
  }

  // Compute our desired size
  // ---------- compute width ----------- 
  aDesiredSize.width = scrollAreaSize.width;

  // add border
  aDesiredSize.width += border.left + border.right;

  // add scrollbar
  if (mHasVerticalScrollbar) 
     aDesiredSize.width += sbSize.width;
  
  // ---------- compute height ----------- 

  /*
  // For the height if we're shrink wrapping then use whatever is smaller between
  // the available height and the child's desired size; otherwise, use the scroll
  // area size
  if (NS_AUTOHEIGHT == aReflowState.mComputedHeight) {
    aDesiredSize.height = PR_MIN(aReflowState.availableHeight, scrollAreaSize.height);
  } else {
  */

    aDesiredSize.height = scrollAreaSize.height;
  //}

  // add the scrollbar in
  if (mHasHorizontalScrollbar)
     aDesiredSize.height += sbSize.height;

  // add in our border
  aDesiredSize.height += border.top + border.bottom;
  

  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;
}

void
nsGfxScrollFrameInner::ScrollbarChanged(nsIPresContext* aPresContext, nscoord aX, nscoord aY)
{
  nsIScrollableView* scrollable = GetScrollableView(aPresContext);
  scrollable->ScrollTo(aX,aY, NS_SCROLL_PROPERTY_ALWAYS_BLIT);
 // printf("scrolling to: %d, %d\n", aX, aY);
}

nsGfxScrollFrameInner::~nsGfxScrollFrameInner()
{
    if (mDocument) {
        mDocument->RemoveObserver(this);
        mDocument = nsnull;
    }
}

/**
 * Returns whether it actually needed to change the attribute
 */
PRBool
nsGfxScrollFrameInner::SetAttribute(nsIFrame* aFrame, nsIAtom* aAtom, nscoord aSize, PRBool aReflow)
{
  // convert to pixels
  aSize /= mOnePixel;

  // only set the attribute if it changed.

  PRInt32 current = GetIntegerAttribute(aFrame, aAtom, -1);
  if (current != aSize)
  {
      nsCOMPtr<nsIContent> content;
      aFrame->GetContent(getter_AddRefs(content));
      char ch[100];
      sprintf(ch,"%d", aSize);
      nsAutoString newValue(ch);
      content->SetAttribute(kNameSpaceID_None, aAtom, newValue, aReflow);
      return PR_TRUE;
  }

  return PR_FALSE;
}

PRInt32
nsGfxScrollFrameInner::GetIntegerAttribute(nsIFrame* aFrame, nsIAtom* atom, PRInt32 defaultValue)
{
    nsCOMPtr<nsIContent> content;
    aFrame->GetContent(getter_AddRefs(content));

    nsAutoString value;
    if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttribute(kNameSpaceID_None, atom, value))
    {
      PRInt32 error;

      // convert it to an integer
      defaultValue = value.ToInteger(&error);
    }

    return defaultValue;
}

NS_IMETHODIMP
nsGfxScrollFrame::InvalidateCache(nsIFrame* aChild)
{
    // we don't cache any layout information. So do nothing.
    return NS_OK;
}

NS_IMETHODIMP
nsGfxScrollFrame::SetDebug(nsIPresContext* aPresContext, PRBool aDebug)
{
  nsIFrame* kid = mFrames.FirstChild();
  while (nsnull != kid) {
    nsIBox* ibox = nsnull;
    if (NS_SUCCEEDED(kid->QueryInterface(NS_GET_IID(nsIBox), (void**)&ibox)) && ibox) {
      ibox->SetDebug(aPresContext, aDebug);
    }

    kid->GetNextSibling(&kid);
  }

  return NS_OK;
}


NS_IMETHODIMP
nsGfxScrollFrame::GetBoxInfo(nsIPresContext* aPresContext, const nsHTMLReflowState& aReflowState, nsBoxInfo& aSize)
{

   nsIFrame* incrementalChild = nsnull;

   // if incremental pop off the child if it is ours and mark it dirty.
   if (aReflowState.reason == eReflowReason_Incremental) {
      aReflowState.reflowCommand->GetNext(incrementalChild, PR_FALSE);
      if (incrementalChild == mInner->mScrollAreaFrame || 
          incrementalChild == mInner->mVScrollbarFrame ||
          incrementalChild == mInner->mHScrollbarFrame) {
             aReflowState.reflowCommand->GetNext(incrementalChild);
             // mark it dirty
             nsFrameState childState;
             incrementalChild->GetFrameState(&childState);
             childState |= NS_FRAME_IS_DIRTY;
             incrementalChild->SetFrameState(childState);
      }
   }

  aSize.Clear();

  nsBoxInfo scrollAreaInfo, vboxInfo, hboxInfo;
  nsCOMPtr<nsIBox> ibox ( do_QueryInterface(mInner->mScrollAreaFrame) );
  if (ibox) ibox->GetBoxInfo(aPresContext, aReflowState, scrollAreaInfo);

  if (mInner->mHasVerticalScrollbar) {
    nsCOMPtr<nsIBox> vScrollbarBox ( do_QueryInterface(mInner->mVScrollbarFrame) );
    if (vScrollbarBox) vScrollbarBox->GetBoxInfo(aPresContext, aReflowState, vboxInfo);
  }

  if (mInner->mHasHorizontalScrollbar) {
    nsCOMPtr<nsIBox> hScrollbarBox ( do_QueryInterface(mInner->mHScrollbarFrame) );
    if (hScrollbarBox) hScrollbarBox->GetBoxInfo(aPresContext, aReflowState, hboxInfo);
  }

  aSize.prefSize.width += scrollAreaInfo.prefSize.width + vboxInfo.prefSize.width;
  aSize.prefSize.height += scrollAreaInfo.prefSize.height + hboxInfo.prefSize.height;

  aSize.minSize.width += vboxInfo.minSize.width;
  aSize.minSize.height += hboxInfo.minSize.height;

  if (scrollAreaInfo.maxSize.width != NS_INTRINSICSIZE && 
      vboxInfo.maxSize.width != NS_INTRINSICSIZE &&
      hboxInfo.maxSize.width != NS_INTRINSICSIZE) 
  {
     aSize.maxSize.width += scrollAreaInfo.maxSize.width + vboxInfo.maxSize.width + hboxInfo.maxSize.width;
  } 

 if (scrollAreaInfo.maxSize.height != NS_INTRINSICSIZE && 
      vboxInfo.maxSize.height != NS_INTRINSICSIZE &&
      hboxInfo.maxSize.height != NS_INTRINSICSIZE) 
  {
     aSize.maxSize.height += scrollAreaInfo.maxSize.height + vboxInfo.maxSize.height + hboxInfo.maxSize.height;
  } 

  return NS_OK;
}

/*
void 
nsBoxFrame::GetRedefinedMinPrefMax(nsIFrame* aFrame, nsBoxInfo& aSize)
{
  // add in the css min, max, pref
    const nsStylePosition* position;
    nsresult rv = aFrame->GetStyleData(eStyleStruct_Position,
                  (const nsStyleStruct*&) position);

    // see if the width or height was specifically set
    if (position->mWidth.GetUnit() == eStyleUnit_Coord)  {
        aSize.prefSize.width = position->mWidth.GetCoordValue();
    }

    if (position->mHeight.GetUnit() == eStyleUnit_Coord) {
        aSize.prefSize.height = position->mHeight.GetCoordValue();     
    }
    
    // same for min size. Unfortunately min size is always set to 0. So for now
    // we will assume 0 means not set.
    if (position->mMinWidth.GetUnit() == eStyleUnit_Coord) {
        nscoord min = position->mMinWidth.GetCoordValue();
        if (min != 0)
           aSize.minSize.width = min;
    }

    if (position->mMinHeight.GetUnit() == eStyleUnit_Coord) {
        nscoord min = position->mMinHeight.GetCoordValue();
        if (min != 0)
           aSize.minSize.height = min;
    }

    // and max
    if (position->mMaxWidth.GetUnit() == eStyleUnit_Coord) {
        nscoord max = position->mMaxWidth.GetCoordValue();
        aSize.maxSize.width = max;
    }

    if (position->mMaxHeight.GetUnit() == eStyleUnit_Coord) {
        nscoord max = position->mMaxHeight.GetCoordValue();
        aSize.maxSize.height = max;
    }

    // get the flexibility
    nsCOMPtr<nsIContent> content;
    aFrame->GetContent(getter_AddRefs(content));

    PRInt32 error;
    nsAutoString value;

    if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttribute(kNameSpaceID_None, nsXULAtoms::flex, value))
    {
        value.Trim("%");
        // convert to a percent.
        aSize.flex = value.ToFloat(&error)/float(100.0);
    }
}
*/


NS_IMETHODIMP
nsGfxScrollFrame::ReflowDirtyChild(nsIPresShell* aPresShell, nsIFrame* aChild)
{
    // if the frame is a vertical or horizontal scrollbar and we are about
    // to reflow the scrollbars then don't propagate up
    if (aChild == mInner->mHScrollbarFrame || aChild == mInner->mVScrollbarFrame) {
       
       // First pass means we will be checking to see if children should be reflowed
      // if (mInner->mFirstPass)
        //   return NS_OK;
    }

    // if we aren't dirty dirty us
    if (!(mState & NS_FRAME_IS_DIRTY)) {
        mState |= NS_FRAME_IS_DIRTY;
        // if out parent is a box ask it to post a reflow for us and mark us as dirty
        return mParent->ReflowDirtyChild(aPresShell,this);
    }

    return NS_OK;
}

nsresult 
nsGfxScrollFrameInner::GetContentOf(nsIFrame* aFrame, nsIContent** aContent)
{
    // If we don't have a content node find a parent that does.
    while(aFrame != nsnull) {
        aFrame->GetContent(aContent);
        if (*aContent != nsnull)
            return NS_OK;

        aFrame->GetParent(&aFrame);
    }

    NS_ERROR("Can't find a parent with a content node");
    return NS_OK;
}

