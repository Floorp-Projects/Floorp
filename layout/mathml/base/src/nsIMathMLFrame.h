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

#ifndef nsIMathMLFrame_h___
#define nsIMathMLFrame_h___

// IID for the nsIMathMLFrame interface (the IID was taken from IIDS.h) 
#define NS_IMATHMLFRAME_IID    \
{ 0xa6cf90f6, 0x15b3, 0x11d2,    \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } }

static NS_DEFINE_IID(kIMathMLFrameIID, NS_IMATHMLFRAME_IID);

class nsIMathMLFrame : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IMATHMLFRAME_IID; return iid; }

 
 /* SUPPORT FOR STRETCHY ELEMENTS: <mo> */
 /*====================================================================*/

 /* method used to ask a stretchy MathML frame to stretch 
  * itself depending on its context
  */

  NS_IMETHOD 
  Stretch(nsIPresContext&    aPresContext,
          nsStretchDirection aStretchDirection,
          nsCharMetrics&     aContainerSize,
          nsCharMetrics&     aDesiredStretchSize) = 0;


 /* SUPPORT FOR SCRIPTING ELEMENTS: */
 /*====================================================================*/

 /* GetPresentationData :
 /* returns the scriptlevel and displaystyle of the frame */
  NS_IMETHOD
  GetPresentationData(PRInt32* aScriptLevel, 
                      PRBool*  aDisplayStyle) = 0;

 /* UpdatePresentationData :
 /* Increment the scriptlevel of the frame, and set its displaystyle. 
  * Note that <mstyle> is the only tag which allows to set
  * <mstyle displaystyle="true|false" scriptlevel="[+|-]number">
  * Therefore <mstyle> has its peculiar version of this method.
  */ 
  NS_IMETHOD
  UpdatePresentationData(PRInt32 aScriptLevelIncrement, 
                         PRBool  aDisplayStyle) = 0;


 /* UpdatePresentationDataFromChildAt :
 /* Increments the scriplevel and set the display level on the whole tree.
  * For child frames at: aIndex, aIndex+1, aIndex+2, etc, this method set 
  * their mDisplayStyle to aDisplayStyle and increment their mScriptLevel
  * with aScriptLevelIncrement. The increment is propagated down to the 
  * subtrees of each of these child frames. Note that <mstyle> is the only
  * tag which allows <mstyle  displaystyle="true|false" scriptlevel="[+|-]number">
  * to reset or increment the scriptlevel in a manual way. Therefore <mstyle> 
  * has its own peculiar version of this method.
  */
  NS_IMETHOD
  UpdatePresentationDataFromChildAt(PRInt32 aIndex, 
                                    PRInt32 aScriptLevelIncrement,
                                    PRBool  aDisplayStyle) = 0;
};

#endif /* nsIMathMLFrame_h___ */

