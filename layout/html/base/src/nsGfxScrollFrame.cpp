/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsCOMPtr.h"
#include "nsHTMLParts.h"
#include "nsIPresContext.h"
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
#include "nsINodeInfo.h"
#include "nsIScrollbarFrame.h"
#include "nsIScrollbarMediator.h"
#include "nsIGfxTextControlFrame.h"
#include "nsIDOMHTMLTextAreaElement.h"

static NS_DEFINE_IID(kWidgetCID, NS_CHILD_CID);
static NS_DEFINE_IID(kScrollingViewCID, NS_SCROLLING_VIEW_CID);
static NS_DEFINE_IID(kViewCID, NS_VIEW_CID);



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
                              PRInt32      aModType,
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

   PRBool AddRemoveScrollbar       (PRBool& aHasScrollbar, 
                                  nscoord& aXY, 
                                  nscoord& aSize, 
                                  nscoord aSbSize, 
                                  PRBool aOnRightOrBottom, 
                                  PRBool aAdd);

   PRBool AddRemoveScrollbar(nsBoxLayoutState& aState, 
                           nsRect& aScrollAreaSize, 
                           PRBool aOnTop, 
                           PRBool aHorizontal, 
                           PRBool aAdd);

   PRBool AddHorizontalScrollbar   (nsBoxLayoutState& aState, nsRect& aScrollAreaSize, PRBool aOnBottom);
   PRBool AddVerticalScrollbar     (nsBoxLayoutState& aState, nsRect& aScrollAreaSize, PRBool aOnRight);
   PRBool RemoveHorizontalScrollbar(nsBoxLayoutState& aState, nsRect& aScrollAreaSize, PRBool aOnBottom);
   PRBool RemoveVerticalScrollbar  (nsBoxLayoutState& aState, nsRect& aScrollAreaSize, PRBool aOnRight);

   nsIScrollableView* GetScrollableView(nsIPresContext* aPresContext);

  void ScrollbarChanged(nsIPresContext* aPresContext, nscoord aX, nscoord aY);

  void SetScrollbarVisibility(nsIBox* aScrollbar, PRBool aVisible);

  NS_IMETHOD GetScrolledSize(nsIPresContext* aPresContext, 
                         nscoord *aWidth, 
                         nscoord *aHeight) const;

  nsIBox* mHScrollbarBox;
  nsIBox* mVScrollbarBox;
  nsIBox* mScrollAreaBox;
  nscoord mOnePixel;
  nsCOMPtr<nsIDocument> mDocument;
  nsGfxScrollFrame* mOuter;
  nsIScrollableView* mScrollableView;
  nsSize mMaxElementSize;

  PRBool mNeverHasVerticalScrollbar;   
  PRBool mNeverHasHorizontalScrollbar; 

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

NS_IMETHODIMP
nsGfxScrollFrame::GetScrollableView(nsIPresContext* aContext, nsIScrollableView** aResult)
{
  *aResult = mInner->GetScrollableView(aContext);
  return NS_OK;
}

NS_IMETHODIMP
nsGfxScrollFrame::GetScrollPosition(nsIPresContext* aContext, nscoord &aX, nscoord& aY) const
{
   nsIScrollableView* s = mInner->GetScrollableView(aContext);
   return s->GetScrollPosition(aX, aY);
}

NS_IMETHODIMP
nsGfxScrollFrame::ScrollTo(nsIPresContext* aContext, nscoord aX, nscoord aY, PRUint32 aFlags)
{
   nsIScrollableView* s = mInner->GetScrollableView(aContext);
   return s->ScrollTo(aX, aY, aFlags);
}

/**
 * Query whether scroll bars should be displayed all the time, never or
 * only when necessary.
 * @return current scrollbar selection
 */
NS_IMETHODIMP
nsGfxScrollFrame::GetScrollPreference(nsIPresContext* aPresContext, nsScrollPref* aScrollPreference) const
{
  const nsStyleDisplay* styleDisplay = nsnull;

  GetStyleData(eStyleStruct_Display,
              (const nsStyleStruct*&)styleDisplay);

  switch (styleDisplay->mOverflow)
  {
    case NS_STYLE_OVERFLOW_SCROLL:
      *aScrollPreference = AlwaysScroll;
    break;

    case NS_STYLE_OVERFLOW_SCROLLBARS_HORIZONTAL:
      *aScrollPreference = AlwaysScrollHorizontal;
    break;

    case NS_STYLE_OVERFLOW_SCROLLBARS_VERTICAL:
      *aScrollPreference = AlwaysScrollVertical;
    break;

    case NS_STYLE_OVERFLOW_AUTO:
      *aScrollPreference = Auto;
    break;

    default:
      *aScrollPreference = NeverScroll;
  }

  return NS_OK;
}

/**
* Gets the size of the area that lies inside the scrollbars but clips the scrolled frame
*/
NS_IMETHODIMP
nsGfxScrollFrame::GetScrollbarSizes(nsIPresContext* aPresContext, 
                             nscoord *aVbarWidth, 
                             nscoord *aHbarHeight) const
{
  nsBoxLayoutState state(aPresContext);

  nsSize hs;
  mInner->mHScrollbarBox->GetPrefSize(state, hs);
  *aHbarHeight = hs.height;

  nsSize vs;
  mInner->mVScrollbarBox->GetPrefSize(state, vs);
  *aVbarWidth = vs.width;

  return NS_OK;
}

NS_IMETHODIMP
nsGfxScrollFrame::SetScrollbarVisibility(nsIPresContext* aPresContext,
                                    PRBool aVerticalVisible,
                                    PRBool aHorizontalVisible)
{
  mInner->mNeverHasVerticalScrollbar = !aVerticalVisible;
  mInner->mNeverHasHorizontalScrollbar = !aHorizontalVisible;
  return NS_OK;
}

NS_IMETHODIMP
nsGfxScrollFrame::GetScrollbarBox(PRBool aVertical, nsIBox** aResult)
{
  *aResult = aVertical ? mInner->mVScrollbarBox : mInner->mHScrollbarBox;
  return NS_OK;
}

NS_IMETHODIMP
nsGfxScrollFrame::CreateAnonymousContent(nsIPresContext* aPresContext,
                                         nsISupportsArray& aAnonymousChildren)
{
  // The anonymous <div> used by <inputs> never gets scrollbars.
  nsCOMPtr<nsIGfxTextControlFrame2> textFrame(do_QueryInterface(mParent));
  if (textFrame) {
    // Make sure we are not a text area.
    nsCOMPtr<nsIContent> content;
    mParent->GetContent(getter_AddRefs(content));
    nsCOMPtr<nsIDOMHTMLTextAreaElement> textAreaElement(do_QueryInterface(content));
    if (!textAreaElement) {
      SetScrollbarVisibility(aPresContext, PR_FALSE, PR_FALSE);
      return NS_OK;
    }
  }

  // create horzontal scrollbar
  nsresult rv;
  nsCOMPtr<nsIElementFactory> elementFactory = 
           do_GetService(NS_ELEMENT_FACTORY_CONTRACTID_PREFIX "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul", &rv);
  if (!elementFactory)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsINodeInfoManager> nodeInfoManager;
  mInner->mDocument->GetNodeInfoManager(*getter_AddRefs(nodeInfoManager));
  NS_ENSURE_TRUE(nodeInfoManager, NS_ERROR_FAILURE);

  nsCOMPtr<nsINodeInfo> nodeInfo;
  nodeInfoManager->GetNodeInfo(NS_ConvertASCIItoUCS2("scrollbar"), nsString(), NS_ConvertASCIItoUCS2("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"),
    *getter_AddRefs(nodeInfo));

  nsCOMPtr<nsIContent> content;
  elementFactory->CreateInstanceByTag(nodeInfo, getter_AddRefs(content));
  content->SetAttr(kNameSpaceID_None, nsXULAtoms::orient,
                   NS_LITERAL_STRING("horizontal"), PR_FALSE);

  content->SetAttr(kNameSpaceID_None, nsXULAtoms::collapsed,
                   NS_LITERAL_STRING("true"), PR_FALSE);

  aAnonymousChildren.AppendElement(content);

  // create vertical scrollbar
  content = nsnull;
  elementFactory->CreateInstanceByTag(nodeInfo, getter_AddRefs(content));
  content->SetAttr(kNameSpaceID_None, nsXULAtoms::orient,
                   NS_LITERAL_STRING("vertical"), PR_FALSE);

  content->SetAttr(kNameSpaceID_None, nsXULAtoms::collapsed,
                   NS_LITERAL_STRING("true"), PR_FALSE);

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
  if (mInner->mHScrollbarBox)
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
  nsIFrame* frame;
  mInner->mScrollAreaBox->GetFrame(&frame);
  return frame->AppendFrames(aPresContext,
                                     aPresShell,
                                     aListName,
                                     aFrameList);
}

NS_IMETHODIMP
nsGfxScrollFrame::InsertFrames(nsIPresContext* aPresContext,
                      nsIPresShell&   aPresShell,
                      nsIAtom*        aListName,
                      nsIFrame*       aPrevFrame,
                      nsIFrame*       aFrameList)
{
  nsIFrame* frame;
  mInner->mScrollAreaBox->GetFrame(&frame);

  return frame->InsertFrames(aPresContext,
                                     aPresShell,
                                     aListName,
                                     aPrevFrame,
                                     aFrameList);
}

NS_IMETHODIMP
nsGfxScrollFrame::RemoveFrame(nsIPresContext* aPresContext,
                     nsIPresShell&   aPresShell,
                     nsIAtom*        aListName,
                     nsIFrame*       aOldFrame)
{
  nsIFrame* frame;
  mInner->mScrollAreaBox->GetFrame(&frame);

  return frame->RemoveFrame (aPresContext,
                                     aPresShell,
                                     aListName,
                                     aOldFrame);
}


NS_IMETHODIMP
nsGfxScrollFrame::ReplaceFrame(nsIPresContext* aPresContext,
                     nsIPresShell&   aPresShell,
                     nsIAtom*        aListName,
                     nsIFrame*       aOldFrame,
                     nsIFrame*       aNewFrame)
{
  nsIFrame* frame;
  mInner->mScrollAreaBox->GetFrame(&frame);

  return frame->ReplaceFrame (aPresContext,
                                     aPresShell,
                                     aListName,
                                     aOldFrame,
                                     aNewFrame);
}




NS_IMETHODIMP
nsGfxScrollFrame::GetPadding(nsMargin& aMargin)
{
   aMargin.SizeTo(0,0,0,0);
   return NS_OK;
}

NS_IMETHODIMP
nsGfxScrollFrame::Paint(nsIPresContext*   aPresContext,
                     nsIRenderingContext& aRenderingContext,
                     const nsRect&        aDirtyRect,
                     nsFramePaintLayer    aWhichLayer,
                     PRUint32             aFlags)
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
nsGfxScrollFrame::GetAscent(nsBoxLayoutState& aState, nscoord& aAscent)
{
  aAscent = 0;
  nsresult rv = mInner->mScrollAreaBox->GetAscent(aState, aAscent);
  nsMargin m(0,0,0,0);
  GetBorderAndPadding(m);
  aAscent += m.top;
  GetMargin(m);
  aAscent += m.top;
  GetInset(m);
  aAscent += m.top;

  return rv;
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
     // make sure they are visible.
     mInner->SetScrollbarVisibility(mInner->mVScrollbarBox, PR_TRUE);
     nsSize vSize(0,0);
     mInner->mVScrollbarBox->GetPrefSize(aState, vSize);
     nsBox::AddMargin(mInner->mVScrollbarBox, vSize);

     aSize.width += vSize.width;
  }
   
  if (styleDisplay->mOverflow == NS_STYLE_OVERFLOW_SCROLL || 
      styleDisplay->mOverflow == NS_STYLE_OVERFLOW_SCROLLBARS_HORIZONTAL) {
     nsSize hSize(0,0);
     mInner->SetScrollbarVisibility(mInner->mHScrollbarBox, PR_TRUE);
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
     
  if (styleDisplay->mOverflow == NS_STYLE_OVERFLOW_SCROLL || 
      styleDisplay->mOverflow == NS_STYLE_OVERFLOW_SCROLLBARS_VERTICAL) {
    nsSize vSize(0,0);
    mInner->mVScrollbarBox->GetMinSize(aState, vSize);
     AddMargin(mInner->mVScrollbarBox, vSize);
     aSize.width += vSize.width;
     if (aSize.height < vSize.height)
        aSize.height = vSize.height;
  }
        
  if (styleDisplay->mOverflow == NS_STYLE_OVERFLOW_SCROLL || 
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

NS_IMETHODIMP
nsGfxScrollFrame::Reflow(nsIPresContext*   aPresContext,
                     nsHTMLReflowMetrics&     aDesiredSize,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsGfxScrollFrame", aReflowState.reason);

  // if there is a max element request then set it to -1 so we can see if it gets set
  if (aDesiredSize.maxElementSize)
  {
    aDesiredSize.maxElementSize->width = -1;
    aDesiredSize.maxElementSize->height = -1;
  }

  nsresult rv = nsBoxFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);

  // if it was set then cache it. Otherwise set it.
  if (aDesiredSize.maxElementSize)
  {
    nsSize* size = aDesiredSize.maxElementSize;

    // if not set then use the cached size. If set then set it.
    if (size->width == -1)
      size->width = mInner->mMaxElementSize.width;
    else 
      mInner->mMaxElementSize.width = size->width;
  
    if (size->height == -1)
      size->height = mInner->mMaxElementSize.height;
    else 
      mInner->mMaxElementSize.height = size->height;
  }
  
  return rv;
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
   mNeverHasVerticalScrollbar   = PR_FALSE;     
   mNeverHasHorizontalScrollbar = PR_FALSE; 
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
   if (mVScrollbarBox)
     SetAttribute(mVScrollbarBox, nsXULAtoms::curpos, aY);
   
   if (mHScrollbarBox)
     SetAttribute(mHScrollbarBox, nsXULAtoms::curpos, aX);
   
   return NS_OK;
}

NS_IMETHODIMP
nsGfxScrollFrameInner::AttributeChanged(nsIDocument *aDocument,
                              nsIContent*     aContent,
                              PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType,
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
        if (NS_CONTENT_ATTR_HAS_VALUE == hcontent->GetAttr(kNameSpaceID_None, nsXULAtoms::curpos, value))
        {
           PRInt32 error;

           // convert it to an integer
           x = value.ToInteger(&error);
        }

        if (NS_CONTENT_ATTR_HAS_VALUE == vcontent->GetAttr(kNameSpaceID_None, nsXULAtoms::curpos, value))
        {
           PRInt32 error;

           // convert it to an integer
           y = value.ToInteger(&error);
        }

        nsIScrollableView* s = GetScrollableView(mOuter->mPresContext);
        s->RemoveScrollPositionListener(this);
        ScrollbarChanged(mOuter->mPresContext, x*mOnePixel, y*mOnePixel);
        s->AddScrollPositionListener(this);

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
  nsresult result = view->QueryInterface(NS_GET_IID(nsIScrollableView), (void**)&scrollingView);
  NS_ASSERTION(NS_SUCCEEDED(result), "assertion gfx scrollframe does not contain a scrollframe");          
  return scrollingView;
}

PRBool
nsGfxScrollFrameInner::AddHorizontalScrollbar(nsBoxLayoutState& aState, nsRect& aScrollAreaSize, PRBool aOnTop)
{
#ifdef IBMBIDI
  if (mHScrollbarBox) {
    PRInt32 dir = GetIntegerAttribute(mHScrollbarBox, nsXULAtoms::dir, -1);
    const nsStyleVisibility* vis;
    mOuter->GetStyleData(eStyleStruct_Visibility, (const nsStyleStruct*&)vis);

    // when creating the scrollbar for the first time, or whenever 
    // display direction is changed, scroll the view horizontally
    if (dir != vis->mDirection) {
      SetAttribute(mHScrollbarBox, nsXULAtoms::curpos,
                   (NS_STYLE_DIRECTION_LTR == vis->mDirection) ? 0 : 0x7FFFFFFF);
      SetAttribute(mHScrollbarBox, nsXULAtoms::dir, vis->mDirection * mOnePixel);
    }
  }
#endif // IBMBIDI
   return AddRemoveScrollbar(aState, aScrollAreaSize, aOnTop, PR_TRUE, PR_TRUE);
}

PRBool
nsGfxScrollFrameInner::AddVerticalScrollbar(nsBoxLayoutState& aState, nsRect& aScrollAreaSize, PRBool aOnRight)
{
   return AddRemoveScrollbar(aState, aScrollAreaSize, aOnRight, PR_FALSE, PR_TRUE);
}

PRBool
nsGfxScrollFrameInner::RemoveHorizontalScrollbar(nsBoxLayoutState& aState, nsRect& aScrollAreaSize, PRBool aOnTop)
{
   return AddRemoveScrollbar(aState, aScrollAreaSize, aOnTop, PR_TRUE, PR_FALSE);
}

PRBool
nsGfxScrollFrameInner::RemoveVerticalScrollbar(nsBoxLayoutState& aState, nsRect& aScrollAreaSize,  PRBool aOnRight)
{
   return AddRemoveScrollbar(aState, aScrollAreaSize, aOnRight, PR_FALSE, PR_FALSE);
}

PRBool
nsGfxScrollFrameInner::AddRemoveScrollbar(nsBoxLayoutState& aState, nsRect& aScrollAreaSize, PRBool aOnTop, PRBool aHorizontal, PRBool aAdd)
{
  if (aHorizontal) {
     if (mNeverHasHorizontalScrollbar)
       return PR_FALSE;

     if (aAdd)
        SetScrollbarVisibility(mHScrollbarBox, aAdd);

     nsSize hSize;
     mHScrollbarBox->GetPrefSize(aState, hSize);
     nsBox::AddMargin(mHScrollbarBox, hSize);

     if (!aAdd)
        SetScrollbarVisibility(mHScrollbarBox, aAdd);

     PRBool fit = AddRemoveScrollbar(mHasHorizontalScrollbar, aScrollAreaSize.y, aScrollAreaSize.height, hSize.height, aOnTop, aAdd);  
     if (!fit)
        SetScrollbarVisibility(mHScrollbarBox, !aAdd);

     return fit;
  } else {
     if (mNeverHasVerticalScrollbar)
       return PR_FALSE;

     if (aAdd)
       SetScrollbarVisibility(mVScrollbarBox, aAdd);

     nsSize vSize;
     mVScrollbarBox->GetPrefSize(aState, vSize);

     if (!aAdd)
       SetScrollbarVisibility(mVScrollbarBox, aAdd);

     nsBox::AddMargin(mVScrollbarBox, vSize);
     PRBool fit = AddRemoveScrollbar(mHasVerticalScrollbar, aScrollAreaSize.x, aScrollAreaSize.width, vSize.width, aOnTop, aAdd);
     if (!fit)
        SetScrollbarVisibility(mVScrollbarBox, !aAdd);

     return fit;
  }
}

PRBool
nsGfxScrollFrameInner::AddRemoveScrollbar(PRBool& aHasScrollbar, nscoord& aXY, nscoord& aSize, nscoord aSbSize, PRBool aRightOrBottom, PRBool aAdd)
{ 
   nscoord size = aSize;

   if (size != NS_INTRINSICSIZE) {
     if (aAdd) {
        size -= aSbSize;
        if (!aRightOrBottom && size >= 0)
          aXY += aSbSize;
     } else {
        size += aSbSize;
        if (!aRightOrBottom)
          aXY -= aSbSize;
     }
   }

   // not enough room? Yes? Return true.
   if (size >= aSbSize) {
       aHasScrollbar = aAdd;
       aSize = size;
       return PR_TRUE;
   }

   return PR_FALSE;
}

nsresult
nsGfxScrollFrameInner::LayoutBox(nsBoxLayoutState& aState, nsIBox* aBox, const nsRect& aRect)
{
  return mOuter->LayoutChildAt(aState, aBox, aRect);
}

NS_IMETHODIMP
nsGfxScrollFrame::DoLayout(nsBoxLayoutState& aState)
{
   PRUint32 flags = 0;
   aState.GetLayoutFlags(flags);
   nsresult rv =  mInner->Layout(aState);
   aState.SetLayoutFlags(flags);

   nsBox::DoLayout(aState);
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

#ifdef IBMBIDI
  const nsStyleVisibility* vis;
  mOuter->GetStyleData(eStyleStruct_Visibility, (const nsStyleStruct*&)vis);

  //
  // Direction Style from this->GetStyleData()
  // now in (vis->mDirection)
  // ------------------
  // NS_STYLE_DIRECTION_LTR : LTR or Default
  // NS_STYLE_DIRECTION_RTL
  // NS_STYLE_DIRECTION_INHERIT
  //

  if (vis->mDirection == NS_STYLE_DIRECTION_RTL){
    // if true places the vertical scrollbar on the right false puts it on the left.
    scrollBarRight = PR_FALSE;

    // if true places the horizontal scrollbar on the bottom false puts it on the top.
    scrollBarBottom = PR_TRUE;
  }
  nsHTMLReflowState* reflowState = (nsHTMLReflowState*)aState.GetReflowState();
#endif // IBMBIDI

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
  nsSize hMinSize(0,0);
  nsSize vMinSize(0,0);

  /*
  mHScrollbarBox->GetPrefSize(aState, hSize);
  mVScrollbarBox->GetPrefSize(aState, vSize);
  mHScrollbarBox->GetMinSize(aState, hMinSize);
  mVScrollbarBox->GetMinSize(aState, vMinSize);

  nsBox::AddMargin(mHScrollbarBox, hSize);
  nsBox::AddMargin(mVScrollbarBox, vSize);
  nsBox::AddMargin(mHScrollbarBox, hMinSize);
  nsBox::AddMargin(mVScrollbarBox, vMinSize);
  */

  // the scroll area size starts off as big as our content area
  nsRect scrollAreaRect(clientRect);

  // Look at our style do we always have vertical or horizontal scrollbars?
  if (styleDisplay->mOverflow == NS_STYLE_OVERFLOW_SCROLL || styleDisplay->mOverflow == NS_STYLE_OVERFLOW_SCROLLBARS_HORIZONTAL)
     mHasHorizontalScrollbar = PR_TRUE;
  if (styleDisplay->mOverflow == NS_STYLE_OVERFLOW_SCROLL || styleDisplay->mOverflow == NS_STYLE_OVERFLOW_SCROLLBARS_VERTICAL)
     mHasVerticalScrollbar = PR_TRUE;

  if (mHasHorizontalScrollbar)
     AddHorizontalScrollbar(aState, scrollAreaRect, scrollBarBottom);

  if (mHasVerticalScrollbar)
     AddVerticalScrollbar(aState, scrollAreaRect, scrollBarRight);
     
  nsRect oldScrollAreaBounds;
  mScrollAreaBox->GetClientRect(oldScrollAreaBounds);

  // layout our the scroll area
  LayoutBox(aState, mScrollAreaBox, scrollAreaRect);
  
  // now look at the content area and see if we need scrollbars or not
  PRBool needsLayout = PR_FALSE;
  nsSize scrolledContentSize(0,0);

  // if we have 'auto' scrollbars look at the vertical case
  if (styleDisplay->mOverflow != NS_STYLE_OVERFLOW_SCROLL
      && styleDisplay->mOverflow != NS_STYLE_OVERFLOW_SCROLLBARS_VERTICAL) {
      // get the area frame is the scrollarea
      GetScrolledSize(aState.GetPresContext(),&scrolledContentSize.width, &scrolledContentSize.height);

    // There are two cases to consider
      if (scrolledContentSize.height <= scrollAreaRect.height
          || styleDisplay->mOverflow == NS_STYLE_OVERFLOW_SCROLLBARS_HORIZONTAL
          || styleDisplay->mOverflow == NS_STYLE_OVERFLOW_SCROLLBARS_NONE) {
        if (mHasVerticalScrollbar) {
          // We left room for the vertical scrollbar, but it's not needed;
          // remove it.
          if (RemoveVerticalScrollbar(aState, scrollAreaRect, scrollBarRight)) {
            needsLayout = PR_TRUE;
            SetAttribute(mVScrollbarBox, nsXULAtoms::curpos, 0);
          }
        }
      } else {
        if (!mHasVerticalScrollbar) {
          // We didn't leave room for the vertical scrollbar, but it turns
          // out we needed it
          if (AddVerticalScrollbar(aState, scrollAreaRect, scrollBarRight))
            needsLayout = PR_TRUE;

        }
    }

    // ok layout at the right size
    if (needsLayout) {
       nsBoxLayoutState resizeState(aState);
       resizeState.SetLayoutReason(nsBoxLayoutState::Resize);
       LayoutBox(resizeState, mScrollAreaBox, scrollAreaRect);
       needsLayout = PR_FALSE;
    }
  }


  // if scrollbars are auto look at the horizontal case
  if ((NS_STYLE_OVERFLOW_SCROLL != styleDisplay->mOverflow)
      && (NS_STYLE_OVERFLOW_SCROLLBARS_HORIZONTAL != styleDisplay->mOverflow))
  {
    // get the area frame is the scrollarea
      GetScrolledSize(aState.GetPresContext(),&scrolledContentSize.width, &scrolledContentSize.height);

    // if the child is wider that the scroll area
    // and we don't have a scrollbar add one.
    if (scrolledContentSize.width > scrollAreaRect.width
        && styleDisplay->mOverflow != NS_STYLE_OVERFLOW_SCROLLBARS_VERTICAL
        && styleDisplay->mOverflow != NS_STYLE_OVERFLOW_SCROLLBARS_NONE) { 

      if (!mHasHorizontalScrollbar) {
           // no scrollbar? 
          if (AddHorizontalScrollbar(aState, scrollAreaRect, scrollBarBottom))
             needsLayout = PR_TRUE;

           // if we added a horizonal scrollbar and we did not have a vertical
           // there is a chance that by adding the horizonal scrollbar we will
           // suddenly need a vertical scrollbar. Is a special case but its 
           // important.
           //if (!mHasVerticalScrollbar && scrolledContentSize.height > scrollAreaRect.height - sbSize.height)
           //  printf("****Gfx Scrollbar Special case hit!!*****\n");
           
      }
#ifdef IBMBIDI
      const nsStyleVisibility* ourVis;
      frame->GetStyleData(eStyleStruct_Visibility, (const nsStyleStruct*&)ourVis);

      if (NS_STYLE_DIRECTION_RTL == ourVis->mDirection) {
        nsCOMPtr<nsIGfxTextControlFrame2> textControl(
          do_QueryInterface(mOuter->mParent) );
        if (textControl) {
          needsLayout = PR_TRUE;
          reflowState->mRightEdge = scrolledContentSize.width;
          mScrollAreaBox->MarkDirty(aState);
        }
      }
#endif // IBMBIDI
    } else {
        // if the area is smaller or equal to and we have a scrollbar then
        // remove it.
      if (mHasHorizontalScrollbar) {
          if (RemoveHorizontalScrollbar(aState, scrollAreaRect, scrollBarBottom))
             needsLayout = PR_TRUE;
             SetAttribute(mHScrollbarBox, nsXULAtoms::curpos, 0);
      }
    }
  }

  // we only need to set the rect. The inner child stays the same size.
  if (needsLayout) {
     nsBoxLayoutState resizeState(aState);
     resizeState.SetLayoutReason(nsBoxLayoutState::Resize);
     LayoutBox(resizeState, mScrollAreaBox, scrollAreaRect); 
     needsLayout = PR_FALSE;
#ifdef IBMBIDI
     reflowState->mRightEdge = NS_UNCONSTRAINEDSIZE;
#endif // IBMBIDI
  }
    
  GetScrolledSize(aState.GetPresContext(),&scrolledContentSize.width, &scrolledContentSize.height);

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
  NS_ASSERTION(fm,"FontMetrics is null assuming fontHeight == 1");
  if (fm)
    fm->GetHeight(fontHeight);

  nscoord maxX = scrolledContentSize.width - scrollAreaRect.width;
  nscoord maxY = scrolledContentSize.height - scrollAreaRect.height;

  nsIScrollableView* scrollable = GetScrollableView(presContext);
  scrollable->SetLineHeight(fontHeight);

  if (mHScrollbarBox)
    mHScrollbarBox->GetPrefSize(aState, hSize);
  
  if (mVScrollbarBox)
    mVScrollbarBox->GetPrefSize(aState, vSize);

  // layout vertical scrollbar
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

  if (mHasVerticalScrollbar) {
    SetAttribute(mVScrollbarBox, nsXULAtoms::maxpos, maxY);
    SetAttribute(mVScrollbarBox, nsXULAtoms::pageincrement, nscoord(scrollAreaRect.height - fontHeight));
    SetAttribute(mVScrollbarBox, nsXULAtoms::increment, fontHeight, PR_FALSE);
  }

  if (mVScrollbarBox) {
    LayoutBox(aState, mVScrollbarBox, vRect);
    mVScrollbarBox->GetPrefSize(aState, vSize);
    mVScrollbarBox->GetMinSize(aState, vMinSize);
  }

  if (mHasVerticalScrollbar && (vMinSize.width > vRect.width || vMinSize.height > vRect.height)) {
    if (RemoveVerticalScrollbar(aState, scrollAreaRect, scrollBarRight)) {
        needsLayout = PR_TRUE;
        SetAttribute(mVScrollbarBox, nsXULAtoms::curpos, 0);
    }


    mVScrollbarBox->GetPrefSize(aState, vSize);
  }

  // layout horizontal scrollbar
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

  if (mHasHorizontalScrollbar) {
    SetAttribute(mHScrollbarBox, nsXULAtoms::maxpos, maxX);
    SetAttribute(mHScrollbarBox, nsXULAtoms::pageincrement, nscoord(float(scrollAreaRect.width)*0.8));
    SetAttribute(mHScrollbarBox, nsXULAtoms::increment, 10*mOnePixel, PR_FALSE);
  } 

  if (mHScrollbarBox) {
    LayoutBox(aState, mHScrollbarBox, hRect);
    mHScrollbarBox->GetMinSize(aState, hMinSize);
  }

  if (mHasHorizontalScrollbar && (hMinSize.width > hRect.width || hMinSize.height > hRect.height)) {
    if (RemoveHorizontalScrollbar(aState, scrollAreaRect, scrollBarBottom)) {
      needsLayout = PR_TRUE;
      SetAttribute(mHScrollbarBox, nsXULAtoms::curpos, 0);
    }
  } 

  // we only need to set the rect. The inner child stays the same size.
  if (needsLayout) {
     nsBoxLayoutState resizeState(aState);
     resizeState.SetLayoutReason(nsBoxLayoutState::Resize);
     LayoutBox(resizeState, mScrollAreaBox, scrollAreaRect); 
     needsLayout = PR_FALSE;
  }

  // may need to update fixed position children of the viewport,
  // if the client area changed size because of some dirty reflow
  // (if the reflow is initial or resize, the fixed children will
  // be re-laid out anyway)
  if ((oldScrollAreaBounds.width != scrollAreaRect.width
      || oldScrollAreaBounds.height != scrollAreaRect.height)
      && nsBoxLayoutState::Dirty == aState.GetLayoutReason()) {
    nsIFrame* parentFrame;
    mOuter->GetParent(&parentFrame);
    if (parentFrame) {
      nsCOMPtr<nsIAtom> parentFrameType;
      parentFrame->GetFrameType(getter_AddRefs(parentFrameType));
      if (parentFrameType.get() == nsLayoutAtoms::viewportFrame) {
        // Usually there are no fixed children, so don't do anything unless there's
        // at least one fixed child
        nsIFrame* child;
        if (NS_SUCCEEDED(parentFrame->FirstChild(mOuter->mPresContext,
          nsLayoutAtoms::fixedList, &child)) && child) {
          nsCOMPtr<nsIPresShell> presShell;
          mOuter->mPresContext->GetShell(getter_AddRefs(presShell));

          // force a reflow of the fixed children
          nsFrame::CreateAndPostReflowCommand(presShell, parentFrame,
            nsIReflowCommand::UserDefined, nsnull, nsnull, nsLayoutAtoms::fixedList);
        }
      }
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
      nsAutoString newValue;
      newValue.AppendInt(aSize);
      content->SetAttr(kNameSpaceID_None, aAtom, newValue, aReflow);
      return PR_TRUE;
  }

  return PR_FALSE;
}

/**
 * Gets the size of the area that lies inside the scrollbars but clips the scrolled frame
 */
NS_IMETHODIMP
nsGfxScrollFrameInner::GetScrolledSize(nsIPresContext* aPresContext, 
                              nscoord *aWidth, 
                              nscoord *aHeight) const
{

  // our scrolled size is the size of our scrolled view.
  nsSize size;
  nsIBox* child = nsnull;
  mScrollAreaBox->GetChildBox(&child);
  nsIFrame* frame;
  child->GetFrame(&frame);
  nsIView* view;
  frame->GetView(aPresContext, &view);
  NS_ASSERTION(view,"Scrolled frame must have a view!!!");
  
  nsRect rect(0,0,0,0);
  view->GetBounds(rect);

  size.width = rect.width;
  size.height = rect.height;

  nsBox::AddMargin(child, size);
  nsBox::AddBorderAndPadding(mScrollAreaBox, size);
  nsBox::AddInset(mScrollAreaBox, size);

  *aWidth = size.width;
  *aHeight = size.height;

  return NS_OK;
}

void
nsGfxScrollFrameInner::SetScrollbarVisibility(nsIBox* aScrollbar, PRBool aVisible)
{
  nsIFrame* frame = nsnull;
  aScrollbar->GetFrame(&frame);

  nsCOMPtr<nsIContent> content;
  frame->GetContent(getter_AddRefs(content));
  
  PRBool old = PR_TRUE;

  nsAutoString value;

  if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttr(kNameSpaceID_None, nsXULAtoms::collapsed, value))
    old = PR_FALSE;
  
  if (aVisible == old)
    return;

  if (!aVisible)
     content->SetAttr(kNameSpaceID_None, nsXULAtoms::collapsed, NS_LITERAL_STRING("true"), PR_TRUE);
  else
     content->UnsetAttr(kNameSpaceID_None, nsXULAtoms::collapsed, PR_TRUE);

  nsCOMPtr<nsIScrollbarFrame> scrollbar(do_QueryInterface(aScrollbar));
  if (scrollbar) {
    // See if we have a mediator.
    nsCOMPtr<nsIScrollbarMediator> mediator;
    scrollbar->GetScrollbarMediator(getter_AddRefs(mediator));
    if (mediator) {
      // Inform the mediator of the visibility change.
      mediator->VisibilityChanged(aVisible);
    }
  }
}

PRInt32
nsGfxScrollFrameInner::GetIntegerAttribute(nsIBox* aBox, nsIAtom* atom, PRInt32 defaultValue)
{
    nsIFrame* frame = nsnull;
    aBox->GetFrame(&frame);

    nsCOMPtr<nsIContent> content;
    frame->GetContent(getter_AddRefs(content));

    nsAutoString value;
    if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttr(kNameSpaceID_None, atom, value))
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
