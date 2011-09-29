/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Shyjan Mahamud <mahamud@cs.cmu.edu>
 *   Frederic Wang <fred.wang@free.fr>
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

#ifndef nsMathMLmfracFrame_h___
#define nsMathMLmfracFrame_h___

#include "nsCOMPtr.h"
#include "nsMathMLContainerFrame.h"

//
// <mfrac> -- form a fraction from two subexpressions 
//

/*
The MathML REC describes:

The <mfrac> element is used for fractions. It can also be used to mark up 
fraction-like objects such as binomial coefficients and Legendre symbols. 
The syntax for <mfrac> is:
  <mfrac> numerator denominator </mfrac>

Attributes of <mfrac>:
     Name                      values                     default 
  linethickness  number [ v-unit ] | thin | medium | thick  1 

E.g., 
linethickness=2 actually means that linethickness=2*DEFAULT_THICKNESS
(DEFAULT_THICKNESS is not specified by MathML, see below.)

The linethickness attribute indicates the thickness of the horizontal
"fraction bar", or "rule", typically used to render fractions. A fraction
with linethickness="0" renders without the bar, and might be used within
binomial coefficients. A linethickness greater than one might be used with
nested fractions. 

In general, the value of linethickness can be a number, as a multiplier
of the default thickness of the fraction bar (the default thickness is
not specified by MathML), or a number with a unit of vertical length (see
Section 2.3.3), or one of the keywords medium (same as 1), thin (thinner
than 1, otherwise up to the renderer), or thick (thicker than 1, otherwise
up to the renderer). 

The <mfrac> element sets displaystyle to "false", or if it was already
false increments scriptlevel by 1, within numerator and denominator.
These attributes are inherited by every element from its rendering 
environment, but can be set explicitly only on the <mstyle> 
element. 
*/

class nsMathMLmfracFrame : public nsMathMLContainerFrame {
public:
  NS_DECL_FRAMEARENA_HELPERS

  friend nsIFrame* NS_NewMathMLmfracFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  virtual eMathMLFrameType GetMathMLFrameType();

  virtual nsresult
  MeasureForWidth(nsRenderingContext& aRenderingContext,
                  nsHTMLReflowMetrics& aDesiredSize);

  virtual nsresult
  Place(nsRenderingContext& aRenderingContext,
        bool                 aPlaceOrigin,
        nsHTMLReflowMetrics& aDesiredSize);

  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);

  NS_IMETHOD
  TransmitAutomaticData();

  NS_IMETHOD
  UpdatePresentationDataFromChildAt(PRInt32         aFirstIndex,
                                    PRInt32         aLastIndex,
                                    PRUint32        aFlagsValues,
                                    PRUint32        aFlagsToUpdate);

  // override the base method so that we can deal with the fraction line
  virtual nscoord
  FixInterFrameSpacing(nsHTMLReflowMetrics& aDesiredSize);

  // helper to translate the thickness attribute into a usable form
  static nscoord 
  CalcLineThickness(nsPresContext*  aPresContext,
                    nsStyleContext*  aStyleContext,
                    nsString&        aThicknessAttribute,
                    nscoord          onePixel,
                    nscoord          aDefaultRuleThickness);

protected:
  nsMathMLmfracFrame(nsStyleContext* aContext) : nsMathMLContainerFrame(aContext) {}
  virtual ~nsMathMLmfracFrame();
  
  virtual PRIntn GetSkipSides() const { return 0; }

  nsresult PlaceInternal(nsRenderingContext& aRenderingContext,
                         bool                 aPlaceOrigin,
                         nsHTMLReflowMetrics& aDesiredSize,
                         bool                 aWidthOnly);

  // Display a slash
  nsresult DisplaySlash(nsDisplayListBuilder* aBuilder,
                        nsIFrame* aFrame, const nsRect& aRect,
                        nscoord aThickness,
                        const nsDisplayListSet& aLists);

  nsRect        mLineRect;
  nsMathMLChar* mSlashChar;
  nscoord       mLineThickness;
  bool          mIsBevelled;
};

#endif /* nsMathMLmfracFrame_h___ */
