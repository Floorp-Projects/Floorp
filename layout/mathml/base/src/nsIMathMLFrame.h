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

struct nsEmbellishState;
struct nsStretchMetrics;
typedef PRInt32 nsStretchDirection;

#define NS_STRETCH_DIRECTION_UNSUPPORTED  -1
#define NS_STRETCH_DIRECTION_DEFAULT       0
#define NS_STRETCH_DIRECTION_HORIZONTAL    1
#define NS_STRETCH_DIRECTION_VERTICAL      2

// IID for the nsIMathMLFrame interface (the IID was taken from IIDS.h) 
/* a6cf9113-15b3-11d2-932e-00805f8add32 */
#define NS_IMATHMLFRAME_IID   \
{ 0xa6cf9113, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } }

static NS_DEFINE_IID(kIMathMLFrameIID, NS_IMATHMLFRAME_IID);

class nsIMathMLFrame : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IMATHMLFRAME_IID; return iid; }

 /* SUPPORT FOR PRECISE POSITIONING */
 /*====================================================================*/
 
 /* Metrics that _exactly_ enclose the text of the frame.
  * The frame *must* have *already* being reflowed, before you can call
  * the GetBoundingMetrics() method.
  * Note that for a frame with nested children, the bounding metrics
  * will exactly enclose its children. For example, the bounding metrics
  * of msub is the smallest rectangle that exactly encloses both the 
  * base and the subscript.
  */
  NS_IMETHOD
  GetBoundingMetrics(nsBoundingMetrics& aBoundingMetrics) = 0;

  NS_IMETHOD
  SetBoundingMetrics(const nsBoundingMetrics& aBoundingMetrics) = 0;
/*
  NS_IMETHOD
  GetReference(nsPoint& aReference) = 0;

  NS_IMETHOD
  SetReference(const nsPoint& aReference) = 0;
*/

 /* SUPPORT FOR STRETCHY ELEMENTS: <mo> */
 /*====================================================================*/

 /* Stretch :
  * Called to ask a stretchy MathML frame to stretch itself depending
  * on its context.
  *
  * @param aStretchDirection [in] the direction where to attempt to
  *        stretch.
  * @param aContainerSize [in] struct that suggests the maximumn size for
  *        the stretched frame. Only member data of the struct that are 
  *        relevant to the direction are used (the rest is ignored). 
  * @param aDesiredStretchSize [in/out] On input the current size
  *        size of the frame, on output the size after stretching.
  */
  NS_IMETHOD 
  Stretch(nsIPresContext*      aPresContext,
          nsIRenderingContext& aRenderingContext,
          nsStretchDirection   aStretchDirection,
          nsStretchMetrics&    aContainerSize,
          nsStretchMetrics&    aDesiredStretchSize) = 0;

#if 0
  NS_IMETHOD
  GetDesiredStretchSize(nsIPresContext*      aPresContext,
                        nsIRenderingContext& aRenderingContext,
                        nsStretchMetrics&    aDesiredStretchSize) = 0;
#endif
 /* Place :
  * This method is used before returning from Reflow(), or when a MathML frame
  * has just been stretched. It is called to fine-tune the positions of the elements.
  *
  * IMPORTANT: This method uses the origin of child frames (rect.x and rect.y) as
  * placeholders between calls: On invocation, child->GetRect(rect) should give a
  * rect such that rect.x holds the child's descent, rect.y holds the child's ascent, 
  * (rect.width should give the width, and rect.height should give the height).
  * The Place() method will use this information to compute the desired size
  * of the frame.
  *
  * @param aPlaceOrigin [in]
  *        If aPlaceOrigin is false, compute your desired size using the
  *        information in your children's rectangles. However, upon return,
  *        the origins of your children should keep their ascent information, i.e., 
  *        a child rect.x, and rect.y should still act like placeholders for the 
  *        child's descent and ascent. 
  *
  *        If aPlaceOrigin is true, reflow is finished. You should position all
  *        your children, and return your desired size. You should now convert
  *        the origins of your child frames into the coordinate system 
  *        expected by Gecko (which is relative to the upper-left
  *        corner of the parent) and use FinishReflowChild() on your children
  *        to complete post-reflow operations.
  *
  * @param aDesiredSize [out] parameter where you should return your
  *        desired size and your ascent/descent info. Compute your desired size 
  *        using the information in your children's rectangles, and include any
  *        space you want for border/padding in the desired size you return.  
  */
  NS_IMETHOD
  Place(nsIPresContext*      aPresContext,
        nsIRenderingContext& aRenderingContext,
        PRBool               aPlaceOrigin,
        nsHTMLReflowMetrics& aDesiredSize) = 0;

 /* EmbellishOperator :
  * Call this method to probe and set a frame as an "embellished operator".
  * Calls must be bottom up. The method will set the frame as an
  * "embellished operator" if the frame satisfies the definition of 
  * the MathML REC. Conversely, it will set the frame as
  * non-embellished if it is not an "embellished operator".
  *
  * Note that this method must only be called from tags who *can* be
  * embellished operators.
  *
  * The MathML REC precisely defines an "embellished operator" as:
  * - an <mo> element;
  * - or one of the elements <msub>, <msup>, <msubsup>, <munder>, <mover>,
  *   <munderover>, <mmultiscripts>, <mfrac>, or <semantics>, whose first 
  *   argument exists and is an embellished operator;
  * - or one of the elements <mstyle>, <mphantom>, or <mpadded>, such that
  *   an <mrow> containing the same arguments would be an embellished
  *   operator;
  * - or an <maction> element whose selected subexpression exists and is an
  *   embellished operator;
  * - or an <mrow> whose arguments consist (in any order) of one embellished
  *   operator and zero or more spacelike elements.
  *
  * When an embellished frame receives a Stretch() command, it passes the
  * command to its first child and the stretched size is bubbled up from the
  * inner-most <mo> frame.
  */
  NS_IMETHOD
  EmbellishOperator() = 0;

 /* GetEmbellishState/SetEmbellishState :
  * Get/Set the mEmbellish member data.
  */

  NS_IMETHOD
  GetEmbellishState(nsEmbellishState& aEmbellishState) = 0;

  NS_IMETHOD
  SetEmbellishState(const nsEmbellishState& aEmbellishState) = 0;


 /* SUPPORT FOR SCRIPTING ELEMENTS: */
 /*====================================================================*/

 /* GetPresentationData :
  * returns the scriptlevel and displaystyle of the frame
  */
  NS_IMETHOD
  GetPresentationData(PRInt32* aScriptLevel, 
                      PRBool*  aDisplayStyle) = 0;

 /* UpdatePresentationData :
  * Increments the scriptlevel of the frame, and set its displaystyle. 
  * Note that <mstyle> is the only tag which allows to set
  * <mstyle displaystyle="true|false" scriptlevel="[+|-]number">
  * to reset or increment the scriptlevel in a manual way. 
  * Therefore <mstyle> has its peculiar version of this method.
  */ 
  NS_IMETHOD
  UpdatePresentationData(PRInt32 aScriptLevelIncrement, 
                         PRBool  aDisplayStyle) = 0;

 /* UpdatePresentationDataFromChildAt :
  * Increments the scriplevel and set the display level on the whole tree.
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

/* Structure used for stretching a frame. */
struct nsStretchMetrics {
//  nscoord leading;
  nscoord descent, ascent;
  nscoord width, height;
  float leftSpace, rightSpace;

  nsStretchMetrics(nscoord aDescent    = 0, 
                   nscoord aAscent     = 0, 
                   nscoord aWidth      = 0, 
                   nscoord aHeight     = 0,
                   float   aLeftSpace  = 0.0f, 
                   float   aRightSpace = 0.0f) {
    width = aWidth; 
    height = aHeight;
    ascent = aAscent; 
    descent = aDescent;
    leftSpace = aLeftSpace;
    rightSpace = aRightSpace;
  }

  nsStretchMetrics(const nsStretchMetrics& aStretchMetrics) {
    width = aStretchMetrics.width; 
    height = aStretchMetrics.height;
    ascent = aStretchMetrics.ascent; 
    descent = aStretchMetrics.descent;
    leftSpace = aStretchMetrics.leftSpace;
    rightSpace = aStretchMetrics.rightSpace;
  }

  nsStretchMetrics(const nsHTMLReflowMetrics& aReflowMetrics) {
    width = aReflowMetrics.width; 
    height = aReflowMetrics.height;
    ascent = aReflowMetrics.ascent; 
    descent = aReflowMetrics.descent;
    leftSpace = 0.0f;
    rightSpace = 0.0f;
  }

  PRBool
  operator == (const nsStretchMetrics& aStretchMetrics) {
    return (width == aStretchMetrics.width &&
            height == aStretchMetrics.height &&
            ascent == aStretchMetrics.ascent &&
            descent == aStretchMetrics.descent &&
            leftSpace == aStretchMetrics.leftSpace &&
            rightSpace == aStretchMetrics.rightSpace);
  }
};

#define NS_MATHML_EMBELLISH_OPERATOR    0x1

#define NS_MATHML_STRETCH_FIRST_CHILD   NS_MATHML_EMBELLISH_OPERATOR
#define NS_MATHML_STRETCH_ALL_CHILDREN  0x2

#define NS_MATHML_STRETCH_DONE          0x4

#define NS_MATHML_IS_EMBELLISH_OPERATOR(_flags) \
  (NS_MATHML_EMBELLISH_OPERATOR == ((_flags) & NS_MATHML_EMBELLISH_OPERATOR))

#define NS_MATHML_STRETCH_WAS_DONE(_flags) \
  (NS_MATHML_STRETCH_DONE == ((_flags) & NS_MATHML_STRETCH_DONE))

// an embellished container will fire a stretch command to its first (non-empty) child
#define NS_MATHML_WILL_STRETCH_FIRST_CHILD(_flags) \
  (NS_MATHML_STRETCH_FIRST_CHILD == ((_flags) & NS_MATHML_STRETCH_FIRST_CHILD))

// <mrow> (or an inferred mrow) will fire a stretch command to all its (non-empty) children
#define NS_MATHML_WILL_STRETCH_ALL_CHILDREN(_flags) \
  (NS_MATHML_STRETCH_ALL_CHILDREN == ((_flags) & NS_MATHML_STRETCH_ALL_CHILDREN))

// struct used by an embellished container to keep track of its embellished child
struct nsEmbellishState {
  PRUint32  flags;
  nsIFrame* firstChild;     // handy pointer on our embellished child 
  nsIFrame* core;           // pointer on the mo frame at the core of the embellished hierarchy
  nsStretchDirection direction;
  nscoord leftSpace, rightSpace;

  nsEmbellishState()
  {
    flags = 0;
    firstChild = nsnull;
    core = nsnull;
    direction = NS_STRETCH_DIRECTION_UNSUPPORTED;
    leftSpace = rightSpace = 0;
  }
};

#endif /* nsIMathMLFrame_h___ */

