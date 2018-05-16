/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* the interface (to internal code) for retrieving computed style data */

#ifndef _ComputedStyle_h_
#define _ComputedStyle_h_

#include "nsIMemoryReporter.h"
#include <algorithm>
#include "mozilla/ArenaObjectID.h"
#include "mozilla/Assertions.h"
#include "mozilla/ServoComputedData.h"
#include "mozilla/ServoTypes.h"
#include "mozilla/ServoUtils.h"
#include "mozilla/StyleComplexColor.h"
#include "mozilla/CachedInheritingStyles.h"
#include "nsCSSAnonBoxes.h"
#include "nsCSSPseudoElements.h"

// Includes nsStyleStructID.
#include "nsStyleStructFwd.h"

// Bits for each struct.
// NS_STYLE_INHERIT_BIT defined in nsStyleStructFwd.h
#define NS_STYLE_INHERIT_MASK              0x0007fffff

// Bits for inherited structs.
#define NS_STYLE_INHERITED_STRUCT_MASK \
  ((nsStyleStructID_size_t(1) << nsStyleStructID_Inherited_Count) - 1)
// Bits for reset structs.
#define NS_STYLE_RESET_STRUCT_MASK \
  (((nsStyleStructID_size_t(1) << nsStyleStructID_Reset_Count) - 1) \
   << nsStyleStructID_Inherited_Count)

// Additional bits for ComputedStyle's mBits:
// (free bit)                              0x000800000
// See ComputedStyle::HasTextDecorationLines
#define NS_STYLE_HAS_TEXT_DECORATION_LINES 0x001000000
// See ComputedStyle::HasPseudoElementData.
#define NS_STYLE_HAS_PSEUDO_ELEMENT_DATA   0x002000000
// See ComputedStyle::RelevantLinkIsVisited
#define NS_STYLE_RELEVANT_LINK_VISITED     0x004000000
// See ComputedStyle::IsStyleIfVisited
#define NS_STYLE_IS_STYLE_IF_VISITED       0x008000000
// See ComputedStyle::HasChildThatUsesGrandancestorStyle
#define NS_STYLE_CHILD_USES_GRANDANCESTOR_STYLE 0x010000000
// See ComputedStyle::IsShared
#define NS_STYLE_IS_SHARED                 0x020000000
// See ComputedStyle::AssertStructsNotUsedElsewhere
// (This bit is currently only used in #ifdef DEBUG code.)
#define NS_STYLE_IS_GOING_AWAY             0x040000000
// See ComputedStyle::ShouldSuppressLineBreak
#define NS_STYLE_SUPPRESS_LINEBREAK        0x080000000
// See ComputedStyle::IsInDisplayNoneSubtree
#define NS_STYLE_IN_DISPLAY_NONE_SUBTREE   0x100000000
// See ComputedStyle::FindChildWithRules
#define NS_STYLE_INELIGIBLE_FOR_SHARING    0x200000000
// See ComputedStyle::HasChildThatUsesResetStyle
#define NS_STYLE_HAS_CHILD_THAT_USES_RESET_STYLE 0x400000000
// See ComputedStyle::IsTextCombined
#define NS_STYLE_IS_TEXT_COMBINED          0x800000000
// Whether a ComputedStyle is a Gecko or Servo context
#define NS_STYLE_CONTEXT_IS_GECKO          0x1000000000
// See ComputedStyle::GetPseudoEnum
#define NS_STYLE_CONTEXT_TYPE_SHIFT        37

class nsAtom;
enum nsChangeHint : uint32_t;
class nsIPresShell;
class nsPresContext;
class nsWindowSizes;

#define STYLE_STRUCT(name_) struct nsStyle##name_;
#include "nsStyleStructList.h"
#undef STYLE_STRUCT

extern "C" {
  void Servo_ComputedStyle_AddRef(const mozilla::ComputedStyle* aStyle);
  void Servo_ComputedStyle_Release(const mozilla::ComputedStyle* aStyle);
  void Gecko_ComputedStyle_Destroy(mozilla::ComputedStyle*);
}

namespace mozilla {

enum class CSSPseudoElementType : uint8_t;
class ComputedStyle;

/**
 * A ComputedStyle represents the computed style data for an element.  The
 * computed style data are stored in a set of structs (see nsStyleStruct.h) that
 * are cached either on the ComputedStyle or in the rule tree (see nsRuleNode.h
 * for a description of this caching and how the cached structs are shared).
 *
 * Since the data in |nsIStyleRule|s and |nsRuleNode|s are immutable (with a few
 * exceptions, like system color changes), the data in an ComputedStyle are also
 * immutable (with the additional exception of GetUniqueStyleData).  When style
 * data change, ElementRestyler::Restyle creates a new ComputedStyle.
 *
 * ComputedStyles are reference counted. References are generally held by:
 *  1. the |nsIFrame|s that are using the ComputedStyle and
 *  2. any *child* ComputedStyle (this might be the reverse of
 *     expectation, but it makes sense in this case)
 *
 * FIXME(emilio): This comment is somewhat outdated now.
 */

class ComputedStyle
{
public:
  ComputedStyle(nsPresContext* aPresContext,
                nsAtom* aPseudoTag,
                CSSPseudoElementType aPseudoType,
                ServoComputedDataForgotten aComputedValues);

  // FIXME(emilio, bug 548397): This will need to go away. Don't add new callers
  // of this methed.
  nsPresContext* PresContextForFrame() const { return mPresContext; }
  const ServoComputedData* ComputedData() const { return &mSource; }

  // These two methods are for use by ArenaRefPtr.
  //
  // FIXME(emilio): Think this can go away.
  static mozilla::ArenaObjectID ArenaObjectID()
  {
    return mozilla::eArenaObjectID_GeckoComputedStyle;
  }
  nsIPresShell* Arena();

  void AddRef() { Servo_ComputedStyle_AddRef(this); }
  void Release() { Servo_ComputedStyle_Release(this); }

  // Return the ComputedStyle whose style data should be used for the R,
  // G, and B components of color, background-color, and border-*-color
  // if RelevantLinkIsVisited().
  //
  // GetPseudo() and GetPseudoType() on this ComputedStyle return the
  // same as on |this|, and its depth in the tree (number of GetParent()
  // calls until null is returned) is the same as |this|, since its
  // parent is either |this|'s parent or |this|'s parent's
  // style-if-visited.
  //
  // Structs on this context should never be examined without also
  // examining the corresponding struct on |this|.  Doing so will likely
  // both (1) lead to a privacy leak and (2) lead to dynamic change bugs
  // related to the Peek code in ComputedStyle::CalcStyleDifference.
  ComputedStyle* GetStyleIfVisited() const
  {
    return ComputedData()->visited_style.mPtr;
  }

  bool IsLazilyCascadedPseudoElement() const
  {
    return IsPseudoElement() &&
           !nsCSSPseudoElements::IsEagerlyCascadedInServo(GetPseudoType());
  }

  nsAtom* GetPseudo() const { return mPseudoTag; }
  mozilla::CSSPseudoElementType GetPseudoType() const {
    return static_cast<mozilla::CSSPseudoElementType>(
             mBits >> NS_STYLE_CONTEXT_TYPE_SHIFT);
  }

  bool IsInheritingAnonBox() const {
    return GetPseudoType() == mozilla::CSSPseudoElementType::InheritingAnonBox;
  }

  bool IsNonInheritingAnonBox() const {
    return GetPseudoType() == mozilla::CSSPseudoElementType::NonInheritingAnonBox;
  }

  // This function is rather slow; you probably don't want to use it outside
  // asserts unless you have to.  We _could_ add a new CSSPseudoElementType for
  // wrapper anon boxes, but that adds a bunch of complexity everywhere we
  // resolve anonymous box styles...
  bool IsWrapperAnonBox() const {
    return nsCSSAnonBoxes::IsWrapperAnonBox(GetPseudo());
  }

  bool IsAnonBox() const {
    return IsInheritingAnonBox() || IsNonInheritingAnonBox();
  }

  bool IsPseudoElement() const { return mPseudoTag && !IsAnonBox(); }


  // Does this ComputedStyle or any of its ancestors have text
  // decoration lines?
  // Differs from nsStyleTextReset::HasTextDecorationLines, which tests
  // only the data for a single context.
  bool HasTextDecorationLines() const
    { return !!(mBits & NS_STYLE_HAS_TEXT_DECORATION_LINES); }

  // Whether any line break inside should be suppressed? If this returns
  // true, the line should not be broken inside, which means inlines act
  // as if nowrap is set, <br> is suppressed, and blocks are inlinized.
  // This bit is propogated to all children of line partitipants. It is
  // currently used by ruby to make its content frames unbreakable.
  // NOTE: for nsTextFrame, use nsTextFrame::ShouldSuppressLineBreak()
  // instead of this method.
  bool ShouldSuppressLineBreak() const
    { return !!(mBits & NS_STYLE_SUPPRESS_LINEBREAK); }

  // Does this ComputedStyle or any of its ancestors have display:none set?
  bool IsInDisplayNoneSubtree() const
    { return !!(mBits & NS_STYLE_IN_DISPLAY_NONE_SUBTREE); }

  // Is this horizontal-in-vertical (tate-chu-yoko) text? This flag is
  // only set on ComputedStyles whose pseudo is nsCSSAnonBoxes::mozText.
  bool IsTextCombined() const
    { return !!(mBits & NS_STYLE_IS_TEXT_COMBINED); }

  // Does this ComputedStyle represent the style for a pseudo-element or
  // inherit data from such a ComputedStyle?  Whether this returns true
  // is equivalent to whether it or any of its ancestors returns
  // non-null for IsPseudoElement().
  bool HasPseudoElementData() const
    { return !!(mBits & NS_STYLE_HAS_PSEUDO_ELEMENT_DATA); }

  bool HasChildThatUsesResetStyle() const
    { return mBits & NS_STYLE_HAS_CHILD_THAT_USES_RESET_STYLE; }

  // Is the only link whose visitedness is allowed to influence the
  // style of the node this ComputedStyle is for (which is that element
  // or its nearest ancestor that is a link) visited?
  bool RelevantLinkVisited() const
    { return !!(mBits & NS_STYLE_RELEVANT_LINK_VISITED); }

  // Is this a ComputedStyle for a link?
  inline bool IsLinkContext() const;

  // Is this ComputedStyle the GetStyleIfVisited() for some other style
  // context?
  bool IsStyleIfVisited() const
    { return !!(mBits & NS_STYLE_IS_STYLE_IF_VISITED); }

  // Tells this ComputedStyle that it should return true from
  // IsStyleIfVisited.
  void SetIsStyleIfVisited()
    { mBits |= NS_STYLE_IS_STYLE_IF_VISITED; }

  // Does any descendant of this ComputedStyle have any style values
  // that were computed based on this ComputedStyle's ancestors?
  bool HasChildThatUsesGrandancestorStyle() const
    { return !!(mBits & NS_STYLE_CHILD_USES_GRANDANCESTOR_STYLE); }

  // Is this ComputedStyle shared with a sibling or cousin?
  // (See nsStyleSet::GetContext.)
  bool IsShared() const
    { return !!(mBits & NS_STYLE_IS_SHARED); }

  /**
   * Returns whether this ComputedStyle has cached style data for a
   * given style struct and it does NOT own that struct.  This can
   * happen because it was inherited from the parent ComputedStyle, or
   * because it was stored conditionally on the rule node.
   */
  bool HasCachedDependentStyleData(nsStyleStructID aSID) {
    return mBits & GetBitForSID(aSID);
  }

  ComputedStyle* GetCachedInheritingAnonBoxStyle(nsAtom* aAnonBox) const
  {
    MOZ_ASSERT(nsCSSAnonBoxes::IsInheritingAnonBox(aAnonBox));
    return mCachedInheritingStyles.Lookup(aAnonBox);
  }

  void SetCachedInheritedAnonBoxStyle(nsAtom* aAnonBox, ComputedStyle* aStyle)
  {
    MOZ_ASSERT(!GetCachedInheritingAnonBoxStyle(aAnonBox));
    mCachedInheritingStyles.Insert(aStyle);
  }

  ComputedStyle* GetCachedLazyPseudoStyle(CSSPseudoElementType aPseudo) const;

  void SetCachedLazyPseudoStyle(ComputedStyle* aStyle)
  {
    MOZ_ASSERT(aStyle->GetPseudo() && !aStyle->IsAnonBox());
    MOZ_ASSERT(!GetCachedLazyPseudoStyle(aStyle->GetPseudoType()));
    MOZ_ASSERT(!IsLazilyCascadedPseudoElement(), "lazy pseudos can't inherit lazy pseudos");
    MOZ_ASSERT(aStyle->IsLazilyCascadedPseudoElement());

    // Since we're caching lazy pseudo styles on the ComputedValues of the
    // originating element, we can assume that we either have the same
    // originating element, or that they were at least similar enough to share
    // the same ComputedValues, which means that they would match the same
    // pseudo rules. This allows us to avoid matching selectors and checking
    // the rule node before deciding to share.
    //
    // The one place this optimization breaks is with pseudo-elements that
    // support state (like :hover). So we just avoid sharing in those cases.
    if (nsCSSPseudoElements::PseudoElementSupportsUserActionState(aStyle->GetPseudoType())) {
      return;
    }

    mCachedInheritingStyles.Insert(aStyle);
  }

  void AddStyleBit(const uint64_t& aBit) { mBits |= aBit; }

  /**
   * Define typesafe getter functions for each style struct by
   * preprocessing the list of style structs.  These functions are the
   * preferred way to get style data.  The macro creates functions like:
   *   const nsStyleBorder* StyleBorder();
   *   const nsStyleColor* StyleColor();
   */
  #define STYLE_STRUCT(name_) \
    inline const nsStyle##name_ * Style##name_() MOZ_NONNULL_RETURN;
  #include "nsStyleStructList.h"
  #undef STYLE_STRUCT

  /**
   * Equivalent to StyleFoo(), except that we skip the cache write during the
   * servo traversal. This can cause incorrect behavior if used improperly,
   * since we won't record that layout potentially depends on the values in
   * this style struct. Use with care.
   */

  #define STYLE_STRUCT(name_) \
    inline const nsStyle##name_ * ThreadsafeStyle##name_();
  #include "nsStyleStructList.h"
  #undef STYLE_STRUCT


  /**
   * PeekStyle* is like Style* but doesn't trigger style
   * computation if the data is not cached on either the ComputedStyle
   * or the rule node.
   *
   * Perhaps this shouldn't be a public ComputedStyle API.
   */
  #define STYLE_STRUCT(name_)  \
    inline const nsStyle##name_ * PeekStyle##name_();
  #include "nsStyleStructList.h"
  #undef STYLE_STRUCT

  /**
   * Compute the style changes needed during restyling when this style
   * context is being replaced by aNewContext.  (This is nonsymmetric since
   * we optimize by skipping comparison for styles that have never been
   * requested.)
   *
   * This method returns a change hint (see nsChangeHint.h).  All change
   * hints apply to the frame and its later continuations or ib-split
   * siblings.  Most (all of those except the "NotHandledForDescendants"
   * hints) also apply to all descendants.
   *
   * aEqualStructs must not be null.  Into it will be stored a bitfield
   * representing which structs were compared to be non-equal.
   *
   * CSS Variables are not compared here. Instead, the caller is responsible for
   * that when needed (basically only for elements).
   */
  nsChangeHint CalcStyleDifference(ComputedStyle* aNewContext,
                                   uint32_t* aEqualStructs);

public:
  /**
   * Get a color that depends on link-visitedness using this and
   * this->GetStyleIfVisited().
   *
   * @param aField A pointer to a member variable in a style struct.
   *               The member variable and its style struct must have
   *               been listed in nsCSSVisitedDependentPropList.h.
   */
  template<typename T, typename S>
  nscolor GetVisitedDependentColor(T S::* aField);

  /**
   * aColors should be a two element array of nscolor in which the first
   * color is the unvisited color and the second is the visited color.
   *
   * Combine the R, G, and B components of whichever of aColors should
   * be used based on aLinkIsVisited with the A component of aColors[0].
   */
  static nscolor CombineVisitedColors(nscolor *aColors,
                                      bool aLinkIsVisited);

  /**
   * Start the background image loads for this ComputedStyle.
   */
  inline void StartBackgroundImageLoads();

  static uint32_t GetBitForSID(const nsStyleStructID aSID) { return 1 << aSID; }

#ifdef DEBUG
  void List(FILE* out, int32_t aIndent);
  static const char* StructName(nsStyleStructID aSID);
  static bool LookupStruct(const nsACString& aName, nsStyleStructID& aResult);
#endif

  /**
   * Makes this context match |aOther| in terms of which style structs have
   * been resolved.
   */
  inline void ResolveSameStructsAs(const ComputedStyle* aOther);

  // The |aCVsSize| outparam on this function is where the actual CVs size
  // value is added. It's done that way because the callers know which value
  // the size should be added to.
  void AddSizeOfIncludingThis(nsWindowSizes& aSizes, size_t* aCVsSize) const;

protected:
  // Needs to be friend so that it can call the destructor without making it
  // public.
  friend void ::Gecko_ComputedStyle_Destroy(ComputedStyle*);

  ~ComputedStyle() = default;

  nsPresContext* mPresContext;

  ServoComputedData mSource;

  // A cache of anonymous box and lazy pseudo styles inheriting from this style.
  CachedInheritingStyles mCachedInheritingStyles;

  // Helper functions for GetStyle* and PeekStyle*
  #define STYLE_STRUCT_INHERITED(name_)         \
    template<bool aComputeData>                 \
    const nsStyle##name_ * DoGetStyle##name_();
  #define STYLE_STRUCT_RESET(name_)             \
    template<bool aComputeData>                 \
    const nsStyle##name_ * DoGetStyle##name_();

  #include "nsStyleStructList.h"
  #undef STYLE_STRUCT_RESET
  #undef STYLE_STRUCT_INHERITED

  // If this ComputedStyle is for a pseudo-element or anonymous box,
  // the relevant atom.
  RefPtr<nsAtom> mPseudoTag;

  // mBits stores a number of things:
  //  - It records (using the style struct bits) which structs have
  //    been requested on this ComputedStyle.
  //  - It also stores the additional bits listed at the top of
  //    nsStyleStruct.h.
  uint64_t                mBits;
};

} // namespace mozilla

#endif
