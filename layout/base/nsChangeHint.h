/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* constants for what needs to be recomputed in response to style changes */

#ifndef nsChangeHint_h___
#define nsChangeHint_h___

#include "mozilla/Types.h"
#include "nsDebug.h"
#include "nsTArray.h"

struct nsCSSSelector;

// Defines for various style related constants

enum nsChangeHint {
  // change was visual only (e.g., COLOR=)
  // Invalidates all descendant frames (including following
  // placeholders to out-of-flow frames).
  nsChangeHint_RepaintFrame = 1 << 0,

  // For reflow, we want flags to give us arbitrary FrameNeedsReflow behavior.
  // just do a FrameNeedsReflow.
  nsChangeHint_NeedReflow = 1 << 1,

  // Invalidate intrinsic widths on the frame's ancestors.  Must not be set
  // without setting nsChangeHint_NeedReflow.
  nsChangeHint_ClearAncestorIntrinsics = 1 << 2,

  // Invalidate intrinsic widths on the frame's descendants.  Must not be set
  // without also setting nsChangeHint_ClearAncestorIntrinsics.
  nsChangeHint_ClearDescendantIntrinsics = 1 << 3,

  // Force unconditional reflow of all descendants.  Must not be set without
  // setting nsChangeHint_NeedReflow, but is independent of both the
  // Clear*Intrinsics flags.
  nsChangeHint_NeedDirtyReflow = 1 << 4,

  // change requires view to be updated, if there is one (e.g., clip:).
  // Updates all descendants (including following placeholders to out-of-flows).
  nsChangeHint_SyncFrameView = 1 << 5,

  // The currently shown mouse cursor needs to be updated
  nsChangeHint_UpdateCursor = 1 << 6,

  /**
   * Used when the computed value (a URI) of one or more of an element's
   * filter/mask/clip/etc CSS properties changes, causing the element's frame
   * to start/stop referencing (or reference different) SVG resource elements.
   * (_Not_ used to handle changes to referenced resource elements.) Using this
   * hint results in nsSVGEffects::UpdateEffects being called on the element's
   * frame.
   */
  nsChangeHint_UpdateEffects = 1 << 7,

  /**
   * Visual change only, but the change can be handled entirely by
   * updating the layer(s) for the frame.
   * Updates all descendants (including following placeholders to out-of-flows).
   */
  nsChangeHint_UpdateOpacityLayer = 1 << 8,
  /**
   * Updates all descendants. Any placeholder descendants' out-of-flows
   * are also descendants of the transformed frame, so they're updated.
   */
  nsChangeHint_UpdateTransformLayer = 1 << 9,

  /**
   * Change requires frame change (e.g., display:).
   * This subsumes all the above. Reconstructs all frame descendants,
   * including following placeholders to out-of-flows.
   */
  nsChangeHint_ReconstructFrame = 1 << 10,

  /**
   * The frame's overflow area has changed. Does not update any descendant
   * frames.
   */
  nsChangeHint_UpdateOverflow = 1 << 11,

  /**
   * The overflow area of the frame and all of its descendants has changed. This
   * can happen through a text-decoration change.   
   */
  nsChangeHint_UpdateSubtreeOverflow = 1 << 12,

  /**
   * The frame's overflow area has changed, through a change in its transform.
   * In other words, the frame's pre-transform overflow is unchanged, but
   * its post-transform overflow has changed, and thus its effect on its
   * parent's overflow has changed.  If the pre-transform overflow has
   * changed, see nsChangeHint_UpdateOverflow.
   * Does not update any descendant frames.
   */
  nsChangeHint_UpdatePostTransformOverflow = 1 << 13,

  /**
   * This frame's effect on its parent's overflow area has changed.
   * (But neither its pre-transform nor post-transform overflow have
   * changed; if those are the case, see
   * nsChangeHint_UpdatePostTransformOverflow.)
   */
  nsChangeHint_UpdateParentOverflow = 1 << 14,

  /**
   * The children-only transform of an SVG frame changed, requiring the
   * overflow rects of the frame's immediate children to be updated.
   */
  nsChangeHint_ChildrenOnlyTransform = 1 << 15,

  /**
   * The frame's offsets have changed, while its dimensions might have
   * changed as well.  This hint is used for positioned frames if their
   * offset changes.  If we decide that the dimensions are likely to
   * change, this will trigger a reflow.
   *
   * Note that this should probably be used in combination with
   * nsChangeHint_UpdateOverflow in order to get the overflow areas of
   * the ancestors updated as well.
   */
  nsChangeHint_RecomputePosition = 1 << 16,

  /**
   * Behaves like ReconstructFrame, but only if the frame has descendants
   * that are absolutely or fixed position. Use this hint when a style change
   * has changed whether the frame is a container for fixed-pos or abs-pos
   * elements, but reframing is otherwise not needed.
   */
  nsChangeHint_UpdateContainingBlock = 1 << 17,

  /**
   * This change hint has *no* change handling behavior.  However, it
   * exists to be a non-inherited hint, because when the border-style
   * changes, and it's inherited by a child, that might require a reflow
   * due to the border-width change on the child.
   */
  nsChangeHint_BorderStyleNoneChange = 1 << 18,

  /**
   * SVG textPath needs to be recomputed because the path has changed.
   * This means that the glyph positions of the text need to be recomputed.
   */
  nsChangeHint_UpdateTextPath = 1 << 19,

  /**
   * This will schedule an invalidating paint. This is useful if something
   * has changed which will be invalidated by DLBI.
   */
  nsChangeHint_SchedulePaint = 1 << 20,

  /**
   * A hint reflecting that style data changed with no change handling
   * behavior.  We need to return this, rather than NS_STYLE_HINT_NONE,
   * so that certain optimizations that manipulate the style context tree are
   * correct.
   *
   * nsChangeHint_NeutralChange must be returned by CalcDifference on a given
   * style struct if the data in the style structs are meaningfully different
   * and if no other change hints are returned.  If any other change hints are
   * set, then nsChangeHint_NeutralChange need not also be included, but it is
   * safe to do so.  (An example of style structs having non-meaningfully
   * different data would be cached information that would be re-calculated
   * to the same values, such as nsStyleBorder::mSubImages.)
   */
  nsChangeHint_NeutralChange = 1 << 21,

  /**
   * This will cause rendering observers to be invalidated.
   */
  nsChangeHint_InvalidateRenderingObservers = 1 << 22,

  /**
   * Indicates that the reflow changes the size or position of the
   * element, and thus the reflow must start from at least the frame's
   * parent.
   */
  nsChangeHint_ReflowChangesSizeOrPosition = 1 << 23,

  /**
   * Indicates that the style changes the computed BSize --- e.g. 'height'.
   */
  nsChangeHint_UpdateComputedBSize = 1 << 24,

  /**
   * Indicates that the 'opacity' property changed between 1 and non-1.
   *
   * Used as extra data for handling UpdateOpacityLayer hints.
   *
   * Note that we do not send this hint if the non-1 value was 0.99 or
   * greater, since in that case we send a RepaintFrame hint instead.
   */
  nsChangeHint_UpdateUsesOpacity = 1 << 25,

  // IMPORTANT NOTE: When adding new hints, consider whether you need to
  // add them to NS_HintsNotHandledForDescendantsIn() below.  Please also
  // add them to RestyleManager::ChangeHintToString.
};

// Redefine these operators to return nothing. This will catch any use
// of these operators on hints. We should not be using these operators
// on nsChangeHints
inline void operator<(nsChangeHint s1, nsChangeHint s2) {}
inline void operator>(nsChangeHint s1, nsChangeHint s2) {}
inline void operator!=(nsChangeHint s1, nsChangeHint s2) {}
inline void operator==(nsChangeHint s1, nsChangeHint s2) {}
inline void operator<=(nsChangeHint s1, nsChangeHint s2) {}
inline void operator>=(nsChangeHint s1, nsChangeHint s2) {}

// Operators on nsChangeHints

// Merge two hints, taking the union
inline nsChangeHint NS_CombineHint(nsChangeHint aH1, nsChangeHint aH2) {
  return (nsChangeHint)(aH1 | aH2);
}

// Merge two hints, taking the union
inline nsChangeHint NS_SubtractHint(nsChangeHint aH1, nsChangeHint aH2) {
  return (nsChangeHint)(aH1 & ~aH2);
}

// Merge the "src" hint into the "dst" hint
// Returns true iff the destination changed
inline bool NS_UpdateHint(nsChangeHint& aDest, nsChangeHint aSrc) {
  nsChangeHint r = (nsChangeHint)(aDest | aSrc);
  bool changed = (int)r != (int)aDest;
  aDest = r;
  return changed;
}

// Returns true iff the second hint contains all the hints of the first hint
inline bool NS_IsHintSubset(nsChangeHint aSubset, nsChangeHint aSuperSet) {
  return (aSubset & aSuperSet) == aSubset;
}

// The functions below need an integral type to cast to to avoid
// infinite recursion.
typedef decltype(nsChangeHint(0) + nsChangeHint(0)) nsChangeHint_size_t;

inline nsChangeHint MOZ_CONSTEXPR
operator|(nsChangeHint aLeft, nsChangeHint aRight)
{
  return nsChangeHint(nsChangeHint_size_t(aLeft) | nsChangeHint_size_t(aRight));
}

inline nsChangeHint MOZ_CONSTEXPR
operator&(nsChangeHint aLeft, nsChangeHint aRight)
{
  return nsChangeHint(nsChangeHint_size_t(aLeft) & nsChangeHint_size_t(aRight));
}

inline nsChangeHint& operator|=(nsChangeHint& aLeft, nsChangeHint aRight)
{
  return aLeft = aLeft | aRight;
}

inline nsChangeHint& operator&=(nsChangeHint& aLeft, nsChangeHint aRight)
{
  return aLeft = aLeft & aRight;
}

inline nsChangeHint MOZ_CONSTEXPR
operator~(nsChangeHint aArg)
{
  return nsChangeHint(~nsChangeHint_size_t(aArg));
}

inline nsChangeHint MOZ_CONSTEXPR
operator^(nsChangeHint aLeft, nsChangeHint aRight)
{
  return nsChangeHint(nsChangeHint_size_t(aLeft) ^ nsChangeHint_size_t(aRight));
}

inline nsChangeHint operator^=(nsChangeHint& aLeft, nsChangeHint aRight)
{
  return aLeft = aLeft ^ aRight;
}

/**
 * We have an optimization when processing change hints which prevents
 * us from visiting the descendants of a node when a hint on that node
 * is being processed.  This optimization does not apply in some of the
 * cases where applying a hint to an element does not necessarily result
 * in the same hint being handled on the descendants.
 */

// The most hints that NS_HintsNotHandledForDescendantsIn could possibly return:
#define nsChangeHint_Hints_NotHandledForDescendants nsChangeHint( \
          nsChangeHint_UpdateTransformLayer | \
          nsChangeHint_UpdateEffects | \
          nsChangeHint_InvalidateRenderingObservers | \
          nsChangeHint_UpdateOpacityLayer | \
          nsChangeHint_UpdateOverflow | \
          nsChangeHint_UpdatePostTransformOverflow | \
          nsChangeHint_UpdateParentOverflow | \
          nsChangeHint_ChildrenOnlyTransform | \
          nsChangeHint_RecomputePosition | \
          nsChangeHint_UpdateContainingBlock | \
          nsChangeHint_BorderStyleNoneChange | \
          nsChangeHint_NeedReflow | \
          nsChangeHint_ReflowChangesSizeOrPosition | \
          nsChangeHint_ClearAncestorIntrinsics | \
          nsChangeHint_UpdateComputedBSize | \
          nsChangeHint_UpdateUsesOpacity)

inline nsChangeHint NS_HintsNotHandledForDescendantsIn(nsChangeHint aChangeHint) {
  nsChangeHint result = nsChangeHint(aChangeHint & (
    nsChangeHint_UpdateTransformLayer |
    nsChangeHint_UpdateEffects |
    nsChangeHint_InvalidateRenderingObservers |
    nsChangeHint_UpdateOpacityLayer |
    nsChangeHint_UpdateOverflow |
    nsChangeHint_UpdatePostTransformOverflow |
    nsChangeHint_UpdateParentOverflow |
    nsChangeHint_ChildrenOnlyTransform |
    nsChangeHint_RecomputePosition |
    nsChangeHint_UpdateContainingBlock |
    nsChangeHint_BorderStyleNoneChange |
    nsChangeHint_UpdateComputedBSize |
    nsChangeHint_UpdateUsesOpacity));

  if (!NS_IsHintSubset(nsChangeHint_NeedDirtyReflow, aChangeHint)) {
    if (NS_IsHintSubset(nsChangeHint_NeedReflow, aChangeHint)) {
      // If NeedDirtyReflow is *not* set, then NeedReflow is a
      // non-inherited hint.
      NS_UpdateHint(result, nsChangeHint_NeedReflow);
    }

    if (NS_IsHintSubset(nsChangeHint_ReflowChangesSizeOrPosition,
                        aChangeHint)) {
      // If NeedDirtyReflow is *not* set, then ReflowChangesSizeOrPosition is a
      // non-inherited hint.
      NS_UpdateHint(result, nsChangeHint_ReflowChangesSizeOrPosition);
    }
  }

  if (!NS_IsHintSubset(nsChangeHint_ClearDescendantIntrinsics, aChangeHint) &&
      NS_IsHintSubset(nsChangeHint_ClearAncestorIntrinsics, aChangeHint)) {
    // If ClearDescendantIntrinsics is *not* set, then
    // ClearAncestorIntrinsics is a non-inherited hint.
    NS_UpdateHint(result, nsChangeHint_ClearAncestorIntrinsics);
  }

  MOZ_ASSERT(NS_IsHintSubset(result,
                             nsChangeHint_Hints_NotHandledForDescendants),
             "something is inconsistent");

  return result;
}

// Redefine the old NS_STYLE_HINT constants in terms of the new hint structure
#define NS_STYLE_HINT_NONE \
  nsChangeHint(0)
#define NS_STYLE_HINT_VISUAL \
  nsChangeHint(nsChangeHint_RepaintFrame | nsChangeHint_SyncFrameView | \
               nsChangeHint_SchedulePaint)
#define nsChangeHint_AllReflowHints                     \
  nsChangeHint(nsChangeHint_NeedReflow |                \
               nsChangeHint_ReflowChangesSizeOrPosition|\
               nsChangeHint_ClearAncestorIntrinsics |   \
               nsChangeHint_ClearDescendantIntrinsics | \
               nsChangeHint_NeedDirtyReflow)
#define NS_STYLE_HINT_REFLOW \
  nsChangeHint(NS_STYLE_HINT_VISUAL | nsChangeHint_AllReflowHints)
#define NS_STYLE_HINT_FRAMECHANGE \
  nsChangeHint(NS_STYLE_HINT_REFLOW | nsChangeHint_ReconstructFrame)

/**
 * |nsRestyleHint| is a bitfield for the result of
 * |HasStateDependentStyle| and |HasAttributeDependentStyle|.  When no
 * restyling is necessary, use |nsRestyleHint(0)|.
 *
 * Without eRestyle_Force or eRestyle_ForceDescendants, the restyling process
 * can stop processing at a frame when it detects no style changes and it is
 * known that the styles of the subtree beneath it will not change, leaving
 * the old style context on the frame.  eRestyle_Force can be used to skip this
 * optimization on a frame, and to force its new style context to be used.
 *
 * Similarly, eRestyle_ForceDescendants will cause the frame and all of its
 * descendants to be traversed and for the new style contexts that are created
 * to be set on the frames.
 *
 * NOTE: When adding new restyle hints, please also add them to
 * RestyleManager::RestyleHintToString.
 */
enum nsRestyleHint {
  // Rerun selector matching on the element.  If a new style context
  // results, update the style contexts of descendants.  (Irrelevant if
  // eRestyle_Subtree is also set, since that implies a superset of the
  // work.)
  eRestyle_Self = (1<<0),

  // Rerun selector matching on descendants of the element that match
  // a given selector.
  eRestyle_SomeDescendants = (1<<1),

  // Rerun selector matching on the element and all of its descendants.
  // (Implies eRestyle_ForceDescendants, which ensures that we continue
  // the restyling process for all descendants, but doesn't cause
  // selector matching.)
  eRestyle_Subtree = (1<<2),

  // Rerun selector matching on all later siblings of the element and
  // all of their descendants.
  eRestyle_LaterSiblings = (1<<3),

  // Replace the style data coming from CSS transitions without updating
  // any other style data.  If a new style context results, update style
  // contexts on the descendants.  (Irrelevant if eRestyle_Self or
  // eRestyle_Subtree is also set, since those imply a superset of the
  // work.)
  eRestyle_CSSTransitions = (1<<4),

  // Replace the style data coming from CSS animations without updating
  // any other style data.  If a new style context results, update style
  // contexts on the descendants.  (Irrelevant if eRestyle_Self or
  // eRestyle_Subtree is also set, since those imply a superset of the
  // work.)
  eRestyle_CSSAnimations = (1<<5),

  // Replace the style data coming from SVG animations (SMIL Animations)
  // without updating any other style data.  If a new style context
  // results, update style contexts on the descendants.  (Irrelevant if
  // eRestyle_Self or eRestyle_Subtree is also set, since those imply a
  // superset of the work.)
  eRestyle_SVGAttrAnimations = (1<<6),

  // Replace the style data coming from inline style without updating
  // any other style data.  If a new style context results, update style
  // contexts on the descendants.  (Irrelevant if eRestyle_Self or
  // eRestyle_Subtree is also set, since those imply a superset of the
  // work.)  Supported only for element style contexts and not for
  // pseudo-elements or anonymous boxes, on which it converts to
  // eRestyle_Self.
  // If the change is for the advance of a declarative animation, use
  // the value below instead.
  eRestyle_StyleAttribute = (1<<7),

  // Same as eRestyle_StyleAttribute, but for when the change results
  // from the advance of a declarative animation.
  eRestyle_StyleAttribute_Animations = (1<<8),

  // Continue the restyling process to the current frame's children even
  // if this frame's restyling resulted in no style changes.
  eRestyle_Force = (1<<9),

  // Continue the restyling process to all of the current frame's
  // descendants, even if any frame's restyling resulted in no style
  // changes.  (Implies eRestyle_Force.)  Note that this is weaker than
  // eRestyle_Subtree, which makes us rerun selector matching on all
  // descendants rather than just continuing the restyling process.
  eRestyle_ForceDescendants = (1<<10),

  // Useful unions:
  eRestyle_AllHintsWithAnimations = eRestyle_CSSTransitions |
                                    eRestyle_CSSAnimations |
                                    eRestyle_SVGAttrAnimations |
                                    eRestyle_StyleAttribute_Animations,
};

// The functions below need an integral type to cast to to avoid
// infinite recursion.
typedef decltype(nsRestyleHint(0) + nsRestyleHint(0)) nsRestyleHint_size_t;

inline nsRestyleHint operator|(nsRestyleHint aLeft, nsRestyleHint aRight)
{
  return nsRestyleHint(nsRestyleHint_size_t(aLeft) |
                       nsRestyleHint_size_t(aRight));
}

inline nsRestyleHint operator&(nsRestyleHint aLeft, nsRestyleHint aRight)
{
  return nsRestyleHint(nsRestyleHint_size_t(aLeft) &
                       nsRestyleHint_size_t(aRight));
}

inline nsRestyleHint& operator|=(nsRestyleHint& aLeft, nsRestyleHint aRight)
{
  return aLeft = aLeft | aRight;
}

inline nsRestyleHint& operator&=(nsRestyleHint& aLeft, nsRestyleHint aRight)
{
  return aLeft = aLeft & aRight;
}

inline nsRestyleHint operator~(nsRestyleHint aArg)
{
  return nsRestyleHint(~nsRestyleHint_size_t(aArg));
}

inline nsRestyleHint operator^(nsRestyleHint aLeft, nsRestyleHint aRight)
{
  return nsRestyleHint(nsRestyleHint_size_t(aLeft) ^
                       nsRestyleHint_size_t(aRight));
}

inline nsRestyleHint operator^=(nsRestyleHint& aLeft, nsRestyleHint aRight)
{
  return aLeft = aLeft ^ aRight;
}

namespace mozilla {

/**
 * Additional data used in conjunction with an nsRestyleHint to control the
 * restyle process.
 */
struct RestyleHintData
{
  // When eRestyle_SomeDescendants is used, this array contains the selectors
  // that identify which descendants will be restyled.
  nsTArray<nsCSSSelector*> mSelectorsForDescendants;
};

} // namespace mozilla

#endif /* nsChangeHint_h___ */
