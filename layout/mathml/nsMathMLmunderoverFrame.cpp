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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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


#include "nsCOMPtr.h"
#include "nsFrame.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"
#include "nsINameSpaceManager.h"
#include "nsRenderingContext.h"

#include "nsMathMLmunderoverFrame.h"
#include "nsMathMLmsubsupFrame.h"
#include "nsMathMLmsupFrame.h"
#include "nsMathMLmsubFrame.h"

//
// <munderover> -- attach an underscript-overscript pair to a base - implementation
// <mover> -- attach an overscript to a base - implementation
// <munder> -- attach an underscript to a base - implementation
//

nsIFrame*
NS_NewMathMLmunderoverFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMathMLmunderoverFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsMathMLmunderoverFrame)

nsMathMLmunderoverFrame::~nsMathMLmunderoverFrame()
{
}

NS_IMETHODIMP
nsMathMLmunderoverFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                          nsIAtom*        aAttribute,
                                          PRInt32         aModType)
{
  if (nsGkAtoms::accent_ == aAttribute ||
      nsGkAtoms::accentunder_ == aAttribute) {
    // When we have automatic data to update within ourselves, we ask our
    // parent to re-layout its children
    return ReLayoutChildren(mParent);
  }

  return nsMathMLContainerFrame::
         AttributeChanged(aNameSpaceID, aAttribute, aModType);
}

NS_IMETHODIMP
nsMathMLmunderoverFrame::UpdatePresentationData(PRUint32        aFlagsValues,
                                                PRUint32        aFlagsToUpdate)
{
  nsMathMLContainerFrame::UpdatePresentationData(aFlagsValues, aFlagsToUpdate);
  // disable the stretch-all flag if we are going to act like a subscript-superscript pair
  if ( NS_MATHML_EMBELLISH_IS_MOVABLELIMITS(mEmbellishData.flags) &&
      !NS_MATHML_IS_DISPLAYSTYLE(mPresentationData.flags)) {
    mPresentationData.flags &= ~NS_MATHML_STRETCH_ALL_CHILDREN_HORIZONTALLY;
  }
  else {
    mPresentationData.flags |= NS_MATHML_STRETCH_ALL_CHILDREN_HORIZONTALLY;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmunderoverFrame::UpdatePresentationDataFromChildAt(PRInt32         aFirstIndex,
                                                           PRInt32         aLastIndex,
                                                           PRUint32        aFlagsValues,
                                                           PRUint32        aFlagsToUpdate)
{
  // munderover is special... The REC says:
  // Within underscript, <munder> always sets displaystyle to "false", 
  // but increments scriptlevel by 1 only when accentunder is "false".
  // Within underscript, <munderover> always sets displaystyle to "false",
  // but increments scriptlevel by 1 only when accentunder is "false". 
  // This means that
  // 1. don't allow displaystyle to change in the underscript & overscript
  // 2a if the value of the accent is changed, we need to recompute the
  //    scriptlevel of the underscript. The problem is that the accent
  //    can change in the <mo> deep down the embellished hierarchy
  // 2b if the value of the accent is changed, we need to recompute the
  //    scriptlevel of the overscript. The problem is that the accent
  //    can change in the <mo> deep down the embellished hierarchy

  // Do #1 here, prevent displaystyle to be changed in the underscript & overscript
  PRInt32 index = 0;
  nsIFrame* childFrame = mFrames.FirstChild();
  while (childFrame) {
    if ((index >= aFirstIndex) &&
        ((aLastIndex <= 0) || ((aLastIndex > 0) && (index <= aLastIndex)))) {
      if (index > 0) {
        // disable the flag
        aFlagsToUpdate &= ~NS_MATHML_DISPLAYSTYLE;
        aFlagsValues &= ~NS_MATHML_DISPLAYSTYLE;
      }
      PropagatePresentationDataFor(childFrame, aFlagsValues, aFlagsToUpdate);
    }
    index++;
    childFrame = childFrame->GetNextSibling();
  }
  return NS_OK;

  // For #2, changing the accent attribute will trigger a re-build of
  // all automatic data in the embellished hierarchy
}

NS_IMETHODIMP
nsMathMLmunderoverFrame::InheritAutomaticData(nsIFrame* aParent)
{
  // let the base class get the default from our parent
  nsMathMLContainerFrame::InheritAutomaticData(aParent);

  mPresentationData.flags |= NS_MATHML_STRETCH_ALL_CHILDREN_HORIZONTALLY;

  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmunderoverFrame::TransmitAutomaticData()
{
  // At this stage, all our children are in sync and we can fully
  // resolve our own mEmbellishData struct
  //---------------------------------------------------------------------

  /* 
  The REC says:

  As regards munder (respectively mover) :
  The default value of accentunder is false, unless underscript
  is an <mo> element or an embellished operator.  If underscript is 
  an <mo> element, the value of its accent attribute is used as the
  default value of accentunder. If underscript is an embellished
  operator, the accent attribute of the <mo> element at its
  core is used as the default value. As with all attributes, an
  explicitly given value overrides the default.

XXX The winner is the outermost setting in conflicting settings like these:
<munder accentunder='true'>
  <mi>...</mi>
  <mo accentunder='false'> ... </mo>
</munder>

  As regards munderover:
  The accent and accentunder attributes have the same effect as
  the attributes with the same names on <mover>  and <munder>, 
  respectively. Their default values are also computed in the 
  same manner as described for those elements, with the default
  value of accent depending on overscript and the default value
  of accentunder depending on underscript.
  */

  nsIFrame* overscriptFrame = nsnull;
  nsIFrame* underscriptFrame = nsnull;
  nsIFrame* baseFrame = mFrames.FirstChild();
  nsIAtom* tag = mContent->Tag();

  if (baseFrame) {
    if (tag == nsGkAtoms::munder_ ||
        tag == nsGkAtoms::munderover_) {
      underscriptFrame = baseFrame->GetNextSibling();
    } else {
      NS_ASSERTION(tag == nsGkAtoms::mover_, "mContent->Tag() not recognized");
      overscriptFrame = baseFrame->GetNextSibling();
    }
  }
  if (underscriptFrame &&
      tag == nsGkAtoms::munderover_) {
    overscriptFrame = underscriptFrame->GetNextSibling();

  }

  // if our base is an embellished operator, let its state bubble to us (in particular,
  // this is where we get the flag for NS_MATHML_EMBELLISH_MOVABLELIMITS). Our flags
  // are reset to the default values of false if the base frame isn't embellished.
  mPresentationData.baseFrame = baseFrame;
  GetEmbellishDataFrom(baseFrame, mEmbellishData);

  // The default value of accentunder is false, unless the underscript is embellished
  // and its core <mo> is an accent
  nsEmbellishData embellishData;
  nsAutoString value;
  if (tag == nsGkAtoms::munder_ ||
      tag == nsGkAtoms::munderover_) {
    GetEmbellishDataFrom(underscriptFrame, embellishData);
    if (NS_MATHML_EMBELLISH_IS_ACCENT(embellishData.flags)) {
      mEmbellishData.flags |= NS_MATHML_EMBELLISH_ACCENTUNDER;
    } else {
      mEmbellishData.flags &= ~NS_MATHML_EMBELLISH_ACCENTUNDER;
    }    

    // if we have an accentunder attribute, it overrides what the underscript said
    if (GetAttribute(mContent, mPresentationData.mstyle, nsGkAtoms::accentunder_,
                     value)) {
      if (value.EqualsLiteral("true")) {
        mEmbellishData.flags |= NS_MATHML_EMBELLISH_ACCENTUNDER;
      } else if (value.EqualsLiteral("false")) {
        mEmbellishData.flags &= ~NS_MATHML_EMBELLISH_ACCENTUNDER;
      }
    }
  }

  // The default value of accent is false, unless the overscript is embellished
  // and its core <mo> is an accent
  if (tag == nsGkAtoms::mover_ ||
      tag == nsGkAtoms::munderover_) {
    GetEmbellishDataFrom(overscriptFrame, embellishData);
    if (NS_MATHML_EMBELLISH_IS_ACCENT(embellishData.flags)) {
      mEmbellishData.flags |= NS_MATHML_EMBELLISH_ACCENTOVER;
    } else {
      mEmbellishData.flags &= ~NS_MATHML_EMBELLISH_ACCENTOVER;
    }

    // if we have an accent attribute, it overrides what the overscript said
    if (GetAttribute(mContent, mPresentationData.mstyle, nsGkAtoms::accent_,
                     value)) {
      if (value.EqualsLiteral("true")) {
        mEmbellishData.flags |= NS_MATHML_EMBELLISH_ACCENTOVER;
      } else if (value.EqualsLiteral("false")) {
        mEmbellishData.flags &= ~NS_MATHML_EMBELLISH_ACCENTOVER;
      }
    }
  }

  PRBool subsupDisplay =
    NS_MATHML_EMBELLISH_IS_MOVABLELIMITS(mEmbellishData.flags) &&
    !NS_MATHML_IS_DISPLAYSTYLE(mPresentationData.flags);

  // disable the stretch-all flag if we are going to act like a superscript
  if (subsupDisplay) {
    mPresentationData.flags &= ~NS_MATHML_STRETCH_ALL_CHILDREN_HORIZONTALLY;
  }

  // Now transmit any change that we want to our children so that they
  // can update their mPresentationData structs
  //---------------------------------------------------------------------

  /* The REC says:
     Within underscript, <munderover> always sets displaystyle to "false",
     but increments scriptlevel by 1 only when accentunder is "false". 

     Within overscript, <munderover> always sets displaystyle to "false", 
     but increments scriptlevel by 1 only when accent is "false".
 
     Within subscript and superscript it increments scriptlevel by 1, and 
     sets displaystyle to "false", but leaves both attributes unchanged within 
     base.

     The TeXBook treats 'over' like a superscript, so p.141 or Rule 13a
     say it shouldn't be compressed. However, The TeXBook says
     that math accents and \overline change uncramped styles to their
     cramped counterparts.
  */
  if (tag == nsGkAtoms::mover_ ||
      tag == nsGkAtoms::munderover_) {
    PRUint32 compress = NS_MATHML_EMBELLISH_IS_ACCENTOVER(mEmbellishData.flags)
      ? NS_MATHML_COMPRESSED : 0;
    SetIncrementScriptLevel(tag == nsGkAtoms::mover_ ? 1 : 2,
                            !NS_MATHML_EMBELLISH_IS_ACCENTOVER(mEmbellishData.flags) || subsupDisplay);
    PropagatePresentationDataFor(overscriptFrame,
                                 ~NS_MATHML_DISPLAYSTYLE | compress,
                                 NS_MATHML_DISPLAYSTYLE | compress);
  }
  /*
     The TeXBook treats 'under' like a subscript, so p.141 or Rule 13a 
     say it should be compressed
  */
  if (tag == nsGkAtoms::munder_ ||
      tag == nsGkAtoms::munderover_) {
    SetIncrementScriptLevel(1, !NS_MATHML_EMBELLISH_IS_ACCENTUNDER(mEmbellishData.flags) || subsupDisplay);
    PropagatePresentationDataFor(underscriptFrame,
                                 ~NS_MATHML_DISPLAYSTYLE | NS_MATHML_COMPRESSED,
                                 NS_MATHML_DISPLAYSTYLE | NS_MATHML_COMPRESSED);
  }
  return NS_OK;
}

/*
The REC says:
*  If the base is an operator with movablelimits="true" (or an embellished
   operator whose <mo> element core has movablelimits="true"), and
   displaystyle="false", then underscript and overscript are drawn in
   a subscript and superscript position, respectively. In this case, 
   the accent and accentunder attributes are ignored. This is often
   used for limits on symbols such as &sum;.

i.e.,:
 if ( NS_MATHML_EMBELLISH_IS_MOVABLELIMITS(mEmbellishDataflags) &&
     !NS_MATHML_IS_DISPLAYSTYLE(mPresentationData.flags)) {
  // place like subscript-superscript pair
 }
 else {
  // place like underscript-overscript pair
 }
*/

/* virtual */ nsresult
nsMathMLmunderoverFrame::Place(nsRenderingContext& aRenderingContext,
                               PRBool               aPlaceOrigin,
                               nsHTMLReflowMetrics& aDesiredSize)
{
  nsIAtom* tag = mContent->Tag();
  if ( NS_MATHML_EMBELLISH_IS_MOVABLELIMITS(mEmbellishData.flags) &&
       !NS_MATHML_IS_DISPLAYSTYLE(mPresentationData.flags)) {
    //place like sub sup or subsup
    nscoord scriptSpace = nsPresContext::CSSPointsToAppUnits(0.5f);
    if (tag == nsGkAtoms::munderover_) {
      return nsMathMLmsubsupFrame::PlaceSubSupScript(PresContext(),
                                                     aRenderingContext,
                                                     aPlaceOrigin,
                                                     aDesiredSize,
                                                     this, 0, 0,
                                                     scriptSpace);
    } else if (tag == nsGkAtoms::munder_) {
      return nsMathMLmsubFrame::PlaceSubScript(PresContext(),
                                               aRenderingContext,
                                               aPlaceOrigin,
                                               aDesiredSize,
                                               this, 0,
                                               scriptSpace);
    } else {
      NS_ASSERTION(tag == nsGkAtoms::mover_, "mContent->Tag() not recognized");
      return nsMathMLmsupFrame::PlaceSuperScript(PresContext(),
                                                 aRenderingContext,
                                                 aPlaceOrigin,
                                                 aDesiredSize,
                                                 this, 0,
                                                 scriptSpace);
    }
    
  }

  ////////////////////////////////////
  // Get the children's desired sizes

  nsBoundingMetrics bmBase, bmUnder, bmOver;
  nsHTMLReflowMetrics baseSize;
  nsHTMLReflowMetrics underSize;
  nsHTMLReflowMetrics overSize;
  nsIFrame* overFrame = nsnull;
  nsIFrame* underFrame = nsnull;
  nsIFrame* baseFrame = mFrames.FirstChild();
  underSize.ascent = 0; 
  overSize.ascent = 0;
  if (baseFrame) {
    if (tag == nsGkAtoms::munder_ ||
        tag == nsGkAtoms::munderover_) {
      underFrame = baseFrame->GetNextSibling();
    } else if (tag == nsGkAtoms::mover_) {
      overFrame = baseFrame->GetNextSibling();
    }
  }
  if (underFrame && tag == nsGkAtoms::munderover_) {
    overFrame = underFrame->GetNextSibling();
  }
  
  if (tag == nsGkAtoms::munder_) {
    if (!baseFrame || !underFrame || underFrame->GetNextSibling()) {
      // report an error, encourage people to get their markups in order
      return ReflowError(aRenderingContext, aDesiredSize);
    }
  }
  if (tag == nsGkAtoms::mover_) {
    if (!baseFrame || !overFrame || overFrame->GetNextSibling()) {
      // report an error, encourage people to get their markups in order
      return ReflowError(aRenderingContext, aDesiredSize);
    }
  }
  if (tag == nsGkAtoms::munderover_) {
    if (!baseFrame || !underFrame || !overFrame || overFrame->GetNextSibling()) {
      // report an error, encourage people to get their markups in order
      return ReflowError(aRenderingContext, aDesiredSize);
    }
  }
  GetReflowAndBoundingMetricsFor(baseFrame, baseSize, bmBase);
  if (underFrame) {
    GetReflowAndBoundingMetricsFor(underFrame, underSize, bmUnder);
  }
  if (overFrame) {
    GetReflowAndBoundingMetricsFor(overFrame, overSize, bmOver);
  }

  nscoord onePixel = nsPresContext::CSSPixelsToAppUnits(1);

  ////////////////////
  // Place Children

  aRenderingContext.SetFont(GetStyleFont()->mFont,
                            PresContext()->GetUserFontSet());
  nsFontMetrics* fm = aRenderingContext.FontMetrics();

  nscoord xHeight = fm->XHeight();

  nscoord ruleThickness;
  GetRuleThickness (aRenderingContext, fm, ruleThickness);

  nscoord correction = 0;
  GetItalicCorrection (bmBase, correction);

  // there are 2 different types of placement depending on 
  // whether we want an accented under or not

  nscoord underDelta1 = 0; // gap between base and underscript
  nscoord underDelta2 = 0; // extra space beneath underscript

  if (!NS_MATHML_EMBELLISH_IS_ACCENTUNDER(mEmbellishData.flags)) {
    // Rule 13a, App. G, TeXbook
    nscoord bigOpSpacing2, bigOpSpacing4, bigOpSpacing5, dummy; 
    GetBigOpSpacings (fm, 
                      dummy, bigOpSpacing2, 
                      dummy, bigOpSpacing4, 
                      bigOpSpacing5);
    underDelta1 = NS_MAX(bigOpSpacing2, (bigOpSpacing4 - bmUnder.ascent));
    underDelta2 = bigOpSpacing5;
  }
  else {
    // No corresponding rule in TeXbook - we are on our own here
    // XXX tune the gap delta between base and underscript 

    // Should we use Rule 10 like \underline does?
    underDelta1 = ruleThickness + onePixel/2;
    underDelta2 = ruleThickness;
  }
  // empty under?
  if (!(bmUnder.ascent + bmUnder.descent)) {
    underDelta1 = 0;
    underDelta2 = 0;
  }

  nscoord overDelta1 = 0; // gap between base and overscript
  nscoord overDelta2 = 0; // extra space above overscript

  if (!NS_MATHML_EMBELLISH_IS_ACCENTOVER(mEmbellishData.flags)) {    
    // Rule 13a, App. G, TeXbook
    nscoord bigOpSpacing1, bigOpSpacing3, bigOpSpacing5, dummy; 
    GetBigOpSpacings (fm, 
                      bigOpSpacing1, dummy, 
                      bigOpSpacing3, dummy, 
                      bigOpSpacing5);
    overDelta1 = NS_MAX(bigOpSpacing1, (bigOpSpacing3 - bmOver.descent));
    overDelta2 = bigOpSpacing5;

    // XXX This is not a TeX rule... 
    // delta1 (as computed abvove) can become really big when bmOver.descent is
    // negative,  e.g., if the content is &OverBar. In such case, we use the height
    if (bmOver.descent < 0)    
      overDelta1 = NS_MAX(bigOpSpacing1, (bigOpSpacing3 - (bmOver.ascent + bmOver.descent)));
  }
  else {
    // Rule 12, App. G, TeXbook
    // We are going to modify this rule to make it more general.
    // The idea behind Rule 12 in the TeXBook is to keep the accent
    // as close to the base as possible, while ensuring that the
    // distance between the *baseline* of the accent char and 
    // the *baseline* of the base is atleast x-height. 
    // The idea is that for normal use, we would like all the accents
    // on a line to line up atleast x-height above the baseline 
    // if possible. 
    // When the ascent of the base is >= x-height, 
    // the baseline of the accent char is placed just above the base
    // (specifically, the baseline of the accent char is placed 
    // above the baseline of the base by the ascent of the base).
    // For ease of implementation, 
    // this assumes that the font-designer designs accents 
    // in such a way that the bottom of the accent is atleast x-height
    // above its baseline, otherwise there will be collisions
    // with the base. Also there should be proper padding between
    // the bottom of the accent char and its baseline.
    // The above rule may not be obvious from a first
    // reading of rule 12 in the TeXBook !!!
    // The mathml <mover> tag can use accent chars that
    // do not follow this convention. So we modify TeX's rule 
    // so that TeX's rule gets subsumed for accents that follow 
    // TeX's convention,
    // while also allowing accents that do not follow the convention :
    // we try to keep the *bottom* of the accent char atleast x-height 
    // from the baseline of the base char. we also slap on an extra
    // padding between the accent and base chars.
    overDelta1 = ruleThickness + onePixel/2;
    if (bmBase.ascent < xHeight) {
      // also ensure at least x-height above the baseline of the base
      overDelta1 += xHeight - bmBase.ascent;
    }
    overDelta2 = ruleThickness;
  }
  // empty over?
  if (!(bmOver.ascent + bmOver.descent)) {
    overDelta1 = 0;
    overDelta2 = 0;
  }

  nscoord dxBase, dxOver = 0, dxUnder = 0;

  //////////
  // pass 1, do what <mover> does: attach the overscript on the base

  // Ad-hoc - This is to override fonts which have ready-made _accent_
  // glyphs with negative lbearing and rbearing. We want to position
  // the overscript ourselves
  nscoord overWidth = bmOver.width;
  if (!overWidth && (bmOver.rightBearing - bmOver.leftBearing > 0)) {
    overWidth = bmOver.rightBearing - bmOver.leftBearing;
    dxOver = -bmOver.leftBearing;
  }

  if (NS_MATHML_EMBELLISH_IS_ACCENTOVER(mEmbellishData.flags)) {
    mBoundingMetrics.width = bmBase.width; 
    dxOver += correction + (mBoundingMetrics.width - overWidth)/2;
  }
  else {
    mBoundingMetrics.width = NS_MAX(bmBase.width, overWidth);
    dxOver += correction/2 + (mBoundingMetrics.width - overWidth)/2;
  }
  dxBase = (mBoundingMetrics.width - bmBase.width)/2;

  mBoundingMetrics.ascent = 
    bmBase.ascent + overDelta1 + bmOver.ascent + bmOver.descent;
  mBoundingMetrics.descent = bmBase.descent;
  mBoundingMetrics.leftBearing = 
    NS_MIN(dxBase + bmBase.leftBearing, dxOver + bmOver.leftBearing);
  mBoundingMetrics.rightBearing = 
    NS_MAX(dxBase + bmBase.rightBearing, dxOver + bmOver.rightBearing);

  //////////
  // pass 2, do what <munder> does: attach the underscript on the previous
  // result. We conceptually view the previous result as an "anynomous base" 
  // from where to attach the underscript. Hence if the underscript is empty,
  // we should end up like <mover>. If the overscript is empty, we should
  // end up like <munder>.

  nsBoundingMetrics bmAnonymousBase = mBoundingMetrics;
  nscoord ascentAnonymousBase =
    NS_MAX(mBoundingMetrics.ascent + overDelta2,
           overSize.ascent + bmOver.descent + overDelta1 + bmBase.ascent);
  ascentAnonymousBase = NS_MAX(ascentAnonymousBase, baseSize.ascent);

  GetItalicCorrection(bmAnonymousBase, correction);

  // Width of non-spacing marks is zero so use left and right bearing.
  nscoord underWidth = bmUnder.width;
  if (!underWidth) {
    underWidth = bmUnder.rightBearing - bmUnder.leftBearing;
    dxUnder = -bmUnder.leftBearing;
  }

  nscoord maxWidth = NS_MAX(bmAnonymousBase.width, underWidth);
  if (NS_MATHML_EMBELLISH_IS_ACCENTUNDER(mEmbellishData.flags)) {
    dxUnder += (maxWidth - underWidth)/2;;
  }
  else {
    dxUnder += -correction/2 + (maxWidth - underWidth)/2;
  }
  nscoord dxAnonymousBase = (maxWidth - bmAnonymousBase.width)/2;

  // adjust the offsets of the real base and overscript since their
  // final offsets should be relative to us...
  dxOver += dxAnonymousBase;
  dxBase += dxAnonymousBase;

  mBoundingMetrics.width =
    NS_MAX(dxAnonymousBase + bmAnonymousBase.width, dxUnder + bmUnder.width);
  // At this point, mBoundingMetrics.ascent = bmAnonymousBase.ascent 
  mBoundingMetrics.descent = 
    bmAnonymousBase.descent + underDelta1 + bmUnder.ascent + bmUnder.descent;
  mBoundingMetrics.leftBearing =
    NS_MIN(dxAnonymousBase + bmAnonymousBase.leftBearing, dxUnder + bmUnder.leftBearing);
  mBoundingMetrics.rightBearing = 
    NS_MAX(dxAnonymousBase + bmAnonymousBase.rightBearing, dxUnder + bmUnder.rightBearing);

  aDesiredSize.ascent = ascentAnonymousBase;
  aDesiredSize.height = aDesiredSize.ascent +
    NS_MAX(mBoundingMetrics.descent + underDelta2,
           bmAnonymousBase.descent + underDelta1 + bmUnder.ascent +
             underSize.height - underSize.ascent);
  aDesiredSize.height = NS_MAX(aDesiredSize.height,
                               aDesiredSize.ascent +
                               baseSize.height - baseSize.ascent);
  aDesiredSize.width = mBoundingMetrics.width;
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;

  mReference.x = 0;
  mReference.y = aDesiredSize.ascent;

  if (aPlaceOrigin) {
    nscoord dy;
    // place overscript
    if (overFrame) {
      dy = aDesiredSize.ascent - mBoundingMetrics.ascent + bmOver.ascent 
        - overSize.ascent;
      FinishReflowChild (overFrame, PresContext(), nsnull, overSize, dxOver, dy, 0);
    }
    // place base
    dy = aDesiredSize.ascent - baseSize.ascent;
    FinishReflowChild (baseFrame, PresContext(), nsnull, baseSize, dxBase, dy, 0);
    // place underscript
    if (underFrame) {
      dy = aDesiredSize.ascent + mBoundingMetrics.descent - bmUnder.descent 
        - underSize.ascent;
      FinishReflowChild (underFrame, PresContext(), nsnull, underSize,
                         dxUnder, dy, 0);
    }
  }
  return NS_OK;
}
