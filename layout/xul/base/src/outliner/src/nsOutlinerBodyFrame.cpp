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
#include "nsISupportsArray.h"
#include "nsIPresContext.h"
#include "nsINameSpaceManager.h"
#include "nsIScrollbarFrame.h"

#include "nsOutlinerBodyFrame.h"
#include "nsXULAtoms.h"
#include "nsHTMLAtoms.h"

#include "nsIContent.h"
#include "nsIStyleContext.h"
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsIContent.h"
#include "nsICSSStyleRule.h"
#include "nsCSSRendering.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"
#include "nsIDeviceContext.h"
#include "nsXPIDLString.h"
#include "nsHTMLContainerFrame.h"
#include "nsIView.h"
#include "nsWidgetsCID.h"
#include "nsBoxFrame.h"

// The style context cache impl
nsresult 
nsOutlinerStyleCache::GetStyleContext(nsICSSPseudoComparator* aComparator,
                                      nsIPresContext* aPresContext, nsIContent* aContent, 
                                      nsIStyleContext* aContext, nsIAtom* aPseudoElement,
                                      nsISupportsArray* aInputWord,
                                      nsIStyleContext** aResult)
{
  *aResult = nsnull;
  
  PRUint32 count;
  aInputWord->Count(&count);
  nsDFAState startState(0);
  nsDFAState* currState = &startState;

  // Go ahead and init the transition table.
  if (!mTransitionTable) {
    // Automatic miss. Build the table
    mTransitionTable = new nsHashtable;
  }

  // The first transition is always made off the supplied pseudo-element.
  nsTransitionKey key(currState->GetStateID(), aPseudoElement);
  currState = NS_STATIC_CAST(nsDFAState*, mTransitionTable->Get(&key));

  if (!currState) {
    // We had a miss. Make a new state and add it to our hash.
    mNextState++;
    currState = new nsDFAState(mNextState);
    mTransitionTable->Put(&key, currState);
  }

  for (PRUint32 i = 0; i < count; i++)
  {
    nsCOMPtr<nsIAtom> pseudo = getter_AddRefs(NS_STATIC_CAST(nsIAtom*, aInputWord->ElementAt(i)));
    nsTransitionKey key(currState->GetStateID(), pseudo);
    currState = NS_STATIC_CAST(nsDFAState*, mTransitionTable->Get(&key));

    if (!currState) {
      // We had a miss. Make a new state and add it to our hash.
      currState = new nsDFAState(mNextState);
      mNextState++;
      mTransitionTable->Put(&key, currState);
    }
  }

  // We're in a final state.
  // Look up our style context for this state.
   if (mCache)
    *aResult = NS_STATIC_CAST(nsIStyleContext*, mCache->Get(currState)); // Addref occurs on *aResult.
  if (!*aResult) {
    // We missed the cache. Resolve this pseudo-style.
    aPresContext->ResolvePseudoStyleWithComparator(aContent, aPseudoElement,
                                                   aContext, PR_FALSE,
                                                   aComparator,
                                                   aResult); // Addref occurs on *aResult.
    // Put it in our table.
    if (!mCache)
      mCache = new nsSupportsHashtable;
    mCache->Put(currState, *aResult);
  }

  return NS_OK;
}

// Column class that caches all the info about our column.
nsOutlinerColumn::nsOutlinerColumn(nsIContent* aColElement, nsIFrame* aFrame)
:mNext(nsnull)
{
  mColFrame = aFrame;
  mColElement = aColElement; 

  // Fetch the ID.
  mColElement->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::id, mID);

  // Fetch the crop style.
  mCropStyle = 0;
  nsAutoString crop;
  mColElement->GetAttribute(kNameSpaceID_None, nsXULAtoms::crop, crop);
  if (crop.EqualsIgnoreCase("middle"))
    mCropStyle = 1;
  else if (crop.EqualsIgnoreCase("right"))
    mCropStyle = 2;

  // Cache our text alignment policy.
  nsCOMPtr<nsIStyleContext> styleContext;
  aFrame->GetStyleContext(getter_AddRefs(styleContext));

  const nsStyleText* textStyle =
        (const nsStyleText*)styleContext->GetStyleData(eStyleStruct_Text);

  mTextAlignment = textStyle->mTextAlign;

  // Figure out if we're the primary column (that has to have indentation
  // and twisties drawn.
  mIsPrimaryCol = PR_FALSE;
  nsAutoString primary;
  mColElement->GetAttribute(kNameSpaceID_None, nsXULAtoms::primary, primary);
  if (primary.EqualsIgnoreCase("true"))
    mIsPrimaryCol = PR_TRUE;

  // Figure out if we're a cycling column (one that doesn't cause a selection
  // to happen).
  mIsCyclerCol = PR_FALSE;
  nsAutoString cycler;
  mColElement->GetAttribute(kNameSpaceID_None, nsXULAtoms::cycler, cycler);
  if (cycler.EqualsIgnoreCase("true"))
    mIsCyclerCol = PR_TRUE;
}

inline nscoord nsOutlinerColumn::GetWidth()
{
  if (mColFrame) {
    nsRect rect;
    mColFrame->GetRect(rect);
    return rect.width;
  }
  return 0;
}

//
// NS_NewOutlinerFrame
//
// Creates a new outliner frame
//
nsresult
NS_NewOutlinerBodyFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsOutlinerBodyFrame* it = new (aPresShell) nsOutlinerBodyFrame(aPresShell);
  if (!it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewOutlinerFrame


// Constructor
nsOutlinerBodyFrame::nsOutlinerBodyFrame(nsIPresShell* aPresShell)
:nsLeafBoxFrame(aPresShell), mPresContext(nsnull),
 mTopRowIndex(0), mColumns(nsnull), mScrollbar(nsnull)
{
  NS_NewISupportsArray(getter_AddRefs(mScratchArray));
}

// Destructor
nsOutlinerBodyFrame::~nsOutlinerBodyFrame()
{
}

NS_IMETHODIMP_(nsrefcnt) 
nsOutlinerBodyFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt)
nsOutlinerBodyFrame::Release(void)
{
  return NS_OK;
}

static nsIFrame* InitScrollbarFrame(nsIPresContext* aPresContext, nsIFrame* aCurrFrame, nsIScrollbarMediator* aSM)
{
  // Check ourselves
  nsCOMPtr<nsIScrollbarFrame> sf(do_QueryInterface(aCurrFrame));
  if (sf) {
    sf->SetScrollbarMediator(aSM);
    return aCurrFrame;
  }

  nsIFrame* child;
  aCurrFrame->FirstChild(aPresContext, nsnull, &child);
  while (child) {
    nsIFrame* result = InitScrollbarFrame(aPresContext, child, aSM);
    if (result)
      return result;
    child->GetNextSibling(&child);
  }

  return nsnull;
}

NS_IMETHODIMP
nsOutlinerBodyFrame::Init(nsIPresContext* aPresContext, nsIContent* aContent,
                          nsIFrame* aParent, nsIStyleContext* aContext, nsIFrame* aPrevInFlow)
{
  mPresContext = aPresContext;
  nsresult rv = nsLeafBoxFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
  nsBoxFrame::CreateViewForFrame(aPresContext, this, aContext, PR_TRUE);
  nsIView* ourView;
  nsLeafBoxFrame::GetView(aPresContext, &ourView);

static NS_DEFINE_IID(kWidgetCID, NS_CHILD_CID);

  ourView->CreateWidget(kWidgetCID);
  ourView->GetWidget(*getter_AddRefs(mOutlinerWidget));

  return rv;
}

NS_IMETHODIMP
nsOutlinerBodyFrame::Destroy(nsIPresContext* aPresContext)
{
  // Delete our column structures.
  delete mColumns;
  mColumns = nsnull;

  // Drop our ref to the view.
  mView->SetOutliner(nsnull);
  mView = nsnull;

  return nsLeafBoxFrame::Destroy(aPresContext);
}

NS_IMETHODIMP nsOutlinerBodyFrame::GetView(nsIOutlinerView * *aView)
{
  *aView = mView;
  NS_IF_ADDREF(*aView);
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::SetView(nsIOutlinerView * aView)
{
  // Outliner, meet the view.
  mView = aView;
  
  if (mView)
    // View, meet the outliner.
    mView->SetOutliner(this);

  // Changing the view causes us to refetch our data.  This will
  // necessarily entail a full invalidation of the outliner.
  mTopRowIndex = 0;
  delete mColumns;
  mColumns = nsnull;
  Invalidate();
  
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::GetIndexOfVisibleRow(PRInt32 *_retval)
{
  *_retval = mTopRowIndex;
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::GetPageCount(PRInt32 *_retval)
{
  *_retval = mPageCount;
  return NS_OK;
}


NS_IMETHODIMP nsOutlinerBodyFrame::Invalidate()
{
  nsLeafBoxFrame::Invalidate(mPresContext, mRect, PR_FALSE);
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::InvalidateRow(PRInt32 aIndex)
{
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::InvalidateCell(PRInt32 aRow, const PRUnichar *aColID)
{
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::InvalidateRange(PRInt32 aStart, PRInt32 aEnd)
{
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::InvalidateScrollbar()
{
  if (!mScrollbar)
    // Try to find it.
    mScrollbar = InitScrollbarFrame(mPresContext, mParent, this);

  if (!mScrollbar || !mView)
    return NS_OK;

  PRInt32 rowCount = 0;
  mView->GetRowCount(&rowCount);
  
  nsCOMPtr<nsIContent> scrollbar;
  mScrollbar->GetContent(getter_AddRefs(scrollbar));

  nsAutoString maxposStr;

  float t2p;
  mPresContext->GetTwipsToPixels(&t2p);
  nscoord rowHeightAsPixels = NSToCoordRound((float)mRowHeight*t2p);

  PRInt32 size = rowHeightAsPixels*(rowCount-mPageCount+1);
  maxposStr.AppendInt(size);
  scrollbar->SetAttribute(kNameSpaceID_None, nsXULAtoms::maxpos, maxposStr, PR_TRUE);

  // Also set our page increment and decrement.
  nscoord pageincrement = mPageCount*rowHeightAsPixels;
  nsAutoString pageStr;
  pageStr.AppendInt(pageincrement);
  scrollbar->SetAttribute(kNameSpaceID_None, nsXULAtoms::pageincrement, pageStr, PR_TRUE);

  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::GetCellAt(PRInt32 x, PRInt32 y, PRInt32 *row, PRUnichar **colID)
{
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::RowsAppended(PRInt32 count)
{
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::RowsInserted(PRInt32 index, PRInt32 count)
{
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::RowsRemoved(PRInt32 index, PRInt32 count)
{
  return NS_OK;
}

PRInt32 nsOutlinerBodyFrame::GetRowHeight(nsIPresContext* aPresContext)
{
  // Look up the correct height.  It is equal to the specified height
  // + the specified margins.
  nsCOMPtr<nsIStyleContext> rowContext;
  mScratchArray->Clear();
  GetPseudoStyleContext(aPresContext, nsXULAtoms::mozoutlinerrow, getter_AddRefs(rowContext));
  if (rowContext) {
    const nsStylePosition* myPosition = (const nsStylePosition*)
          rowContext->GetStyleData(eStyleStruct_Position);
    if (myPosition->mHeight.GetUnit() == eStyleUnit_Coord)  {
      PRInt32 val = myPosition->mHeight.GetCoordValue();
      if (val > 0) {
        // XXX Check box-sizing to determine if border/padding should augment the height
        // Inflate the height by our margins.
        nsRect rowRect(0,0,0,val);
        const nsStyleMargin* rowMarginData = (const nsStyleMargin*)rowContext->GetStyleData(eStyleStruct_Margin);
        nsMargin rowMargin;
        rowMarginData->GetMargin(rowMargin);
        rowRect.Inflate(rowMargin);
        val = rowRect.height;
      }
      return val;
    }
  }
  return 16; // As good a default as any.
}

nsRect nsOutlinerBodyFrame::GetInnerBox()
{
  nsRect r(0,0,mRect.width, mRect.height);
  nsMargin m(0,0,0,0);
  nsStyleBorderPadding  bPad;
  mStyleContext->GetStyle(eStyleStruct_BorderPaddingShortcut, (nsStyleStruct&)bPad);
  bPad.GetBorderPadding(m);
  r.Deflate(m);
  return r;
}

// Painting routines
NS_IMETHODIMP nsOutlinerBodyFrame::Paint(nsIPresContext*      aPresContext,
                                         nsIRenderingContext& aRenderingContext,
                                         const nsRect&        aDirtyRect,
                                         nsFramePaintLayer    aWhichLayer)
{
  // XXX This trap handles an odd bogus 1 pixel invalidation that we keep getting 
  // when scrolling.
  if (aDirtyRect.width == 1)
    return NS_OK;

  const nsStyleDisplay* disp = (const nsStyleDisplay*)
      mStyleContext->GetStyleData(eStyleStruct_Display);
  if (!disp->IsVisibleOrCollapsed())
    return NS_OK; // We're invisible.  Don't paint.

  // Handles painting our background, border, and outline.
  nsresult rv = nsLeafFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
  if (NS_FAILED(rv)) return rv;

  if (!mView)
    return NS_OK;

  PRBool clipState = PR_FALSE;
  nsRect clipRect(mRect);
  aRenderingContext.PushState();
  aRenderingContext.SetClipRect(clipRect, nsClipCombine_kReplace, clipState);

  // Update our page count, our available height and our row height.
  PRInt32 oldRowHeight = mRowHeight;
  mRowHeight = GetRowHeight(aPresContext);
  mInnerBox = GetInnerBox();
  mPageCount = mInnerBox.height/mRowHeight;

  if (mRowHeight != oldRowHeight)
    InvalidateScrollbar();

  PRInt32 rowCount = 0;
  mView->GetRowCount(&rowCount);
  
  // Ensure our column info is built.
  EnsureColumns(aPresContext);

  // Loop through our on-screen rows.
  for (PRInt32 i = mTopRowIndex; i < rowCount && i < mTopRowIndex+mPageCount+1; i++) {
    nsRect rowRect(0, mRowHeight*(i-mTopRowIndex), mInnerBox.width, mRowHeight);
    nsRect dirtyRect;
    if (dirtyRect.IntersectRect(aDirtyRect, rowRect)) {
      PaintRow(i, rowRect, aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
    }
  }

  aRenderingContext.PopState(clipState);
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::PaintRow(int aRowIndex, const nsRect& aRowRect,
                                            nsIPresContext*      aPresContext,
                                            nsIRenderingContext& aRenderingContext,
                                            const nsRect&        aDirtyRect,
                                            nsFramePaintLayer    aWhichLayer)
{
  // We have been given a rect for our row.  We treat this row like a full-blown
  // frame, meaning that it can have borders, margins, padding, and a background.
  
  // Without a view, we have no data. Check for this up front.
  if (!mView)
    return NS_OK;

  // Now obtain the properties for our row.
  // XXX Automatically fill in the following props: open, container, selected, focused
  mScratchArray->Clear();
  mView->GetRowProperties(aRowIndex, mScratchArray);

  // Resolve style for the row.  It contains all the info we need to lay ourselves
  // out and to paint.
  nsCOMPtr<nsIStyleContext> rowContext;
  GetPseudoStyleContext(aPresContext, nsXULAtoms::mozoutlinerrow, getter_AddRefs(rowContext));

  // Obtain the margins for the row and then deflate our rect by that 
  // amount.  The row is assumed to be contained within the deflated rect.
  nsRect rowRect(aRowRect);
  const nsStyleMargin* rowMarginData = (const nsStyleMargin*)rowContext->GetStyleData(eStyleStruct_Margin);
  nsMargin rowMargin;
  rowMarginData->GetMargin(rowMargin);
  rowRect.Deflate(rowMargin);

  // If the layer is the background layer, we must paint our borders and background for our
  // row rect.
  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer)
    PaintBackgroundLayer(rowContext, aPresContext, aRenderingContext, rowRect, aDirtyRect);

  // Now loop over our cells. Only paint a cell if it intersects with our dirty rect.
  nscoord currX = rowRect.x;
  for (nsOutlinerColumn* currCol = mColumns; currCol; currCol = currCol->GetNext()) {
    nsRect cellRect(currX, rowRect.y, currCol->GetWidth(), rowRect.height);
    nsRect dirtyRect;
    if (dirtyRect.IntersectRect(aDirtyRect, cellRect)) {
      PaintCell(aRowIndex, currCol, cellRect, aPresContext, aRenderingContext, aDirtyRect, aWhichLayer); 
    }
    currX += currCol->GetWidth();
  }

  return NS_OK;
}
  
NS_IMETHODIMP nsOutlinerBodyFrame::PaintCell(int aRowIndex, 
                                             nsOutlinerColumn*    aColumn,
                                             const nsRect& aCellRect,
                                             nsIPresContext*      aPresContext,
                                             nsIRenderingContext& aRenderingContext,
                                             const nsRect&        aDirtyRect,
                                             nsFramePaintLayer    aWhichLayer)
{
  // Now obtain the properties for our cell.
  // XXX Automatically fill in the following props: open, container, selected, focused, and the col ID.
  mScratchArray->Clear();
  mView->GetCellProperties(aRowIndex, aColumn->GetID(), mScratchArray);

  // Resolve style for the cell.  It contains all the info we need to lay ourselves
  // out and to paint.
  nsCOMPtr<nsIStyleContext> cellContext;
  GetPseudoStyleContext(aPresContext, nsXULAtoms::mozoutlinercell, getter_AddRefs(cellContext));

  // Obtain the margins for the cell and then deflate our rect by that 
  // amount.  The cell is assumed to be contained within the deflated rect.
  nsRect cellRect(aCellRect);
  const nsStyleMargin* cellMarginData = (const nsStyleMargin*)cellContext->GetStyleData(eStyleStruct_Margin);
  nsMargin cellMargin;
  cellMarginData->GetMargin(cellMargin);
  cellRect.Deflate(cellMargin);

  // If the layer is the background layer, we must paint our borders and background for our
  // row rect.
  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer)
    PaintBackgroundLayer(cellContext, aPresContext, aRenderingContext, cellRect, aDirtyRect);

  nscoord currX = cellRect.x;
  nscoord remainingWidth = cellRect.width;

  // Now we paint the contents of the cells.
  // Text alignment determines the order in which we paint.  
  // LEFT means paint from left to right.
  // RIGHT means paint from right to left.
  // XXX Implement RIGHT alignment!

  // XXX If we're the primary column, we need to indent and paint the twisty.

  // XXX Now paint the various images.

  // Now paint our text.
  nsRect textRect(currX, cellRect.y, remainingWidth, cellRect.height);
  nsRect dirtyRect;
  if (dirtyRect.IntersectRect(aDirtyRect, textRect))
    PaintText(aRowIndex, aColumn, textRect, aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerBodyFrame::PaintText(int aRowIndex, 
                                             nsOutlinerColumn*    aColumn,
                                             const nsRect& aTextRect,
                                             nsIPresContext*      aPresContext,
                                             nsIRenderingContext& aRenderingContext,
                                             const nsRect&        aDirtyRect,
                                             nsFramePaintLayer    aWhichLayer)
{
  // Now obtain the text for our cell.
  nsXPIDLString text;
  mView->GetCellText(aRowIndex, aColumn->GetID(), getter_Copies(text));

  nsAutoString realText(text);

  // Resolve style for the text.  It contains all the info we need to lay ourselves
  // out and to paint.
  nsCOMPtr<nsIStyleContext> textContext;
  GetPseudoStyleContext(aPresContext, nsXULAtoms::mozoutlinercelltext, getter_AddRefs(textContext));

  // Obtain the margins for the text and then deflate our rect by that 
  // amount.  The text is assumed to be contained within the deflated rect.
  nsRect textRect(aTextRect);
  const nsStyleMargin* textMarginData = (const nsStyleMargin*)textContext->GetStyleData(eStyleStruct_Margin);
  nsMargin textMargin;
  textMarginData->GetMargin(textMargin);
  textRect.Deflate(textMargin);

  // If the layer is the background layer, we must paint our borders and background for our
  // text rect.
  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer)
    PaintBackgroundLayer(textContext, aPresContext, aRenderingContext, textRect, aDirtyRect);
  else {
    // Time to paint our text. 
    
    // Compute our text size.
    const nsStyleFont* fontStyle = (const nsStyleFont*)textContext->GetStyleData(eStyleStruct_Font);

    nsCOMPtr<nsIDeviceContext> deviceContext;
    aPresContext->GetDeviceContext(getter_AddRefs(deviceContext));

    nsCOMPtr<nsIFontMetrics> fontMet;
    deviceContext->GetMetricsFor(fontStyle->mFont, *getter_AddRefs(fontMet));
    nscoord height;
    fontMet->GetHeight(height);

    // Center the text. XXX Obey vertical-align style prop?
    if (height < textRect.height) {
      textRect.y += (textRect.height - height)/2;
      textRect.height = height;
    }

    // XXX Crop if the width is too big!
    nscoord width;
    aRenderingContext.GetWidth(realText, width);

    // Set our font.
    aRenderingContext.SetFont(fontMet);

    // Set our color.
    const nsStyleColor* colorStyle = (const nsStyleColor*)textContext->GetStyleData(eStyleStruct_Color);
    aRenderingContext.SetColor(colorStyle->mColor);

    aRenderingContext.DrawString(realText, textRect.x, textRect.y);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerBodyFrame::PaintBackgroundLayer(nsIStyleContext* aStyleContext, nsIPresContext* aPresContext, 
                                          nsIRenderingContext& aRenderingContext, 
                                          const nsRect& aRect, const nsRect& aDirtyRect)
{

  const nsStyleDisplay* disp = (const nsStyleDisplay*)
      aStyleContext->GetStyleData(eStyleStruct_Display);
  const nsStyleColor* myColor = (const nsStyleColor*)
      aStyleContext->GetStyleData(eStyleStruct_Color);
  const nsStyleBorder* myBorder = (const nsStyleBorder*)
      aStyleContext->GetStyleData(eStyleStruct_Border);
  const nsStyleOutline* myOutline = (const nsStyleOutline*)
      aStyleContext->GetStyleData(eStyleStruct_Outline);
  
  nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                  aDirtyRect, aRect, *myColor, *myBorder, 0, 0);

  nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                              aDirtyRect, aRect, *myBorder, mStyleContext, 0);

  nsCSSRendering::PaintOutline(aPresContext, aRenderingContext, this,
                              aDirtyRect, aRect, *myBorder, *myOutline, aStyleContext, 0);

  return NS_OK;
}

// Scrolling
NS_IMETHODIMP nsOutlinerBodyFrame::ScrollToRow(PRInt32 aRow)
{
  if (!mView)
    return NS_OK;

  PRInt32 rowCount;
  mView->GetRowCount(&rowCount);

  PRInt32 delta = aRow - mTopRowIndex;

  if (delta > 0) {
    if (mTopRowIndex == (rowCount - mPageCount + 1))
      return NS_OK;
  }
  else {
    if (mTopRowIndex == 0)
      return NS_OK;
  }

  mTopRowIndex += delta;

  float t2p;
  mPresContext->GetTwipsToPixels(&t2p);
  nscoord rowHeightAsPixels = NSToCoordRound((float)mRowHeight*t2p);

  PRInt32 absDelta = delta > 0 ? delta : -delta;
  if (absDelta*mRowHeight >= mRect.height)
    Invalidate();
  else if (mOutlinerWidget)
    mOutlinerWidget->Scroll(0, delta > 0 ? -delta*rowHeightAsPixels : delta*rowHeightAsPixels, nsnull);
 
  return NS_OK;
}

NS_IMETHODIMP
nsOutlinerBodyFrame::ScrollbarButtonPressed(PRInt32 aOldIndex, PRInt32 aNewIndex)
{
  if (aNewIndex > aOldIndex)
    ScrollToRow(mTopRowIndex+1);
  else ScrollToRow(mTopRowIndex-1);

  // Update the scrollbar.
  nsCOMPtr<nsIContent> scrollbarContent;
  mScrollbar->GetContent(getter_AddRefs(scrollbarContent));
  float t2p;
  mPresContext->GetTwipsToPixels(&t2p);
  nscoord rowHeightAsPixels = NSToCoordRound((float)mRowHeight*t2p);

  nsAutoString curPos;
  curPos.AppendInt(mTopRowIndex*rowHeightAsPixels);
  scrollbarContent->SetAttribute(kNameSpaceID_None, nsXULAtoms::curpos, curPos, PR_TRUE);

  return NS_OK;
}
  
NS_IMETHODIMP
nsOutlinerBodyFrame::PositionChanged(PRInt32 aOldIndex, PRInt32& aNewIndex)
{
  float t2p;
  mPresContext->GetTwipsToPixels(&t2p);
  nscoord rh = NSToCoordRound((float)mRowHeight*t2p);

  nscoord oldrow = aOldIndex/rh;
  nscoord newrow = aNewIndex/rh;

  if (oldrow != newrow)
    ScrollToRow(newrow);

  // Go exactly where we're supposed to
  // Update the scrollbar.
  nsCOMPtr<nsIContent> scrollbarContent;
  mScrollbar->GetContent(getter_AddRefs(scrollbarContent));
  nsAutoString curPos;
  curPos.AppendInt(aNewIndex);
  scrollbarContent->SetAttribute(kNameSpaceID_None, nsXULAtoms::curpos, curPos, PR_TRUE);

  return NS_OK;
}

// The style cache.
nsresult 
nsOutlinerBodyFrame::GetPseudoStyleContext(nsIPresContext* aPresContext, nsIAtom* aPseudoElement, 
                                           nsIStyleContext** aResult)
{
  return mStyleCache.GetStyleContext(this, aPresContext, mContent, mStyleContext, aPseudoElement,
                                     mScratchArray, aResult);
}

// Our comparator for resolving our complex pseudos
NS_IMETHODIMP
nsOutlinerBodyFrame::PseudoMatches(nsIAtom* aTag, nsCSSSelector* aSelector, PRBool* aResult)
{
  if (aSelector->mTag == aTag) {
    // Iterate the pseudoclass list.  For each item in the list, see if
    // it is contained in our scratch array.  If we have a miss, then
    // we aren't a match.  If all items in the pseudoclass list are
    // present in the scratch array, then we have a match.
    nsAtomList* curr = aSelector->mPseudoClassList;
    while (curr) {
      PRInt32 index;
      mScratchArray->GetIndexOf(curr->mAtom, &index);
      if (index == -1) {
        *aResult = PR_FALSE;
        break;
      }
      curr = curr->mNext;
    }
    *aResult = PR_TRUE;
  }
  else 
    *aResult = PR_FALSE;

  return NS_OK;
}

void
nsOutlinerBodyFrame::EnsureColumns(nsIPresContext* aPresContext)
{
  if (!mColumns) {
    nsCOMPtr<nsIContent> parent;
    mContent->GetParent(*getter_AddRefs(parent));
    nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(parent));

    nsCOMPtr<nsIDOMNodeList> cols;
    elt->GetElementsByTagName(NS_LITERAL_STRING("outlinercol"), getter_AddRefs(cols));

    nsCOMPtr<nsIPresShell> shell; 
    aPresContext->GetShell(getter_AddRefs(shell));

    PRUint32 count;
    cols->GetLength(&count);

    nsOutlinerColumn* currCol = nsnull;
    for (PRUint32 i = 0; i < count; i++) {
      nsCOMPtr<nsIDOMNode> node;
      cols->Item(i, getter_AddRefs(node));
      nsCOMPtr<nsIContent> child(do_QueryInterface(node));
      
      // Get the frame for this column.
      nsIFrame* frame;
      shell->GetPrimaryFrameFor(child, &frame);
      
      // Create a new column structure.
      nsOutlinerColumn* col = new nsOutlinerColumn(child, frame);
      if (currCol)
        currCol->SetNext(col);
      else mColumns = col;
      currCol = col;
    }
  }
}

//
// QueryInterface
//
NS_INTERFACE_MAP_BEGIN(nsOutlinerBodyFrame)
  NS_INTERFACE_MAP_ENTRY(nsIOutlinerBoxObject)
  NS_INTERFACE_MAP_ENTRY(nsICSSPseudoComparator)
  NS_INTERFACE_MAP_ENTRY(nsIScrollbarMediator)
NS_INTERFACE_MAP_END_INHERITING(nsLeafFrame)