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

#ifndef nsMathMLmpaddedFrame_h___
#define nsMathMLmpaddedFrame_h___

#include "nsCOMPtr.h"
#include "nsMathMLContainerFrame.h"

//
// <mpadded> -- adjust space around content  
//

class nsMathMLmpaddedFrame : public nsMathMLContainerFrame {
public:
  friend nsresult NS_NewMathMLmpaddedFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

  NS_IMETHOD
  InheritAutomaticData(nsPresContext* aPresContext,
                       nsIFrame*       aParent);

  NS_IMETHOD
  Reflow(nsPresContext*          aPresContext,
         nsHTMLReflowMetrics&     aDesiredSize,
         const nsHTMLReflowState& aReflowState,
         nsReflowStatus&          aStatus);
  
protected:
  nsMathMLmpaddedFrame();
  virtual ~nsMathMLmpaddedFrame();
  
  virtual PRIntn GetSkipSides() const { return 0; }

private:
  nsCSSValue mWidth;
  nsCSSValue mHeight;
  nsCSSValue mDepth;
  nsCSSValue mLeftSpace;

  PRInt32    mWidthSign;
  PRInt32    mHeightSign;
  PRInt32    mDepthSign;
  PRInt32    mLeftSpaceSign;

  PRInt32    mWidthPseudoUnit;
  PRInt32    mHeightPseudoUnit;
  PRInt32    mDepthPseudoUnit;
  PRInt32    mLeftSpacePseudoUnit;

  // helpers to process the attributes
  void
  ProcessAttributes(nsPresContext* aPresContext);

  static PRBool
  ParseAttribute(nsString&   aString,
                 PRInt32&    aSign,
                 nsCSSValue& aCSSValue,
                 PRInt32&    aPseudoUnit);

  static void
  UpdateValue(nsPresContext*      aPresContext,
              nsStyleContext*      aStyleContext,
              PRInt32              aSign,
              PRInt32              aPseudoUnit,
              nsCSSValue&          aCSSValue,
              nscoord              aLeftSpace,
              nsBoundingMetrics&   aBoundingMetrics,
              nscoord&             aValueToUpdate);
};

#endif /* nsMathMLmpaddedFrame_h___ */
