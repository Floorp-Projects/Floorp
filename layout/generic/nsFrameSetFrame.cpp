/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* rendering object for HTML <frameset> elements */

#include "nsCOMPtr.h"
#include "nsFrameSetFrame.h"
#include "nsContentUtils.h"
#include "nsGenericHTMLElement.h"
#include "nsLeafFrame.h"
#include "nsHTMLContainerFrame.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsIComponentManager.h"
#include "nsIStreamListener.h"
#include "nsIURL.h"
#include "nsIDocument.h"
#include "nsINodeInfo.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsWidgetsCID.h"
#include "nsGkAtoms.h"
#include "nsIScrollableView.h"
#include "nsStyleCoord.h"
#include "nsStyleConsts.h"
#include "nsStyleContext.h"
#include "nsHTMLParts.h"
#include "nsILookAndFeel.h"
#include "nsWidgetsCID.h"
#include "nsIComponentManager.h"
#include "nsGUIEvent.h"
#include "nsIRenderingContext.h"
#include "nsIServiceManager.h"
#include "nsIDOMMutationEvent.h"
#include "nsINameSpaceManager.h"
#include "nsCSSPseudoElements.h"
#include "nsCSSAnonBoxes.h"
#include "nsAutoPtr.h"
#include "nsStyleSet.h"
#include "nsIContent.h"
#include "nsDisplayList.h"
#include "nsNodeUtils.h"

// masks for mEdgeVisibility
#define LEFT_VIS   0x0001
#define RIGHT_VIS  0x0002
#define TOP_VIS    0x0004
#define BOTTOM_VIS 0x0008
#define ALL_VIS    0x000F
#define NONE_VIS   0x0000

static NS_DEFINE_CID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);

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

  NS_IMETHOD HandleEvent(nsPresContext* aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus* aEventStatus);

  NS_IMETHOD GetCursor(const nsPoint&    aPoint,
                       nsIFrame::Cursor& aCursor);
  
  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);

  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  PRBool GetVisibility() { return mVisibility || mVisibilityOverride; }
  void SetVisibility(PRBool aVisibility);
  void SetColor(nscolor aColor);

  void PaintBorder(nsIRenderingContext& aRenderingContext, nsPoint aPt);

protected:
  nsHTMLFramesetBorderFrame(nsStyleContext* aContext, PRInt32 aWidth, PRBool aVertical, PRBool aVisible);
  virtual ~nsHTMLFramesetBorderFrame();
  virtual nscoord GetIntrinsicWidth();

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
  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;
#endif

  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);

  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

protected:
  nsHTMLFramesetBlankFrame(nsStyleContext* aContext) : nsLeafFrame(aContext) {}
  virtual ~nsHTMLFramesetBlankFrame();
  virtual nscoord GetIntrinsicWidth();

  friend class nsHTMLFramesetFrame;
  friend class nsHTMLFrameset;
};

/*******************************************************************************
 * nsHTMLFramesetFrame
 ******************************************************************************/
PRBool  nsHTMLFramesetFrame::gDragInProgress = PR_FALSE;
#define kFrameResizePref "layout.frames.force_resizability"
#define DEFAULT_BORDER_WIDTH_PX 6

nsHTMLFramesetFrame::nsHTMLFramesetFrame(nsStyleContext* aContext)
  : nsHTMLContainerFrame(aContext)
{
  mNumRows             = 0;
  mRowSizes            = nsnull;
  mNumCols             = 0;
  mColSizes            = nsnull;
  mEdgeVisibility      = 0;
  mParentFrameborder   = eFrameborder_Yes; // default
  mParentBorderWidth   = -1; // default not set
  mParentBorderColor   = NO_COLOR; // default not set
  mFirstDragPoint.x     = mFirstDragPoint.y = 0;
  mMinDrag             = nsPresContext::CSSPixelsToAppUnits(2);
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
}

nsHTMLFramesetFrame::~nsHTMLFramesetFrame()
{
  delete[] mRowSizes;
  delete[] mColSizes;
  delete[] mVerBorders;
  delete[] mHorBorders;
  delete[] mChildTypes;
  delete[] mChildFrameborder;
  delete[] mChildBorderColors;

  nsContentUtils::UnregisterPrefCallback(kFrameResizePref,
                                         FrameResizePrefCallback, this);
}

nsresult nsHTMLFramesetFrame::QueryInterface(const nsIID& aIID, 
                                             void**       aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aIID.Equals(NS_GET_IID(nsHTMLFramesetFrame))) {
    *aInstancePtr = (void*)this;
    return NS_OK;
  } 

  return nsHTMLContainerFrame::QueryInterface(aIID, aInstancePtr);
}

// static
int
nsHTMLFramesetFrame::FrameResizePrefCallback(const char* aPref, void* aClosure)
{
  nsHTMLFramesetFrame *frame =
    NS_REINTERPRET_CAST(nsHTMLFramesetFrame *, aClosure);

  nsIDocument* doc = frame->mContent->GetDocument();
  mozAutoDocUpdate updateBatch(doc, UPDATE_CONTENT_MODEL, PR_TRUE);
  if (doc) {
    doc->AttributeWillChange(frame->mContent,
                             kNameSpaceID_None,
                             nsGkAtoms::frameborder);
  }

  frame->mForceFrameResizability =
    nsContentUtils::GetBoolPref(kFrameResizePref,
                                frame->mForceFrameResizability);

  frame->RecalculateBorderResize();
  if (doc) {
    nsNodeUtils::AttributeChanged(frame->GetContent(),
                                  kNameSpaceID_None,
                                  nsGkAtoms::frameborder,
                                  nsIDOMMutationEvent::MODIFICATION);
  }

  return 0;
}

#define FRAMESET 0
#define FRAME 1
#define BLANK 2

NS_IMETHODIMP
nsHTMLFramesetFrame::Init(nsIContent*      aContent,
                          nsIFrame*        aParent,
                          nsIFrame*        aPrevInFlow)
{
  nsHTMLContainerFrame::Init(aContent, aParent, aPrevInFlow);
  // find the highest ancestor that is a frameset
  nsresult rv = NS_OK;
  nsIFrame* parentFrame = GetParent();
  mTopLevelFrameset = (nsHTMLFramesetFrame*)this;
  while (parentFrame) {
    nsHTMLFramesetFrame* frameset = nsnull;
    CallQueryInterface(parentFrame, &frameset);
    
    if (frameset) {
      mTopLevelFrameset = frameset;
      parentFrame = parentFrame->GetParent();
    } else {
      break;
    }
  }

  nsPresContext* aPresContext = PresContext();

  // create the view. a view is needed since it needs to be a mouse grabber
  nsIViewManager* viewMan = aPresContext->GetViewManager();

  nsIView *parView = GetAncestorWithView()->GetView();
  nsRect boundBox(0, 0, 0, 0); 
  nsIView* view = viewMan->CreateView(boundBox, parView);
  if (!view)
    return NS_ERROR_OUT_OF_MEMORY;

  // XXX Put it last in document order until we can do better
  viewMan->InsertChild(parView, view, nsnull, PR_TRUE);
  SetView(view);

  nsIPresShell *shell = aPresContext->PresShell();
  
  nsFrameborder  frameborder = GetFrameBorder();
  PRInt32 borderWidth = GetBorderWidth(aPresContext, PR_FALSE);
  nscolor borderColor = GetBorderColor();
 
  // Get the rows= cols= data
  nsCOMPtr<nsIFrameSetElement> ourContent(do_QueryInterface(mContent));
  NS_ASSERTION(ourContent, "Someone gave us a broken frameset element!");
  const nsFramesetSpec* rowSpecs = nsnull;
  const nsFramesetSpec* colSpecs = nsnull;
  nsresult result = ourContent->GetRowSpec(&mNumRows, &rowSpecs);
  NS_ENSURE_SUCCESS(result, result);
  result = ourContent->GetColSpec(&mNumCols, &colSpecs);
  NS_ENSURE_SUCCESS(result, result);
  mRowSizes  = new nscoord[mNumRows];
  mColSizes  = new nscoord[mNumCols];
  if (!mRowSizes || !mColSizes)
    return NS_ERROR_OUT_OF_MEMORY; 

  PRInt32 numCells = mNumRows*mNumCols;

  mVerBorders    = new nsHTMLFramesetBorderFrame*[mNumCols];  // 1 more than number of ver borders
  if (!mVerBorders)
    return NS_ERROR_OUT_OF_MEMORY;

  for (int verX  = 0; verX < mNumCols; verX++)
    mVerBorders[verX]    = nsnull;

  mHorBorders    = new nsHTMLFramesetBorderFrame*[mNumRows];  // 1 more than number of hor borders
  if (!mHorBorders)
    return NS_ERROR_OUT_OF_MEMORY;

  for (int horX = 0; horX < mNumRows; horX++)
    mHorBorders[horX]    = nsnull;
     
  mChildTypes = new PRInt32[numCells]; 
  mChildFrameborder  = new nsFrameborder[numCells]; 
  mChildBorderColors  = new nsBorderColor[numCells]; 
  if (!mChildTypes || !mChildFrameborder || !mChildBorderColors)
    return NS_ERROR_OUT_OF_MEMORY;

  // create the children frames; skip content which isn't <frameset> or <frame>
  nsIFrame* lastChild = nsnull;
  mChildCount = 0; // number of <frame> or <frameset> children
  nsIFrame* frame;

  // number of any type of children
  PRUint32 numChildren = mContent->GetChildCount();

  for (PRUint32 childX = 0; childX < numChildren; childX++) {
    if (mChildCount == numCells) { // we have more <frame> or <frameset> than cells
      break;
    }
    nsIContent *child = mContent->GetChildAt(childX);

    // IMPORTANT: This must match the conditions in
    // nsCSSFrameConstructor::ContentAppended/Inserted/Removed    
    if (!child->IsNodeOfType(nsINode::eHTML))
      continue;

    nsIAtom *tag = child->Tag();
    if (tag == nsGkAtoms::frameset || tag == nsGkAtoms::frame) {
      nsRefPtr<nsStyleContext> kidSC;
      nsresult result;

      kidSC = shell->StyleSet()->ResolveStyleFor(child, mStyleContext);
      if (tag == nsGkAtoms::frameset) {
        frame = NS_NewHTMLFramesetFrame(shell, kidSC);
        if (NS_UNLIKELY(!frame))
          return NS_ERROR_OUT_OF_MEMORY;

        mChildTypes[mChildCount] = FRAMESET;
        nsHTMLFramesetFrame* childFrame = (nsHTMLFramesetFrame*)frame;
        childFrame->SetParentFrameborder(frameborder);
        childFrame->SetParentBorderWidth(borderWidth);
        childFrame->SetParentBorderColor(borderColor);
        result = frame->Init(child, this, nsnull);
        if (NS_FAILED(result)) {
          frame->Destroy();
          return result;
        }

        mChildBorderColors[mChildCount].Set(childFrame->GetBorderColor());
      } else { // frame
        frame = NS_NewSubDocumentFrame(shell, kidSC);
        if (NS_UNLIKELY(!frame))
          return NS_ERROR_OUT_OF_MEMORY;

        result = frame->Init(child, this, nsnull);
        if (NS_FAILED(result)) {
          frame->Destroy();
          return result;
        }

        mChildTypes[mChildCount] = FRAME;
        
        mChildFrameborder[mChildCount] = GetFrameBorder(child);
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
    nsRefPtr<nsStyleContext> pseudoStyleContext;
    pseudoStyleContext = shell->StyleSet()->
      ResolvePseudoStyleFor(nsnull, nsCSSAnonBoxes::framesetBlank, mStyleContext);
    if (!pseudoStyleContext) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    // XXX the blank frame is using the content of its parent - at some point it 
    // should just have null content, if we support that                                                            
    nsHTMLFramesetBlankFrame* blankFrame = new (shell) nsHTMLFramesetBlankFrame(pseudoStyleContext);
    if (!blankFrame)
      return NS_ERROR_OUT_OF_MEMORY;

    result = blankFrame->Init(mContent, this, nsnull);
    if (NS_FAILED(result)) {
      blankFrame->Destroy();
      return result;
    }
   
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

  if (actual > 0) {
    float factor = (float)aDesired / (float)actual;
    actual = 0;
    // scale the items up or down
    for (i = 0; i < aNumIndicies; i++) {
      j = aIndicies[i];
      aItems[j] = NSToCoordRound((float)aItems[j] * factor);
      actual += aItems[j];
    }
  } else if (aNumIndicies != 0) {
    // All the specs say zero width, but we have to fill up space
    // somehow.  Distribute it equally.
    nscoord width = NSToCoordRound((float)aDesired / (float)aNumIndicies);
    actual = width * aNumIndicies;
    for (i = 0; i < aNumIndicies; i++) {
      aItems[aIndicies[i]] = width;
    }
  }

  if (aNumIndicies > 0 && aDesired != actual) {
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
void nsHTMLFramesetFrame::CalculateRowCol(nsPresContext*       aPresContext, 
                                          nscoord               aSize, 
                                          PRInt32               aNumSpecs, 
                                          const nsFramesetSpec* aSpecs, 
                                          nscoord*              aValues)
{
  PRInt32  fixedTotal = 0;
  PRInt32  numFixed = 0;
  nsAutoArrayPtr<PRInt32> fixed(new PRInt32[aNumSpecs]);
  PRInt32  numPercent = 0;
  nsAutoArrayPtr<PRInt32> percent(new PRInt32[aNumSpecs]);
  PRInt32  relativeSums = 0;
  PRInt32  numRelative = 0;
  nsAutoArrayPtr<PRInt32> relative(new PRInt32[aNumSpecs]);

  if (NS_UNLIKELY(!fixed || !percent || !relative)) {
    return; // NS_ERROR_OUT_OF_MEMORY
  }

  PRInt32 i, j;
 
  // initialize the fixed, percent, relative indices, allocate the fixed sizes and zero the others
  for (i = 0; i < aNumSpecs; i++) {   
    aValues[i] = 0;
    switch (aSpecs[i].mUnit) {
      case eFramesetUnit_Fixed:
        aValues[i] = nsPresContext::CSSPixelsToAppUnits(aSpecs[i].mValue);
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
}


/**
  * Translate the rows/cols integer sizes into an array of specs for
  * each cell in the frameset.  Reverse of CalculateRowCol() behaviour.
  * This allows us to maintain the user size info through reflows.
  */
void nsHTMLFramesetFrame::GenerateRowCol(nsPresContext*       aPresContext, 
                                         nscoord               aSize, 
                                         PRInt32               aNumSpecs, 
                                         const nsFramesetSpec* aSpecs,
                                         nscoord*              aValues,
                                         nsString&             aNewAttr)
{
  PRInt32 i;
 
  for (i = 0; i < aNumSpecs; i++) {
    if (!aNewAttr.IsEmpty())
      aNewAttr.Append(PRUnichar(','));
    
    switch (aSpecs[i].mUnit) {
      case eFramesetUnit_Fixed:
        aNewAttr.AppendInt(nsPresContext::AppUnitsToIntCSSPixels(aValues[i]));
        break;
      case eFramesetUnit_Percent: // XXX Only accurate to 1%, need 1 pixel
      case eFramesetUnit_Relative:
        // Add 0.5 to the percentage to make rounding work right.
        aNewAttr.AppendInt(PRUint32((100.0*aValues[i])/aSize + 0.5)); 
        aNewAttr.Append(PRUnichar('%'));
        break;
    }
  }
}

PRInt32 nsHTMLFramesetFrame::GetBorderWidth(nsPresContext* aPresContext,
                                            PRBool aTakeForcingIntoAccount)
{
  PRBool forcing = mForceFrameResizability && aTakeForcingIntoAccount;
  
  if (!forcing) {
    nsFrameborder frameborder = GetFrameBorder();
    if (frameborder == eFrameborder_No) {
      return 0;
    }
  }
  nsGenericHTMLElement *content = nsGenericHTMLElement::FromContent(mContent);

  if (content) {
    const nsAttrValue* attr = content->GetParsedAttr(nsGkAtoms::border);
    if (attr) {
      PRInt32 intVal = 0;
      if (attr->Type() == nsAttrValue::eInteger) {
        intVal = attr->GetIntegerValue();
        if (intVal < 0) {
          intVal = 0;
        }
      }

      if (forcing && intVal == 0) {
        intVal = DEFAULT_BORDER_WIDTH_PX;
      }
      return nsPresContext::CSSPixelsToAppUnits(intVal);
    }
  }

  if (mParentBorderWidth > 0 ||
      (mParentBorderWidth == 0 && !forcing)) {
    return mParentBorderWidth;
  }

  return nsPresContext::CSSPixelsToAppUnits(DEFAULT_BORDER_WIDTH_PX);
}


PRIntn
nsHTMLFramesetFrame::GetSkipSides() const
{
  return 0;
}

void 
nsHTMLFramesetFrame::GetDesiredSize(nsPresContext*          aPresContext,
                                    const nsHTMLReflowState& aReflowState,
                                    nsHTMLReflowMetrics&     aDesiredSize)
{
  nsHTMLFramesetFrame* framesetParent = GetFramesetParent(this);
  if (nsnull == framesetParent) {
    if (aPresContext->IsPaginated()) {
      // XXX This needs to be changed when framesets paginate properly
      aDesiredSize.width = aReflowState.availableWidth;
      aDesiredSize.height = aReflowState.availableHeight;
    } else {
      nsRect area = aPresContext->GetVisibleArea();

      aDesiredSize.width = area.width;
      aDesiredSize.height= area.height;
    }
  } else {
    nsSize size;
    framesetParent->GetSizeOfChild(this, size);
    aDesiredSize.width  = size.width;
    aDesiredSize.height = size.height;
  } 
}


nsHTMLFramesetFrame* nsHTMLFramesetFrame::GetFramesetParent(nsIFrame* aChild)
{
  nsHTMLFramesetFrame* parent = nsnull;
  nsIContent* content = aChild->GetContent();

  if (content) { 
    nsCOMPtr<nsIContent> contentParent = content->GetParent();

    if (contentParent && contentParent->IsNodeOfType(nsINode::eHTML) &&
        contentParent->Tag() == nsGkAtoms::frameset) {
      nsIFrame* fptr = aChild->GetParent();
      parent = (nsHTMLFramesetFrame*) fptr;
    }
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
       child = child->GetNextSibling()) {
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

  
NS_METHOD nsHTMLFramesetFrame::HandleEvent(nsPresContext* aPresContext, 
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
      case NS_MOUSE_BUTTON_UP:
        if (aEvent->eventStructType == NS_MOUSE_EVENT &&
            NS_STATIC_CAST(nsMouseEvent*, aEvent)->button ==
              nsMouseEvent::eLeftButton) {
          EndMouseDrag(aPresContext);
        }
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
  nsIView* view = GetView();
  if (view) {
    nsIViewManager* viewMan = view->GetViewManager();
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
nsHTMLFramesetFrame::GetCursor(const nsPoint&    aPoint,
                               nsIFrame::Cursor& aCursor)
{
  if (mDragger) {
    aCursor.mCursor = (mDragger->mVertical) ? NS_STYLE_CURSOR_W_RESIZE : NS_STYLE_CURSOR_N_RESIZE;
  } else {
    aCursor.mCursor = NS_STYLE_CURSOR_DEFAULT;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFramesetFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                      const nsRect&           aDirtyRect,
                                      const nsDisplayListSet& aLists)
{
  nsresult rv = nsHTMLContainerFrame::BuildDisplayList(aBuilder, aDirtyRect, aLists);
  NS_ENSURE_SUCCESS(rv, rv);
  
  if (mDragger && aBuilder->IsForEventDelivery()) {
    // REVIEW: GetFrameForPoint would always target ourselves if mDragger set
    nsDisplayItem* item = new (aBuilder) nsDisplayEventReceiver(this);
    if (!item)
      return NS_ERROR_OUT_OF_MEMORY;
    aLists.Content()->AppendToTop(item);
  }
  return rv;
}

void 
nsHTMLFramesetFrame::ReflowPlaceChild(nsIFrame*                aChild,
                                      nsPresContext*          aPresContext,
                                      const nsHTMLReflowState& aReflowState,
                                      nsPoint&                 aOffset,
                                      nsSize&                  aSize,
                                      nsPoint*                 aCellIndex)
{
  // reflow the child
  nsHTMLReflowState  reflowState(aPresContext, aReflowState, aChild, aSize);
  nsHTMLReflowMetrics metrics;
  metrics.width = aSize.width;
  metrics.height= aSize.height;
  nsReflowStatus status;
  
  ReflowChild(aChild, aPresContext, metrics, reflowState, aOffset.x,
              aOffset.y, 0, status);
  NS_ASSERTION(NS_FRAME_IS_COMPLETE(status), "bad status");
  
  // Place and size the child
  metrics.width = aSize.width;
  metrics.height = aSize.height;
  FinishReflowChild(aChild, aPresContext, nsnull, metrics, aOffset.x, aOffset.y, 0);
}

static
nsFrameborder GetFrameBorderHelper(nsGenericHTMLElement* aContent)
{
  if (nsnull != aContent) {
    const nsAttrValue* attr = aContent->GetParsedAttr(nsGkAtoms::frameborder);
    if (attr && attr->Type() == nsAttrValue::eEnum) {
      switch (attr->GetEnumValue())
      {
        case NS_STYLE_FRAME_YES:
        case NS_STYLE_FRAME_1:
          return eFrameborder_Yes;
          break;

        case NS_STYLE_FRAME_NO:
        case NS_STYLE_FRAME_0:
          return eFrameborder_No;
          break;
      }
    }
  }
  return eFrameborder_Notset;
}

nsFrameborder nsHTMLFramesetFrame::GetFrameBorder() 
{
  nsFrameborder result = eFrameborder_Notset;
  nsGenericHTMLElement *content = nsGenericHTMLElement::FromContent(mContent);

  if (content) {
    result = GetFrameBorderHelper(content);
  }
  if (eFrameborder_Notset == result) {
    return mParentFrameborder;
  }
  return result;
}

nsFrameborder nsHTMLFramesetFrame::GetFrameBorder(nsIContent* aContent)
{
  nsFrameborder result = eFrameborder_Notset;

  nsGenericHTMLElement *content = nsGenericHTMLElement::FromContent(aContent);

  if (content) {
    result = GetFrameBorderHelper(content);
  }
  if (eFrameborder_Notset == result) {
    return GetFrameBorder();
  }
  return result;
}

nscolor nsHTMLFramesetFrame::GetBorderColor() 
{
  nsGenericHTMLElement *content = nsGenericHTMLElement::FromContent(mContent);

  if (content) {
    const nsAttrValue* attr = content->GetParsedAttr(nsGkAtoms::bordercolor);
    if (attr) {
      nscolor color;
      if (attr->GetColorValue(color)) {
        return color;
      }
    }
  }

  return mParentBorderColor;
}

nscolor nsHTMLFramesetFrame::GetBorderColor(nsIContent* aContent) 
{
  nsGenericHTMLElement *content = nsGenericHTMLElement::FromContent(aContent);

  if (content) {
    const nsAttrValue* attr = content->GetParsedAttr(nsGkAtoms::bordercolor);
    if (attr) {
      nscolor color;
      if (attr->GetColorValue(color)) {
        return color;
      }
    }
  }
  return GetBorderColor();
}

NS_IMETHODIMP
nsHTMLFramesetFrame::Reflow(nsPresContext*          aPresContext,
                            nsHTMLReflowMetrics&     aDesiredSize,
                            const nsHTMLReflowState& aReflowState,
                            nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsHTMLFramesetFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
  nsIPresShell *shell = aPresContext->PresShell();
  nsStyleSet *styleSet = shell->StyleSet();

  mParent->AddStateBits(NS_FRAME_CONTAINS_RELATIVE_HEIGHT);
  
  //printf("FramesetFrame2::Reflow %X (%d,%d) \n", this, aReflowState.availableWidth, aReflowState.availableHeight); 
  // Always get the size so that the caller knows how big we are
  GetDesiredSize(aPresContext, aReflowState, aDesiredSize);
  
  nscoord width  = (aDesiredSize.width <= aReflowState.availableWidth)
    ? aDesiredSize.width : aReflowState.availableWidth;
  nscoord height = (aDesiredSize.height <= aReflowState.availableHeight)
    ? aDesiredSize.height : aReflowState.availableHeight;

  PRBool firstTime = (GetStateBits() & NS_FRAME_FIRST_REFLOW) != 0;
  if (firstTime) {
    nsContentUtils::RegisterPrefCallback(kFrameResizePref,
                                         FrameResizePrefCallback, this);
    mForceFrameResizability =
      nsContentUtils::GetBoolPref(kFrameResizePref);
  }
  
  // subtract out the width of all of the potential borders. There are
  // only borders between <frame>s. There are none on the edges (e.g the
  // leftmost <frame> has no left border).
  PRInt32 borderWidth = GetBorderWidth(aPresContext, PR_TRUE);

  width  -= (mNumCols - 1) * borderWidth;
  if (width < 0) width = 0;

  height -= (mNumRows - 1) * borderWidth;
  if (height < 0) height = 0;

  nsCOMPtr<nsIFrameSetElement> ourContent(do_QueryInterface(mContent));
  NS_ASSERTION(ourContent, "Someone gave us a broken frameset element!");
  const nsFramesetSpec* rowSpecs = nsnull;
  const nsFramesetSpec* colSpecs = nsnull;
  PRInt32 rows = 0;
  PRInt32 cols = 0;
  ourContent->GetRowSpec(&rows, &rowSpecs);
  ourContent->GetColSpec(&cols, &colSpecs);
  // If the number of cols or rows has changed, the frame for the frameset
  // will be re-created.
  if (mNumRows != rows || mNumCols != cols) {
    aStatus = NS_FRAME_COMPLETE;
    mDrag.UnSet();
    NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
    return NS_OK;
  }

  if (!mDrag.mActive) {
    CalculateRowCol(aPresContext, width, mNumCols, colSpecs, mColSizes);
    CalculateRowCol(aPresContext, height, mNumRows, rowSpecs, mRowSizes);
  }

  nsAutoArrayPtr<PRBool>  verBordersVis; // vertical borders visibility
  nsAutoArrayPtr<nscolor> verBorderColors;
  nsAutoArrayPtr<PRBool>  horBordersVis; // horizontal borders visibility
  nsAutoArrayPtr<nscolor> horBorderColors;
  nscolor                 borderColor = GetBorderColor();
  nsFrameborder           frameborder = GetFrameBorder();

  if (firstTime) {
    verBordersVis = new PRBool[mNumCols];
    NS_ENSURE_TRUE(verBordersVis, NS_ERROR_OUT_OF_MEMORY);
    verBorderColors = new nscolor[mNumCols];
    NS_ENSURE_TRUE(verBorderColors, NS_ERROR_OUT_OF_MEMORY);
    for (int verX  = 0; verX < mNumCols; verX++) {
      verBordersVis[verX] = PR_FALSE;
      verBorderColors[verX] = NO_COLOR;
    }

    horBordersVis = new PRBool[mNumRows];
    NS_ENSURE_TRUE(horBordersVis, NS_ERROR_OUT_OF_MEMORY);
    horBorderColors = new nscolor[mNumRows];
    NS_ENSURE_TRUE(horBorderColors, NS_ERROR_OUT_OF_MEMORY);
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

        nsRefPtr<nsStyleContext> pseudoStyleContext;
        pseudoStyleContext = styleSet->ResolvePseudoStyleFor(mContent,
                                                             nsCSSPseudoElements::horizontalFramesetBorder,
                                                             mStyleContext);

        borderFrame = new (shell) nsHTMLFramesetBorderFrame(pseudoStyleContext,
                                                            borderWidth,
                                                            PR_FALSE,
                                                            PR_FALSE);
        if (NS_LIKELY(borderFrame != nsnull)) {
          borderFrame->Init(mContent, this, nsnull);
          mChildCount++;
          lastChild->SetNextSibling(borderFrame);
          lastChild = borderFrame;
          mHorBorders[cellIndex.y-1] = borderFrame;
          // set the neighbors for determining drag boundaries
          borderFrame->mPrevNeighbor = lastRow;
          borderFrame->mNextNeighbor = cellIndex.y;
        }
      } else {
        borderFrame = (nsHTMLFramesetBorderFrame*)mFrames.FrameAt(borderChildX);
        if (NS_LIKELY(borderFrame != nsnull)) {
          borderFrame->mWidth = borderWidth;
          borderChildX++;
        }
      }
      if (NS_LIKELY(borderFrame != nsnull)) {
        nsSize borderSize(aDesiredSize.width, borderWidth);
        ReflowPlaceChild(borderFrame, aPresContext, aReflowState, offset, borderSize);
        borderFrame = nsnull;
      }
      offset.y += borderWidth;
    } else {
      if (cellIndex.x > 0) {  // moved to next col in same row
        if (0 == cellIndex.y) { // in 1st row
          if (firstTime) { // create vertical border
            
            nsRefPtr<nsStyleContext> pseudoStyleContext;
            pseudoStyleContext = styleSet->ResolvePseudoStyleFor(mContent,
                                                                 nsCSSPseudoElements::verticalFramesetBorder,
                                                                 mStyleContext);

            borderFrame = new (shell) nsHTMLFramesetBorderFrame(pseudoStyleContext, 
                                                                borderWidth,
                                                                PR_TRUE,
                                                                PR_FALSE);
            if (NS_LIKELY(borderFrame != nsnull)) {
              borderFrame->Init(mContent, this, nsnull);
              mChildCount++;
              lastChild->SetNextSibling(borderFrame);
              lastChild = borderFrame;
              mVerBorders[cellIndex.x-1] = borderFrame;
              // set the neighbors for determining drag boundaries
              borderFrame->mPrevNeighbor = lastCol;
              borderFrame->mNextNeighbor = cellIndex.x;
            }
          } else {         
            borderFrame = (nsHTMLFramesetBorderFrame*)mFrames.FrameAt(borderChildX);
            if (NS_LIKELY(borderFrame != nsnull)) {
              borderFrame->mWidth = borderWidth;
              borderChildX++;
            }
          }
          if (NS_LIKELY(borderFrame != nsnull)) {
            nsSize borderSize(borderWidth, aDesiredSize.height);
            ReflowPlaceChild(borderFrame, aPresContext, aReflowState, offset, borderSize);
            borderFrame = nsnull;
          }
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
    child = child->GetNextSibling();
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

    delete[] mChildTypes; 
    delete[] mChildFrameborder;
    delete[] mChildBorderColors;

    mChildTypes = nsnull;
    mChildFrameborder = nsnull;
    mChildBorderColors = nsnull;
  }

  aStatus = NS_FRAME_COMPLETE;
  mDrag.UnSet();

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return NS_OK;
}

nsIAtom*
nsHTMLFramesetFrame::GetType() const
{
  return nsGkAtoms::frameSetFrame;
}

#ifdef DEBUG
NS_IMETHODIMP
nsHTMLFramesetFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Frameset"), aResult);
}
#endif

PRBool
nsHTMLFramesetFrame::IsLeaf() const
{
  // We handle constructing our kids manually
  return PR_TRUE;
}

PRBool 
nsHTMLFramesetFrame::ChildIsFrameset(nsIFrame* aChild) 
{
  nsIFrame* childFrame = nsnull;
  aChild->QueryInterface(NS_GET_IID(nsHTMLFramesetFrame), (void**)&childFrame);
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
  nsIContent* content = aChildFrame->GetContent();

  return content && content->HasAttr(kNameSpaceID_None, nsGkAtoms::noresize);
}

PRBool 
nsHTMLFramesetFrame::CanChildResize(PRBool  aVertical, 
                                    PRBool  aLeft, 
                                    PRInt32 aChildX, 
                                    PRBool  aFrameset) 
{
  nsIFrame* child = mFrames.FrameAt(aChildX);
  if (aFrameset) {
    NS_ASSERTION(ChildIsFrameset(child), "Child frame is not a frameset!");
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
  nsAutoArrayPtr<PRInt32> childTypes(new PRInt32[numCells]);
  if (NS_UNLIKELY(!childTypes)) {
    return;
  }
  PRUint32 childIndex, childTypeIndex = 0;

  // number of any type of children
  PRUint32 numChildren = mContent->GetChildCount();
  for (childIndex = 0; childIndex < numChildren; childIndex++) {
    nsIContent *child = mContent->GetChildAt(childIndex);

    if (child->IsNodeOfType(nsINode::eHTML)) {
      nsINodeInfo *ni = child->NodeInfo();

      if (ni->Equals(nsGkAtoms::frameset)) {
        childTypes[childTypeIndex++] = FRAMESET;
      } else if (ni->Equals(nsGkAtoms::frame)) {
        childTypes[childTypeIndex++] = FRAME;
      }
      // Don't overflow childTypes array
      if (((PRInt32)childTypeIndex) >= numCells) {
        break;
      }
    }
  }
  for (; childTypeIndex < numCells; ++childTypeIndex) {
    childTypes[childTypeIndex] = BLANK;
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
nsHTMLFramesetFrame::StartMouseDrag(nsPresContext*            aPresContext, 
                                    nsHTMLFramesetBorderFrame* aBorder, 
                                    nsGUIEvent*                aEvent)
{
#if 0
  PRInt32 index;
  IndexOf(aBorder, index);
  NS_ASSERTION((nsnull != aBorder) && (index >= 0), "invalid dragger");
#endif
  nsIView* view = GetView();
  if (view) {
    nsIViewManager* viewMan = view->GetViewManager();
    if (viewMan) {
      PRBool ignore;
      viewMan->GrabMouseEvents(view, ignore);
      mDragger = aBorder;

      //XXX This should go away!  Border should have own view instead
      viewMan->SetViewCheckChildEvents(view, PR_FALSE);

      mFirstDragPoint = aEvent->refPoint;

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
nsHTMLFramesetFrame::MouseDrag(nsPresContext* aPresContext, 
                               nsGUIEvent*     aEvent)
{
  PRInt32 change; // measured positive from left-to-right or top-to-bottom
  nsWeakFrame weakFrame(this);
  if (mDragger->mVertical) {
    change = aPresContext->DevPixelsToAppUnits(aEvent->refPoint.x - mFirstDragPoint.x);
    if (change > mNextNeighborOrigSize - mMinDrag) {
      change = mNextNeighborOrigSize - mMinDrag;
    } else if (change <= mMinDrag - mPrevNeighborOrigSize) {
      change = mMinDrag - mPrevNeighborOrigSize;
    }
    mColSizes[mDragger->mPrevNeighbor] = mPrevNeighborOrigSize + change;
    mColSizes[mDragger->mNextNeighbor] = mNextNeighborOrigSize - change;

    if (change != 0) {
      // Recompute the specs from the new sizes.
      nscoord width = mRect.width - (mNumCols - 1) * GetBorderWidth(aPresContext, PR_TRUE);
      nsCOMPtr<nsIFrameSetElement> ourContent(do_QueryInterface(mContent));
      NS_ASSERTION(ourContent, "Someone gave us a broken frameset element!");
      const nsFramesetSpec* colSpecs = nsnull;
      ourContent->GetColSpec(&mNumCols, &colSpecs);
      nsAutoString newColAttr;
      GenerateRowCol(aPresContext, width, mNumCols, colSpecs, mColSizes,
                     newColAttr);
      // Setting the attr will trigger a reflow
      mContent->SetAttr(kNameSpaceID_None, nsGkAtoms::cols, newColAttr, PR_TRUE);
    }
  } else {
    change = aPresContext->DevPixelsToAppUnits(aEvent->refPoint.y - mFirstDragPoint.y);
    if (change > mNextNeighborOrigSize - mMinDrag) {
      change = mNextNeighborOrigSize - mMinDrag;
    } else if (change <= mMinDrag - mPrevNeighborOrigSize) {
      change = mMinDrag - mPrevNeighborOrigSize;
    }
    mRowSizes[mDragger->mPrevNeighbor] = mPrevNeighborOrigSize + change;
    mRowSizes[mDragger->mNextNeighbor] = mNextNeighborOrigSize - change;

    if (change != 0) {
      // Recompute the specs from the new sizes.
      nscoord height = mRect.height - (mNumRows - 1) * GetBorderWidth(aPresContext, PR_TRUE);
      nsCOMPtr<nsIFrameSetElement> ourContent(do_QueryInterface(mContent));
      NS_ASSERTION(ourContent, "Someone gave us a broken frameset element!");
      const nsFramesetSpec* rowSpecs = nsnull;
      ourContent->GetRowSpec(&mNumRows, &rowSpecs);
      nsAutoString newRowAttr;
      GenerateRowCol(aPresContext, height, mNumRows, rowSpecs, mRowSizes,
                     newRowAttr);
      // Setting the attr will trigger a reflow
      mContent->SetAttr(kNameSpaceID_None, nsGkAtoms::rows, newRowAttr, PR_TRUE);
    }
  }

  ENSURE_TRUE(weakFrame.IsAlive());
  if (change != 0) {
    mDrag.Reset(mDragger->mVertical, mDragger->mPrevNeighbor, change, this);
    nsIFrame* parentFrame = GetParent();
    if (!parentFrame) {
      return;
    }

    // Update the view immediately (make drag appear snappier)
    nsIViewManager* vm = aPresContext->GetViewManager();
    if (vm) {
      nsIView* root;
      vm->GetRootView(root);
      if (root) {
        vm->UpdateView(root, NS_VMREFRESH_IMMEDIATE);
      }
    }
  }
}  

void
nsHTMLFramesetFrame::EndMouseDrag(nsPresContext* aPresContext)
{
  nsIView* view = GetView();
  if (view) {
    nsIViewManager* viewMan = view->GetViewManager();
    if (viewMan) {
      mDragger = nsnull;
      PRBool ignore;
      viewMan->GrabMouseEvents(nsnull, ignore);
      //XXX This should go away!  Border should have own view instead
      viewMan->SetViewCheckChildEvents(view, PR_TRUE);
    }
  }
  gDragInProgress = PR_FALSE;
}  

nsIFrame*
NS_NewHTMLFramesetFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsHTMLFramesetFrame(aContext);
}

/*******************************************************************************
 * nsHTMLFramesetBorderFrame
 ******************************************************************************/
nsHTMLFramesetBorderFrame::nsHTMLFramesetBorderFrame(nsStyleContext* aContext,
                                                     PRInt32 aWidth,
                                                     PRBool  aVertical,
                                                     PRBool  aVisibility)
  : nsLeafFrame(aContext), mWidth(aWidth), mVertical(aVertical), mVisibility(aVisibility)
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

nscoord nsHTMLFramesetBorderFrame::GetIntrinsicWidth()
{
  // No intrinsic width
  return 0;
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
nsHTMLFramesetBorderFrame::Reflow(nsPresContext*          aPresContext,
                                  nsHTMLReflowMetrics&     aDesiredSize,
                                  const nsHTMLReflowState& aReflowState,
                                  nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsHTMLFramesetBorderFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  // Override Reflow(), since we don't want to deal with what our
  // computed values are.
  SizeToAvailSize(aReflowState, aDesiredSize);

  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}

class nsDisplayFramesetBorder : public nsDisplayItem {
public:
  nsDisplayFramesetBorder(nsHTMLFramesetBorderFrame* aFrame)
    : nsDisplayItem(aFrame) {
    MOZ_COUNT_CTOR(nsDisplayFramesetBorder);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayFramesetBorder() {
    MOZ_COUNT_DTOR(nsDisplayFramesetBorder);
  }
#endif

  // REVIEW: see old GetFrameForPoint
  // Receives events in its bounds
  virtual nsIFrame* HitTest(nsDisplayListBuilder* aBuilder, nsPoint aPt) { return mFrame; }
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx,
     const nsRect& aDirtyRect);
  NS_DISPLAY_DECL_NAME("FramesetBorder")
};

void nsDisplayFramesetBorder::Paint(nsDisplayListBuilder* aBuilder,
     nsIRenderingContext* aCtx, const nsRect& aDirtyRect)
{
  NS_STATIC_CAST(nsHTMLFramesetBorderFrame*, mFrame)->
    PaintBorder(*aCtx, aBuilder->ToReferenceFrame(mFrame));
}

NS_IMETHODIMP
nsHTMLFramesetBorderFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                            const nsRect&           aDirtyRect,
                                            const nsDisplayListSet& aLists)
{
  nsDisplayItem* item = new (aBuilder) nsDisplayFramesetBorder(this);
  if (!item)
    return NS_ERROR_OUT_OF_MEMORY;
  aLists.Content()->AppendToTop(item);
  return NS_OK;
}

void nsHTMLFramesetBorderFrame::PaintBorder(nsIRenderingContext& aRenderingContext,
                                            nsPoint aPt)
{
  nscolor WHITE    = NS_RGB(255, 255, 255);
  nscolor bgColor  = NS_RGB(200,200,200);
  nscolor fgColor  = NS_RGB(0,0,0);
  nscolor hltColor = NS_RGB(255,255,255);
  nscolor sdwColor = NS_RGB(128,128,128);

  nsIRenderingContext::AutoPushTranslation
    translate(&aRenderingContext, aPt.x, aPt.y);

  {
    nsCOMPtr<nsILookAndFeel> lookAndFeel = do_GetService(kLookAndFeelCID);
    if (lookAndFeel) {
      lookAndFeel->GetColor(nsILookAndFeel::eColor_WidgetBackground,  bgColor);
      lookAndFeel->GetColor(nsILookAndFeel::eColor_WidgetForeground,  fgColor);
      lookAndFeel->GetColor(nsILookAndFeel::eColor_Widget3DShadow,    sdwColor);
      lookAndFeel->GetColor(nsILookAndFeel::eColor_Widget3DHighlight, hltColor);
    }
  }

  nscoord widthInPixels = nsPresContext::AppUnitsToIntCSSPixels(mWidth);
  nscoord pixelWidth    = nsPresContext::CSSPixelsToAppUnits(1);

  if (widthInPixels <= 0)
    return;

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

  if (!mVisibility && !mVisibilityOverride)
    return;

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
}


NS_IMETHODIMP
nsHTMLFramesetBorderFrame::HandleEvent(nsPresContext* aPresContext, 
                                       nsGUIEvent*     aEvent,
                                       nsEventStatus*  aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);
  *aEventStatus = nsEventStatus_eIgnore;

  //XXX Mouse setting logic removed.  The remaining logic should also move.
  if (!mCanResize) {
    return NS_OK;
  }

  if (aEvent->eventStructType == NS_MOUSE_EVENT &&
      aEvent->message == NS_MOUSE_BUTTON_DOWN &&
      NS_STATIC_CAST(nsMouseEvent*, aEvent)->button == nsMouseEvent::eLeftButton) {
    nsHTMLFramesetFrame* parentFrame;
    nsIFrame* fptr = GetParent();
    parentFrame = (nsHTMLFramesetFrame*) fptr;
    parentFrame->StartMouseDrag(aPresContext, this, aEvent);
    *aEventStatus = nsEventStatus_eConsumeNoDefault;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFramesetBorderFrame::GetCursor(const nsPoint&    aPoint,
                                     nsIFrame::Cursor& aCursor)
{
  if (!mCanResize) {
    aCursor.mCursor = NS_STYLE_CURSOR_DEFAULT;
  } else {   
    aCursor.mCursor = (mVertical) ? NS_STYLE_CURSOR_W_RESIZE : NS_STYLE_CURSOR_N_RESIZE;
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

nscoord nsHTMLFramesetBlankFrame::GetIntrinsicWidth()
{
  // No intrinsic width
  return 0;
}

NS_IMETHODIMP
nsHTMLFramesetBlankFrame::Reflow(nsPresContext*          aPresContext,
                                 nsHTMLReflowMetrics&     aDesiredSize,
                                 const nsHTMLReflowState& aReflowState,
                                 nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsHTMLFramesetBlankFrame");

  // Override Reflow(), since we don't want to deal with what our
  // computed values are.
  SizeToAvailSize(aReflowState, aDesiredSize);

  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}

class nsDisplayFramesetBlank : public nsDisplayItem {
public:
  nsDisplayFramesetBlank(nsIFrame* aFrame) : nsDisplayItem(aFrame) {
    MOZ_COUNT_CTOR(nsDisplayFramesetBlank);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayFramesetBlank() {
    MOZ_COUNT_DTOR(nsDisplayFramesetBlank);
  }
#endif

  virtual void Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx,
     const nsRect& aDirtyRect);
  NS_DISPLAY_DECL_NAME("FramesetBlank")
};
void nsDisplayFramesetBlank::Paint(nsDisplayListBuilder* aBuilder,
     nsIRenderingContext* aCtx, const nsRect& aDirtyRect)
{
  nscolor white = NS_RGB(255,255,255);
  aCtx->SetColor (white);
  aCtx->FillRect (nsRect(aBuilder->ToReferenceFrame(mFrame), mFrame->GetSize()));
}

#ifdef DEBUG
NS_IMETHODIMP nsHTMLFramesetBlankFrame::List(FILE*   out, 
                                             PRInt32 aIndent) const
{
  for (PRInt32 i = aIndent; --i >= 0; ) fputs("  ", out);   // Indent
  fprintf(out, "%p BLANK \n", (void*)this);
  return nsLeafFrame::List(out, aIndent);
}
#endif

NS_IMETHODIMP
nsHTMLFramesetBlankFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                           const nsRect&           aDirtyRect,
                                           const nsDisplayListSet& aLists)
{
  nsDisplayItem* item = new (aBuilder) nsDisplayFramesetBlank(this);
  if (!item)
    return NS_ERROR_OUT_OF_MEMORY;
  aLists.Content()->AppendToTop(item);
  return NS_OK;
}
