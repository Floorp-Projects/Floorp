
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
#include "nsFrameSetFrame.h"
#include "nsIHTMLContent.h"
#include "nsLeafFrame.h"
#include "nsHTMLContainerFrame.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsHTMLIIDs.h"
#include "nsIComponentManager.h"
#include "nsIStreamListener.h"
#include "nsIURL.h"
#include "nsIDocument.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsWidgetsCID.h"
#include "nsViewsCID.h"
#include "nsHTMLAtoms.h"
#include "nsIScrollableView.h"
#include "nsStyleCoord.h"
#include "nsStyleConsts.h"
#include "nsIStyleContext.h"
#include "nsIDocumentLoader.h"
#include "nsHTMLParts.h"
#include "nsILookAndFeel.h"
#include "nsWidgetsCID.h"
#include "nsIComponentManager.h"
#include "nsGUIEvent.h"
#include "nsIRenderingContext.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefBranchInternal.h"
#include "nsIServiceManager.h"
#include "nsIDOMMutationEvent.h"
#include "nsINameSpaceManager.h"

// masks for mEdgeVisibility
#define LEFT_VIS   0x0001
#define RIGHT_VIS  0x0002
#define TOP_VIS    0x0004
#define BOTTOM_VIS 0x0008
#define ALL_VIS    0x000F
#define NONE_VIS   0x0000

static NS_DEFINE_IID(kIFramesetFrameIID, NS_IFRAMESETFRAME_IID);
static NS_DEFINE_IID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);

/*******************************************************************************
 * nsFramesetDrag
 ******************************************************************************/
nsFramesetDrag::nsFramesetDrag() 
{
  UnSet();
}

nsFramesetDrag::nsFramesetDrag(PRBool               aVertical, 
                               PRInt32              aIndex, 
                               PRInt32              aChange, 
                               nsHTMLFramesetFrame* aSource) 
{
  mVertical = aVertical;
  mIndex    = aIndex;
  mChange   = aChange; 
  mSource   = aSource;
}
void nsFramesetDrag::Reset(PRBool               aVertical, 
                           PRInt32              aIndex, 
                           PRInt32              aChange, 
                           nsHTMLFramesetFrame* aSource) 
{
  mVertical = aVertical;
  mIndex    = aIndex;
  mChange   = aChange;
  mSource   = aSource;
  mActive   = PR_TRUE;
}

void nsFramesetDrag::UnSet()
{
  mVertical = PR_TRUE;
  mIndex    = -1;
  mChange   = 0;
  mSource   = nsnull;
  mActive   = PR_FALSE;
}

/*******************************************************************************
 * nsHTMLFramesetBorderFrame
 ******************************************************************************/
class nsHTMLFramesetBorderFrame : public nsLeafFrame {

public:
#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif

  NS_IMETHOD HandleEvent(nsIPresContext* aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus* aEventStatus);

  NS_IMETHOD GetFrameForPoint(nsIPresContext* aPresContext,
                              const nsPoint& aPoint, 
                              nsFramePaintLayer aWhichLayer,
                              nsIFrame**     aFrame);

  NS_IMETHOD GetCursor(nsIPresContext* aPresContext,
                             nsPoint&        aPoint,
                             PRInt32&        aCursor);
  
  NS_IMETHOD Paint(nsIPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer,
                   PRUint32             aFlags);

  NS_IMETHOD Reflow(nsIPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  PRBool GetVisibility() { return mVisibility || mVisibilityOverride; }
  void SetVisibility(PRBool aVisibility);
  void SetColor(nscolor aColor);

protected:
  nsHTMLFramesetBorderFrame(PRInt32 aWidth, PRBool aVertical, PRBool aVisible);
  virtual ~nsHTMLFramesetBorderFrame();
  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredSize);
  PRInt32 mWidth;
  PRPackedBool mVertical;
  PRPackedBool mVisibility;
  PRPackedBool mVisibilityOverride;
  nscolor mColor;
  // the prev and next neighbors are indexes into the row (for a horizontal border) or col (for
  // a vertical border) of nsHTMLFramesetFrames or nsHTMLFrames
  PRInt32 mPrevNeighbor; 
  PRInt32 mNextNeighbor;
  PRBool mCanResize;
  friend class nsHTMLFramesetFrame;
};
/*******************************************************************************
 * nsHTMLFramesetBlankFrame
 ******************************************************************************/
class nsHTMLFramesetBlankFrame : public nsLeafFrame {

public:
#ifdef DEBUG
  NS_IMETHOD List(nsIPresContext* aPresContext, FILE* out = stdout, PRInt32 aIndent = 0) const;
#endif

  NS_IMETHOD Paint(nsIPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer,
                   PRUint32             aFlags);

  NS_IMETHOD Reflow(nsIPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

protected:
  virtual ~nsHTMLFramesetBlankFrame();
  virtual void GetDesiredSize(nsIPresContext*          aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics&     aDesiredSize);
  friend class nsHTMLFramesetFrame;
  friend class nsHTMLFrameset;
};

/*******************************************************************************
 * nsHTMLFramesetFrame
 ******************************************************************************/
PRInt32 nsHTMLFramesetFrame::gMaxNumRowColSpecs = 25;
PRBool  nsHTMLFramesetFrame::gDragInProgress = PR_FALSE;
#define kFrameResizePref "layout.frames.force_resizability"
#define DEFAULT_BORDER_WIDTH_PX 6

nsHTMLFramesetFrame::nsHTMLFramesetFrame()
  : nsHTMLContainerFrame()
{
  mNumRows             = 0;
  mRowSpecs            = nsnull;
  mRowSizes            = nsnull;
  mNumCols             = 0;
  mColSpecs            = nsnull;
  mColSizes            = nsnull;
  mEdgeVisibility      = 0;
  mParentFrameborder   = eFrameborder_Yes; // default
  mParentBorderWidth   = -1; // default not set
  mParentBorderColor   = NO_COLOR; // default not set
  mFirstDragPoint.x     = mFirstDragPoint.y = 0;
  mMinDrag             = 0;
  mNonBorderChildCount = 0;
  mNonBlankChildCount  = 0;
  mDragger             = nsnull;
  mChildCount          = 0;
  mTopLevelFrameset    = nsnull;
  mEdgeColors.Set(NO_COLOR);
  mVerBorders          = nsnull;
  mHorBorders          = nsnull;
  mChildTypes          = nsnull;
  mChildFrameborder    = nsnull;
  mChildBorderColors   = nsnull;
  mForceFrameResizability = PR_FALSE;
  mPrefBranchWeakRef   = nsnull;
}

nsHTMLFramesetFrame::~nsHTMLFramesetFrame()
{
  if (mRowSizes) delete [] mRowSizes;
  if (mRowSpecs) delete [] mRowSpecs;
  if (mColSizes) delete [] mColSizes;
  if (mColSpecs) delete [] mColSpecs;
  if (mVerBorders) delete[] mVerBorders;
  if (mHorBorders) delete[] mHorBorders;
  mRowSizes = mColSizes = nsnull;
  mRowSpecs = mColSpecs = nsnull;
  nsCOMPtr<nsIPrefBranchInternal> prefBranch(do_QueryReferent(mPrefBranchWeakRef));
  if (prefBranch) {
    nsresult rv = prefBranch->RemoveObserver(kFrameResizePref, this);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Can't remove frameset as pref branch observer");
  }
  mPrefBranchWeakRef = nsnull;
}

nsresult nsHTMLFramesetFrame::QueryInterface(const nsIID& aIID, 
                                             void**       aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  } else if (aIID.Equals(kIFramesetFrameIID)) {
    *aInstancePtr = (void*)this;
    return NS_OK;
  } else if (aIID.Equals(NS_GET_IID(nsIObserver))) {
    *aInstancePtr = (void*)this;
    return NS_OK;
  } 

  return nsHTMLContainerFrame::QueryInterface(aIID, aInstancePtr);
}

nsrefcnt nsHTMLFramesetFrame::AddRef(void)
{
  return 0;
}

nsrefcnt nsHTMLFramesetFrame::Release(void)
{
  return 0;
}

NS_IMETHODIMP
nsHTMLFramesetFrame::Observe(nsISupports* aObject, const char* aAction,
                             const PRUnichar* aPrefName)
{
  nsAutoString prefName(aPrefName);
  if (prefName.Equals(NS_LITERAL_STRING(kFrameResizePref))) {
    nsCOMPtr<nsIDocument> doc;
    mContent->GetDocument(*getter_AddRefs(doc));
    if (doc) {
      doc->BeginUpdate();
      doc->AttributeWillChange(mContent,
                               kNameSpaceID_None,
                               nsHTMLAtoms::frameborder);
    }
    nsCOMPtr<nsIPrefBranch> prefBranch(do_QueryInterface(aObject));
    if (prefBranch) {
      prefBranch->GetBoolPref(kFrameResizePref, &mForceFrameResizability);
    }
    RecalculateBorderResize();
    mRect.width = mRect.height = -1;  // force a recalculation of frame sizes
    if (doc) {
      doc->AttributeChanged(mContent,
                            kNameSpaceID_None,
                            nsHTMLAtoms::frameborder,
                            nsIDOMMutationEvent::MODIFICATION,
                            NS_STYLE_HINT_REFLOW);
      doc->EndUpdate();
    }
  }
  return NS_OK;
}

static NS_DEFINE_IID(kViewCID, NS_VIEW_CID);

#define FRAMESET 0
#define FRAME 1
#define BLANK 2

NS_IMETHODIMP
nsHTMLFramesetFrame::Init(nsIPresContext*  aPresContext,
                          nsIContent*      aContent,
                          nsIFrame*        aParent,
                          nsIStyleContext* aContext,
                          nsIFrame*        aPrevInFlow)
{
  nsHTMLContainerFrame::Init(aPresContext, aContent, aParent,
                             aContext, aPrevInFlow);
  // find the highest ancestor that is a frameset
  nsresult rv = NS_OK;
  nsIFrame* parentFrame = nsnull;
  GetParent((nsIFrame**)&parentFrame);
  mTopLevelFrameset = (nsHTMLFramesetFrame*)this;
  while (parentFrame) {
    nsHTMLFramesetFrame* frameset;
    rv = parentFrame->QueryInterface(kIFramesetFrameIID, (void**)&frameset);
    if (NS_SUCCEEDED(rv)) {
      mTopLevelFrameset = frameset;
      parentFrame->GetParent((nsIFrame**)&parentFrame);
    } else {
      break;
    }
  }

  // create the view. a view is needed since it needs to be a mouse grabber
  nsIView* view;
  nsresult result = nsComponentManager::CreateInstance(kViewCID, nsnull, NS_GET_IID(nsIView),
                                                 (void **)&view);
  nsCOMPtr<nsIPresShell> presShell;
  aPresContext->GetShell(getter_AddRefs(presShell));
  nsCOMPtr<nsIViewManager> viewMan;
  presShell->GetViewManager(getter_AddRefs(viewMan));

  nsIFrame* parWithView;
  nsIView *parView;
  GetParentWithView(aPresContext, &parWithView);
  parWithView->GetView(aPresContext, &parView);
  nsRect boundBox(0, 0, 0, 0); 
  result = view->Init(viewMan, boundBox, parView);
  viewMan->InsertChild(parView, view, 0);
  SetView(aPresContext, view);

  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));
  
  nsFrameborder  frameborder = GetFrameBorder(PR_FALSE);
  PRInt32 borderWidth = GetBorderWidth(aPresContext, PR_FALSE);
  nscolor borderColor = GetBorderColor();
 
  // parse the rows= cols= data
  ParseRowCol(aPresContext, nsHTMLAtoms::rows, mNumRows, &mRowSpecs);
  ParseRowCol(aPresContext, nsHTMLAtoms::cols, mNumCols, &mColSpecs);
  mRowSizes  = new nscoord[mNumRows];
  mColSizes  = new nscoord[mNumCols];

  PRInt32 numCells = mNumRows*mNumCols;

  mVerBorders    = new nsHTMLFramesetBorderFrame*[mNumCols];  // 1 more than number of ver borders
  for (int verX  = 0; verX < mNumCols; verX++)
    mVerBorders[verX]    = nsnull;

  mHorBorders    = new nsHTMLFramesetBorderFrame*[mNumRows];  // 1 more than number of hor borders
  for (int horX = 0; horX < mNumRows; horX++)
    mHorBorders[horX]    = nsnull;
     
  mChildTypes = new PRInt32[numCells]; 
  mChildFrameborder  = new nsFrameborder[numCells]; 
  mChildBorderColors  = new nsBorderColor[numCells]; 

  // create the children frames; skip content which isn't <frameset> or <frame>
  nsIFrame* lastChild = nsnull;
  mChildCount = 0; // number of <frame> or <frameset> children
  nsIFrame* frame;
  PRInt32 numChildren; // number of any type of children
  mContent->ChildCount(numChildren);
  for (int childX = 0; childX < numChildren; childX++) {
    if (mChildCount == numCells) { // we have more <frame> or <frameset> than cells
      break;
    }
    nsCOMPtr<nsIContent> child;
    mContent->ChildAt(childX, *getter_AddRefs(child));
    if (!child->IsContentOfType(nsIContent::eHTML))
      continue;
    nsCOMPtr<nsIAtom> tag;
    child->GetTag(*getter_AddRefs(tag));
    if ((nsHTMLAtoms::frameset == tag) || (nsHTMLAtoms::frame == tag)) {
      nsCOMPtr<nsIStyleContext> kidSC;
      nsresult         result;

      aPresContext->ResolveStyleContextFor(child, mStyleContext,
                                           PR_FALSE, getter_AddRefs(kidSC));
      if (nsHTMLAtoms::frameset == tag.get()) {
        result = NS_NewHTMLFramesetFrame(shell, &frame);

        mChildTypes[mChildCount] = FRAMESET;
        nsHTMLFramesetFrame* childFrame = (nsHTMLFramesetFrame*)frame;
        childFrame->SetParentFrameborder(frameborder);
        childFrame->SetParentBorderWidth(borderWidth);
        childFrame->SetParentBorderColor(borderColor);
        frame->Init(aPresContext, child, this, kidSC, nsnull);
        
        mChildBorderColors[mChildCount].Set(childFrame->GetBorderColor());
      } else { // frame
        result = NS_NewHTMLFrameOuterFrame(shell, &frame);
        frame->Init(aPresContext, child, this, kidSC, nsnull);

        mChildTypes[mChildCount] = FRAME;
        
        mChildFrameborder[mChildCount] = GetFrameBorder(child, PR_FALSE);
        mChildBorderColors[mChildCount].Set(GetBorderColor(child));
      }
      
      if (NS_FAILED(result))
        return result;

      if (lastChild)
        lastChild->SetNextSibling(frame);
      else
        mFrames.SetFrames(frame);
      
      lastChild = frame;
      mChildCount++;
    }
  }

  mNonBlankChildCount = mChildCount;
  // add blank frames for frameset cells that had no content provided
  for (int blankX = mChildCount; blankX < numCells; blankX++) {
    // XXX the blank frame is using the content of its parent - at some point it 
    // should just have null content, if we support that
    nsHTMLFramesetBlankFrame* blankFrame = new (shell.get()) nsHTMLFramesetBlankFrame;
    nsCOMPtr<nsIStyleContext> pseudoStyleContext;
    aPresContext->ResolvePseudoStyleContextFor(mContent, nsHTMLAtoms::framesetBlankPseudo,
                                               mStyleContext, PR_FALSE,
                                               getter_AddRefs(pseudoStyleContext));
    if(blankFrame)
      blankFrame->Init(aPresContext, mContent, this, pseudoStyleContext, nsnull);
   
    if (lastChild)
      lastChild->SetNextSibling(blankFrame);
    else
      mFrames.SetFrames(blankFrame);
    
    lastChild = blankFrame;
    mChildTypes[mChildCount] = BLANK;
    mChildBorderColors[mChildCount].Set(NO_COLOR);
    mChildCount++;
  }

  mNonBorderChildCount = mChildCount;
  return rv;
}

// XXX should this try to allocate twips based on an even pixel boundary?
void nsHTMLFramesetFrame::Scale(nscoord  aDesired, 
                                PRInt32  aNumIndicies, 
                                PRInt32* aIndicies, 
                                PRInt32  aNumItems,
                                PRInt32* aItems)
{
  PRInt32 actual = 0;
  PRInt32 i, j;
  // get the actual total
  for (i = 0; i < aNumIndicies; i++) {
    j = aIndicies[i];
    actual += aItems[j];
  }

  float factor = (float)aDesired / (float)actual;
  actual = 0;
  // scale the items up or down
  for (i = 0; i < aNumIndicies; i++) {
    j = aIndicies[i];
    aItems[j] = NSToCoordRound((float)aItems[j] * factor);
    actual += aItems[j];
  }

  if ((aNumIndicies > 0) && (aDesired != actual)) {
    PRInt32 unit = (aDesired > actual) ? 1 : -1;
    for (i=0; (i < aNumIndicies) && (aDesired != actual); i++) {
      j = aIndicies[i];
      if (j < aNumItems) {
        aItems[j] += unit;
        actual += unit;
      }
    }
  }
}
  

/**
  * Translate the rows/cols specs into an array of integer sizes for
  * each cell in the frameset. Sizes are allocated based on the priorities of the
  * specifier - fixed sizes have the highest priority, percentage sizes have the next
  * highest priority and relative sizes have the lowest.
  */
void nsHTMLFramesetFrame::CalculateRowCol(nsIPresContext* aPresContext, 
                                          nscoord         aSize, 
                                          PRInt32         aNumSpecs, 
                                          nsFramesetSpec* aSpecs, 
                                          nscoord*        aValues)
{
  PRInt32  fixedTotal = 0;
  PRInt32  numFixed = 0;
  PRInt32* fixed = new PRInt32[aNumSpecs];
  PRInt32  numPercent = 0;
  PRInt32* percent = new PRInt32[aNumSpecs];
  PRInt32  relativeSums = 0;
  PRInt32  numRelative = 0;
  PRInt32* relative= new PRInt32[aNumSpecs];

  float p2t;
  aPresContext->GetScaledPixelsToTwips(&p2t);
  PRInt32 i, j;
 
  // initialize the fixed, percent, relative indices, allocate the fixed sizes and zero the others
  for (i = 0; i < aNumSpecs; i++) {   
    aValues[i] = 0;
    switch (aSpecs[i].mUnit) {
      case eFramesetUnit_Fixed:
        aValues[i] = NSToCoordRound(p2t * aSpecs[i].mValue);
        fixedTotal += aValues[i];
        fixed[numFixed] = i;
        numFixed++;
        break;
      case eFramesetUnit_Percent:
        percent[numPercent] = i;
        numPercent++;
        break;
      case eFramesetUnit_Relative:
        relative[numRelative] = i;
        numRelative++;
        relativeSums += aSpecs[i].mValue;
        break;
    }
  }

  // scale the fixed sizes if they total too much (or too little and there aren't any percent or relative)
  if ((fixedTotal > aSize) || ((fixedTotal < aSize) && (0 == numPercent) && (0 == numRelative))) { 
    Scale(aSize, numFixed, fixed, aNumSpecs, aValues);
    delete [] fixed; delete [] percent; delete [] relative;
    return;
  }

  PRInt32 percentMax = aSize - fixedTotal;
  PRInt32 percentTotal = 0;
  // allocate the percentage sizes from what is left over from the fixed allocation
  for (i = 0; i < numPercent; i++) {
    j = percent[i];
    aValues[j] = NSToCoordRound((float)aSpecs[j].mValue * (float)aSize / 100.0f);
    percentTotal += aValues[j];
  }

  // scale the percent sizes if they total too much (or too little and there aren't any relative)
  if ((percentTotal > percentMax) || ((percentTotal < percentMax) && (0 == numRelative))) { 
    Scale(percentMax, numPercent, percent, aNumSpecs, aValues);
    delete [] fixed; delete [] percent; delete [] relative;
    return;
  }

  PRInt32 relativeMax = percentMax - percentTotal;
  PRInt32 relativeTotal = 0;
  // allocate the relative sizes from what is left over from the percent allocation
  for (i = 0; i < numRelative; i++) {
    j = relative[i];
    aValues[j] = NSToCoordRound((float)aSpecs[j].mValue * (float)relativeMax / (float)relativeSums);
    relativeTotal += aValues[j];
  }

  // scale the relative sizes if they take up too much or too little
  if (relativeTotal != relativeMax) { 
    Scale(relativeMax, numRelative, relative, aNumSpecs, aValues);
  }
  
  delete [] fixed; delete [] percent; delete [] relative;
}


/**
  * Translate the rows/cols integer sizes into an array of specs for
  * each cell in the frameset.  Reverse of CalculateRowCol() behaviour.
  * This allows us to maintain the user size info through reflows.
  */
void nsHTMLFramesetFrame::GenerateRowCol(nsIPresContext* aPresContext, 
                                          nscoord         aSize, 
                                          PRInt32         aNumSpecs, 
                                          nsFramesetSpec* aSpecs, 
                                          nscoord*        aValues)
{
  float t2p;
  aPresContext->GetTwipsToPixels(&t2p);
  PRInt32 i;
 
  for (i = 0; i < aNumSpecs; i++) {   
    switch (aSpecs[i].mUnit) {
      case eFramesetUnit_Fixed:
        aSpecs[i].mValue = NSToCoordRound(t2p * aValues[i]);
        break;
      case eFramesetUnit_Percent: // XXX Only accurate to 1%, need 1 pixel
        aSpecs[i].mValue = (100*aValues[i])/aSize;
        break;
      case eFramesetUnit_Relative:
        aSpecs[i].mValue = aValues[i];
        break;
    }
  }
}

PRInt32 nsHTMLFramesetFrame::GetBorderWidth(nsIPresContext* aPresContext,
                                            PRBool aTakeForcingIntoAccount)
{
  PRBool forcing = mForceFrameResizability && aTakeForcingIntoAccount;
  
  if (!forcing) {
    nsFrameborder frameborder = GetFrameBorder(PR_FALSE);
    if (frameborder == eFrameborder_No) {
      return 0;
    }
  }
  float p2t;
  aPresContext->GetScaledPixelsToTwips(&p2t);
  nsHTMLValue htmlVal;
  nsIHTMLContent* content = nsnull;
  mContent->QueryInterface(kIHTMLContentIID, (void**)&content);
  if (nsnull != content) {
    if (NS_CONTENT_ATTR_HAS_VALUE == (content->GetHTMLAttribute(nsHTMLAtoms::border, htmlVal))) {
      nsHTMLUnit unit = htmlVal.GetUnit();
      PRInt32 intVal = 0;
      if (eHTMLUnit_Pixel == unit) {
        intVal = htmlVal.GetPixelValue();
      } else if (eHTMLUnit_Integer == unit) {
        intVal = htmlVal.GetIntValue();
      } 
      if (intVal < 0) {
        intVal = 0;
      }
      NS_RELEASE(content);
      if (forcing && intVal == 0) {
        intVal = DEFAULT_BORDER_WIDTH_PX;
      }
      return NSIntPixelsToTwips(intVal, p2t);
    }
    NS_RELEASE(content);
  }

  if (mParentBorderWidth > 0 ||
      (mParentBorderWidth == 0 && !forcing)) {
    return mParentBorderWidth;
  }

  return NSIntPixelsToTwips(DEFAULT_BORDER_WIDTH_PX, p2t);
}


PRIntn
nsHTMLFramesetFrame::GetSkipSides() const
{
  return 0;
}

void 
nsHTMLFramesetFrame::GetDesiredSize(nsIPresContext*          aPresContext,
                                    const nsHTMLReflowState& aReflowState,
                                    nsHTMLReflowMetrics&     aDesiredSize)
{
  nsHTMLFramesetFrame* framesetParent = GetFramesetParent(this);
  if (nsnull == framesetParent) {
    nsRect area;
    aPresContext->GetVisibleArea(area);

    aDesiredSize.width = area.width;
    aDesiredSize.height= area.height;
  } else {
    nsSize size;
    framesetParent->GetSizeOfChild(this, size);
    aDesiredSize.width  = size.width;
    aDesiredSize.height = size.height;
  } 
  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;
}


nsHTMLFramesetFrame* nsHTMLFramesetFrame::GetFramesetParent(nsIFrame* aChild)
{
  nsHTMLFramesetFrame* parent = nsnull;
  nsIContent* content = nsnull;
  aChild->GetContent(&content);
  if (nsnull != content) { 
    nsIContent* contentParent = nsnull;
    content->GetParent(contentParent);
    if (nsnull != contentParent) {
      nsIHTMLContent* contentParent2 = nsnull;
      contentParent->QueryInterface(kIHTMLContentIID, (void**)&contentParent2);
      if (nsnull != contentParent2) {
        nsIAtom* tag;
        contentParent2->GetTag(tag);
        if (nsHTMLAtoms::frameset == tag) {
          aChild->GetParent((nsIFrame**)&parent);
        }
        NS_IF_RELEASE(tag);
        NS_RELEASE(contentParent2);
      }
      NS_RELEASE(contentParent);
    }
    NS_RELEASE(content);
  }

  return parent;
}

// only valid for non border children
void nsHTMLFramesetFrame::GetSizeOfChildAt(PRInt32  aIndexInParent, 
                                           nsSize&  aSize, 
                                           nsPoint& aCellIndex)
{
  PRInt32 row = aIndexInParent / mNumCols;
  PRInt32 col = aIndexInParent - (row * mNumCols); // remainder from dividing index by mNumCols
  if ((row < mNumRows) && (col < mNumCols)) {
    aSize.width  = mColSizes[col];
    aSize.height = mRowSizes[row];
    aCellIndex.x = col;
    aCellIndex.y = row;
  } else {
    aSize.width = aSize.height = aCellIndex.x = aCellIndex.y = 0;
  }
}

// only valid for non border children
void nsHTMLFramesetFrame::GetSizeOfChild(nsIFrame* aChild, 
                                         nsSize&   aSize)
{
  // Reflow only creates children frames for <frameset> and <frame> content.
  // this assumption is used here
  int i = 0;
  for (nsIFrame* child = mFrames.FirstChild(); child;
       child->GetNextSibling(&child)) {
    if (aChild == child) {
      nsPoint ignore;
      GetSizeOfChildAt(i, aSize, ignore);
      return;
    }
    i++;
  }
  aSize.width  = 0;
  aSize.height = 0;
}  

  
NS_METHOD nsHTMLFramesetFrame::HandleEvent(nsIPresContext* aPresContext, 
                                           nsGUIEvent*     aEvent,
                                           nsEventStatus*  aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);
  if (mDragger) {
    // the nsFramesetBorderFrame has captured NS_MOUSE_DOWN
    switch (aEvent->message) {
      case NS_MOUSE_MOVE:
        MouseDrag(aPresContext, aEvent);
	      break;
      case NS_MOUSE_LEFT_BUTTON_UP:
        EndMouseDrag(aPresContext);
	      break;
    }
    *aEventStatus = nsEventStatus_eConsumeNoDefault;
  } else {
    *aEventStatus = nsEventStatus_eIgnore;
  }
  return NS_OK;
}

#if 0
PRBool 
nsHTMLFramesetFrame::IsGrabbingMouse()
{
  PRBool result = PR_FALSE;
  nsIView* view;
  GetView(&view);
  if (view) {
    nsIViewManager* viewMan;
    view->GetViewManager(viewMan);
    if (viewMan) {
      nsIView* grabber;
      viewMan->GetMouseEventGrabber(grabber);
      if (grabber == view) {
        // the nsFramesetBorderFrame has captured NS_MOUSE_DOWN
        result = PR_TRUE;
      }
      NS_RELEASE(viewMan);
    }
  }
  return result;
}
#endif

NS_IMETHODIMP
nsHTMLFramesetFrame::GetCursor(nsIPresContext* aPresContext,
                               nsPoint&        aPoint,
                               PRInt32&        aCursor)
{
  if (mDragger) {
    aCursor = (mDragger->mVertical) ? NS_STYLE_CURSOR_W_RESIZE : NS_STYLE_CURSOR_N_RESIZE;
  } else {
    aCursor = NS_STYLE_CURSOR_DEFAULT;
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLFramesetFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                                      const nsPoint& aPoint, 
                                      nsFramePaintLayer aWhichLayer,
                                      nsIFrame**     aFrame)
{
  //XXX Temporary to deal with event handling in both this and FramsetBorderFrame
  if (mDragger) { 
    *aFrame = this;
    return NS_OK;
  } else {
    return nsContainerFrame::GetFrameForPoint(aPresContext, aPoint, aWhichLayer, aFrame);
  }
}

NS_IMETHODIMP
nsHTMLFramesetFrame::Paint(nsIPresContext*      aPresContext,
                           nsIRenderingContext& aRenderingContext,
                           const nsRect&        aDirtyRect,
                           nsFramePaintLayer    aWhichLayer,
                           PRUint32             aFlags)
{
  //printf("frameset paint %X (%d,%d,%d,%d) \n", this, aDirtyRect.x, aDirtyRect.y, aDirtyRect.width, aDirtyRect.height);
  return nsHTMLContainerFrame::Paint(aPresContext, aRenderingContext,
                                     aDirtyRect, aWhichLayer);
}

void nsHTMLFramesetFrame::ParseRowCol(nsIPresContext* aPresContext, nsIAtom* aAttrType, PRInt32& aNumSpecs, nsFramesetSpec** aSpecs) 
{
  nsHTMLValue value;
  nsAutoString rowsCols;
  nsIHTMLContent* content = nsnull;
  mContent->QueryInterface(kIHTMLContentIID, (void**)&content);
  if (nsnull != content) {
    if (NS_CONTENT_ATTR_HAS_VALUE == content->GetHTMLAttribute(aAttrType, value)) {
      if (eHTMLUnit_String == value.GetUnit()) {
        value.GetStringValue(rowsCols);
        nsFramesetSpec* specs = new nsFramesetSpec[gMaxNumRowColSpecs];
        aNumSpecs = ParseRowColSpec(aPresContext, rowsCols, gMaxNumRowColSpecs, specs);
        *aSpecs = new nsFramesetSpec[aNumSpecs];
        for (int i = 0; i < aNumSpecs; i++) {
          (*aSpecs)[i] = specs[i];
        }
        delete [] specs;
        NS_RELEASE(content);
        return;
      }
    }
    NS_RELEASE(content);
  }
  aNumSpecs = 1; 
  *aSpecs = new nsFramesetSpec[1];
  aSpecs[0]->mUnit  = eFramesetUnit_Relative;
  aSpecs[0]->mValue = 1;
}

/**
  * Translate a "rows" or "cols" spec into an array of nsFramesetSpecs
  */
PRInt32 
nsHTMLFramesetFrame::ParseRowColSpec(nsIPresContext* aPresContext,
                                     nsString&       aSpec, 
                                     PRInt32         aMaxNumValues, 
                                     nsFramesetSpec* aSpecs) 
{
  static const PRUnichar ASTER('*');
  static const PRUnichar PERCENT('%');
  static const PRUnichar COMMA(',');

  // remove whitespace (Bug 33699)
  // also remove leading/trailing commas (bug 31482)
  aSpec.StripChars(" \n\r\t");
  aSpec.Trim(",");
  
  // Count the commas 
  PRInt32 commaX = aSpec.FindChar(COMMA);
  PRInt32 count = 1;
  while (commaX >= 0) {
    count++;
    commaX = aSpec.FindChar(COMMA, PR_FALSE,commaX + 1);
  }

  if (count > aMaxNumValues) {
    NS_ASSERTION(0, "Not enough space for values");
    count = aMaxNumValues;
  }

  // Parse each comma separated token

  PRInt32 start = 0;
  PRInt32 specLen = aSpec.Length();

  for (PRInt32 i = 0; i < count; i++) {
    // Find our comma
    commaX = aSpec.FindChar(COMMA, PR_FALSE,start);
    PRInt32 end = (commaX < 0) ? specLen : commaX;

    // Note: If end == start then it means that the token has no
    // data in it other than a terminating comma (or the end of the spec)
    aSpecs[i].mUnit = eFramesetUnit_Fixed;
    if (end > start) {
      PRInt32 numberEnd = end;
      PRUnichar ch = aSpec.CharAt(numberEnd - 1);
      if (ASTER == ch) {
        aSpecs[i].mUnit = eFramesetUnit_Relative;
        numberEnd--;
      } else if (PERCENT == ch) {
        aSpecs[i].mUnit = eFramesetUnit_Percent;
        numberEnd--;
        // check for "*%"
        if (numberEnd > start) {
          ch = aSpec.CharAt(numberEnd - 1);
          if (ASTER == ch) {
            aSpecs[i].mUnit = eFramesetUnit_Relative;
            numberEnd--;
          }
        }
      }

      // Translate value to an integer
      nsString token;
      aSpec.Mid(token, start, numberEnd - start);

      // Treat * as 1*
      if ((eFramesetUnit_Relative == aSpecs[i].mUnit) &&
        (0 == token.Length())) {
        aSpecs[i].mValue = 1;
      }

      // Otherwise just convert to integer.
      else {
        PRInt32 err;
        aSpecs[i].mValue = token.ToInteger(&err);
        if (err) {
          aSpecs[i].mValue = 0;
        }
      }

      // Treat 0* as 1* in quirks mode (bug 40383)
      nsCompatibility mode;
      aPresContext->GetCompatibilityMode(&mode);
      if (eCompatibility_NavQuirks == mode) {
        if ((eFramesetUnit_Relative == aSpecs[i].mUnit) &&
          (0 == aSpecs[i].mValue)) {
          aSpecs[i].mValue = 1;
        }
      }
        
      // Catch zero and negative frame sizes for Nav compatability
      // Nav resized absolute and relative frames to "1" and
      // percent frames to an even percentage of the width
      //
      //if ((eCompatibility_NavQuirks == aMode) && (aSpecs[i].mValue <= 0)) {
      //  if (eFramesetUnit_Percent == aSpecs[i].mUnit) {
      //    aSpecs[i].mValue = 100 / count;
      //  } else {
      //    aSpecs[i].mValue = 1;
      //  }
      //} else {

      // In standards mode, just set negative sizes to zero
      if (aSpecs[i].mValue < 0) {
        aSpecs[i].mValue = 0;
      }
      start = end + 1;
    }
  }
  return count;
}

void 
nsHTMLFramesetFrame::ReflowPlaceChild(nsIFrame*                aChild,
                                      nsIPresContext*          aPresContext,
                                      const nsHTMLReflowState& aReflowState,
                                      nsPoint&                 aOffset,
                                      nsSize&                  aSize,
                                      nsPoint*                 aCellIndex)
{
  // reflow the child
  nsHTMLReflowState  reflowState(aPresContext, aReflowState, aChild, aSize);
  nsHTMLReflowMetrics metrics(nsnull);
  metrics.width = aSize.width;
  metrics.height= aSize.height;
  nsReflowStatus status;
  
  ReflowChild(aChild, aPresContext, metrics, reflowState, aOffset.x,
              aOffset.y, 0, status);
  NS_ASSERTION(NS_FRAME_IS_COMPLETE(status), "bad status");
  
  // Place and size the child
  metrics.width = aSize.width;
  metrics.height = aSize.height;
  FinishReflowChild(aChild, aPresContext, metrics, aOffset.x, aOffset.y, 0);
}

static
nsFrameborder GetFrameBorderHelper(nsIHTMLContent* aContent, 
                                   PRBool          aStandardMode)
{
  if (nsnull != aContent) {
    nsHTMLValue value;
    if (NS_CONTENT_ATTR_HAS_VALUE == (aContent->GetHTMLAttribute(nsHTMLAtoms::frameborder, value))) {
      if (eHTMLUnit_Enumerated == value.GetUnit()) {
        PRInt32 intValue;
        intValue = value.GetIntValue();
        if (!aStandardMode) {
          if (NS_STYLE_FRAME_YES == intValue) {
            intValue = NS_STYLE_FRAME_1;
          } 
          else if (NS_STYLE_FRAME_NO == intValue) {
            intValue = NS_STYLE_FRAME_0;
          }
        }
        if (NS_STYLE_FRAME_0 == intValue) {
          return eFrameborder_No;
        } 
        else if (NS_STYLE_FRAME_1 == intValue) {
          return eFrameborder_Yes;
        }
      }      
    }
  }
  return eFrameborder_Notset;
}

nsFrameborder nsHTMLFramesetFrame::GetFrameBorder(PRBool aStandardMode) 
{
  nsFrameborder result = eFrameborder_Notset;
  nsIHTMLContent* content = nsnull;
  mContent->QueryInterface(kIHTMLContentIID, (void**) &content);
  if (nsnull != content) {
    result = GetFrameBorderHelper(content, aStandardMode);
    NS_RELEASE(content);
  }
  if (eFrameborder_Notset == result) {
    return mParentFrameborder;
  }
  return result;
}

nsFrameborder nsHTMLFramesetFrame::GetFrameBorder(nsIContent* aContent, 
                                                  PRBool      aStandardMode)
{
  nsFrameborder result = eFrameborder_Notset;
  nsIHTMLContent* content = nsnull;
  aContent->QueryInterface(kIHTMLContentIID, (void**) &content);
  if (nsnull != content) {
    result = GetFrameBorderHelper(content, aStandardMode);
    NS_RELEASE(content);
  }
  if (eFrameborder_Notset == result) {
    return GetFrameBorder(aStandardMode);
  }
  return result;
}

nscolor nsHTMLFramesetFrame::GetBorderColor() 
{
  nscolor result = NO_COLOR;
  nsIHTMLContent* content = nsnull;
  mContent->QueryInterface(kIHTMLContentIID, (void**)&content);
  if (nsnull != content) {
    nsHTMLValue value;
    if (NS_CONTENT_ATTR_HAS_VALUE == (content->GetHTMLAttribute(nsHTMLAtoms::bordercolor, value))) {
      if ((eHTMLUnit_Color == value.GetUnit()) ||
          (eHTMLUnit_ColorName == value.GetUnit())) {
        result = value.GetColorValue();
      }
    }
    NS_RELEASE(content);
  }
  if (NO_COLOR == result) {
    return mParentBorderColor;
  } 
  return result;
}

nscolor nsHTMLFramesetFrame::GetBorderColor(nsIContent* aContent) 
{
  nscolor result = NO_COLOR;
  nsIHTMLContent* content = nsnull;
  aContent->QueryInterface(kIHTMLContentIID, (void**)&content);
  if (nsnull != content) {
    nsHTMLValue value;
    if (NS_CONTENT_ATTR_HAS_VALUE == (content->GetHTMLAttribute(nsHTMLAtoms::bordercolor, value))) {
      if ((eHTMLUnit_Color == value.GetUnit()) ||
          (eHTMLUnit_ColorName == value.GetUnit())) {
        result = value.GetColorValue();
      }
    }
    NS_RELEASE(content);
  }
  if (NO_COLOR == result) {
    return GetBorderColor();
  }
  return result;
}

NS_IMETHODIMP
nsHTMLFramesetFrame::Reflow(nsIPresContext*          aPresContext,
                            nsHTMLReflowMetrics&     aDesiredSize,
                            const nsHTMLReflowState& aReflowState,
                            nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsHTMLFramesetFrame", aReflowState.reason);
  DISPLAY_REFLOW(this, aReflowState, aDesiredSize, aStatus);
  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));
            
  //printf("FramesetFrame2::Reflow %X (%d,%d) \n", this, aReflowState.availableWidth, aReflowState.availableHeight); 
  // Always get the size so that the caller knows how big we are
  GetDesiredSize(aPresContext, aReflowState, aDesiredSize);
  
  nscoord width  = (aDesiredSize.width <= aReflowState.availableWidth)
    ? aDesiredSize.width : aReflowState.availableWidth;
  nscoord height = (aDesiredSize.height <= aReflowState.availableHeight)
    ? aDesiredSize.height : aReflowState.availableHeight;

  PRBool firstTime = (eReflowReason_Initial == aReflowState.reason);
  if (firstTime) {
    nsCOMPtr<nsIPrefService> prefService(do_GetService(NS_PREFSERVICE_CONTRACTID));
    if (prefService) {
      nsCOMPtr<nsIPrefBranch> prefBranch;
      prefService->GetBranch(nsnull, getter_AddRefs(prefBranch));
      if (prefBranch) {
        nsCOMPtr<nsIPrefBranchInternal> prefBranchInternal(do_QueryInterface(prefBranch));
        if (prefBranchInternal) {
          mPrefBranchWeakRef = getter_AddRefs(NS_GetWeakReference(prefBranchInternal));
          prefBranchInternal->AddObserver(kFrameResizePref, this, PR_FALSE);
        }
        prefBranch->GetBoolPref(kFrameResizePref, &mForceFrameResizability);
      }
    }
  }
  
  // subtract out the width of all of the potential borders. There are
  // only borders between <frame>s. There are none on the edges (e.g the
  // leftmost <frame> has no left border).
  PRInt32 borderWidth = GetBorderWidth(aPresContext, PR_TRUE);

  width  -= (mNumCols - 1) * borderWidth;
  if (width < 0) width = 0;

  height -= (mNumRows - 1) * borderWidth;
  if (height < 0) height = 0;

  if (!mDrag.mActive && ( (firstTime) ||
                  ( (mRect.width != 0) && (mRect.height != 0) &&
                    (aDesiredSize.width != 0) && (aDesiredSize.height != 0) &&
                    ((aDesiredSize.width != mRect.width) || (aDesiredSize.height != mRect.height))) ) )  { 
    CalculateRowCol(aPresContext, width, mNumCols, mColSpecs, mColSizes);
    CalculateRowCol(aPresContext, height, mNumRows, mRowSpecs, mRowSizes);
  }

  PRBool*        verBordersVis     = nsnull; // vertical borders visibility
  nscolor*       verBorderColors   = nsnull;
  PRBool*        horBordersVis     = nsnull; // horizontal borders visibility
  nscolor*       horBorderColors   = nsnull;
  nscolor        borderColor       = GetBorderColor();
  nsFrameborder  frameborder       = GetFrameBorder(PR_FALSE);

  if (firstTime) {
    verBordersVis = new PRBool[mNumCols];
    verBorderColors = new nscolor[mNumCols];
    for (int verX  = 0; verX < mNumCols; verX++) {
      verBordersVis[verX] = PR_FALSE;
      verBorderColors[verX] = NO_COLOR;
    }

    horBordersVis = new PRBool[mNumRows];
    horBorderColors = new nscolor[mNumRows];
    for (int horX = 0; horX < mNumRows; horX++) {
      horBordersVis[horX] = PR_FALSE;
      horBorderColors[horX] = NO_COLOR;
    }
  }

  // reflow the children
  PRInt32 lastRow = 0;
  PRInt32 lastCol = 0;
  PRInt32 borderChildX = mNonBorderChildCount; // index of border children
  nsHTMLFramesetBorderFrame* borderFrame = nsnull;
  nsPoint offset(0,0);
  nsSize size, lastSize;
  nsIFrame* child = mFrames.FirstChild();
  nsIFrame* lastChild = mFrames.LastChild();

  for (PRInt32 childX = 0; childX < mNonBorderChildCount; childX++) {
    nsPoint cellIndex;
    GetSizeOfChildAt(childX, size, cellIndex);

    if (lastRow != cellIndex.y) {  // changed to next row
      offset.x = 0;
      offset.y += lastSize.height;
      if (firstTime) { // create horizontal border
        borderFrame = new (shell.get()) nsHTMLFramesetBorderFrame(borderWidth, PR_FALSE, PR_FALSE);
        nsIStyleContext* pseudoStyleContext;
        aPresContext->ResolvePseudoStyleContextFor(mContent, nsHTMLAtoms::horizontalFramesetBorderPseudo,
                                                   mStyleContext, PR_FALSE,
                                                   &pseudoStyleContext);
        borderFrame->Init(aPresContext, mContent, this, pseudoStyleContext, nsnull);
        NS_RELEASE(pseudoStyleContext);

        mChildCount++;
        lastChild->SetNextSibling(borderFrame);
        lastChild = borderFrame;
        mHorBorders[cellIndex.y-1] = borderFrame;
        // set the neighbors for determining drag boundaries
        borderFrame->mPrevNeighbor = lastRow; 
        borderFrame->mNextNeighbor = cellIndex.y;  
      } else {
        borderFrame = (nsHTMLFramesetBorderFrame*)mFrames.FrameAt(borderChildX);
        borderFrame->mWidth = borderWidth;
        borderChildX++;
      }
      nsSize borderSize(aDesiredSize.width, borderWidth);
      ReflowPlaceChild(borderFrame, aPresContext, aReflowState, offset, borderSize);
      offset.y += borderWidth;
      borderFrame = nsnull;
    } else {
      if (cellIndex.x > 0) {  // moved to next col in same row
        if (0 == cellIndex.y) { // in 1st row
          if (firstTime) { // create vertical border
            borderFrame = new (shell.get()) nsHTMLFramesetBorderFrame(borderWidth, PR_TRUE, PR_FALSE);
            nsIStyleContext* pseudoStyleContext;
            aPresContext->ResolvePseudoStyleContextFor(mContent, nsHTMLAtoms::verticalFramesetBorderPseudo,
                                                       mStyleContext,
                                                       PR_FALSE,
                                                       &pseudoStyleContext);
            borderFrame->Init(aPresContext, mContent, this, pseudoStyleContext, nsnull);
            NS_RELEASE(pseudoStyleContext);

            mChildCount++;
            lastChild->SetNextSibling(borderFrame);
            lastChild = borderFrame;
            mVerBorders[cellIndex.x-1] = borderFrame;
            // set the neighbors for determining drag boundaries
            borderFrame->mPrevNeighbor = lastCol; 
            borderFrame->mNextNeighbor = cellIndex.x;  
          } else {         
            borderFrame = (nsHTMLFramesetBorderFrame*)mFrames.FrameAt(borderChildX);
            borderFrame->mWidth = borderWidth;
            borderChildX++;
          }
          nsSize borderSize(borderWidth, aDesiredSize.height);
          ReflowPlaceChild(borderFrame, aPresContext, aReflowState, offset, borderSize);
          borderFrame = nsnull;
        }
        offset.x += borderWidth;
      }
    }

    ReflowPlaceChild(child, aPresContext, aReflowState, offset, size, &cellIndex);

    if (firstTime) {
      PRInt32 childVis; 
      if (FRAMESET == mChildTypes[childX]) {
        nsHTMLFramesetFrame* childFS = (nsHTMLFramesetFrame*)child; 
        childVis = childFS->mEdgeVisibility;
        mChildBorderColors[childX] = childFS->mEdgeColors;
      } else if (FRAME == mChildTypes[childX]) {
        if (eFrameborder_Yes == mChildFrameborder[childX]) {
          childVis = ALL_VIS;
        } else if (eFrameborder_No == mChildFrameborder[childX]) {
          childVis = NONE_VIS;
        } else {  // notset
          childVis = (eFrameborder_No == frameborder) ? NONE_VIS : ALL_VIS;
        }
      } else {  // blank 
        childVis = NONE_VIS;
      }
      nsBorderColor childColors = mChildBorderColors[childX];
      // set the visibility, color of our edge borders based on children
      if (0 == cellIndex.x) {
        if (!(mEdgeVisibility & LEFT_VIS)) {
          mEdgeVisibility |= (LEFT_VIS & childVis);
        }
        if (NO_COLOR == mEdgeColors.mLeft) {
          mEdgeColors.mLeft = childColors.mLeft;
        }
      }
      if (0 == cellIndex.y) {
        if (!(mEdgeVisibility & TOP_VIS)) {
          mEdgeVisibility |= (TOP_VIS & childVis);
        }
        if (NO_COLOR == mEdgeColors.mTop) {
          mEdgeColors.mTop = childColors.mTop;
        }
      }
      if (mNumCols-1 == cellIndex.x) {
        if (!(mEdgeVisibility & RIGHT_VIS)) {
          mEdgeVisibility |= (RIGHT_VIS & childVis);
        }
        if (NO_COLOR == mEdgeColors.mRight) {
          mEdgeColors.mRight = childColors.mRight;
        }
      }
      if (mNumRows-1 == cellIndex.y) {
        if (!(mEdgeVisibility & BOTTOM_VIS)) {
          mEdgeVisibility |= (BOTTOM_VIS & childVis);
        }
        if (NO_COLOR == mEdgeColors.mBottom) {
          mEdgeColors.mBottom = childColors.mBottom;
        }
      }
      // set the visibility of borders that the child may affect
      if (childVis & RIGHT_VIS) {
        verBordersVis[cellIndex.x] = PR_TRUE;
      }
      if (childVis & BOTTOM_VIS) {
        horBordersVis[cellIndex.y] = PR_TRUE;
      }
      if ((cellIndex.x > 0) && (childVis & LEFT_VIS)) {
        verBordersVis[cellIndex.x-1] = PR_TRUE;
      }
      if ((cellIndex.y > 0) && (childVis & TOP_VIS)) {
        horBordersVis[cellIndex.y-1] = PR_TRUE;
      }
      // set the colors of borders that the child may affect
      if (NO_COLOR == verBorderColors[cellIndex.x]) {
        verBorderColors[cellIndex.x] = mChildBorderColors[childX].mRight;
      }
      if (NO_COLOR == horBorderColors[cellIndex.y]) {
        horBorderColors[cellIndex.y] = mChildBorderColors[childX].mBottom;
      }
      if ((cellIndex.x > 0) && (NO_COLOR == verBorderColors[cellIndex.x-1])) {
        verBorderColors[cellIndex.x-1] = mChildBorderColors[childX].mLeft;
      }
      if ((cellIndex.y > 0) && (NO_COLOR == horBorderColors[cellIndex.y-1])) {
        horBorderColors[cellIndex.y-1] = mChildBorderColors[childX].mTop;
      }
    }
    lastRow  = cellIndex.y;
    lastCol  = cellIndex.x;
    lastSize = size;
    offset.x += size.width;
    child->GetNextSibling(&child);
  }

  if (firstTime) {
    nscolor childColor;
    // set the visibility, color, mouse sensitivity of borders
    for (int verX = 0; verX < mNumCols-1; verX++) {
      if (mVerBorders[verX]) {
        mVerBorders[verX]->SetVisibility(verBordersVis[verX]);
        if (mForceFrameResizability) {
          mVerBorders[verX]->mVisibilityOverride = PR_TRUE;
        } else {
          SetBorderResize(mChildTypes, mVerBorders[verX]);
        }
        childColor = (NO_COLOR == verBorderColors[verX]) ? borderColor : verBorderColors[verX];
        mVerBorders[verX]->SetColor(childColor);
      }
    }
    for (int horX = 0; horX < mNumRows-1; horX++) {
      if (mHorBorders[horX]) {
        mHorBorders[horX]->SetVisibility(horBordersVis[horX]);
        if (mForceFrameResizability) {
          mHorBorders[horX]->mVisibilityOverride = PR_TRUE;
        } else {
          SetBorderResize(mChildTypes, mHorBorders[horX]);
        }
        childColor = (NO_COLOR == horBorderColors[horX]) ? borderColor : horBorderColors[horX]; 
        mHorBorders[horX]->SetColor(childColor);
      }
    }

    delete[] verBordersVis;    
    delete[] verBorderColors;
    delete[] horBordersVis; 
    delete[] horBorderColors;
    delete[] mChildTypes; 
    delete[] mChildFrameborder;
    delete[] mChildBorderColors;
  }

  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = aDesiredSize.width;
    aDesiredSize.maxElementSize->height = aDesiredSize.height;
  }

  aStatus = NS_FRAME_COMPLETE;
  mDrag.UnSet();
  return NS_OK;
}

PRBool 
nsHTMLFramesetFrame::ChildIsFrameset(nsIFrame* aChild) 
{
  nsIFrame* childFrame = nsnull;
  aChild->QueryInterface(kIFramesetFrameIID, (void**)&childFrame);
  if (childFrame) {
    return PR_TRUE;
  }
  return PR_FALSE;
}


PRBool 
nsHTMLFramesetFrame::CanResize(PRBool aVertical, 
                               PRBool aLeft) 
{
  nsIFrame* child;
  PRInt32 childX;
  PRInt32 startX;
  if (aVertical) {
    startX = (aLeft) ? 0 : mNumCols-1;
    for (childX = startX; childX < mNonBorderChildCount; childX += mNumCols) {
      child = mFrames.FrameAt(childX);
      if (!CanChildResize(aVertical, aLeft, childX, ChildIsFrameset(child))) {
        return PR_FALSE;
      }
    } 
  } else {
    startX = (aLeft) ? 0 : (mNumRows - 1) * mNumCols;
    PRInt32 endX = startX + mNumCols;
    for (childX = startX; childX < endX; childX++) {
      child = mFrames.FrameAt(childX);
      if (!CanChildResize(aVertical, aLeft, childX, ChildIsFrameset(child))) {
        return PR_FALSE;
      }
    }
  }
  return PR_TRUE;
}

PRBool
nsHTMLFramesetFrame::GetNoResize(nsIFrame* aChildFrame) 
{
  PRBool result = PR_FALSE;
  nsIContent* content = nsnull;
  aChildFrame->GetContent(&content);
  if (nsnull != content) {
    nsIHTMLContent* htmlContent = nsnull;
    content->QueryInterface(kIHTMLContentIID, (void**)&htmlContent);
    if (nsnull != htmlContent) {
      nsHTMLValue value;
      if (NS_CONTENT_ATTR_HAS_VALUE == htmlContent->GetHTMLAttribute(nsHTMLAtoms::noresize, value)) {
        result = PR_TRUE;
      }
      NS_RELEASE(htmlContent);
    }
    NS_RELEASE(content);
  }
  return result;
}

PRBool 
nsHTMLFramesetFrame::CanChildResize(PRBool  aVertical, 
                                    PRBool  aLeft, 
                                    PRInt32 aChildX, 
                                    PRBool  aFrameset) 
{
  nsIFrame* child = mFrames.FrameAt(aChildX);
  if (aFrameset) {
    return ((nsHTMLFramesetFrame*)child)->CanResize(aVertical, aLeft);
  } else {
    return !GetNoResize(child);
  }
}

// This calculates and sets the resizability of all border frames

void
nsHTMLFramesetFrame::RecalculateBorderResize()
{
  if (!mContent) {
    return;
  }

  PRInt32 numCells = mNumRows * mNumCols; // max number of cells
  PRInt32* childTypes = new PRInt32[numCells];
  PRInt32 childIndex, frameOrFramesetChildIndex = 0;

  PRInt32 numChildren; // number of any type of children
  mContent->ChildCount(numChildren);
  for (childIndex = 0; childIndex < numChildren; childIndex++) {
    nsCOMPtr<nsIContent> childCon;
    mContent->ChildAt(childIndex, *getter_AddRefs(childCon));
    nsCOMPtr<nsIHTMLContent> child(do_QueryInterface(childCon));
    if (child) {
      nsCOMPtr<nsIAtom> tag;
      child->GetTag(*getter_AddRefs(tag));
      if (nsHTMLAtoms::frameset == tag) {
        childTypes[frameOrFramesetChildIndex++] = FRAMESET;
      } else if (nsHTMLAtoms::frame == tag) {
        childTypes[frameOrFramesetChildIndex++] = FRAME;
      }
      // Don't overflow childTypes array
      if (frameOrFramesetChildIndex >= numCells) {
        break;
      }
    }
  }

  // set the visibility and mouse sensitivity of borders
  PRInt32 verX;
  for (verX = 0; verX < mNumCols-1; verX++) {
    if (mVerBorders[verX]) {
      mVerBorders[verX]->mCanResize = PR_TRUE;
      if (mForceFrameResizability) {
        mVerBorders[verX]->mVisibilityOverride = PR_TRUE;
      } else {
        mVerBorders[verX]->mVisibilityOverride = PR_FALSE;
        SetBorderResize(childTypes, mVerBorders[verX]);
      }
    }
  }
  PRInt32 horX;
  for (horX = 0; horX < mNumRows-1; horX++) {
    if (mHorBorders[horX]) {
      mHorBorders[horX]->mCanResize = PR_TRUE;
      if (mForceFrameResizability) {
        mHorBorders[horX]->mVisibilityOverride = PR_TRUE;
      } else {
        mHorBorders[horX]->mVisibilityOverride = PR_FALSE;
        SetBorderResize(childTypes, mHorBorders[horX]);
      }
    }
  }
  delete[] childTypes;
}

void 
nsHTMLFramesetFrame::SetBorderResize(PRInt32*                   aChildTypes, 
                                     nsHTMLFramesetBorderFrame* aBorderFrame)
{
  if (aBorderFrame->mVertical) {
    for (int rowX = 0; rowX < mNumRows; rowX++) {
      PRInt32 childX = aBorderFrame->mPrevNeighbor + (rowX * mNumCols);
      if (!CanChildResize(PR_TRUE, PR_FALSE, childX, (FRAMESET == aChildTypes[childX])) ||
          !CanChildResize(PR_TRUE, PR_TRUE, childX+1,(FRAMESET == aChildTypes[childX+1]))) {
        aBorderFrame->mCanResize = PR_FALSE;
      }
    }
  } else {
    PRInt32 childX = aBorderFrame->mPrevNeighbor * mNumCols;
    PRInt32 endX   = childX + mNumCols;
    for (; childX < endX; childX++) {
      if (!CanChildResize(PR_FALSE, PR_FALSE, childX, (FRAMESET == aChildTypes[childX]))) {
        aBorderFrame->mCanResize = PR_FALSE;
      }
    }
    endX = endX + mNumCols;
    for (; childX < endX; childX++) {
      if (!CanChildResize(PR_FALSE, PR_TRUE, childX, (FRAMESET == aChildTypes[childX]))) {
        aBorderFrame->mCanResize = PR_FALSE;
      }
    }
  }
}
  
        
NS_IMETHODIMP
nsHTMLFramesetFrame::VerifyTree() const
{
  // XXX Completely disabled for now; once pseud-frames are reworked
  // then we can turn it back on.
  return NS_OK;
}

void
nsHTMLFramesetFrame::StartMouseDrag(nsIPresContext*            aPresContext, 
                                    nsHTMLFramesetBorderFrame* aBorder, 
                                    nsGUIEvent*                aEvent)
{
  if (mMinDrag == 0) {
    float p2t;
    aPresContext->GetPixelsToTwips(&p2t);
    mMinDrag = NSIntPixelsToTwips(2, p2t);  // set min drag and min frame size to 2 pixels
  }

#if 0
  PRInt32 index;
  IndexOf(aBorder, index);
  NS_ASSERTION((nsnull != aBorder) && (index >= 0), "invalid dragger");
#endif
  nsIView* view;
  GetView(aPresContext, &view);
  if (view) {
    nsIViewManager* viewMan;
    view->GetViewManager(viewMan);
    if (viewMan) {
      PRBool ignore;
      viewMan->GrabMouseEvents(view, ignore);
      NS_RELEASE(viewMan);
      mDragger = aBorder;

      //XXX This should go away!  Border should have own view instead
      view->SetViewFlags(NS_VIEW_PUBLIC_FLAG_DONT_CHECK_CHILDREN);

      // The point isn't in frameset coords, but we're using it to compute
      // moves relative to the start position.
      mFirstDragPoint.MoveTo(aEvent->point.x, aEvent->point.y);

      // Store the original frame sizes
      if (mDragger->mVertical) {
        mPrevNeighborOrigSize = mColSizes[mDragger->mPrevNeighbor];
	mNextNeighborOrigSize = mColSizes[mDragger->mNextNeighbor];
      } else {
        mPrevNeighborOrigSize = mRowSizes[mDragger->mPrevNeighbor];
	mNextNeighborOrigSize = mRowSizes[mDragger->mNextNeighbor];
      }

      gDragInProgress = PR_TRUE;
    }
  }
}
  

void
nsHTMLFramesetFrame::MouseDrag(nsIPresContext* aPresContext, 
                               nsGUIEvent*     aEvent)
{
  PRInt32 change; // measured positive from left-to-right or top-to-bottom
  if (mDragger->mVertical) {
    change = aEvent->point.x - mFirstDragPoint.x;
    if (change > mNextNeighborOrigSize - mMinDrag) {
      change = mNextNeighborOrigSize - mMinDrag;
    } else if (change <= mMinDrag - mPrevNeighborOrigSize) {
      change = mMinDrag - mPrevNeighborOrigSize;
    }
    mColSizes[mDragger->mPrevNeighbor] = mPrevNeighborOrigSize + change;
    mColSizes[mDragger->mNextNeighbor] = mNextNeighborOrigSize - change;

    // Recompute the specs from the new sizes.
    nscoord width = mRect.width - (mNumCols - 1) * GetBorderWidth(aPresContext, PR_TRUE);
    GenerateRowCol(aPresContext, width, mNumCols, mColSpecs, mColSizes);
  } else {
    change = aEvent->point.y - mFirstDragPoint.y;
    if (change > mNextNeighborOrigSize - mMinDrag) {
      change = mNextNeighborOrigSize - mMinDrag;
    } else if (change <= mMinDrag - mPrevNeighborOrigSize) {
      change = mMinDrag - mPrevNeighborOrigSize;
    }
    mRowSizes[mDragger->mPrevNeighbor] = mPrevNeighborOrigSize + change;
    mRowSizes[mDragger->mNextNeighbor] = mNextNeighborOrigSize - change;

    // Recompute the specs from the new sizes.
    nscoord height = mRect.height - (mNumRows - 1) * GetBorderWidth(aPresContext, PR_TRUE);
    GenerateRowCol(aPresContext, height, mNumRows, mRowSpecs, mRowSizes);
  }

  if (change != 0) {
    mDrag.Reset(mDragger->mVertical, mDragger->mPrevNeighbor, change, this);
    nsIFrame* parentFrame = nsnull;
    GetParent((nsIFrame**)&parentFrame);
    if (!parentFrame) {
      return;
    }

    nsCOMPtr<nsIPresShell> shell;
    aPresContext->GetShell(getter_AddRefs(shell));
    if (!shell) {
      return;
    }

    parentFrame->ReflowDirtyChild(shell, this);

    // Update the view immediately (make drag appear snappier)
    nsCOMPtr<nsIViewManager> vm;
    shell->GetViewManager(getter_AddRefs(vm));
    if (vm) {
      nsIView* root;
      vm->GetRootView(root);
      if (root) {
        vm->UpdateView(root, NS_VMREFRESH_IMMEDIATE | NS_VMREFRESH_AUTO_DOUBLE_BUFFER);
      }
    }
  }
}  

void
nsHTMLFramesetFrame::EndMouseDrag(nsIPresContext* aPresContext)
{
  nsIView* view;
  GetView(aPresContext, &view);
  if (view) {
    nsIViewManager* viewMan;
    view->GetViewManager(viewMan);
    if (viewMan) {
      mDragger = nsnull;
      PRBool ignore;
      viewMan->GrabMouseEvents(nsnull, ignore);
      NS_RELEASE(viewMan);
      //XXX This should go away!  Border should have own view instead
      view->ClearViewFlags(NS_VIEW_PUBLIC_FLAG_DONT_CHECK_CHILDREN);
    }
  }
  gDragInProgress = PR_FALSE;
}  

nsresult
NS_NewHTMLFramesetFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsHTMLFramesetFrame* it = new (aPresShell) nsHTMLFramesetFrame;
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

/*******************************************************************************
 * nsHTMLFramesetBorderFrame
 ******************************************************************************/
nsHTMLFramesetBorderFrame::nsHTMLFramesetBorderFrame(PRInt32 aWidth, 
                                                     PRBool  aVertical, 
                                                     PRBool  aVisibility)
  : nsLeafFrame(), mWidth(aWidth), mVertical(aVertical), mVisibility(aVisibility)
{
   mVisibilityOverride = PR_FALSE;
   mCanResize    = PR_TRUE;
   mColor        = NO_COLOR;
   mPrevNeighbor = 0;
   mNextNeighbor = 0;
}

nsHTMLFramesetBorderFrame::~nsHTMLFramesetBorderFrame()
{
  //printf("nsHTMLFramesetBorderFrame destructor %p \n", this);
}

void nsHTMLFramesetBorderFrame::GetDesiredSize(nsIPresContext*          aPresContext,
                                               const nsHTMLReflowState& aReflowState,
                                               nsHTMLReflowMetrics&     aDesiredSize)
{
  aDesiredSize.width   = aReflowState.availableWidth;
  aDesiredSize.height  = aReflowState.availableHeight;
  aDesiredSize.ascent  = aDesiredSize.width;
  aDesiredSize.descent = 0;
}

void nsHTMLFramesetBorderFrame::SetVisibility(PRBool aVisibility)
{ 
  mVisibility = aVisibility; 
}

void nsHTMLFramesetBorderFrame::SetColor(nscolor aColor)
{ 
  mColor = aColor;
}


NS_IMETHODIMP
nsHTMLFramesetBorderFrame::Reflow(nsIPresContext*          aPresContext,
                                  nsHTMLReflowMetrics&     aDesiredSize,
                                  const nsHTMLReflowState& aReflowState,
                                  nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsHTMLFramesetBorderFrame", aReflowState.reason);
  GetDesiredSize(aPresContext, aReflowState, aDesiredSize);
  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}

NS_METHOD
nsHTMLFramesetBorderFrame::Paint(nsIPresContext*      aPresContext,
                                 nsIRenderingContext& aRenderingContext,
                                 const nsRect&        aDirtyRect,
                                 nsFramePaintLayer    aWhichLayer,
                                 PRUint32             aFlags)
{
  if (NS_FRAME_PAINT_LAYER_FOREGROUND != aWhichLayer) {
    return NS_OK;
  }
  //printf("border frame paint %X (%d,%d,%d,%d) \n", this, aDirtyRect.x, aDirtyRect.y, aDirtyRect.width, aDirtyRect.height);
  nscolor WHITE    = NS_RGB(255, 255, 255);
  nscolor bgColor  = NS_RGB(200,200,200);
  nscolor fgColor  = NS_RGB(0,0,0);
  nscolor hltColor = NS_RGB(255,255,255);
  nscolor sdwColor = NS_RGB(128,128,128);

  nsILookAndFeel * lookAndFeel;
  if (NS_OK == nsComponentManager::CreateInstance(kLookAndFeelCID, nsnull, NS_GET_IID(nsILookAndFeel), (void**)&lookAndFeel)) {
   lookAndFeel->GetColor(nsILookAndFeel::eColor_WidgetBackground,  bgColor);
   lookAndFeel->GetColor(nsILookAndFeel::eColor_WidgetForeground,  fgColor);
   lookAndFeel->GetColor(nsILookAndFeel::eColor_Widget3DShadow,    sdwColor);
   lookAndFeel->GetColor(nsILookAndFeel::eColor_Widget3DHighlight, hltColor);
   NS_RELEASE(lookAndFeel);
  }

  float t2p;
  aPresContext->GetTwipsToPixels(&t2p);
  nscoord widthInPixels = NSTwipsToIntPixels(mWidth, t2p);
  float p2t;
  aPresContext->GetPixelsToTwips(&p2t);
  nscoord pixelWidth    = NSIntPixelsToTwips(1, p2t);

  if (widthInPixels <= 0) {
    return NS_OK;
  }

  nscoord x0 = 0;
  nscoord y0 = 0;
  nscoord x1 = (mVertical) ? x0 : mRect.width;
  nscoord y1 = (mVertical) ? mRect.height : x0;

  nscolor color = WHITE;
  if (mVisibility || mVisibilityOverride) {
    color = (NO_COLOR == mColor) ? bgColor : mColor;
  }
  aRenderingContext.SetColor(color);
  // draw grey or white first
  for (int i = 0; i < widthInPixels; i++) {
    aRenderingContext.DrawLine (x0, y0, x1, y1);
    if (mVertical) {
      x0 += pixelWidth;
      x1 =  x0;
    } else {
      y0 += pixelWidth;
      y1 =  y0;
    }
  }

  if (!mVisibility && !mVisibilityOverride) {
    return NS_OK;
  }

  if (widthInPixels >= 5) {
    aRenderingContext.SetColor(hltColor);
    x0 = (mVertical) ? pixelWidth : 0;
    y0 = (mVertical) ? 0 : pixelWidth;
    x1 = (mVertical) ? x0 : mRect.width;
    y1 = (mVertical) ? mRect.height : y0;
    aRenderingContext.DrawLine(x0, y0, x1, y1);
  }

  if (widthInPixels >= 2) {
    aRenderingContext.SetColor(sdwColor);
    x0 = (mVertical) ? mRect.width - (2 * pixelWidth) : 0;
    y0 = (mVertical) ? 0 : mRect.height - (2 * pixelWidth);
    x1 = (mVertical) ? x0 : mRect.width;
    y1 = (mVertical) ? mRect.height : y0;
    aRenderingContext.DrawLine(x0, y0, x1, y1);
  }

  if (widthInPixels >= 1) {
    aRenderingContext.SetColor(fgColor);
    x0 = (mVertical) ? mRect.width - pixelWidth : 0;
    y0 = (mVertical) ? 0 : mRect.height - pixelWidth;
    x1 = (mVertical) ? x0 : mRect.width;
    y1 = (mVertical) ? mRect.height : y0;
    aRenderingContext.DrawLine(x0, y0, x1, y1);
  }

  return NS_OK;
}


NS_IMETHODIMP
nsHTMLFramesetBorderFrame::HandleEvent(nsIPresContext* aPresContext, 
                                       nsGUIEvent*     aEvent,
                                       nsEventStatus*  aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);
  *aEventStatus = nsEventStatus_eIgnore;

  //XXX Mouse setting logic removed.  The remaining logic should also move.
  if (!mCanResize) {
    return NS_OK;
  }

  switch (aEvent->message) {
    case NS_MOUSE_LEFT_BUTTON_DOWN:
      nsHTMLFramesetFrame* parentFrame = nsnull;
      GetParent((nsIFrame**)&parentFrame);
      parentFrame->StartMouseDrag(aPresContext, this, aEvent);
      *aEventStatus = nsEventStatus_eConsumeNoDefault;
	    break;
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLFramesetBorderFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                                            const nsPoint& aPoint, 
                                            nsFramePaintLayer aWhichLayer,
                                            nsIFrame**     aFrame)
{
  if ( (aWhichLayer != NS_FRAME_PAINT_LAYER_FOREGROUND) ||
       (!((mState & NS_FRAME_OUTSIDE_CHILDREN) || mRect.Contains(aPoint) )))
  {
    return NS_ERROR_FAILURE;
  }

  *aFrame = this;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFramesetBorderFrame::GetCursor(nsIPresContext* aPresContext,
                                     nsPoint&        aPoint,
                                     PRInt32&        aCursor)
{
  if (!mCanResize) {
    aCursor = NS_STYLE_CURSOR_DEFAULT;
  } else {   
    aCursor = (mVertical) ? NS_STYLE_CURSOR_W_RESIZE : NS_STYLE_CURSOR_N_RESIZE;
  }
  return NS_OK;
}

#ifdef DEBUG
NS_IMETHODIMP nsHTMLFramesetBorderFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("FramesetBorder"), aResult);
}
#endif

/*******************************************************************************
 * nsHTMLFramesetBlankFrame
 ******************************************************************************/

nsHTMLFramesetBlankFrame::~nsHTMLFramesetBlankFrame()
{
  //printf("nsHTMLFramesetBlankFrame destructor %p \n", this);
}

void nsHTMLFramesetBlankFrame::GetDesiredSize(nsIPresContext*          aPresContext,
                                              const nsHTMLReflowState& aReflowState,
                                              nsHTMLReflowMetrics&     aDesiredSize)
{
  aDesiredSize.width   = aReflowState.availableWidth;
  aDesiredSize.height  = aReflowState.availableHeight;
  aDesiredSize.ascent  = aDesiredSize.width;
  aDesiredSize.descent = 0;
}

NS_IMETHODIMP
nsHTMLFramesetBlankFrame::Reflow(nsIPresContext*          aPresContext,
                                 nsHTMLReflowMetrics&     aDesiredSize,
                                 const nsHTMLReflowState& aReflowState,
                                 nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsHTMLFramesetBlankFrame", aReflowState.reason);
  GetDesiredSize(aPresContext, aReflowState, aDesiredSize);
  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}

NS_METHOD
nsHTMLFramesetBlankFrame::Paint(nsIPresContext*      aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                const nsRect&        aDirtyRect,
                                nsFramePaintLayer    aWhichLayer,
                                PRUint32             aFlags)
{
  if (NS_FRAME_PAINT_LAYER_FOREGROUND != aWhichLayer) {
    return NS_OK;
  }
  nscolor white = NS_RGB(255,255,255);
  aRenderingContext.SetColor (white);
  // XXX FillRect doesn't seem to work
  //aRenderingContext.FillRect (mRect);

  float p2t;
  aPresContext->GetPixelsToTwips(&p2t);
  nscoord x0 = 0;
  nscoord y0 = 0;
  nscoord x1 = x0;
  nscoord y1 = mRect.height;
  nscoord pixel = NSIntPixelsToTwips(1, p2t);

  aRenderingContext.SetColor(white);
  for (int i = 0; i < mRect.width; i += pixel) {
    aRenderingContext.DrawLine (x0, y0, x1, y1);
    x0 += NSIntPixelsToTwips(1, p2t);
    x1 =  x0;
  }

  return NS_OK;
}

#ifdef DEBUG
NS_IMETHODIMP nsHTMLFramesetBlankFrame::List(nsIPresContext* aPresContext,
                                             FILE*   out, 
                                             PRInt32 aIndent) const
{
  for (PRInt32 i = aIndent; --i >= 0; ) fputs("  ", out);   // Indent
  fprintf(out, "%p BLANK \n", this);
  return nsLeafFrame::List(aPresContext, out, aIndent);
}
#endif
