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

struct nsPresentationData;
struct nsEmbellishData;
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

  NS_IMETHOD
  GetReference(nsPoint& aReference) = 0;

  NS_IMETHOD
  SetReference(const nsPoint& aReference) = 0;


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

 /* GetEmbellishData/SetEmbellishData :
  * Get/Set the mEmbellishData member variable.
  */

  NS_IMETHOD
  GetEmbellishData(nsEmbellishData& aEmbellishData) = 0;

  NS_IMETHOD
  SetEmbellishData(const nsEmbellishData& aEmbellishData) = 0;


 /* SUPPORT FOR SCRIPTING ELEMENTS: */
 /*====================================================================*/

 /* GetPresentationData/SetPresentationData :
  * Get/Set the mPresentationData member variable.
  */

  NS_IMETHOD
  GetPresentationData(nsPresentationData& aPresentationData) = 0;

  NS_IMETHOD
  SetPresentationData(const nsPresentationData& aPresentationData) = 0;

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
  * tag which allows <mstyle displaystyle="true|false" scriptlevel="[+|-]number">
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

// struct used by a frame to modulate its presentation
struct nsPresentationData {
  PRUint32  flags; // bits for: displaystyle, compressed, etc
  nsIFrame* mstyle; // up-pointer on the mstyle frame, if any, that defines the scope
  PRUint32  scriptLevel; // Relevant to nested frames within: msub, msup, msubsup, munder,
                         // mover, munderover, mmultiscripts, mfrac, mroot, mtable.
  nsPresentationData()
  {
    flags = 0;
    mstyle = nsnull;
    scriptLevel = 0;
  }
};

// struct used by an embellished container to keep track of its embellished child
struct nsEmbellishData {
  PRUint32  flags;
  nsIFrame* firstChild; // handy pointer on our embellished child 
  nsIFrame* core; // pointer on the mo frame at the core of the embellished hierarchy
  nsStretchDirection direction;
  nscoord leftSpace, rightSpace;

  nsEmbellishData()
  {
    flags = 0;
    firstChild = nsnull;
    core = nsnull;
    direction = NS_STRETCH_DIRECTION_UNSUPPORTED;
    leftSpace = rightSpace = 0;
  }
};

// -------------
// Bits used for the presentation flags -- these bits are set
// in their relevant situation as they become available

// This bit is set if the frame is in the *context* of displaystyle=true.
// Note: This doesn't mean that the frame has displaystyle=true as attribute,
// <mstyle> is the only tag which allows <mstyle displaystyle="true|false">.
// The bit merely tells the context of the frame. In the context of 
// displaystyle="false", it is intended to slightly alter how the
// rendering is done in inline mode.
#define NS_MATHML_DISPLAYSTYLE                        0x00000001

// This bit is used to emulate TeX rendering. 
// Internal use only, cannot be set by the user with an attribute.
#define NS_MATHML_COMPRESSED                          0x00000002

// This bit is set if the frame is actually an <mstyle> frame *and* that
// <mstyle> frame has an explicit attribute scriptlevel="value".
// Note: the flag is not set if the <mstyle> instead has an incremental +/-value.
#define NS_MATHML_MSTYLE_WITH_EXPLICIT_SCRIPTLEVEL    0x00000004

// This bit is set if the frame is actually an <mstyle> *and* that
// <mstyle> has an explicit attribute displaystyle="true" or "false"
#define NS_MATHML_MSTYLE_WITH_DISPLAYSTYLE            0x00000008

// This bit is set if the frame is an <mover> or <munderover> with
// an accent frame
#define NS_MATHML_ACCENTOVER                          0x00000010

// This bit is set if the frame is an <munder> or <munderover> with
// an accentunder frame
#define NS_MATHML_ACCENTUNDER                         0x00000020

// This bit is set if the frame is <mover>, <munder> or <munderover>
// whose base frame is a <mo> frame (or an embellished container with
// a core <mo>) for which the movablelimits attribute is set to true
#define NS_MATHML_MOVABLELIMITS                       0x00000040

// This bit is used for visual debug. When set, the bounding box
// of your frame is painted. You should therefore ensure that you
// have properly filled your mReference and mBoundingMetrics in
// Place().
#define NS_MATHML_SHOW_BOUNDING_METRICS               0x80000000

// Macros that retrieve those bits
#define NS_MATHML_IS_DISPLAYSTYLE(_flags) \
  (NS_MATHML_DISPLAYSTYLE == ((_flags) & NS_MATHML_DISPLAYSTYLE))

#define NS_MATHML_IS_COMPRESSED(_flags) \
  (NS_MATHML_COMPRESSED == ((_flags) & NS_MATHML_COMPRESSED))

#define NS_MATHML_IS_MSTYLE_WITH_DISPLAYSTYLE(_flags) \
  (NS_MATHML_MSTYLE_WITH_DISPLAYSTYLE == ((_flags) & NS_MATHML_MSTYLE_WITH_DISPLAYSTYLE))

#define NS_MATHML_IS_MSTYLE_WITH_EXPLICIT_SCRIPTLEVEL(_flags) \
  (NS_MATHML_MSTYLE_WITH_EXPLICIT_SCRIPTLEVEL == ((_flags) & NS_MATHML_MSTYLE_WITH_EXPLICIT_SCRIPTLEVEL))

#define NS_MATHML_IS_ACCENTOVER(_flags) \
  (NS_MATHML_ACCENTOVER == ((_flags) & NS_MATHML_ACCENTOVER))

#define NS_MATHML_IS_ACCENTUNDER(_flags) \
  (NS_MATHML_ACCENTUNDER == ((_flags) & NS_MATHML_ACCENTUNDER))

#define NS_MATHML_IS_MOVABLELIMITS(_flags) \
  (NS_MATHML_MOVABLELIMITS == ((_flags) & NS_MATHML_MOVABLELIMITS))

#define NS_MATHML_PAINT_BOUNDING_METRICS(_flags) \
  (NS_MATHML_SHOW_BOUNDING_METRICS == ((_flags) & NS_MATHML_SHOW_BOUNDING_METRICS))

// --------------
// Bits used for the embellish flags -- these bits are set
// in their relevant situation as they become available

// This bit is set if the frame is an embellished operator. 
#define NS_MATHML_EMBELLISH_OPERATOR             0x00000001

// This bit is set if the frame is an embellished container that
// will fire a stretch command on its first (non-empty) child.
#define NS_MATHML_STRETCH_FIRST_CHILD            NS_MATHML_EMBELLISH_OPERATOR

// This bit is set if the frame will fire a stretch command on all
// its (non-empty) children.
// Tags like <mrow> (or an inferred mrow), munderover, etc,
// will fire a stretch command on all their non-empty children
#define NS_MATHML_STRETCH_ALL_CHILDREN            0x00000002

// This bit is set if the frame is an <mo> frame that should behave
// like an accent
#define NS_MATHML_EMBELLISH_ACCENT                0x00000004

// This bit is set if the frame is an <mo> frame with the movablelimits
// attribute set to true 
#define NS_MATHML_EMBELLISH_MOVABLELIMITS         0x00000008

// a bit used for debug
#define NS_MATHML_STRETCH_DONE                    0x80000000


// Macros that retrieve those bits

#define NS_MATHML_IS_EMBELLISH_OPERATOR(_flags) \
  (NS_MATHML_EMBELLISH_OPERATOR == ((_flags) & NS_MATHML_EMBELLISH_OPERATOR))

#define NS_MATHML_WILL_STRETCH_FIRST_CHILD(_flags) \
  (NS_MATHML_STRETCH_FIRST_CHILD == ((_flags) & NS_MATHML_STRETCH_FIRST_CHILD))

#define NS_MATHML_WILL_STRETCH_ALL_CHILDREN(_flags) \
  (NS_MATHML_STRETCH_ALL_CHILDREN == ((_flags) & NS_MATHML_STRETCH_ALL_CHILDREN))

#define NS_MATHML_STRETCH_WAS_DONE(_flags) \
  (NS_MATHML_STRETCH_DONE == ((_flags) & NS_MATHML_STRETCH_DONE))

#define NS_MATHML_EMBELLISH_IS_ACCENT(_flags) \
  (NS_MATHML_EMBELLISH_ACCENT == ((_flags) & NS_MATHML_EMBELLISH_ACCENT))

#define NS_MATHML_EMBELLISH_IS_MOVABLELIMITS(_flags) \
  (NS_MATHML_EMBELLISH_MOVABLELIMITS == ((_flags) & NS_MATHML_EMBELLISH_MOVABLELIMITS))

#endif /* nsIMathMLFrame_h___ */
