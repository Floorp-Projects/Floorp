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


#include "nsCOMPtr.h"
#include "nsFrame.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"
#include "nsINameSpaceManager.h"

#include "nsMathMLmstyleFrame.h"

//
// <mstyle> -- style change
//

nsIFrame*
NS_NewMathMLmstyleFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMathMLmstyleFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsMathMLmstyleFrame)

nsMathMLmstyleFrame::~nsMathMLmstyleFrame()
{
}

// mstyle needs special care for its scriptlevel and displaystyle attributes
NS_IMETHODIMP
nsMathMLmstyleFrame::InheritAutomaticData(nsIFrame* aParent) 
{
  // let the base class get the default from our parent
  nsMathMLContainerFrame::InheritAutomaticData(aParent);

  // sync with our current state
  mPresentationData.flags |= NS_MATHML_STRETCH_ALL_CHILDREN_VERTICALLY;
  mPresentationData.mstyle = this;

  // see if the displaystyle attribute is there
  nsMathMLFrame::FindAttrDisplaystyle(mContent, mPresentationData);

  // see if the directionality attribute is there
  nsMathMLFrame::FindAttrDirectionality(mContent, mPresentationData);

  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmstyleFrame::TransmitAutomaticData()
{
  return TransmitAutomaticDataForMrowLikeElement();
}

// displaystyle and scriptlevel are special in <mstyle>...
// Since UpdatePresentation() and UpdatePresentationDataFromChildAt() can be called
// by a parent, ensure that the explicit attributes of <mstyle> take precedence
NS_IMETHODIMP
nsMathMLmstyleFrame::UpdatePresentationData(PRUint32        aFlagsValues,
                                            PRUint32        aWhichFlags)
{
  if (NS_MATHML_HAS_EXPLICIT_DISPLAYSTYLE(mPresentationData.flags)) {
    // our current state takes precedence, disallow updating the displastyle
    aWhichFlags &= ~NS_MATHML_DISPLAYSTYLE;
    aFlagsValues &= ~NS_MATHML_DISPLAYSTYLE;
  }

  return nsMathMLContainerFrame::UpdatePresentationData(aFlagsValues, aWhichFlags);
}

NS_IMETHODIMP
nsMathMLmstyleFrame::UpdatePresentationDataFromChildAt(PRInt32         aFirstIndex,
                                                       PRInt32         aLastIndex,
                                                       PRUint32        aFlagsValues,
                                                       PRUint32        aWhichFlags)
{
  if (NS_MATHML_HAS_EXPLICIT_DISPLAYSTYLE(mPresentationData.flags)) {
    // our current state takes precedence, disallow updating the displastyle
    aWhichFlags &= ~NS_MATHML_DISPLAYSTYLE;
    aFlagsValues &= ~NS_MATHML_DISPLAYSTYLE;
  }

  // let the base class worry about the update
  return
    nsMathMLContainerFrame::UpdatePresentationDataFromChildAt(
      aFirstIndex, aLastIndex, aFlagsValues, aWhichFlags); 
}

NS_IMETHODIMP
nsMathMLmstyleFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                      nsIAtom*        aAttribute,
                                      PRInt32         aModType)
{
  // Other attributes can affect too many things, ask our parent to re-layout
  // its children so that we can pick up changes in our attributes & transmit
  // them in our subtree. However, our siblings will be re-laid too. We used
  // to have a more speedier but more verbose alternative that didn't re-layout
  // our siblings. See bug 114909 - attachment 67668.
  return ReLayoutChildren(mParent);
}
