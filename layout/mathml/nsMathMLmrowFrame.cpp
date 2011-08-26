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

#include "nsMathMLmrowFrame.h"

//
// <mrow> -- horizontally group any number of subexpressions - implementation
//

nsIFrame*
NS_NewMathMLmrowFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMathMLmrowFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsMathMLmrowFrame)

nsMathMLmrowFrame::~nsMathMLmrowFrame()
{
}

NS_IMETHODIMP
nsMathMLmrowFrame::InheritAutomaticData(nsIFrame* aParent)
{
  // let the base class get the default from our parent
  nsMathMLContainerFrame::InheritAutomaticData(aParent);

  mPresentationData.flags |= NS_MATHML_STRETCH_ALL_CHILDREN_VERTICALLY;

  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmrowFrame::AttributeChanged(PRInt32  aNameSpaceID,
                                    nsIAtom* aAttribute,
                                    PRInt32  aModType)
{
  // Special for <mtable>: In the frame construction code, we also use
  // this frame class as a wrapper for mtable. Hence, we should pass the
  // notification to the real mtable
  if (mContent->Tag() == nsGkAtoms::mtable_) {
    nsIFrame* frame = mFrames.FirstChild();
    for ( ; frame; frame = frame->GetFirstPrincipalChild()) {
      // drill down to the real mtable
      if (frame->GetType() == nsGkAtoms::tableOuterFrame)
        return frame->AttributeChanged(aNameSpaceID, aAttribute, aModType);
    }
    NS_NOTREACHED("mtable wrapper without the real table frame");
  }

  return nsMathMLContainerFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);
}
