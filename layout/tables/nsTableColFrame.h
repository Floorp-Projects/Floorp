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
#ifndef nsTableColFrame_h__
#define nsTableColFrame_h__

#include "nscore.h"
#include "nsContainerFrame.h"

class nsVoidArray;
class nsTableCellFrame;

// this is used to index arrays of widths in nsColFrame and to group important widths
// for calculations. It is important that the order: min, desired, fixed be maintained
// for each category (con, adj).
// XXX MIN_ADJ, DES_ADJ, PCT_ADJ, DES_PRO can probably go away and be replaced
// by MIN_CON, DES_CON, PCT_CON, DES_CON saving 16 bytes per col frame
#define WIDTH_NOT_SET   -1
#define NUM_WIDTHS      10
#define NUM_MAJOR_WIDTHS 3 // MIN, DES, FIX
#define MIN_CON          0 // minimum width required of the content + padding
#define DES_CON          1 // desired width of the content + padding
#define FIX              2 // fixed width either from the content or cell, col, etc. + padding
#define MIN_ADJ          3 // minimum width + padding due to col spans
#define DES_ADJ          4 // desired width + padding due to col spans
#define FIX_ADJ          5 // fixed width + padding due to col spans
#define PCT              6 // percent width of cell or col 
#define PCT_ADJ          7 // percent width of cell or col from percent colspan
#define MIN_PRO          8 // desired width due to proportional <col>s or cols attribute
#define FINAL            9 // width after the table has been balanced, considering all of the others
enum nsColConstraint {
  eNoConstraint          = 0,
  ePixelConstraint       = 1,      // pixel width 
  ePercentConstraint     = 2,      // percent width
  eProportionConstraint  = 3,      // 1*, 2*, etc. cols attribute assigns 1*
  e0ProportionConstraint = 4       // 0*, means to force to min width
};

enum nsTableColType {
  eColContent            = 0, // there is real col content associated   
  eColAnonymousCol       = 1, // the result of a span on a col
  eColAnonymousColGroup  = 2, // the result of a span on a col group
  eColAnonymousCell      = 3 // the result of a cell alone
};

class nsTableColFrame : public nsFrame {
public:

  enum {eWIDTH_SOURCE_NONE          =0,   // no cell has contributed to the width style
        eWIDTH_SOURCE_CELL          =1,   // a cell specified a width
        eWIDTH_SOURCE_CELL_WITH_SPAN=2    // a cell implicitly specified a width via colspan
  };

  nsTableColType GetType() const;
  void SetType(nsTableColType aType);

  /** instantiate a new instance of nsTableColFrame.
    * @param aResult    the new object is returned in this out-param
    * @param aContent   the table object to map
    * @param aParent    the parent of the new frame
    *
    * @return  NS_OK if the frame was properly allocated, otherwise an error code
    */
  friend nsresult 
  NS_NewTableColFrame(nsIPresShell* aPresShell, nsIFrame** aResult);

  nsStyleCoord GetStyleWidth() const;

  PRInt32 GetColIndex() const;
  
  void SetColIndex (PRInt32 aColIndex);

  NS_IMETHOD Init(nsIPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext,
                  nsIFrame*        aPrevInFlow);

  NS_IMETHOD Paint(nsIPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer,
                   PRUint32             aFlags = 0);

  NS_IMETHOD GetFrameForPoint(nsIPresContext* aPresContext,
                              const nsPoint& aPoint, 
                              nsFramePaintLayer aWhichLayer,
                              nsIFrame**     aFrame);

  NS_IMETHOD Reflow(nsIPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::tableColFrame
   */
  NS_IMETHOD GetFrameType(nsIAtom** aType) const;
  
#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const;
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
#endif

  /** return the number of the columns the col represents.  always >= 0 */
  virtual PRInt32 GetSpan ();

  /** convenience method, calls into cellmap */
  nsVoidArray * GetCells();

  nscoord GetWidth(PRUint32 aWidthType);
  void    SetWidth(PRUint32 aWidthType,
                   nscoord  aWidth);
  nscoord GetMinWidth();
  nscoord GetDesWidth();
  nscoord GetFixWidth();
  nscoord GetPctWidth();

  void            SetConstraint(nsColConstraint aConstraint);
  nsColConstraint GetConstraint() const;

  void              SetConstrainingCell(nsTableCellFrame* aCellFrame);
  nsTableCellFrame* GetConstrainingCell() const;

  /** convenience method, calls into cellmap */
  PRInt32 Count() const;

  /** Return true if this col was constructed implicitly due to cells needing a col.
    * Return false if this col was constructed due to content having display type of table-col
    */
  PRBool IsAnonymous();
  void   SetIsAnonymous(PRBool aValue);

  void ResetSizingInfo();

  void Dump(PRInt32 aIndent);

protected:

  struct ColBits {
    unsigned mType:4;
    unsigned mIsAnonymous:1;
    unsigned mUnused:27;                         
  } mBits;

  nsTableColFrame();
  ~nsTableColFrame();

  /** the starting index of the column (starting at 0) that this col object represents */
  PRInt32           mColIndex;

  // Widths including MIN_CON, DES_CON, FIX_CON, MIN_ADJ, DES_ADJ, FIX_ADJ, PCT, PCT_ADJ, MIN_PRO
  nscoord           mWidths[NUM_WIDTHS];
  nsColConstraint   mConstraint;
  nsTableCellFrame* mConstrainingCell;
};

inline PRInt32 nsTableColFrame::GetColIndex() const
{ return mColIndex; }

inline void nsTableColFrame::SetColIndex (PRInt32 aColIndex)
{ mColIndex = aColIndex; }

inline nsColConstraint nsTableColFrame::GetConstraint() const
{ return mConstraint; }

inline void nsTableColFrame::SetConstraint(nsColConstraint aConstraint)
{  mConstraint = aConstraint;}

inline PRBool nsTableColFrame::IsAnonymous()
{   return (PRBool)mBits.mIsAnonymous; }

inline void nsTableColFrame::SetIsAnonymous(PRBool aIsAnonymous)
{   mBits.mIsAnonymous = (unsigned)aIsAnonymous; }

inline void nsTableColFrame::SetConstrainingCell(nsTableCellFrame* aCellFrame) 
{ mConstrainingCell = aCellFrame; }

inline nsTableCellFrame* nsTableColFrame::GetConstrainingCell() const 
{ return mConstrainingCell; }


#endif

