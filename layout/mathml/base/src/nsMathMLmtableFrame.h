/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla MathML Project.
 *
 * The Initial Developer of the Original Code is
 * The University Of Queensland.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
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

#ifndef nsMathMLmtableFrame_h___
#define nsMathMLmtableFrame_h___

#include "nsCOMPtr.h"
#include "nsMathMLContainerFrame.h"

//
// <mtable> -- table or matrix
//

class nsMathMLmtableOuterFrame : public nsTableOuterFrame,
                                 public nsMathMLFrame
{
public:
  friend nsIFrame* NS_NewMathMLmtableOuterFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  NS_DECL_ISUPPORTS_INHERITED

  // Overloaded nsIMathMLFrame methods

  NS_IMETHOD
  InheritAutomaticData(nsIFrame* aParent);

  NS_IMETHOD
  UpdatePresentationData(PRInt32  aScriptLevelIncrement,
                         PRUint32 aFlagsValues,
                         PRUint32 aWhichFlags);

  NS_IMETHOD
  UpdatePresentationDataFromChildAt(PRInt32         aFirstIndex,
                                    PRInt32         aLastIndex,
                                    PRInt32         aScriptLevelIncrement,
                                    PRUint32        aFlagsValues,
                                    PRUint32        aWhichFlags);

  NS_IMETHOD
  ReResolveScriptStyle(PRInt32 aParentScriptLevel)
  {
    nsMathMLContainerFrame::PropagateScriptStyleFor(this, aParentScriptLevel);
    return NS_OK;
  }

  // overloaded nsTableOuterFrame methods

  NS_IMETHOD
  Init(nsIContent*      aContent,
       nsIFrame*        aParent,
       nsIFrame*        aPrevInFlow);

  NS_IMETHOD
  Reflow(nsPresContext*          aPresContext,
         nsHTMLReflowMetrics&     aDesiredSize,
         const nsHTMLReflowState& aReflowState,
         nsReflowStatus&          aStatus);

  NS_IMETHOD
  AttributeChanged(PRInt32  aNameSpaceID,
                   nsIAtom* aAttribute,
                   PRInt32  aModType);

  virtual PRBool IsFrameOfType(PRUint32 aFlags) const;

protected:
  nsMathMLmtableOuterFrame(nsStyleContext* aContext) : nsTableOuterFrame(aContext) {}
  virtual ~nsMathMLmtableOuterFrame();

  // helper to find the row frame at a given index, positive or negative, e.g.,
  // 1..n means the first row down to the last row, -1..-n means the last row
  // up to the first row. Used for alignments that are relative to a given row
  nsIFrame*
  GetRowFrameAt(nsPresContext* aPresContext,
                PRInt32         aRowIndex);
}; // class nsMathMLmtableOuterFrame

// --------------

class nsMathMLmtableFrame : public nsTableFrame
{
public:
  friend nsIFrame* NS_NewMathMLmtableFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  NS_DECL_ISUPPORTS_INHERITED

  // Overloaded nsTableFrame methods

  NS_IMETHOD
  SetInitialChildList(nsIAtom*  aListName,
                      nsIFrame* aChildList);

  NS_IMETHOD
  AppendFrames(nsIAtom*  aListName,
               nsIFrame* aFrameList)
  {
    nsresult rv = nsTableFrame::AppendFrames(aListName, aFrameList);
    RestyleTable();
    return rv;
  }

  NS_IMETHOD
  InsertFrames(nsIAtom*  aListName,
               nsIFrame* aPrevFrame,
               nsIFrame* aFrameList)
  {
    nsresult rv = nsTableFrame::InsertFrames(aListName, aPrevFrame, aFrameList);
    RestyleTable();
    return rv;
  }

  NS_IMETHOD
  RemoveFrame(nsIAtom*  aListName,
              nsIFrame* aOldFrame)
  {
    nsresult rv = nsTableFrame::RemoveFrame(aListName, aOldFrame);
    RestyleTable();
    return rv;
  }

  virtual PRBool IsFrameOfType(PRUint32 aFlags) const;

  // helper to restyle and reflow the table when a row is changed -- since MathML
  // attributes are inter-dependent and row/colspan can affect the table, it is
  // safer (albeit grossly suboptimal) to just relayout the whole thing.
  void RestyleTable();

protected:
  nsMathMLmtableFrame(nsStyleContext* aContext) : nsTableFrame(aContext) {}
  virtual ~nsMathMLmtableFrame();
}; // class nsMathMLmtableFrame

// --------------

class nsMathMLmtrFrame : public nsTableRowFrame
{
public:
  friend nsIFrame* NS_NewMathMLmtrFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  NS_DECL_ISUPPORTS_INHERITED

  // overloaded nsTableRowFrame methods

  NS_IMETHOD
  Init(nsIContent* aContent,
       nsIFrame*   aParent,
       nsIFrame*   aPrevInFlow);

  NS_IMETHOD
  AttributeChanged(PRInt32  aNameSpaceID,
                   nsIAtom* aAttribute,
                   PRInt32  aModType);

  NS_IMETHOD
  AppendFrames(nsIAtom*  aListName,
               nsIFrame* aFrameList)
  {
    nsresult rv = nsTableRowFrame::AppendFrames(aListName, aFrameList);
    RestyleTable();
    return rv;
  }

  NS_IMETHOD
  InsertFrames(nsIAtom*  aListName,
               nsIFrame* aPrevFrame,
               nsIFrame* aFrameList)
  {
    nsresult rv = nsTableRowFrame::InsertFrames(aListName, aPrevFrame, aFrameList);
    RestyleTable();
    return rv;
  }

  NS_IMETHOD
  RemoveFrame(nsIAtom*  aListName,
              nsIFrame* aOldFrame)
  {
    nsresult rv = nsTableRowFrame::RemoveFrame(aListName, aOldFrame);
    RestyleTable();
    return rv;
  }

  virtual PRBool IsFrameOfType(PRUint32 aFlags) const;

  // helper to restyle and reflow the table -- @see nsMathMLmtableFrame.
  void RestyleTable()
  {
    nsTableFrame* tableFrame = nsTableFrame::GetTableFrame(this);
    if (tableFrame && tableFrame->IsFrameOfType(nsIFrame::eMathML)) {
      // relayout the table
      ((nsMathMLmtableFrame*)tableFrame)->RestyleTable();
    }
  }

protected:
  nsMathMLmtrFrame(nsStyleContext* aContext) : nsTableRowFrame(aContext) {}
  virtual ~nsMathMLmtrFrame();
}; // class nsMathMLmtrFrame

// --------------

class nsMathMLmtdFrame : public nsTableCellFrame
{
public:
  friend nsIFrame* NS_NewMathMLmtdFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  NS_DECL_ISUPPORTS_INHERITED

  // overloaded nsTableCellFrame methods

  NS_IMETHOD
  Init(nsIContent* aContent,
       nsIFrame*   aParent,
       nsIFrame*   aPrevInFlow);

  NS_IMETHOD
  AttributeChanged(PRInt32  aNameSpaceID,
                   nsIAtom* aAttribute,
                   PRInt32  aModType);

  virtual PRInt32 GetRowSpan();
  virtual PRInt32 GetColSpan();
  virtual PRBool IsFrameOfType(PRUint32 aFlags) const;

protected:
  nsMathMLmtdFrame(nsStyleContext* aContext) : nsTableCellFrame(aContext) {}
  virtual ~nsMathMLmtdFrame();
}; // class nsMathMLmtdFrame

// --------------

class nsMathMLmtdInnerFrame : public nsBlockFrame,
                              public nsMathMLFrame {
public:
  friend nsIFrame* NS_NewMathMLmtdInnerFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  NS_DECL_ISUPPORTS_INHERITED

  // Overloaded nsIMathMLFrame methods

  NS_IMETHOD
  UpdatePresentationDataFromChildAt(PRInt32         aFirstIndex,
                                    PRInt32         aLastIndex,
                                    PRInt32         aScriptLevelIncrement,
                                    PRUint32        aFlagsValues,
                                    PRUint32        aFlagsToUpdate)
  {
    nsMathMLContainerFrame::PropagatePresentationDataFromChildAt(this,
      aFirstIndex, aLastIndex, aScriptLevelIncrement, aFlagsValues, aFlagsToUpdate);
    return NS_OK;
  }

  NS_IMETHOD
  ReResolveScriptStyle(PRInt32 aParentScriptLevel)
  {
    nsMathMLContainerFrame::PropagateScriptStyleFor(this, aParentScriptLevel);
    return NS_OK;
  }

  // overloaded nsBlockFrame methods

  NS_IMETHOD
  Init(nsIContent*      aContent,
       nsIFrame*        aParent,
       nsIFrame*        aPrevInFlow);

  NS_IMETHOD
  Reflow(nsPresContext*          aPresContext,
         nsHTMLReflowMetrics&     aDesiredSize,
         const nsHTMLReflowState& aReflowState,
         nsReflowStatus&          aStatus);
  virtual PRBool IsFrameOfType(PRUint32 aFlags) const;

protected:
  nsMathMLmtdInnerFrame(nsStyleContext* aContext) : nsBlockFrame(aContext) {}
  virtual ~nsMathMLmtdInnerFrame();

  virtual PRIntn GetSkipSides() const { return 0; }
};  // class nsMathMLmtdInnerFrame

#endif /* nsMathMLmtableFrame_h___ */
