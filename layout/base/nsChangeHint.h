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

enum nsChangeHint : uint32_t {
  nsChangeHint_Empty = 0,

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
  // without also setting nsChangeHint_ClearAncestorIntrinsics,
  // nsChangeHint_NeedDirtyReflow and nsChangeHint_NeedReflow.
  nsChangeHint_ClearDescendantIntrinsics = 1 << 3,

  // Force unconditional reflow of all descendants.  Must not be set without
  // setting nsChangeHint_NeedReflow, but can be set regardless of whether the
  // Clear*Intrinsics flags are set.
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
   * Reconstructs all frame descendants, including following placeholders
   * to out-of-flows.
   *
   * Note that this subsumes all the other change hints. (see
   * RestyleManager::ProcessRestyledFrames for details).
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
   *
   * Note that nsStyleContext::CalcStyleDifference adjusts results
   * returned by style struct CalcDifference methods to return this hint
   * only if there was a change to whether the element's overall style
   * indicates that it establishes a containing block.
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
   * behavior.  We need to return this, rather than nsChangeHint(0),
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
   * parent.  Must be not be set without also setting nsChangeHint_NeedReflow
   * and nsChangeHint_ClearAncestorIntrinsics.
   */
  nsChangeHint_ReflowChangesSizeOrPosition = 1 << 23,

  /**
   * Indicates that the style changes the computed BSize --- e.g. 'height'.
   * Must not be set without also setting nsChangeHint_NeedReflow.
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

  /**
   * Indicates that the 'background-position' property changed.
   * Regular frames can invalidate these changes using DLBI, but
   * for some frame types we need to repaint the whole frame because
   * the frame does not build individual background image display items
   * for each background layer.
   */
  nsChangeHint_UpdateBackgroundPosition = 1 << 26,

  /**
   * Indicates that a frame has changed to or from having the CSS
   * transform property set.
   */
  nsChangeHint_AddOrRemoveTransform = 1 << 27,

  /**
   * Indicates that the overflow-x and/or overflow-y property changed.
   *
   * In most cases, this is equivalent to nsChangeHint_ReconstructFrame. But
   * in some special cases where the change is really targeting the viewport's
   * scrollframe, this is instead equivalent to nsChangeHint_AllReflowHints
   * (because the viewport always has an associated scrollframe).
   */
  nsChangeHint_CSSOverflowChange = 1 << 28,

  /**
   * Indicates that nsIFrame::UpdateWidgetProperties needs to be called.
   * This is used for -moz-window-* properties.
   */
  nsChangeHint_UpdateWidgetProperties = 1 << 29,

  // IMPORTANT NOTE: When adding a new hint, you will need to add it to
  // one of:
  //
  //   * nsChangeHint_Hints_NeverHandledForDescendants
  //   * nsChangeHint_Hints_AlwaysHandledForDescendants
  //   * nsChangeHint_Hints_SometimesHandledForDescendants
  //
  // and you also may need to handle it in NS_HintsNotHandledForDescendantsIn.
  //
  // Please also add it to RestyleManager::ChangeHintToString and
  // modify nsChangeHint_AllHints below accordingly.

  /**
   * Dummy hint value for all hints. It exists for compile time check.
   */
  nsChangeHint_AllHints = (1 << 30) - 1,
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

// Returns true iff the second hint contains all the hints of the first hint
inline bool NS_IsHintSubset(nsChangeHint aSubset, nsChangeHint aSuperSet) {
  return (aSubset & aSuperSet) == aSubset;
}

// The functions below need an integral type to cast to to avoid
// infinite recursion.
typedef decltype(nsChangeHint(0) + nsChangeHint(0)) nsChangeHint_size_t;

inline nsChangeHint constexpr
operator|(nsChangeHint aLeft, nsChangeHint aRight)
{
  return nsChangeHint(nsChangeHint_size_t(aLeft) | nsChangeHint_size_t(aRight));
}

inline nsChangeHint constexpr
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

inline nsChangeHint constexpr
operator~(nsChangeHint aArg)
{
  return nsChangeHint(~nsChangeHint_size_t(aArg));
}

inline nsChangeHint constexpr
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

// The change hints that are always handled for descendants.
#define nsChangeHint_Hints_AlwaysHandledForDescendants (   \
  nsChangeHint_ClearDescendantIntrinsics |                 \
  nsChangeHint_NeedDirtyReflow |                           \
  nsChangeHint_NeutralChange |                             \
  nsChangeHint_ReconstructFrame |                          \
  nsChangeHint_RepaintFrame |                              \
  nsChangeHint_SchedulePaint |                             \
  nsChangeHint_SyncFrameView |                             \
  nsChangeHint_UpdateCursor |                              \
  nsChangeHint_UpdateSubtreeOverflow |                     \
  nsChangeHint_UpdateTextPath                              \
)

// The change hints that are never handled for descendants.
#define nsChangeHint_Hints_NeverHandledForDescendants (    \
  nsChangeHint_BorderStyleNoneChange |                     \
  nsChangeHint_ChildrenOnlyTransform |                     \
  nsChangeHint_CSSOverflowChange |                         \
  nsChangeHint_InvalidateRenderingObservers |              \
  nsChangeHint_RecomputePosition |                         \
  nsChangeHint_UpdateBackgroundPosition |                  \
  nsChangeHint_UpdateComputedBSize |                       \
  nsChangeHint_UpdateContainingBlock |                     \
  nsChangeHint_UpdateEffects |                             \
  nsChangeHint_UpdateOpacityLayer |                        \
  nsChangeHint_UpdateOverflow |                            \
  nsChangeHint_UpdateParentOverflow |                      \
  nsChangeHint_UpdatePostTransformOverflow |               \
  nsChangeHint_UpdateTransformLayer |                      \
  nsChangeHint_UpdateUsesOpacity |                         \
  nsChangeHint_AddOrRemoveTransform |                      \
  nsChangeHint_UpdateWidgetProperties                      \
)

// The change hints that are sometimes considered to be handled for descendants.
#define nsChangeHint_Hints_SometimesHandledForDescendants (\
  nsChangeHint_ClearAncestorIntrinsics |                   \
  nsChangeHint_NeedReflow |                                \
  nsChangeHint_ReflowChangesSizeOrPosition                 \
)

static_assert(!(nsChangeHint_Hints_AlwaysHandledForDescendants &
                nsChangeHint_Hints_NeverHandledForDescendants) &&
              !(nsChangeHint_Hints_AlwaysHandledForDescendants &
                nsChangeHint_Hints_SometimesHandledForDescendants) &&
              !(nsChangeHint_Hints_NeverHandledForDescendants &
                nsChangeHint_Hints_SometimesHandledForDescendants) &&
              !(nsChangeHint_AllHints ^
                nsChangeHint_Hints_AlwaysHandledForDescendants ^
                nsChangeHint_Hints_NeverHandledForDescendants ^
                nsChangeHint_Hints_SometimesHandledForDescendants),
              "change hints must be present in exactly one of "
              "nsChangeHint_Hints_{Always,Never,Sometimes}"
              "HandledForDescendants");

// The most hints that NS_HintsNotHandledForDescendantsIn could possibly return:
#define nsChangeHint_Hints_NotHandledForDescendants (      \
  nsChangeHint_Hints_NeverHandledForDescendants |          \
  nsChangeHint_Hints_SometimesHandledForDescendants        \
)

// Redefine the old NS_STYLE_HINT constants in terms of the new hint structure
#define NS_STYLE_HINT_VISUAL \
  nsChangeHint(nsChangeHint_RepaintFrame | nsChangeHint_SyncFrameView | \
               nsChangeHint_SchedulePaint)
#define nsChangeHint_AllReflowHints                     \
  nsChangeHint(nsChangeHint_NeedReflow |                \
               nsChangeHint_ReflowChangesSizeOrPosition|\
               nsChangeHint_ClearAncestorIntrinsics |   \
               nsChangeHint_ClearDescendantIntrinsics | \
               nsChangeHint_NeedDirtyReflow)

// Below are the change hints that we send for ISize & BSize changes.
// Each is similar to nsChangeHint_AllReflowHints with a few changes.

// * For an ISize change, we send nsChangeHint_AllReflowHints, with two bits
// excluded: nsChangeHint_ClearDescendantIntrinsics (because an ancestor's
// inline-size change can't affect descendant intrinsic sizes), and
// nsChangeHint_NeedDirtyReflow (because ISize changes don't need to *force*
// all descendants to reflow).
#define nsChangeHint_ReflowHintsForISizeChange            \
  nsChangeHint(nsChangeHint_AllReflowHints &              \
               ~(nsChangeHint_ClearDescendantIntrinsics | \
                 nsChangeHint_NeedDirtyReflow))

// * For a BSize change, we send almost the same hints as for ISize changes,
// with one extra: nsChangeHint_UpdateComputedBSize.  We need this hint because
// BSize changes CAN affect descendant intrinsic sizes, due to replaced
// elements with percentage BSizes in descendants which also have percentage
// BSizes. nsChangeHint_UpdateComputedBSize clears intrinsic sizes for frames
// that have such replaced elements. (We could instead send
// nsChangeHint_ClearDescendantIntrinsics, but that's broader than we need.)
//
// NOTE: You might think that BSize changes could exclude
// nsChangeHint_ClearAncestorIntrinsics (which is inline-axis specific), but we
// do need to send it, to clear cached results from CSS Flex measuring reflows.
#define nsChangeHint_ReflowHintsForBSizeChange            \
  nsChangeHint((nsChangeHint_AllReflowHints |             \
                nsChangeHint_UpdateComputedBSize) &       \
               ~(nsChangeHint_ClearDescendantIntrinsics | \
                 nsChangeHint_NeedDirtyReflow))

#define NS_STYLE_HINT_REFLOW \
  nsChangeHint(NS_STYLE_HINT_VISUAL | nsChangeHint_AllReflowHints)

#define nsChangeHint_Hints_CanIgnoreIfNotVisible   \
  nsChangeHint(NS_STYLE_HINT_VISUAL |              \
               nsChangeHint_NeutralChange |        \
               nsChangeHint_UpdateOpacityLayer |   \
               nsChangeHint_UpdateTransformLayer | \
               nsChangeHint_UpdateUsesOpacity)

// NB: Once we drop support for the old style system, this logic should be
// inlined in the Servo style system to eliminate the FFI call.
inline nsChangeHint NS_HintsNotHandledForDescendantsIn(nsChangeHint aChangeHint) {
  nsChangeHint result =
    aChangeHint & nsChangeHint_Hints_NeverHandledForDescendants;

  if (!NS_IsHintSubset(nsChangeHint_NeedDirtyReflow, aChangeHint)) {
    if (NS_IsHintSubset(nsChangeHint_NeedReflow, aChangeHint)) {
      // If NeedDirtyReflow is *not* set, then NeedReflow is a
      // non-inherited hint.
      result |= nsChangeHint_NeedReflow;
    }

    if (NS_IsHintSubset(nsChangeHint_ReflowChangesSizeOrPosition,
                        aChangeHint)) {
      // If NeedDirtyReflow is *not* set, then ReflowChangesSizeOrPosition is a
      // non-inherited hint.
      result |= nsChangeHint_ReflowChangesSizeOrPosition;
    }
  }

  if (!NS_IsHintSubset(nsChangeHint_ClearDescendantIntrinsics, aChangeHint) &&
      NS_IsHintSubset(nsChangeHint_ClearAncestorIntrinsics, aChangeHint)) {
    // If ClearDescendantIntrinsics is *not* set, then
    // ClearAncestorIntrinsics is a non-inherited hint.
    result |= nsChangeHint_ClearAncestorIntrinsics;
  }

  MOZ_ASSERT(NS_IsHintSubset(result,
                             nsChangeHint_Hints_NotHandledForDescendants),
             "something is inconsistent");

  return result;
}

inline nsChangeHint
NS_HintsHandledForDescendantsIn(nsChangeHint aChangeHint)
{
  return aChangeHint & ~NS_HintsNotHandledForDescendantsIn(aChangeHint);
}

// Returns the change hints in aOurChange that are not subsumed by those
// in aHintsHandled (which are hints that have been handled by an ancestor).
inline nsChangeHint
NS_RemoveSubsumedHints(nsChangeHint aOurChange, nsChangeHint aHintsHandled)
{
  nsChangeHint result =
    aOurChange & ~NS_HintsHandledForDescendantsIn(aHintsHandled);

  if (result & (nsChangeHint_ClearAncestorIntrinsics |
                nsChangeHint_ClearDescendantIntrinsics |
                nsChangeHint_NeedDirtyReflow |
                nsChangeHint_ReflowChangesSizeOrPosition |
                nsChangeHint_UpdateComputedBSize)) {
    result |= nsChangeHint_NeedReflow;
  }

  if (result & (nsChangeHint_ClearDescendantIntrinsics)) {
    MOZ_ASSERT(result & nsChangeHint_ClearAncestorIntrinsics);
    result |= // nsChangeHint_ClearAncestorIntrinsics |
              nsChangeHint_NeedDirtyReflow;
  }

  return result;
}

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
enum nsRestyleHint : uint32_t {
  // Rerun selector matching on the element.  If a new style context
  // results, update the style contexts of descendants.  (Irrelevant if
  // eRestyle_Subtree is also set, since that implies a superset of the
  // work.)
  eRestyle_Self = 1 << 0,

  // Rerun selector matching on descendants of the element that match
  // a given selector.
  eRestyle_SomeDescendants = 1 << 1,

  // Rerun selector matching on the element and all of its descendants.
  // (Implies eRestyle_ForceDescendants, which ensures that we continue
  // the restyling process for all descendants, but doesn't cause
  // selector matching.)
  eRestyle_Subtree = 1 << 2,

  // Rerun selector matching on all later siblings of the element and
  // all of their descendants.
  eRestyle_LaterSiblings = 1 << 3,

  // Replace the style data coming from CSS transitions without updating
  // any other style data.  If a new style context results, update style
  // contexts on the descendants.  (Irrelevant if eRestyle_Self or
  // eRestyle_Subtree is also set, since those imply a superset of the
  // work.)
  eRestyle_CSSTransitions = 1 << 4,

  // Replace the style data coming from CSS animations without updating
  // any other style data.  If a new style context results, update style
  // contexts on the descendants.  (Irrelevant if eRestyle_Self or
  // eRestyle_Subtree is also set, since those imply a superset of the
  // work.)
  eRestyle_CSSAnimations = 1 << 5,

  // Replace the style data coming from inline style without updating
  // any other style data.  If a new style context results, update style
  // contexts on the descendants.  (Irrelevant if eRestyle_Self or
  // eRestyle_Subtree is also set, since those imply a superset of the
  // work.)  Supported only for element style contexts and not for
  // pseudo-elements or anonymous boxes, on which it converts to
  // eRestyle_Self.
  // If the change is for the advance of a declarative animation, use
  // the value below instead.
  eRestyle_StyleAttribute = 1 << 6,

  // Same as eRestyle_StyleAttribute, but for when the change results
  // from the advance of a declarative animation.
  eRestyle_StyleAttribute_Animations = 1 << 7,

  // Continue the restyling process to the current frame's children even
  // if this frame's restyling resulted in no style changes.
  eRestyle_Force = 1 << 8,

  // Continue the restyling process to all of the current frame's
  // descendants, even if any frame's restyling resulted in no style
  // changes.  (Implies eRestyle_Force.)  Note that this is weaker than
  // eRestyle_Subtree, which makes us rerun selector matching on all
  // descendants rather than just continuing the restyling process.
  eRestyle_ForceDescendants = 1 << 9,

  // Useful unions:
  eRestyle_AllHintsWithAnimations = eRestyle_CSSTransitions |
                                    eRestyle_CSSAnimations |
                                    eRestyle_StyleAttribute_Animations,
};

// The functions below need an integral type to cast to to avoid
// infinite recursion.
typedef decltype(nsRestyleHint(0) + nsRestyleHint(0)) nsRestyleHint_size_t;

inline constexpr nsRestyleHint operator|(nsRestyleHint aLeft,
                                             nsRestyleHint aRight)
{
  return nsRestyleHint(nsRestyleHint_size_t(aLeft) |
                       nsRestyleHint_size_t(aRight));
}

inline constexpr nsRestyleHint operator&(nsRestyleHint aLeft,
                                             nsRestyleHint aRight)
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

inline constexpr nsRestyleHint operator~(nsRestyleHint aArg)
{
  return nsRestyleHint(~nsRestyleHint_size_t(aArg));
}

inline constexpr nsRestyleHint operator^(nsRestyleHint aLeft,
                                             nsRestyleHint aRight)
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
