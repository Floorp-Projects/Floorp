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
 *   David J. Fiddes <D.J.Fiddes@hw.ac.uk>
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

#ifndef nsMathMLmfencedFrame_h___
#define nsMathMLmfencedFrame_h___

#include "nsCOMPtr.h"
#include "nsMathMLContainerFrame.h"

//
// <mfenced> -- surround content with a pair of fences
//

class nsMathMLmfencedFrame : public nsMathMLContainerFrame {
public:
  friend nsresult NS_NewMathMLmfencedFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

  virtual void
  SetAdditionalStyleContext(PRInt32          aIndex, 
                            nsStyleContext*  aStyleContext);
  virtual nsStyleContext*
  GetAdditionalStyleContext(PRInt32 aIndex) const;

  NS_IMETHOD
  InheritAutomaticData(nsPresContext* aPresContext,
                       nsIFrame*       aParent);

  NS_IMETHOD
  SetInitialChildList(nsPresContext* aPresContext,
                      nsIAtom*        aListName,
                      nsIFrame*       aChildList);

  NS_IMETHOD
  Reflow(nsPresContext*          aPresContext,
         nsHTMLReflowMetrics&     aDesiredSize,
         const nsHTMLReflowState& aReflowState,
         nsReflowStatus&          aStatus);

  NS_IMETHOD 
  Paint(nsPresContext*      aPresContext,
        nsIRenderingContext& aRenderingContext,
        const nsRect&        aDirtyRect,
        nsFramePaintLayer    aWhichLayer,
        PRUint32             aFlags = 0);

  NS_IMETHOD
  AttributeChanged(nsPresContext* aPresContext,
                   nsIContent*     aContent,
                   PRInt32         aNameSpaceID,
                   nsIAtom*        aAttribute,
                   PRInt32         aModType);

  // override the base method because we must keep separators in sync
  virtual nsresult
  ChildListChanged(nsPresContext* aPresContext,
                   PRInt32         aModType);

  // exported routine that both mfenced and mfrac share.
  // mfrac uses this when its bevelled attribute is set.
  static nsresult
  doReflow(nsPresContext*          aPresContext,
           const nsHTMLReflowState& aReflowState,
           nsHTMLReflowMetrics&     aDesiredSize,
           nsReflowStatus&          aStatus,
           nsIFrame*                aForFrame,
           nsMathMLChar*            aOpenChar,
           nsMathMLChar*            aCloseChar,
           nsMathMLChar*            aSeparatorsChar,
           PRInt32                  aSeparatorsCount);

  // helper routines to format the MathMLChars involved here
  static nsresult
  ReflowChar(nsPresContext*      aPresContext,
             nsIRenderingContext& aRenderingContext,
             nsMathMLChar*        aMathMLChar,
             nsOperatorFlags      aForm,
             PRInt32              aScriptLevel,
             nscoord              axisHeight,
             nscoord              leading,
             nscoord              em,
             nsBoundingMetrics&   aContainerSize,
             nsHTMLReflowMetrics& aDesiredSize);

  static void
  PlaceChar(nsMathMLChar*      aMathMLChar,
            nscoord            aDesiredSize,
            nsBoundingMetrics& bm,
            nscoord&           dx);

protected:
  nsMathMLmfencedFrame();
  virtual ~nsMathMLmfencedFrame();
  
  virtual PRIntn GetSkipSides() const { return 0; }

  nsMathMLChar* mOpenChar;
  nsMathMLChar* mCloseChar;
  nsMathMLChar* mSeparatorsChar;
  PRInt32       mSeparatorsCount;

  // clean up
  void
  RemoveFencesAndSeparators();

  // add fences and separators when all child frames are known
  nsresult
  CreateFencesAndSeparators(nsPresContext* aPresContext);
};

#endif /* nsMathMLmfencedFrame_h___ */
