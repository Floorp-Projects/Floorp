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

#include "nsMathMLAtoms.h"
#include "nsMathMLParts.h"
#include "nsMathMLmstyleFrame.h"

//
// <mstyle> -- style change
//

nsresult
NS_NewMathMLmstyleFrame(nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLmstyleFrame* it = new nsMathMLmstyleFrame;
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
nsMathMLmstyleFrame::Init(nsIPresContext&  aPresContext,
                          nsIContent*      aContent,
                          nsIFrame*        aParent,
                          nsIStyleContext* aContext,
                          nsIFrame*        aPrevInFlow)
{
  nsresult rv = nsMathMLContainerFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  mFlags = 0;

  // see if the displaystyle attribute is there
  nsAutoString value;
  if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None, 
                   nsMathMLAtoms::displaystyle, value)) {
    if (value == "true") {
      mDisplayStyle = PR_TRUE;
      mFlags |= NS_MATHML_MSTYLE_DISPLAYSTYLE;
    }
    else if (value == "false") {
      mDisplayStyle = PR_FALSE;
      mFlags |= NS_MATHML_MSTYLE_DISPLAYSTYLE;
    }
  }

  // see if the scriptlevel attribute is there
  if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None, 
                   nsMathMLAtoms::scriptlevel, value)) {
    PRInt32 aErrorCode, aUserValue;
    aUserValue = value.ToInteger(&aErrorCode); 
    if (NS_SUCCEEDED(aErrorCode)) {
      if (value[0] != '+' && value[0] != '-') { // record that it is an explicit value
        mFlags |= NS_MATHML_MSTYLE_SCRIPTLEVEL_EXPLICIT;
        mScriptLevel = aUserValue; // explicit value...
      }
      else {
        mScriptLevel += aUserValue; // incremental value...
      }
    }
  }


// TODO:
// Examine all other attributes and update the style context to be 
// inherited by all children.

  return rv;
}

NS_IMETHODIMP
nsMathMLmstyleFrame::UpdatePresentationData(PRInt32 aScriptLevelIncrement, 
                                            PRBool  aDisplayStyle)
{
  // The scriptlevel and displaystyle attributes of mstyle take precedence.
  // Update only if attributes are not there
  if (!(NS_MATHML_MSTYLE_HAS_DISPLAYSTYLE(mFlags))) {
    mDisplayStyle = aDisplayStyle;
  }
  if (!(NS_MATHML_MSTYLE_HAS_SCRIPTLEVEL_EXPLICIT(mFlags))) {
    mScriptLevel += aScriptLevelIncrement;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmstyleFrame::UpdatePresentationDataFromChildAt(PRInt32 aIndex, 
                                                       PRInt32 aScriptLevelIncrement,
                                                       PRBool  aDisplayStyle)
{
  // mstyle is special...
  // Since UpdatePresentationDataFromChildAt() can be called by a parent frame,
  // wee need to ensure that the attributes of mstyle take precedence
  if (NS_MATHML_MSTYLE_HAS_DISPLAYSTYLE(mFlags)) {
    aDisplayStyle = mDisplayStyle;
  }
  if (NS_MATHML_MSTYLE_HAS_SCRIPTLEVEL_EXPLICIT(mFlags)) {
    aScriptLevelIncrement = 0;
  }
  if (0 == aScriptLevelIncrement && aDisplayStyle == mDisplayStyle)
    return NS_OK;

  // let the base class worry about the update
  return nsMathMLContainerFrame::UpdatePresentationDataFromChildAt(aIndex,
                                         aScriptLevelIncrement, aDisplayStyle);
}
