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
#include "nsBoxLayoutState.h"

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


  PRBool SetAttribute(nsIBox* aBox, nsIAtom* aAtom, nscoord aSize, PRBool aReflow=PR_TRUE);
  PRInt32 GetIntegerAttribute(nsIBox* aFrame, nsIAtom* atom, PRInt32 defaultValue);

  nsresult Layout(nsBoxLayoutState& aState);
  nsresult LayoutBox(nsBoxLayoutState& aState, nsIBox* aBox, const nsRect& aRect);

   void AddRemoveScrollbar       (PRBool& aHasScrollbar, 
                                  nscoord& aXY, 
                                  nscoord& aSize, 
                                  nscoord aSbSize, 
                                  PRBool aOnRightOrBottom, 
                                  PRBool aAdd);

   void AddHorizontalScrollbar   (const nsSize& aSbSize, nsRect& aScrollAreaSize, PRBool aOnBottom);
   void AddVerticalScrollbar     (const nsSize& aSbSize, nsRect& aScrollAreaSize, PRBool aOnRight);
   void RemoveHorizontalScrollbar(const nsSize& aSbSize, nsRect& aScrollAreaSize, PRBool aOnBottom);
   void RemoveVerticalScrollbar  (const nsSize& aSbSize, nsRect& aScrollAreaSize, PRBool aOnRight);

   nsIScrollableView* GetScrollableView(nsIPresContext* aPresContext);

   void GetScrolledContentSize(nsSize& aSize);

  void ScrollbarChanged(nsIPresContext* aPresContext, nscoord aX, nscoord aY);

  nsIBox* mHScrollbarBox;
  nsIBox* mVScrollbarBox;
  nsIBox* mScrollAreaBox;
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
NS_NewGfxScrollFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame, nsIDocument* aDocument, PRBool aIsRoot)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsGfxScrollFrame* it = new (aPresShell) nsGfxScrollFrame(aPresShell, aDocument, aIsRoot);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsGfxScrollFrame::nsGfxScrollFrame(nsIPresShell* aShell, nsIDocument* aDocument, PRBool aIsRoot):nsBoxFrame(aShell, aIsRoot)
{
    mInner = new nsGfxScrollFrameInner(this);
    mInner->AddRef();
    mInner->mDocument = aDocument;
    mPresContext = nsnull;
    mInner->mIsRoot = PR_FALSE;
    mInner->mNeverReflowed = PR_TRUE;
    SetLayoutManager(nsnull);
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
   NS_ERROR("Not implemented!");
  /*
   mFrames.DestroyFrame(aPresContext, mInner->mScrollAreaBox);
   mInner->mScrollAreaBox = aScrolledFrame;
   mFrames.InsertFrame(nsnull, nsnull, mInner->mScrollAreaBox);
   */
   return NS_OK;
}

/**
* Get the view that we are scrolling within the scrolling view. 
* @result child view
*/
NS_IMETHODIMP
nsGfxScrollFrame::GetScrolledFrame(nsIPresContext* aPresContext, nsIFrame *&aScrolledFrame) const
{
   nsIBox* child = nsnull;
   mInner->mScrollAreaBox->GetChildBox(&child);
   child->GetFrame(&aScrolledFrame);
   return NS_OK;
}

/**
 * Gets the size of the area that lies inside the scrollbars but clips the scrolled frame
 */
NS_IMETHODIMP
nsGfxScrollFrame::GetClipSize(nsIPresContext* aPresContext, 
                              nscoord *aWidth, 
                              nscoord *aHeight) const
{
   nsRect rect;
   mInner->mScrollAreaBox->GetBounds(rect);
   *aWidth = rect.width;
   *aHeight = rect.height;
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
  return nsBoxFrame::Destroy(aPresContext);
}

NS_IMETHODIMP
nsGfxScrollFrame::Init(nsIPresContext*  aPresContext,
                    nsIContent*      aContent,
                    nsIFrame*        aParent,
                    nsIStyleContext* aStyleContext,
                    nsIFrame*        aPrevInFlow)
{
  mPresContext = aPresContext;
  nsresult  rv = nsBoxFrame::Init(aPresContext, aContent,
                                            aParent, aStyleContext,
                                            aPrevInFlow);
  mInner->mDocument->AddObserver(mInner);
  return rv;
}
  
NS_IMETHODIMP
nsGfxScrollFrame::SetInitialChildList(nsIPresContext* aPresContext,
                                   nsIAtom*        aListName,
                                   nsIFrame*       aChildList)
{
  nsresult  rv = nsBoxFrame::SetInitialChildList(aPresContext, aListName,
                                                           aChildList);
  // get scroll area
  GetChildBox(&mInner->mScrollAreaBox);
  
  // horizontal scrollbar
  mInner->mScrollAreaBox->GetNextBox(&mInner->mHScrollbarBox);

  // vertical scrollbar
  mInner->mHScrollbarBox->GetNextBox(&mInner->mVScrollbarBox);

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
nsGfxScrollFrame::GetPadding(nsMargin& aMargin)
{
   aMargin.SizeTo(0,0,0,0);
   return NS_OK;
}

NS_IMETHODIMP
nsGfxScrollFrame::Paint(nsIPresContext*      aPresContext,
                     nsIRenderingContext& aRenderingContext,
                     const nsRect&        aDirtyRect,
                     nsFramePaintLayer    aWhichLayer)
{
  // Paint our children
  return nsBoxFrame::Paint(aPresContext, aRenderingContext, aDirtyRect,
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

  nsIFrame* frame = nsnull;
  mInner->mScrollAreaBox->GetFrame(&frame);

  return frame->GetContentAndOffsetsFromPoint(aCX, aPoint, aNewContent, aContentOffset, aContentOffsetEnd, aBeginFrameContent);
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


NS_IMETHODIMP
nsGfxScrollFrame::GetPrefSize(nsBoxLayoutState& aState, nsSize& aSize)
{
  PropagateDebug(aState);

  nsIFrame* frame = nsnull;
  GetFrame(&frame);

  const nsStyleDisplay* styleDisplay = nsnull;

  frame->GetStyleData(eStyleStruct_Display,
                      (const nsStyleStruct*&)styleDisplay);

  nsresult rv = mInner->mScrollAreaBox->GetPrefSize(aState, aSize);
  nsBox::AddMargin(mInner->mScrollAreaBox, aSize);

  if (styleDisplay->mOverflow == NS_STYLE_OVERFLOW_SCROLL || 
      styleDisplay->mOverflow == NS_STYLE_OVERFLOW_SCROLLBARS_VERTICAL) {
     nsSize vSize(0,0);
     mInner->mVScrollbarBox->GetPrefSize(aState, vSize);
     nsBox::AddMargin(mInner->mVScrollbarBox, vSize);

     aSize.width += vSize.width;
  }
   
  if (styleDisplay->mOverflow == NS_STYLE_OVERFLOW_SCROLL || 
      styleDisplay->mOverflow == NS_STYLE_OVERFLOW_SCROLLBARS_HORIZONTAL) {
     nsSize hSize(0,0);
     mInner->mHScrollbarBox->GetPrefSize(aState, hSize);
     nsBox::AddMargin(mInner->mHScrollbarBox, hSize);

     aSize.height += hSize.height;
  }

  AddBorderAndPadding(aSize);
  AddInset(aSize);
  nsIBox::AddCSSPrefSize(aState, this, aSize);

  return rv;
}

NS_IMETHODIMP
nsGfxScrollFrame::GetMinSize(nsBoxLayoutState& aState, nsSize& aSize)
{
  PropagateDebug(aState);

    nsIFrame* frame = nsnull;
  GetFrame(&frame);

  const nsStyleDisplay* styleDisplay = nsnull;

  frame->GetStyleData(eStyleStruct_Display,
                      (const nsStyleStruct*&)styleDisplay);

  nsresult rv = mInner->mScrollAreaBox->GetMinSize(aState, aSize);
     
  if (mInner->mHasVerticalScrollbar || 
      styleDisplay->mOverflow == NS_STYLE_OVERFLOW_SCROLL || 
      styleDisplay->mOverflow == NS_STYLE_OVERFLOW_SCROLLBARS_VERTICAL) {
    nsSize vSize(0,0);
    mInner->mVScrollbarBox->GetMinSize(aState, vSize);
     AddMargin(mInner->mVScrollbarBox, vSize);
     aSize.width += vSize.width;
     if (aSize.height < vSize.height)
        aSize.height = vSize.height;
  }
        
  if (mInner->mHasHorizontalScrollbar ||
      styleDisplay->mOverflow == NS_STYLE_OVERFLOW_SCROLL || 
      styleDisplay->mOverflow == NS_STYLE_OVERFLOW_SCROLLBARS_HORIZONTAL) {
     nsSize hSize(0,0);
     mInner->mHScrollbarBox->GetMinSize(aState, hSize);
     AddMargin(mInner->mHScrollbarBox, hSize);
     aSize.height += hSize.height;
     if (aSize.width < hSize.width)
        aSize.width = hSize.width;
  }

  AddBorderAndPadding(aSize);
  AddInset(aSize);
  nsIBox::AddCSSMinSize(aState, this, aSize);
  return rv;
}

NS_IMETHODIMP
nsGfxScrollFrame::GetMaxSize(nsBoxLayoutState& aState, nsSize& aSize)
{
  PropagateDebug(aState);

  aSize.width = NS_INTRINSICSIZE;
  aSize.height = NS_INTRINSICSIZE;

  AddBorderAndPadding(aSize);
  AddInset(aSize);
  nsIBox::AddCSSMaxSize(aState, this, aSize);
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt) 
nsGfxScrollFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt)
nsGfxScrollFrame::Release(void)
{
    return NS_OK;
}

#ifdef NS_DEBUG
NS_IMETHODIMP
nsGfxScrollFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("GfxScroll", aResult);
}
#endif

NS_INTERFACE_MAP_BEGIN(nsGfxScrollFrame)
  NS_INTERFACE_MAP_ENTRY(nsIAnonymousContentCreator)
#ifdef NS_DEBUG
  NS_INTERFACE_MAP_ENTRY(nsIFrameDebug)
#endif
  NS_INTERFACE_MAP_ENTRY(nsIScrollableFrame)
NS_INTERFACE_MAP_END_INHERITING(nsBoxFrame)



//-------------------- Inner ----------------------

nsGfxScrollFrameInner::nsGfxScrollFrameInner(nsGfxScrollFrame* aOuter):mHScrollbarBox(nsnull),
                                               mVScrollbarBox(nsnull),
                                               mScrollAreaBox(nsnull),
                                               mOnePixel(20),
                                               mHasVerticalScrollbar(PR_FALSE), 
                                               mHasHorizontalScrollbar(PR_FALSE)
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
   SetAttribute(mVScrollbarBox, nsXULAtoms::curpos, aY);
   SetAttribute(mHScrollbarBox, nsXULAtoms::curpos, aX);
   return NS_OK;
}

NS_IMETHODIMP
nsGfxScrollFrameInner::AttributeChanged(nsIDocument *aDocument,
                              nsIContent*     aContent,
                              PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aHint) 
{
   if (mHScrollbarBox && mVScrollbarBox)
   {
     nsIFrame* hframe = nsnull;
     mHScrollbarBox->GetFrame(&hframe);

     nsIFrame* vframe = nsnull;
     mVScrollbarBox->GetFrame(&vframe);

     nsCOMPtr<nsIContent> vcontent;
     nsCOMPtr<nsIContent> hcontent;

     hframe->GetContent(getter_AddRefs(hcontent));
     vframe->GetContent(getter_AddRefs(vcontent));

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
  nsIFrame* frame = nsnull;
  mScrollAreaBox->GetFrame(&frame);
  frame->GetView(aPresContext, &view);
  nsresult result = view->QueryInterface(kScrollViewIID, (void**)&scrollingView);
  NS_ASSERTION(NS_SUCCEEDED(result), "assertion gfx scrollframe does not contain a scrollframe");          
  return scrollingView;
}

void
nsGfxScrollFrameInner::GetScrolledContentSize(nsSize& aSize)
{
    // get the ara frame is the scrollarea
    nsIBox* child = nsnull;
    mScrollAreaBox->GetChildBox(&child);

    nsRect rect(0,0,0,0);
    child->GetBounds(rect);

    aSize.width = rect.width;
    aSize.height = rect.height;
    nsBox::AddMargin(child, aSize);
    nsBox::AddBorderAndPadding(mScrollAreaBox, aSize);
    nsBox::AddInset(mScrollAreaBox, aSize);
}

void
nsGfxScrollFrameInner::AddRemoveScrollbar(PRBool& aHasScrollbar, nscoord& aXY, nscoord& aSize, nscoord aSbSize, PRBool aRightOrBottom, PRBool aAdd)
{
   if ((aAdd && aHasScrollbar) || (!aAdd && !aHasScrollbar))
      return;
 
   nscoord size = aSize;

   if (size != NS_INTRINSICSIZE) {
     if (aAdd) {
        size -= aSbSize;
        if (!aRightOrBottom)
          aXY += aSbSize;
     } else {
        size += aSbSize;
        if (!aRightOrBottom)
          aXY -= aSbSize;
     }
   }

   // not enough room? If not don't do anything.
   if (size >= aSbSize) {
       aHasScrollbar = aAdd;
       aSize = size;
   }
}

void
nsGfxScrollFrameInner::AddHorizontalScrollbar(const nsSize& aSbSize, nsRect& aScrollAreaSize, PRBool aOnTop)
{
   AddRemoveScrollbar(mHasHorizontalScrollbar, aScrollAreaSize.y, aScrollAreaSize.height, aSbSize.height, aOnTop, PR_TRUE);
}

void
nsGfxScrollFrameInner::AddVerticalScrollbar(const nsSize& aSbSize, nsRect& aScrollAreaSize, PRBool aOnRight)
{
   AddRemoveScrollbar(mHasVerticalScrollbar, aScrollAreaSize.x, aScrollAreaSize.width, aSbSize.width, aOnRight, PR_TRUE);
}

void
nsGfxScrollFrameInner::RemoveHorizontalScrollbar(const nsSize& aSbSize, nsRect& aScrollAreaSize, PRBool aOnTop)
{
   AddRemoveScrollbar(mHasHorizontalScrollbar, aScrollAreaSize.y, aScrollAreaSize.height, aSbSize.height, aOnTop, PR_FALSE);
}

void
nsGfxScrollFrameInner::RemoveVerticalScrollbar(const nsSize& aSbSize, nsRect& aScrollAreaSize,  PRBool aOnRight)
{
   AddRemoveScrollbar(mHasVerticalScrollbar, aScrollAreaSize.x, aScrollAreaSize.width, aSbSize.width, aOnRight, PR_FALSE);
}


nsresult
nsGfxScrollFrameInner::LayoutBox(nsBoxLayoutState& aState, nsIBox* aBox, const nsRect& aRect)
{
  return mOuter->LayoutChildAt(aState, aBox, aRect);
}

NS_IMETHODIMP
nsGfxScrollFrame::Layout(nsBoxLayoutState& aState)
{
   PropagateDebug(aState);
   nsresult rv =  mInner->Layout(aState);
   nsBox::Layout(aState);
   return rv;
}

/**
 * Reflow the scroll area if it needs it and return its size. Also determine if the reflow will
 * cause any of the scrollbars to need to be reflowed.
 */
nsresult
nsGfxScrollFrameInner::Layout(nsBoxLayoutState& aState)
{
  //TODO make bidi code set these from preferences

  // if true places the vertical scrollbar on the right false puts it on the left.
  PRBool scrollBarRight = PR_TRUE;

  // if true places the horizontal scrollbar on the bottom false puts it on the top.
  PRBool scrollBarBottom = PR_TRUE;

  nsIFrame* frame = nsnull;
  mOuter->GetFrame(&frame);

  const nsStyleDisplay* styleDisplay = nsnull;

  frame->GetStyleData(eStyleStruct_Display,
                      (const nsStyleStruct*&)styleDisplay);

 
  // get the content rect
  nsRect clientRect(0,0,0,0);
  mOuter->GetClientRect(clientRect);

  // get the preferred size of the scrollbars
  nsSize hSize(0,0);
  nsSize vSize(0,0);
  mHScrollbarBox->GetPrefSize(aState, hSize);
  mVScrollbarBox->GetPrefSize(aState, vSize);
  nsSize hMinSize(0,0);
  nsSize vMinSize(0,0);
  mHScrollbarBox->GetMinSize(aState, hMinSize);
  mVScrollbarBox->GetMinSize(aState, vMinSize);

  nsBox::AddMargin(mHScrollbarBox, hSize);
  nsBox::AddMargin(mVScrollbarBox, vSize);
  nsBox::AddMargin(mHScrollbarBox, hMinSize);
  nsBox::AddMargin(mVScrollbarBox, vMinSize);

  // the scroll area size starts off as big as our content area
  nsRect scrollAreaRect(clientRect);

  nsSize sbSize(vSize.width, hSize.height);

  // Look at our style do we always have vertical or horizontal scrollbars?
  if (styleDisplay->mOverflow == NS_STYLE_OVERFLOW_SCROLL || styleDisplay->mOverflow == NS_STYLE_OVERFLOW_SCROLLBARS_HORIZONTAL)
     mHasHorizontalScrollbar = PR_TRUE;
  if (styleDisplay->mOverflow == NS_STYLE_OVERFLOW_SCROLL || styleDisplay->mOverflow == NS_STYLE_OVERFLOW_SCROLLBARS_VERTICAL)
     mHasVerticalScrollbar = PR_TRUE;

  // see if we currenly have a vertical or horizotal scrollbar and subtract them
  // from the scroll area size.
  if (mHasHorizontalScrollbar) {
      if (scrollAreaRect.height - sbSize.height < 0)
      {
          mHasHorizontalScrollbar = PR_FALSE;
          mHScrollbarBox->Collapse(aState);
      } else {
          scrollAreaRect.height -= sbSize.height;
          if (!scrollBarBottom)
             scrollAreaRect.y += sbSize.height;
      }
  }

  if (mHasVerticalScrollbar) {
      if (scrollAreaRect.width - sbSize.width < 0)
      {
          mHasVerticalScrollbar = PR_FALSE;
          mVScrollbarBox->Collapse(aState);
      } else {
          scrollAreaRect.width -= sbSize.width;
          if (!scrollBarRight)
             scrollAreaRect.x += sbSize.width;

      }
  }

  // layout our the scroll area
  LayoutBox(aState, mScrollAreaBox, scrollAreaRect);
  
  // now look at the content area and see if we need scrollbars or not
  PRBool needsLayout = PR_FALSE;
  nsSize scrolledContentSize(0,0);

  // if we have 'auto' scrollbars look at the vertical case
  if (styleDisplay->mOverflow != NS_STYLE_OVERFLOW_SCROLL
      && styleDisplay->mOverflow != NS_STYLE_OVERFLOW_SCROLLBARS_VERTICAL) {
      // get the area frame is the scrollarea
      GetScrolledContentSize(scrolledContentSize);

    // There are two cases to consider
      if (scrolledContentSize.height <= scrollAreaRect.height
          || styleDisplay->mOverflow == NS_STYLE_OVERFLOW_SCROLLBARS_HORIZONTAL
          || styleDisplay->mOverflow == NS_STYLE_OVERFLOW_SCROLLBARS_NONE) {
        if (mHasVerticalScrollbar) {
          // We left room for the vertical scrollbar, but it's not needed;
          // remove it.
          RemoveVerticalScrollbar(sbSize, scrollAreaRect, scrollBarRight);
          needsLayout = PR_TRUE;
          mVScrollbarBox->Collapse(aState);
        }
      } else {
        if (!mHasVerticalScrollbar) {
          // We didn't leave room for the vertical scrollbar, but it turns
          // out we needed it
          if (scrollAreaRect.width - sbSize.width >= 0) {
            AddVerticalScrollbar(sbSize, scrollAreaRect, scrollBarRight);
            mVScrollbarBox->UnCollapse(aState);
            needsLayout = PR_TRUE;
          }
        }
    }

    // ok layout at the right size
    if (needsLayout) {
       nsBoxLayoutState resizeState(aState);
       resizeState.SetLayoutReason(nsBoxLayoutState::Resize);
       LayoutBox(resizeState, mScrollAreaBox, scrollAreaRect);
    }
  }

  needsLayout = PR_FALSE;

  // if scrollbars are auto look at the horizontal case
  if ((NS_STYLE_OVERFLOW_SCROLL != styleDisplay->mOverflow)
      && (NS_STYLE_OVERFLOW_SCROLLBARS_HORIZONTAL != styleDisplay->mOverflow))
  {
    // get the area frame is the scrollarea
    GetScrolledContentSize(scrolledContentSize);

    // if the child is wider that the scroll area
    // and we don't have a scrollbar add one.
    if (scrolledContentSize.width > scrollAreaRect.width
        && styleDisplay->mOverflow != NS_STYLE_OVERFLOW_SCROLLBARS_VERTICAL
        && styleDisplay->mOverflow != NS_STYLE_OVERFLOW_SCROLLBARS_NONE) { 

      if (!mHasHorizontalScrollbar) {
           if (scrollAreaRect.height - sbSize.height >= 0) {
             AddHorizontalScrollbar(sbSize, scrollAreaRect, scrollBarBottom);
             mHScrollbarBox->UnCollapse(aState);
             needsLayout = PR_TRUE;
           }
           // if we added a horizonal scrollbar and we did not have a vertical
           // there is a chance that by adding the horizonal scrollbar we will
           // suddenly need a vertical scrollbar. Is a special case but its 
           // important.
           if (!mHasVerticalScrollbar && scrolledContentSize.height > scrollAreaRect.height - sbSize.height)
             printf("****Gfx Scrollbar Special case hit!!*****\n");
           
      }
    } else {
        // if the area is smaller or equal to and we have a scrollbar then
        // remove it.
      if (mHasHorizontalScrollbar) {
          RemoveHorizontalScrollbar(sbSize, scrollAreaRect, scrollBarBottom);
          mHScrollbarBox->Collapse(aState);
          needsLayout = PR_TRUE;
      }
    }
  }

  // we only need to set the rect. The inner child stays the same size.
  if (needsLayout) {
     nsBoxLayoutState resizeState(aState);
     resizeState.SetLayoutReason(nsBoxLayoutState::Resize);
     LayoutBox(resizeState, mScrollAreaBox, scrollAreaRect); 
  }
    
  GetScrolledContentSize(scrolledContentSize);

  nsIPresContext* presContext = aState.GetPresContext();
  float p2t;
  presContext->GetScaledPixelsToTwips(&p2t);
  mOnePixel = NSIntPixelsToTwips(1, p2t);
  const nsStyleFont* font;
  mOuter->GetStyleData(eStyleStruct_Font, (const nsStyleStruct*&) font);
  const nsFont& f = font->mFont;
  nsCOMPtr<nsIFontMetrics> fm;
  presContext->GetMetricsFor(f, getter_AddRefs(fm));
  nscoord fontHeight = 1;
  fm->GetHeight(fontHeight);

  nscoord maxX = scrolledContentSize.width - scrollAreaRect.width;
  nscoord maxY = scrolledContentSize.height - scrollAreaRect.height;

  nsIScrollableView* scrollable = GetScrollableView(presContext);
  scrollable->SetLineHeight(fontHeight);


    // layout the vertical and horizontal scrollbars


  // set the scrollbars properties. Mark the scrollbars for reflow if there values change.
  if (mHasVerticalScrollbar) {
      nsRect vRect(clientRect);
      vRect.width = vSize.width;
      vRect.y = clientRect.y;

      if (mHasHorizontalScrollbar) {
        vRect.height -= hSize.height;
        if (!scrollBarBottom)
            vRect.y += hSize.height;
      }

      vRect.x = clientRect.x;

      if (scrollBarRight)
         vRect.x += clientRect.width - vSize.width;

      if (vMinSize.width > vRect.width || vMinSize.height > vRect.height) {
        mVScrollbarBox->Collapse(aState);
      } else {
        SetAttribute(mVScrollbarBox, nsXULAtoms::maxpos, maxY);
        SetAttribute(mVScrollbarBox, nsXULAtoms::pageincrement, nscoord(scrollAreaRect.height - fontHeight));
        SetAttribute(mVScrollbarBox, nsXULAtoms::increment, fontHeight, PR_FALSE);

        LayoutBox(aState, mVScrollbarBox, vRect);
      }
  }

  if (mHasHorizontalScrollbar) {

      nsRect hRect(clientRect);
      hRect.height = hSize.height;

      hRect.x = clientRect.x;

      if (mHasVerticalScrollbar) {
         hRect.width -= vSize.width;
         if (!scrollBarRight)
            hRect.x += vSize.width;
      }

      hRect.y = clientRect.y;

      if (scrollBarBottom)
         hRect.y += clientRect.height - hSize.height;


      if (hMinSize.width > hRect.width || hMinSize.height > hRect.height) {
        mHScrollbarBox->Collapse(aState);
      } else {
        SetAttribute(mHScrollbarBox, nsXULAtoms::maxpos, maxX);
        SetAttribute(mHScrollbarBox, nsXULAtoms::pageincrement, nscoord(float(scrollAreaRect.width)*0.8));
        SetAttribute(mHScrollbarBox, nsXULAtoms::increment, 10*mOnePixel, PR_FALSE);

        LayoutBox(aState, mHScrollbarBox, hRect);
      }
  }
      
 return NS_OK;
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
nsGfxScrollFrameInner::SetAttribute(nsIBox* aBox, nsIAtom* aAtom, nscoord aSize, PRBool aReflow)
{
  // convert to pixels
  aSize /= mOnePixel;

  // only set the attribute if it changed.

  PRInt32 current = GetIntegerAttribute(aBox, aAtom, -1);
  if (current != aSize)
  {
      nsIFrame* frame = nsnull;
      aBox->GetFrame(&frame);
      nsCOMPtr<nsIContent> content;
      frame->GetContent(getter_AddRefs(content));
      char ch[100];
      sprintf(ch,"%d", aSize);
      nsAutoString newValue(ch);
      content->SetAttribute(kNameSpaceID_None, aAtom, newValue, aReflow);
      return PR_TRUE;
  }

  return PR_FALSE;
}

PRInt32
nsGfxScrollFrameInner::GetIntegerAttribute(nsIBox* aBox, nsIAtom* atom, PRInt32 defaultValue)
{
    nsIFrame* frame = nsnull;
    aBox->GetFrame(&frame);

    nsCOMPtr<nsIContent> content;
    frame->GetContent(getter_AddRefs(content));

    nsAutoString value;
    if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttribute(kNameSpaceID_None, atom, value))
    {
      PRInt32 error;

      // convert it to an integer
      defaultValue = value.ToInteger(&error);
    }

    return defaultValue;
}

nsresult 
nsGfxScrollFrame::GetContentOf(nsIContent** aContent)
{
    GetContent(aContent);
    return NS_OK;
}
