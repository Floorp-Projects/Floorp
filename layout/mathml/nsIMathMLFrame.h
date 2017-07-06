/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
//#define SHOW_BOUNDING_BOX 1
#ifndef nsIMathMLFrame_h___
#define nsIMathMLFrame_h___

#include "nsQueryFrame.h"
#include "nsMathMLOperators.h"

struct nsPresentationData;
struct nsEmbellishData;
class gfxContext;
class nsIFrame;
namespace mozilla {
class ReflowOutput;
} // namespace mozilla

// For MathML, this 'type' will be used to determine the spacing between frames
// Subclasses can return a 'type' that will give them a particular spacing
enum eMathMLFrameType {
  eMathMLFrameType_UNKNOWN = -1,
  eMathMLFrameType_Ordinary,
  eMathMLFrameType_OperatorOrdinary,
  eMathMLFrameType_OperatorInvisible,
  eMathMLFrameType_OperatorUserDefined,
  eMathMLFrameType_Inner,
  eMathMLFrameType_ItalicIdentifier,
  eMathMLFrameType_UprightIdentifier,
  eMathMLFrameType_COUNT
};

// Abstract base class that provides additional methods for MathML frames
class nsIMathMLFrame
{
public:
  NS_DECL_QUERYFRAME_TARGET(nsIMathMLFrame)

  // helper to check whether the frame is "space-like", as defined by the spec.
  virtual bool IsSpaceLike() = 0;

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
  SetReference(const nsPoint& aReference) = 0;

  virtual eMathMLFrameType GetMathMLFrameType() = 0;

 /* SUPPORT FOR STRETCHY ELEMENTS */
 /*====================================================================*/

 /* Stretch :
  * Called to ask a stretchy MathML frame to stretch itself depending
  * on its context.
  *
  * An embellished frame is treated in a special way. When it receives a
  * Stretch() command, it passes the command to its embellished child and
  * the stretched size is bubbled up from the inner-most <mo> frame. In other
  * words, the stretch command descend through the embellished hierarchy.
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
  Stretch(mozilla::gfx::DrawTarget* aDrawTarget,
          nsStretchDirection   aStretchDirection,
          nsBoundingMetrics&   aContainerSize,
          mozilla::ReflowOutput& aDesiredStretchSize) = 0;

 /* Get the mEmbellishData member variable. */

  NS_IMETHOD
  GetEmbellishData(nsEmbellishData& aEmbellishData) = 0;


 /* SUPPORT FOR SCRIPTING ELEMENTS */
 /*====================================================================*/

 /* Get the mPresentationData member variable. */

  NS_IMETHOD
  GetPresentationData(nsPresentationData& aPresentationData) = 0;

  /* InheritAutomaticData() / TransmitAutomaticData() :
   * There are precise rules governing each MathML frame and its children.
   * Properties such as the scriptlevel or the embellished nature of a frame
   * depend on those rules. Also, certain properties that we use to emulate
   * TeX rendering rules are frame-dependent too. These two methods are meant
   * to be implemented by frame classes that need to assert specific properties
   * within their subtrees.
   *
   * InheritAutomaticData() is called in a top-down manner [like nsIFrame::Init],
   * as we descend the frame tree, whereas TransmitAutomaticData() is called in a
   * bottom-up manner, as we ascend the tree [like nsIFrame::SetInitialChildList].
   * However, unlike Init() and SetInitialChildList() which are called only once
   * during the life-time of a frame (when initially constructing the frame tree),
   * these two methods are called to build automatic data after the <math>...</math>
   * subtree has been constructed fully, and are called again as we walk a child's
   * subtree to handle dynamic changes that happen in the content model.
   *
   * As a rule of thumb:
   *
   * 1. Use InheritAutomaticData() to set properties related to your ancestors:
   *    - set properties that are intrinsic to yourself
   *    - set properties that depend on the state that you expect your ancestors
   *      to have already reached in their own InheritAutomaticData().
   *    - set properties that your descendants assume that you would have set in
   *      your InheritAutomaticData() -- this way, they can safely query them and
   *      the process will feed upon itself.
   *
   * 2. Use TransmitAutomaticData() to set properties related to your descendants:
   *    - set properties that depend on the state that you expect your descendants
   *      to have reached upon processing their own TransmitAutomaticData().
   *    - transmit properties that your descendants expect that you will transmit to
   *      them in your TransmitAutomaticData() -- this way, they remain up-to-date.
   *    - set properties that your ancestors expect that you would set in your
   *      TransmitAutomaticData() -- this way, they can safely query them and the
   *      process will feed upon itself.
   */

  NS_IMETHOD
  InheritAutomaticData(nsIFrame* aParent) = 0;

  NS_IMETHOD
  TransmitAutomaticData() = 0;

 /* UpdatePresentationData:
  * Updates the frame's compression flag.
  * A frame becomes "compressed" (or "cramped") according to TeX rendering
  * rules (TeXBook, Ch.17, p.140-141).
  *
  * @param aFlagsValues [in]
  *        The new values (e.g., compress) that are going to be
  *        updated.
  *
  * @param aWhichFlags [in]
  *        The flags that are relevant to this call. Since not all calls
  *        are meant to update all flags at once, aWhichFlags is used
  *        to distinguish flags that need to retain their existing values
  *        from flags that need to be turned on (or turned off). If a bit
  *        is set in aWhichFlags, then the corresponding value (which
  *        can be 0 or 1) is taken from aFlagsValues and applied to the
  *        frame. Therefore, by setting their bits in aWhichFlags, and
  *        setting their desired values in aFlagsValues, it is possible to
  *        update some flags in the frame, leaving the other flags unchanged.
  */
  NS_IMETHOD
  UpdatePresentationData(uint32_t        aFlagsValues,
                         uint32_t        aWhichFlags) = 0;

 /* UpdatePresentationDataFromChildAt :
  * Sets compression flag on the whole tree. For child frames
  * at aFirstIndex up to aLastIndex, this method sets their
  * compression flags. The update is propagated down the subtrees of each of
  * these child frames.
  *
  * @param aFirstIndex [in]
  *        Index of the first child from where the update is propagated.
  *
  * @param aLastIndex [in]
  *        Index of the last child where to stop the update.
  *        A value of -1 means up to last existing child.
  *
  * @param aFlagsValues [in]
  *        The new values (e.g., compress) that are going to be
  *        assigned in the whole sub-trees.
  *
  * @param aWhichFlags [in]
  *        The flags that are relevant to this call. See UpdatePresentationData()
  *        for more details about this parameter.
  */
  NS_IMETHOD
  UpdatePresentationDataFromChildAt(int32_t         aFirstIndex,
                                    int32_t         aLastIndex,
                                    uint32_t        aFlagsValues,
                                    uint32_t        aWhichFlags) = 0;

  // If aFrame is a child frame, returns the script increment which this frame
  // imposes on the specified frame, ignoring any artificial adjustments to
  // scriptlevel.
  // Returns 0 if the specified frame isn't a child frame.
  virtual uint8_t
  ScriptIncrement(nsIFrame* aFrame) = 0;

  // Returns true if the frame is considered to be an mrow for layout purposes.
  // This includes inferred mrows, but excludes <mrow> elements with a single
  // child.  In the latter case, the child is to be treated as if it wasn't
  // within an mrow, so we pretend the mrow isn't mrow-like.
  virtual bool
  IsMrowLike() = 0;
};

// struct used by a container frame to keep track of its embellishments.
// By convention, the data that we keep here is bubbled from the embellished
// hierarchy, and it remains unchanged unless we have to recover from a change
// that occurs in the embellished hierarchy. The struct remains in its nil
// state in those frames that are not part of the embellished hierarchy.
struct nsEmbellishData {
  // bits used to mark certain properties of our embellishments
  uint32_t flags;

  // pointer on the <mo> frame at the core of the embellished hierarchy
  nsIFrame* coreFrame;

  // stretchy direction that the nsMathMLChar owned by the core <mo> supports
  nsStretchDirection direction;

  // spacing that may come from <mo> depending on its 'form'. Since
  // the 'form' may also depend on the position of the outermost
  // embellished ancestor, the set up of these values may require
  // looking up the position of our ancestors.
  nscoord leadingSpace;
  nscoord trailingSpace;

  nsEmbellishData() {
    flags = 0;
    coreFrame = nullptr;
    direction = NS_STRETCH_DIRECTION_UNSUPPORTED;
    leadingSpace = 0;
    trailingSpace = 0;
  }
};

// struct used by a container frame to modulate its presentation.
// By convention, the data that we keep in this struct can change depending
// on any of our ancestors and/or descendants. If a data can be resolved
// solely from the embellished hierarchy, and it remains immutable once
// resolved, we put it in |nsEmbellishData|. If it can be affected by other
// things, it comes here. This struct is updated as we receive information
// transmitted by our ancestors and is kept in sync with changes in our
// descendants that affects us.
struct nsPresentationData {
  // bits for: compressed, etc
  uint32_t flags;

  // handy pointer on our base child (the 'nucleus' in TeX), but it may be
  // null here (e.g., tags like <mrow>, <mfrac>, <mtable>, etc, won't
  // pick a particular child in their child list to be the base)
  nsIFrame* baseFrame;

  nsPresentationData() {
    flags = 0;
    baseFrame = nullptr;
  }
};

// ==========================================================================
// Bits used for the presentation flags -- these bits are set
// in their relevant situation as they become available

// This bit is used to emulate TeX rendering.
// Internal use only, cannot be set by the user with an attribute.
#define NS_MATHML_COMPRESSED                          0x00000002U

// This bit is set if the frame will fire a vertical stretch
// command on all its (non-empty) children.
// Tags like <mrow> (or an inferred mrow), mpadded, etc, will fire a
// vertical stretch command on all their non-empty children
#define NS_MATHML_STRETCH_ALL_CHILDREN_VERTICALLY     0x00000004U

// This bit is set if the frame will fire a horizontal stretch
// command on all its (non-empty) children.
// Tags like munder, mover, munderover, will fire a
// horizontal stretch command on all their non-empty children
#define NS_MATHML_STRETCH_ALL_CHILDREN_HORIZONTALLY   0x00000008U

// This bit is set if the frame is "space-like", as defined by the spec.
#define NS_MATHML_SPACE_LIKE                          0x00000040U

// This bit is set if a token frame should be rendered with the dtls font
// feature setting.
#define NS_MATHML_DTLS                                0x00000080U

// This bit is set when the frame cannot be formatted due to an
// error (e.g., invalid markup such as a <msup> without an overscript).
// When set, a visual feedback will be provided to the user.
#define NS_MATHML_ERROR                               0x80000000U

// a bit used for debug
#define NS_MATHML_STRETCH_DONE                        0x20000000U

// This bit is used for visual debug. When set, the bounding box
// of your frame is painted. This visual debug enable to ensure that
// you have properly filled your mReference and mBoundingMetrics in
// Place().
#define NS_MATHML_SHOW_BOUNDING_METRICS               0x10000000U

// Macros that retrieve those bits

#define NS_MATHML_IS_COMPRESSED(_flags) \
  (NS_MATHML_COMPRESSED == ((_flags) & NS_MATHML_COMPRESSED))

#define NS_MATHML_WILL_STRETCH_ALL_CHILDREN_VERTICALLY(_flags) \
  (NS_MATHML_STRETCH_ALL_CHILDREN_VERTICALLY == ((_flags) & NS_MATHML_STRETCH_ALL_CHILDREN_VERTICALLY))

#define NS_MATHML_WILL_STRETCH_ALL_CHILDREN_HORIZONTALLY(_flags) \
  (NS_MATHML_STRETCH_ALL_CHILDREN_HORIZONTALLY == ((_flags) & NS_MATHML_STRETCH_ALL_CHILDREN_HORIZONTALLY))

#define NS_MATHML_IS_SPACE_LIKE(_flags) \
  (NS_MATHML_SPACE_LIKE == ((_flags) & NS_MATHML_SPACE_LIKE))

#define NS_MATHML_IS_DTLS_SET(_flags) \
  (NS_MATHML_DTLS == ((_flags) & NS_MATHML_DTLS))

#define NS_MATHML_HAS_ERROR(_flags) \
  (NS_MATHML_ERROR == ((_flags) & NS_MATHML_ERROR))

#define NS_MATHML_STRETCH_WAS_DONE(_flags) \
  (NS_MATHML_STRETCH_DONE == ((_flags) & NS_MATHML_STRETCH_DONE))

#define NS_MATHML_PAINT_BOUNDING_METRICS(_flags) \
  (NS_MATHML_SHOW_BOUNDING_METRICS == ((_flags) & NS_MATHML_SHOW_BOUNDING_METRICS))

// ==========================================================================
// Bits used for the embellish flags -- these bits are set
// in their relevant situation as they become available

// This bit is set if the frame is an embellished operator.
#define NS_MATHML_EMBELLISH_OPERATOR                0x00000001

// This bit is set if the frame is an <mo> frame or an embellihsed
// operator for which the core <mo> has movablelimits="true"
#define NS_MATHML_EMBELLISH_MOVABLELIMITS           0x00000002

// This bit is set if the frame is an <mo> frame or an embellihsed
// operator for which the core <mo> has accent="true"
#define NS_MATHML_EMBELLISH_ACCENT                  0x00000004

// This bit is set if the frame is an <mover> or <munderover> with
// an accent frame
#define NS_MATHML_EMBELLISH_ACCENTOVER              0x00000008

// This bit is set if the frame is an <munder> or <munderover> with
// an accentunder frame
#define NS_MATHML_EMBELLISH_ACCENTUNDER             0x00000010

// This bit is set on the core if it is a fence operator.
#define NS_MATHML_EMBELLISH_FENCE                   0x00000020

// This bit is set on the core if it is a separator operator.
#define NS_MATHML_EMBELLISH_SEPARATOR               0x00000040

// Macros that retrieve those bits

#define NS_MATHML_IS_EMBELLISH_OPERATOR(_flags) \
  (NS_MATHML_EMBELLISH_OPERATOR == ((_flags) & NS_MATHML_EMBELLISH_OPERATOR))

#define NS_MATHML_EMBELLISH_IS_MOVABLELIMITS(_flags) \
  (NS_MATHML_EMBELLISH_MOVABLELIMITS == ((_flags) & NS_MATHML_EMBELLISH_MOVABLELIMITS))

#define NS_MATHML_EMBELLISH_IS_ACCENT(_flags) \
  (NS_MATHML_EMBELLISH_ACCENT == ((_flags) & NS_MATHML_EMBELLISH_ACCENT))

#define NS_MATHML_EMBELLISH_IS_ACCENTOVER(_flags) \
  (NS_MATHML_EMBELLISH_ACCENTOVER == ((_flags) & NS_MATHML_EMBELLISH_ACCENTOVER))

#define NS_MATHML_EMBELLISH_IS_ACCENTUNDER(_flags) \
  (NS_MATHML_EMBELLISH_ACCENTUNDER == ((_flags) & NS_MATHML_EMBELLISH_ACCENTUNDER))

#define NS_MATHML_EMBELLISH_IS_FENCE(_flags) \
  (NS_MATHML_EMBELLISH_FENCE == ((_flags) & NS_MATHML_EMBELLISH_FENCE))

#define NS_MATHML_EMBELLISH_IS_SEPARATOR(_flags) \
  (NS_MATHML_EMBELLISH_SEPARATOR == ((_flags) & NS_MATHML_EMBELLISH_SEPARATOR))

#endif /* nsIMathMLFrame_h___ */
