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

  // cache the values that we would have if we were not special...
  // In the event of dynamic updates, e.g., if our displastyle and/or
  // scriptlevel attributes are removed, we will recover our state using
  // these cached values
  mInheritedScriptLevel = mPresentationData.scriptLevel;
  mInheritedDisplayStyle = mPresentationData.flags & NS_MATHML_DISPLAYSTYLE;

  // see if the displaystyle attribute is there
  nsAutoString value;
  if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttr(kNameSpaceID_None, 
                   nsMathMLAtoms::displaystyle_, value)) {
    if (value.Equals(NS_LITERAL_STRING("true"))) {
      mPresentationData.flags |= NS_MATHML_MSTYLE_WITH_DISPLAYSTYLE;
      mPresentationData.flags |= NS_MATHML_DISPLAYSTYLE;
    }
    else if (value.Equals(NS_LITERAL_STRING("false"))) {
      mPresentationData.flags |= NS_MATHML_MSTYLE_WITH_DISPLAYSTYLE;
      mPresentationData.flags &= ~NS_MATHML_DISPLAYSTYLE;
    }
  }

  // see if the scriptlevel attribute is there
  if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttr(kNameSpaceID_None, 
                   nsMathMLAtoms::scriptlevel_, value)) {
    PRInt32 errorCode, userValue;
    userValue = value.ToInteger(&errorCode); 
    if (!errorCode) {
      if (value[0] != '+' && value[0] != '-') { // record that it is an explicit value
        mPresentationData.flags |= NS_MATHML_MSTYLE_WITH_EXPLICIT_SCRIPTLEVEL;
        mPresentationData.scriptLevel = userValue;
      }
      else {
        mPresentationData.scriptLevel += userValue; // incremental value...
      }
    }
  }


// TODO:
// Examine all other attributes

  return rv;
}

NS_IMETHODIMP
nsMathMLmstyleFrame::UpdatePresentationData(nsIPresContext* aPresContext,
                                            PRInt32         aScriptLevelIncrement,
                                            PRUint32        aFlagsValues,
                                            PRUint32        aFlagsToUpdate)
{
  // mstyle is special...
  // Since UpdatePresentationData() can be called by a parent frame, the
  // scriptlevel and displaystyle attributes of mstyle must take precedence.
  // Update only if attributes are not there

  // But... cache the values that we would have if we were not special...
  // In the event of dynamic updates, e.g., if our displastyle and/or
  // scriptlevel attributes are removed, we will recover our state using
  // these cached values
  mInheritedScriptLevel += aScriptLevelIncrement;
  if (NS_MATHML_IS_DISPLAYSTYLE(aFlagsToUpdate)) {
    mInheritedDisplayStyle = aFlagsValues & NS_MATHML_DISPLAYSTYLE;
  }

  // see if updating the displaystyle flag is allowed
  if (!NS_MATHML_IS_MSTYLE_WITH_DISPLAYSTYLE(mPresentationData.flags)) {
    // see if the displaystyle flag is relevant to this call
    if (NS_MATHML_IS_DISPLAYSTYLE(aFlagsToUpdate)) {
      if (NS_MATHML_IS_DISPLAYSTYLE(aFlagsValues)) {
        mPresentationData.flags |= NS_MATHML_DISPLAYSTYLE;
      }
      else {
        mPresentationData.flags &= ~NS_MATHML_DISPLAYSTYLE;
      }
    }
  }

  // see if updating the scriptlevel is allowed
  if (!NS_MATHML_IS_MSTYLE_WITH_EXPLICIT_SCRIPTLEVEL(mPresentationData.flags)) {
    mPresentationData.scriptLevel += aScriptLevelIncrement;
  }

  // see if the compression flag is relevant to this call
  if (NS_MATHML_IS_COMPRESSED(aFlagsToUpdate)) {
    if (NS_MATHML_IS_COMPRESSED(aFlagsValues)) {
      // 'compressed' means 'prime' style in App. G, TeXbook
      mPresentationData.flags |= NS_MATHML_COMPRESSED;
    }
    // no else. the flag is sticky. it retains its value once it is set
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmstyleFrame::UpdatePresentationDataFromChildAt(nsIPresContext* aPresContext,
                                                       PRInt32         aFirstIndex,
                                                       PRInt32         aLastIndex,
                                                       PRInt32         aScriptLevelIncrement,
                                                       PRUint32        aFlagsValues,
                                                       PRUint32        aFlagsToUpdate)
{
  // mstyle is special...
  // Since UpdatePresentationDataFromChildAt() can be called by a parent frame,
  // wee need to ensure that the attributes of mstyle take precedence

  if (NS_MATHML_IS_DISPLAYSTYLE(aFlagsToUpdate)) {
    if (NS_MATHML_IS_MSTYLE_WITH_DISPLAYSTYLE(mPresentationData.flags)) {
      // our current state takes precedence, updating is not allowed
      aFlagsToUpdate &= ~NS_MATHML_DISPLAYSTYLE;
      aFlagsValues &= ~NS_MATHML_DISPLAYSTYLE;
    }
  }

  if (NS_MATHML_IS_MSTYLE_WITH_EXPLICIT_SCRIPTLEVEL(mPresentationData.flags)) {
    // our current state takes precedence, updating is not allowed
    aScriptLevelIncrement = 0;
  }

  // let the base class worry about the update
  return
    nsMathMLContainerFrame::UpdatePresentationDataFromChildAt(
      aPresContext, aFirstIndex, aLastIndex, aScriptLevelIncrement,
      aFlagsValues, aFlagsToUpdate); 
}

NS_IMETHODIMP
nsMathMLmstyleFrame::AttributeChanged(nsIPresContext* aPresContext,
                                      nsIContent*     aContent,
                                      PRInt32         aNameSpaceID,
                                      nsIAtom*        aAttribute,
                                      PRInt32         aModType, 
                                      PRInt32         aHint)
{
  if (nsMathMLAtoms::displaystyle_ == aAttribute ||
      nsMathMLAtoms::scriptlevel_ == aAttribute) {
    nsPresentationData oldData = mPresentationData;
    // process our attributes
    nsAutoString value;
    if (NS_CONTENT_ATTR_NOT_THERE == mContent->GetAttr(kNameSpaceID_None,
                     nsMathMLAtoms::scriptlevel_, value)) {
      // when our scriptlevel attribute is gone, we recover our inherited scriptlevel
      mPresentationData.flags &= ~NS_MATHML_MSTYLE_WITH_EXPLICIT_SCRIPTLEVEL;
      mPresentationData.scriptLevel = mInheritedScriptLevel;
    }
    else {
      PRInt32 errorCode, userValue;
      userValue = value.ToInteger(&errorCode); 
      if (!errorCode) {
        if (value[0] != '+' && value[0] != '-') {
          // record that it is an explicit value
          mPresentationData.flags |= NS_MATHML_MSTYLE_WITH_EXPLICIT_SCRIPTLEVEL;
          mPresentationData.scriptLevel = userValue;
        }
        else {
          // incremental value...
          mPresentationData.flags &= ~NS_MATHML_MSTYLE_WITH_EXPLICIT_SCRIPTLEVEL;
          mPresentationData.scriptLevel = mInheritedScriptLevel + userValue;
        }
      }
    }
    if (NS_CONTENT_ATTR_NOT_THERE == mContent->GetAttr(kNameSpaceID_None,
                     nsMathMLAtoms::displaystyle_, value)) {
      // when our displaystyle attribute is gone, we recover our inherited displaystyle
      mPresentationData.flags &= ~NS_MATHML_MSTYLE_WITH_DISPLAYSTYLE;
      mPresentationData.flags |= mInheritedDisplayStyle & NS_MATHML_DISPLAYSTYLE;
    }
    else {
      if (value.Equals(NS_LITERAL_STRING("true"))) {
        mPresentationData.flags |= NS_MATHML_MSTYLE_WITH_DISPLAYSTYLE;
        mPresentationData.flags |= NS_MATHML_DISPLAYSTYLE;
      }
      else if (value.Equals(NS_LITERAL_STRING("false"))) {
        mPresentationData.flags |= NS_MATHML_MSTYLE_WITH_DISPLAYSTYLE;
        mPresentationData.flags &= ~NS_MATHML_DISPLAYSTYLE;
      }
    }

    // propagate to our children if something changed
    if (oldData.flags != mPresentationData.flags ||
        oldData.scriptLevel != mPresentationData.scriptLevel) {
      PRUint32 newValues = 0, whichFlags = 0;
      if (NS_MATHML_IS_DISPLAYSTYLE(oldData.flags) !=
          NS_MATHML_IS_DISPLAYSTYLE(mPresentationData.flags)) {
        newValues = NS_MATHML_DISPLAYSTYLE & mPresentationData.flags;
        whichFlags = NS_MATHML_DISPLAYSTYLE;
      }
      // use the base method here because we really want to reflect any updates
      nsMathMLContainerFrame::UpdatePresentationDataFromChildAt(aPresContext, 0, -1, 
        mPresentationData.scriptLevel - oldData.scriptLevel, newValues, whichFlags);
      // now walk up to our immediate ancestor that implements the
      // nsIMathMLFrame interface and grab its scriptlevel
      PRInt32 parentScriptLevel = oldData.scriptLevel;
      nsIFrame* parent = mParent;
      while (parent) {
        nsIMathMLFrame* mathMLFrame;
        parent->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&mathMLFrame);
        if (mathMLFrame) {
        	nsPresentationData parentData;
          mathMLFrame->GetPresentationData(parentData);
          parentScriptLevel = parentData.scriptLevel;
          break;
        }
        parent->GetParent(&parent);
      }
      // re-resolve style data in our subtree to sync any change of script sizes
      PropagateScriptStyleFor(aPresContext, this, parentScriptLevel);
    }
  }

  return nsMathMLContainerFrame::
         AttributeChanged(aPresContext, aContent, aNameSpaceID,
                          aAttribute, aModType, aHint);
}
