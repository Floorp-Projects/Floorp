/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla MathML Project.
 * 
 * The Initial Developer of the Original Code is The University Of 
 * Queensland.  Portions created by The University Of Queensland are
 * Copyright (C) 1999 The University Of Queensland.  All Rights Reserved.
 * 
 * Contributor(s): 
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
 *   David J. Fiddes <D.J.Fiddes@hw.ac.uk>
 */


#include "nsCOMPtr.h"
#include "nsHTMLParts.h"
#include "nsIHTMLContent.h"
#include "nsFrame.h"
#include "nsLineLayout.h"
#include "nsHTMLIIDs.h"
#include "nsIPresContext.h"
#include "nsHTMLAtoms.h"
#include "nsUnitConversion.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsINameSpaceManager.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"
#include "nsStyleUtil.h"

#include "nsMathMLmstyleFrame.h"

//
// <mstyle> -- style change
//

nsresult
NS_NewMathMLmstyleFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLmstyleFrame* it = new (aPresShell) nsMathMLmstyleFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;  
  return NS_OK;
}

nsMathMLmstyleFrame::nsMathMLmstyleFrame()
{
}

nsMathMLmstyleFrame::~nsMathMLmstyleFrame()
{
}

// mstyle needs special care for its scriptlevel and displaystyle attributes
NS_IMETHODIMP
nsMathMLmstyleFrame::Init(nsIPresContext*  aPresContext,
                          nsIContent*      aContent,
                          nsIFrame*        aParent,
                          nsIStyleContext* aContext,
                          nsIFrame*        aPrevInFlow)
{
  nsresult rv = nsMathMLContainerFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  mEmbellishData.flags |= NS_MATHML_STRETCH_ALL_CHILDREN_VERTICALLY;

  mPresentationData.mstyle = this;

  mInnerScriptLevelIncrement = 0;

  // see if the displaystyle attribute is there
  nsAutoString value;
  if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None, 
                   nsMathMLAtoms::displaystyle_, value)) {
    if (value.EqualsWithConversion("true")) {
      mPresentationData.flags |= NS_MATHML_MSTYLE_WITH_DISPLAYSTYLE;
      mPresentationData.flags |= NS_MATHML_DISPLAYSTYLE;
    }
    else if (value.EqualsWithConversion("false")) {
      mPresentationData.flags |= NS_MATHML_MSTYLE_WITH_DISPLAYSTYLE;
      mPresentationData.flags &= ~NS_MATHML_DISPLAYSTYLE;
    }
  }

  // see if the scriptlevel attribute is there
  if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None, 
                   nsMathMLAtoms::scriptlevel_, value)) {
    PRInt32 aErrorCode, aUserValue;
    aUserValue = value.ToInteger(&aErrorCode); 
    if (NS_SUCCEEDED(aErrorCode)) {
      if (value[0] != '+' && value[0] != '-') { // record that it is an explicit value
        mPresentationData.flags |= NS_MATHML_MSTYLE_WITH_EXPLICIT_SCRIPTLEVEL;
        mPresentationData.scriptLevel = aUserValue;
      }
      else {
//        mScriptLevel += aUserValue; // incremental value...
        mInnerScriptLevelIncrement = aUserValue;
      }
    }
  }


// TODO:
// Examine all other attributes

  return rv;
}

NS_IMETHODIMP
nsMathMLmstyleFrame::UpdatePresentationData(PRInt32  aScriptLevelIncrement,
                                            PRUint32 aFlagsValues,
                                            PRUint32 aFlagsToUpdate)
{
  // mstyle is special...
  // Since UpdatePresentationData() can be called by a parent frame, the
  // scriptlevel and displaystyle attributes of mstyle must take precedence.
  // Update only if attributes are not there

  if (!NS_MATHML_IS_MSTYLE_WITH_DISPLAYSTYLE(mPresentationData.flags)) {
    // update the flag if it is relevant to this call
    if (NS_MATHML_IS_DISPLAYSTYLE(aFlagsToUpdate)) {
      // updating the displaystyle flag is allowed
      if (NS_MATHML_IS_DISPLAYSTYLE(aFlagsValues)) {
        mPresentationData.flags |= NS_MATHML_DISPLAYSTYLE;
      }
      else {
        mPresentationData.flags &= ~NS_MATHML_DISPLAYSTYLE;
      }
    }
  }

  if (!NS_MATHML_IS_MSTYLE_WITH_EXPLICIT_SCRIPTLEVEL(mPresentationData.flags)) {
    mPresentationData.scriptLevel += aScriptLevelIncrement;
  }

  if (NS_MATHML_IS_COMPRESSED(aFlagsToUpdate)) {
    // updating the compression flag is allowed
    if (NS_MATHML_IS_COMPRESSED(aFlagsValues)) {
      // 'compressed' means 'prime' style in App. G, TeXbook
      mPresentationData.flags |= NS_MATHML_COMPRESSED;
    }
    // no else. the flag is sticky. it retains its value once it is set
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmstyleFrame::UpdatePresentationDataFromChildAt(PRInt32  aFirstIndex,
                                                       PRInt32  aLastIndex,
                                                       PRInt32  aScriptLevelIncrement,
                                                       PRUint32 aFlagsValues,
                                                       PRUint32 aFlagsToUpdate)
{
  // mstyle is special...
  // Since UpdatePresentationDataFromChildAt() can be called by a parent frame,
  // wee need to ensure that the attributes of mstyle take precedence

  // see if the caller cares about the displaystyle flag
  PRBool displaystyleChanged = PR_FALSE;
  if (NS_MATHML_IS_DISPLAYSTYLE(aFlagsToUpdate)) {
    if (!NS_MATHML_IS_MSTYLE_WITH_DISPLAYSTYLE(mPresentationData.flags)) {
      // updating the displaystyle flag is allowed
      displaystyleChanged = PR_TRUE;
    }
    else {
      // our value takes precedence, updating is not allowed
      aFlagsToUpdate &= ~NS_MATHML_DISPLAYSTYLE;
    }
  }

  // see if the caller cares about the compression flag
  PRBool compressionChanged = PR_FALSE;
  if (NS_MATHML_IS_COMPRESSED(aFlagsToUpdate)) {
    compressionChanged = PR_TRUE;
  }

  if (NS_MATHML_IS_MSTYLE_WITH_EXPLICIT_SCRIPTLEVEL(mPresentationData.flags)) {
    aScriptLevelIncrement = 0;
  }

  if (!aScriptLevelIncrement && !displaystyleChanged && !compressionChanged)
    return NS_OK;  // quick return, there is nothing to change
 
  // let the base class worry about the update
  return
    nsMathMLContainerFrame::UpdatePresentationDataFromChildAt(
      aFirstIndex, aLastIndex, aScriptLevelIncrement,
      aFlagsValues, aFlagsToUpdate); 
}
