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
//#define SHOW_BOUNDING_BOX 1
#ifndef nsIMathMLFrame_h___
#define nsIMathMLFrame_h___

struct nsPresentationData;
struct nsEmbellishData;

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
  *        of the frame, on output the size after stretching.
  */
  NS_IMETHOD 
  Stretch(nsIPresContext*      aPresContext,
          nsIRenderingContext& aRenderingContext,
          nsStretchDirection   aStretchDirection,
          nsBoundingMetrics&   aContainerSize,
          nsHTMLReflowMetrics& aDesiredStretchSize) = 0;

#if 0
  NS_IMETHOD
  GetDesiredStretchSize(nsIPresContext*      aPresContext,
                        nsIRenderingContext& aRenderingContext,
                        nsBoundingMetrics&   aDesiredStretchSize) = 0;
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
  * Increments the scriptlevel of the frame, and updates its displaystyle and
  * compression flags. The displaystyle flag of an environment gets updated
  * according to the MathML specification. A frame becomes "compressed" (or
  * "cramped") according to TeX rendering rules (TeXBook, Ch.17, p.140-141).
  *
  * Note that <mstyle> is the only tag which allows to set
  * <mstyle displaystyle="true|false" scriptlevel="[+|-]number">
  * to reset or increment the scriptlevel in a manual way. 
  * Therefore <mstyle> has its own peculiar version of this method.
  *
  * @param aScriptLevelIncrement [in]
  *        The value with which to increment mScriptLevel in the frame.
  *
  * @param aFlagsValues [in]
  *        The new values (e.g., display, compress) that are going to be
  *        updated.
  *
  * @param aFlagsToUpdate [in]
  *        The flags that are relevant to this call. Since not all calls
  *        are meant to update all flags at once, aFlagsToUpdate is used
  *        to distinguish flags that need to retain their existing values
  *        from flags that need to be turned on (or turned off). If a bit
  *        is set in aFlagsToUpdate, then the corresponding value (which
  *        can be 0 or 1) is taken from aFlagsValues and applied to the
  *        frame. Therefore, by setting their bits in aFlagsToUpdate, and
  *        setting their desired values in aFlagsValues, it is possible to
  *        update some flags in the frame, leaving the other flags unchanged.
  */
  NS_IMETHOD
  UpdatePresentationData(PRInt32  aScriptLevelIncrement,
                         PRUint32 aFlagsValues,
                         PRUint32 aFlagsToUpdate) = 0;

 /* UpdatePresentationDataFromChildAt :
  * Increments the scriplevel and sets the displaystyle and compression flags
  * on the whole tree. For child frames at aFirstIndex up to aLastIndex, this
  * method sets their displaystyle and compression flags, and increment their
  * mScriptLevel with aScriptLevelIncrement. The update is propagated down 
  * the subtrees of each of these child frames. 
  *
  * Note that <mstyle> is the only tag which allows
  * <mstyle displaystyle="true|false" scriptlevel="[+|-]number">
  * to reset or increment the scriptlevel in a manual way.
  * Therefore <mstyle> has its own peculiar version of this method.
  *
  * @param aFirstIndex [in]
  *        Index of the first child from where the update is propagated.
  *
  * @param aLastIndex [in]
  *        Index of the last child where to stop the update.
  *        A value of -1 means up to last existing child.
  *
  * @param aScriptLevelIncrement [in]
  *        The value with which to increment mScriptLevel in the whole sub-trees.
  *
  * @param aFlagsValues [in]
  *        The new values (e.g., display, compress) that are going to be
  *        assigned in the whole sub-trees.
  *
  * @param aFlagsToUpdate [in]
  *        The flags that are relevant to this call. See UpdatePresentationData()
  *        for more details about this parameter.
  */
  NS_IMETHOD
  UpdatePresentationDataFromChildAt(PRInt32  aFirstIndex,
                                    PRInt32  aLastIndex,
                                    PRInt32  aScriptLevelIncrement,
                                    PRUint32 aFlagsValues,
                                    PRUint32 aFlagsToUpdate) = 0;
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
  float leftSpace, rightSpace;

  nsEmbellishData()
  {
    flags = 0;
    firstChild = nsnull;
    core = nsnull;
    direction = NS_STRETCH_DIRECTION_UNSUPPORTED;
    leftSpace = rightSpace = 0.0f;
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

// This bit is set if the frame is an <mover>, <munder> or <munderover>
// whose base frame is a <mo> frame (or an embellished container with
// a core <mo>) for which the movablelimits attribute is set to true
#define NS_MATHML_MOVABLELIMITS                       0x00000040

// This bit is set when the frame cannot be formatted due to an
// error (e.g., invalid markup such as a <msup> without an overscript).
// When set, a visual feedback will be provided to the user.
#define NS_MATHML_ERROR                               0x80000000

// This bit is used for visual debug. When set, the bounding box
// of your frame is painted. This visual debug enable to ensure that
// you have properly filled your mReference and mBoundingMetrics in
// Place().
#define NS_MATHML_SHOW_BOUNDING_METRICS               0x40000000

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

#define NS_MATHML_HAS_ERROR(_flags) \
  (NS_MATHML_ERROR == ((_flags) & NS_MATHML_ERROR))

#define NS_MATHML_PAINT_BOUNDING_METRICS(_flags) \
  (NS_MATHML_SHOW_BOUNDING_METRICS == ((_flags) & NS_MATHML_SHOW_BOUNDING_METRICS))

// --------------
// Bits used for the embellish flags -- these bits are set
// in their relevant situation as they become available

// This bit is set if the frame is an embellished operator. 
#define NS_MATHML_EMBELLISH_OPERATOR                0x00000001

// This bit is set if the frame will fire a vertical stretch
// command on all its (non-empty) children.
// Tags like <mrow> (or an inferred mrow), mpadded, etc, will fire a
// vertical stretch command on all their non-empty children
#define NS_MATHML_STRETCH_ALL_CHILDREN_VERTICALLY   0x00000002

// This bit is set if the frame will fire a horizontal stretch
// command on all its (non-empty) children.
// Tags like munder, mover, munderover, will fire a 
// horizontal stretch command on all their non-empty children
#define NS_MATHML_STRETCH_ALL_CHILDREN_HORIZONTALLY 0x00000004

// This bit is set if the frame is an <mo> frame that should behave
// like an accent XXX since it is <mo> specific, use NS_MATHML_EMBELLISH_MO_ACCENT instead?
#define NS_MATHML_EMBELLISH_ACCENT                  0x00000008

// This bit is set if the frame is an <mo> frame with the movablelimits
// attribute set to true XXX since it is <mo> specific, use NS_MATHML_EMBELLISH_MO_MOVABLELIMITS instead?
#define NS_MATHML_EMBELLISH_MOVABLELIMITS           0x00000010

// a bit used for debug
#define NS_MATHML_STRETCH_DONE                      0x80000000


// Macros that retrieve those bits

#define NS_MATHML_IS_EMBELLISH_OPERATOR(_flags) \
  (NS_MATHML_EMBELLISH_OPERATOR == ((_flags) & NS_MATHML_EMBELLISH_OPERATOR))

#define NS_MATHML_WILL_STRETCH_ALL_CHILDREN_VERTICALLY(_flags) \
  (NS_MATHML_STRETCH_ALL_CHILDREN_VERTICALLY == ((_flags) & NS_MATHML_STRETCH_ALL_CHILDREN_VERTICALLY))

#define NS_MATHML_WILL_STRETCH_ALL_CHILDREN_HORIZONTALLY(_flags) \
  (NS_MATHML_STRETCH_ALL_CHILDREN_HORIZONTALLY == ((_flags) & NS_MATHML_STRETCH_ALL_CHILDREN_HORIZONTALLY))

#define NS_MATHML_STRETCH_WAS_DONE(_flags) \
  (NS_MATHML_STRETCH_DONE == ((_flags) & NS_MATHML_STRETCH_DONE))

#define NS_MATHML_EMBELLISH_IS_ACCENT(_flags) \
  (NS_MATHML_EMBELLISH_ACCENT == ((_flags) & NS_MATHML_EMBELLISH_ACCENT))

#define NS_MATHML_EMBELLISH_IS_MOVABLELIMITS(_flags) \
  (NS_MATHML_EMBELLISH_MOVABLELIMITS == ((_flags) & NS_MATHML_EMBELLISH_MOVABLELIMITS))

#endif /* nsIMathMLFrame_h___ */
