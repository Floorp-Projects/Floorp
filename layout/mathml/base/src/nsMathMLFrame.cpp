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
 */

#include "nsMathMLFrame.h"

NS_IMPL_QUERY_INTERFACE1(nsMathMLFrame, nsIMathMLFrame)

NS_IMETHODIMP
nsMathMLFrame::UpdatePresentationData(PRInt32  aScriptLevelIncrement,
                                      PRUint32 aFlagsValues,
                                      PRUint32 aFlagsToUpdate)
{
  mPresentationData.scriptLevel += aScriptLevelIncrement;
  // update flags that are relevant to this call
  if (NS_MATHML_IS_DISPLAYSTYLE(aFlagsToUpdate)) {
    // updating the displaystyle flag is allowed
    if (NS_MATHML_IS_DISPLAYSTYLE(aFlagsValues)) {
      mPresentationData.flags |= NS_MATHML_DISPLAYSTYLE;
    }
    else {
      mPresentationData.flags &= ~NS_MATHML_DISPLAYSTYLE;
    }
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
