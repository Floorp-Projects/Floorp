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
#ifndef nsHTMLFrameset_h___
#define nsHTMLFrameset_h___

#include "nsHTMLAtoms.h"
#include "nsHTMLContainerFrame.h"
#include "nsColor.h"

class  nsIContent;
class  nsIFrame;
class  nsIPresContext;
class  nsIRenderingContext;
struct nsRect;
struct nsHTMLReflowState;
struct nsSize;
class  nsIAtom;
class  nsIWebShell;
class  nsHTMLFramesetBorderFrame;
struct nsGUIEvent;
class  nsHTMLFramesetFrame;

#define NS_IFRAMESETFRAME_IID \
{ 0xf47deac0, 0x4200, 0x11d2, { 0x80, 0x3c, 0x0, 0x60, 0x8, 0x15, 0xa7, 0x91 } }

#define NO_COLOR 0xFFFFFFFA

struct nsBorderColor 
{
  nscolor mLeft;
  nscolor mRight;
  nscolor mTop;
  nscolor mBottom;

  nsBorderColor() { Set(NO_COLOR); }
  ~nsBorderColor() {}
  void Set(nscolor aColor) { mLeft = mRight = mTop = mBottom = aColor; }
};

enum nsFramesetUnit {
  eFramesetUnit_Fixed = 0,
  eFramesetUnit_Percent,
  eFramesetUnit_Relative
};

enum nsFrameborder {
  eFrameborder_Yes = 0,
  eFrameborder_No,
  eFrameborder_Notset
};

struct nsFramesetSpec {
  nsFramesetUnit mUnit;
  nscoord        mValue;
};

struct nsFramesetDrag {
  PRBool               mVertical;  // vertical if true, otherwise horizontal
  PRInt32              mIndex;     // index of left col or top row of effected area
  PRInt32              mChange;    // pos for left to right or top to bottom, neg otherwise
  nsHTMLFramesetFrame* mSource;    // frameset whose border was dragged to cause the resize
  PRBool               mActive;

  nsFramesetDrag();
  nsFramesetDrag(PRBool               aVertical, 
                 PRInt32              aIndex, 
                 PRInt32              aChange, 
                 nsHTMLFramesetFrame* aSource); 
  void Reset(PRBool               aVertical, 
             PRInt32              aIndex, 
             PRInt32              aChange, 
             nsHTMLFramesetFrame* aSource); 
  void UnSet();
};

/*******************************************************************************
 * nsHTMLFramesetFrame
 ******************************************************************************/
class nsHTMLFramesetFrame : public nsHTMLContainerFrame {

public:
  nsHTMLFramesetFrame();

  virtual ~nsHTMLFramesetFrame();

  NS_IMETHOD  QueryInterface(const nsIID& aIID, 
                             void**       aInstancePtr);

  NS_IMETHOD Init(nsIPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext,
                  nsIFrame*        aPrevInFlow);

  static PRBool  gDragInProgress;

  static PRInt32 gMaxNumRowColSpecs;

  void GetSizeOfChild(nsIFrame* aChild, nsSize& aSize);

  void GetSizeOfChildAt(PRInt32  aIndexInParent, 
                        nsSize&  aSize, 
                        nsPoint& aCellIndex);

  static nsHTMLFramesetFrame* GetFramesetParent(nsIFrame* aChild);

  NS_IMETHOD HandleEvent(nsIPresContext* aPresContext, 
                         nsGUIEvent*     aEvent,
                         nsEventStatus*  aEventStatus);

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
                   PRUint32             aFlags = 0);

  NS_IMETHOD Reflow(nsIPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD  VerifyTree() const;

  void StartMouseDrag(nsIPresContext*            aPresContext, 
                      nsHTMLFramesetBorderFrame* aBorder, 
                      nsGUIEvent*                aEvent);

  void MouseDrag(nsIPresContext* aPresContext, 
                 nsGUIEvent*     aEvent);

  void EndMouseDrag(nsIPresContext* aPresContext);

  nsFrameborder GetParentFrameborder() { return mParentFrameborder; }

  void SetParentFrameborder(nsFrameborder aValue) { mParentFrameborder = aValue; }

  nsFramesetDrag& GetDrag() { return mDrag; }

  void RecalculateBorderResize();

protected:
  void Scale(nscoord  aDesired, 
             PRInt32  aNumIndicies, 
             PRInt32* aIndicies, 
             PRInt32  aNumItems,
             PRInt32* aItems);

  void CalculateRowCol(nsIPresContext* aPresContext, 
                       nscoord         aSize, 
                       PRInt32         aNumSpecs, 
                       nsFramesetSpec* aSpecs, 
                       nscoord*        aValues);

  void GenerateRowCol(nsIPresContext* aPresContext,
                       nscoord         aSize,
                       PRInt32         aNumSpecs,
                       nsFramesetSpec* aSpecs,
                       nscoord*        aValues);

  virtual void GetDesiredSize(nsIPresContext*          aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics&     aDesiredSize);

  PRInt32 GetBorderWidth(nsIPresContext* aPresContext);

  PRInt32 GetParentBorderWidth() { return mParentBorderWidth; }

  void SetParentBorderWidth(PRInt32 aWidth) { mParentBorderWidth = aWidth; }

  nscolor GetParentBorderColor() { return mParentBorderColor; }

  void SetParentBorderColor(nscolor aColor) { mParentBorderColor = aColor; }

  nsFrameborder GetFrameBorder(PRBool aStandardMode);

  nsFrameborder GetFrameBorder(nsIContent* aContent, PRBool aStandardMode);

  nscolor GetBorderColor();

  nscolor GetBorderColor(nsIContent* aFrameContent);

  PRBool GetNoResize(nsIFrame* aChildFrame); 
  
  virtual PRIntn GetSkipSides() const;

  void ParseRowCol(nsIPresContext* aPresContext, nsIAtom* aAttrType, PRInt32& aNumSpecs, nsFramesetSpec** aSpecs); 

  PRInt32 ParseRowColSpec(nsIPresContext* aPresContext,
                          nsString&       aSpec, 
                          PRInt32         aMaxNumValues,
                          nsFramesetSpec* aSpecs);

  void ReflowPlaceChild(nsIFrame*                aChild,
                        nsIPresContext*          aPresContext,
                        const nsHTMLReflowState& aReflowState,
                        nsPoint&                 aOffset,
                        nsSize&                  aSize,
                        nsPoint*                 aCellIndex = 0);
  
  PRBool CanResize(PRBool aVertical, 
                   PRBool aLeft); 

  PRBool CanChildResize(PRBool  aVertical, 
                        PRBool  aLeft, 
                        PRInt32 aChildX,
                        PRBool  aFrameset);
  
  void SetBorderResize(PRInt32*                   aChildTypes, 
                       nsHTMLFramesetBorderFrame* aBorderFrame);

  PRBool ChildIsFrameset(nsIFrame* aChild); 

  PRInt32          mNumRows;
  nsFramesetSpec*  mRowSpecs;  // parsed, non-computed dimensions
  nscoord*         mRowSizes;  // currently computed row sizes 
  PRInt32          mNumCols;
  nsFramesetSpec*  mColSpecs;  // parsed, non-computed dimensions
  nscoord*         mColSizes;  // currently computed col sizes 
  PRInt32          mNonBorderChildCount; 
  PRInt32          mNonBlankChildCount; 
  PRInt32          mEdgeVisibility;
  nsBorderColor    mEdgeColors;
  nsFrameborder    mParentFrameborder;
  nscolor          mParentBorderColor;
  PRInt32          mParentBorderWidth;

  nsHTMLFramesetBorderFrame* mDragger;
  nsFramesetDrag   mDrag;
  nsPoint          mFirstDragPoint;
  PRInt32          mPrevNeighborOrigSize; // used during resize
  PRInt32          mNextNeighborOrigSize;
  PRInt32          mMinDrag;
  PRInt32          mChildCount;
  nsHTMLFramesetFrame* mTopLevelFrameset;
  nsHTMLFramesetBorderFrame** mVerBorders;  // vertical borders
  nsHTMLFramesetBorderFrame** mHorBorders;  // horizontal borders

  PRInt32*         mChildTypes; // frameset/frame distinction of children  
  nsFrameborder*   mChildFrameborder; // the frameborder attr of children 
  nsBorderColor*   mChildBorderColors;
  
};


#endif
