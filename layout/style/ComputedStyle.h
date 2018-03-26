/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* the interface (to internal code) for retrieving computed style data */

#ifndef _ComputedStyle_h_
#define _ComputedStyle_h_

#include "nsIMemoryReporter.h"
#include "nsWindowSizes.h"
#include <algorithm>
#include "mozilla/Assertions.h"
#include "mozilla/RestyleLogging.h"
#include "mozilla/ServoStyleSet.h"
#include "mozilla/ServoUtils.h"
#include "mozilla/StyleComplexColor.h"
#include "mozilla/CachedInheritingStyles.h"
#include "nsCSSAnonBoxes.h"

class nsAtom;
class nsPresContext;

namespace mozilla {

enum class CSSPseudoElementType : uint8_t;
class ComputedStyle;

extern "C" {
  void Servo_ComputedStyle_AddRef(const mozilla::ComputedStyle* aStyle);
  void Servo_ComputedStyle_Release(const mozilla::ComputedStyle* aStyle);
  void Gecko_ComputedStyle_Destroy(mozilla::ComputedStyle*);
}

MOZ_DEFINE_MALLOC_ENCLOSING_SIZE_OF(ServoComputedValuesMallocEnclosingSizeOf)

/**
 * A ComputedStyle represents the computed style data for an element.  The
 * computed style data are stored in a set of structs (see nsStyleStruct.h) that
 * are cached either on the style context or in the rule tree (see nsRuleNode.h
 * for a description of this caching and how the cached structs are shared).
 *
 * Since the data in |nsIStyleRule|s and |nsRuleNode|s are immutable (with a few
 * exceptions, like system color changes), the data in an ComputedStyle are also
 * immutable (with the additional exception of GetUniqueStyleData).  When style
 * data change, ElementRestyler::Restyle creates a new style context.
 *
 * ComputedStyles are reference counted. References are generally held by:
 *  1. the |nsIFrame|s that are using the style context and
 *  2. any *child* style contexts (this might be the reverse of
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

  nsPresContext* PresContext() const { return mPresContext; }
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

  // Return the style context whose style data should be used for the R,
  // G, and B components of color, background-color, and border-*-color
  // if RelevantLinkIsVisited().
  //
  // GetPseudo() and GetPseudoType() on this style context return the
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


  // Does this style context or any of its ancestors have text
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

  // Does this style context or any of its ancestors have display:none set?
  bool IsInDisplayNoneSubtree() const
    { return !!(mBits & NS_STYLE_IN_DISPLAY_NONE_SUBTREE); }

  // Is this horizontal-in-vertical (tate-chu-yoko) text? This flag is
  // only set on style contexts whose pseudo is nsCSSAnonBoxes::mozText.
  bool IsTextCombined() const
    { return !!(mBits & NS_STYLE_IS_TEXT_COMBINED); }

  // Does this style context represent the style for a pseudo-element or
  // inherit data from such a style context?  Whether this returns true
  // is equivalent to whether it or any of its ancestors returns
  // non-null for IsPseudoElement().
  bool HasPseudoElementData() const
    { return !!(mBits & NS_STYLE_HAS_PSEUDO_ELEMENT_DATA); }

  bool HasChildThatUsesResetStyle() const
    { return mBits & NS_STYLE_HAS_CHILD_THAT_USES_RESET_STYLE; }

  // Is the only link whose visitedness is allowed to influence the
  // style of the node this style context is for (which is that element
  // or its nearest ancestor that is a link) visited?
  bool RelevantLinkVisited() const
    { return !!(mBits & NS_STYLE_RELEVANT_LINK_VISITED); }

  // Is this a style context for a link?
  inline bool IsLinkContext() const;

  // Is this style context the GetStyleIfVisited() for some other style
  // context?
  bool IsStyleIfVisited() const
    { return !!(mBits & NS_STYLE_IS_STYLE_IF_VISITED); }

  // Tells this style context that it should return true from
  // IsStyleIfVisited.
  void SetIsStyleIfVisited()
    { mBits |= NS_STYLE_IS_STYLE_IF_VISITED; }

  // Does any descendant of this style context have any style values
  // that were computed based on this style context's ancestors?
  bool HasChildThatUsesGrandancestorStyle() const
    { return !!(mBits & NS_STYLE_CHILD_USES_GRANDANCESTOR_STYLE); }

  // Is this style context shared with a sibling or cousin?
  // (See nsStyleSet::GetContext.)
  bool IsShared() const
    { return !!(mBits & NS_STYLE_IS_SHARED); }

  /**
   * Returns whether this style context has cached style data for a
   * given style struct and it does NOT own that struct.  This can
   * happen because it was inherited from the parent style context, or
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
   * computation if the data is not cached on either the style context
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
   * that when needed (basically only for elements). The Variables bit in
   * aEqualStructs is always set.
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
   * Start the background image loads for this style context.
   */
  inline void StartBackgroundImageLoads();

  static bool IsReset(const nsStyleStructID aSID) {
    MOZ_ASSERT(0 <= aSID && aSID < nsStyleStructID_Length,
               "must be an inherited or reset SID");
    return nsStyleStructID_Reset_Start <= aSID;
  }
  static bool IsInherited(const nsStyleStructID aSID) { return !IsReset(aSID); }
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
  void AddSizeOfIncludingThis(nsWindowSizes& aSizes, size_t* aCVsSize) const
  {
    // Note: |this| sits within a servo_arc::Arc, i.e. it is preceded by a
    // refcount. So we need to measure it with a function that can handle an
    // interior pointer. We use ServoComputedValuesMallocEnclosingSizeOf to
    // clearly identify in DMD's output the memory measured here.
    *aCVsSize += ServoComputedValuesMallocEnclosingSizeOf(this);
    mSource.AddSizeOfExcludingThis(aSizes);
    mCachedInheritingStyles.AddSizeOfIncludingThis(aSizes, aCVsSize);
  }

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

  // If this style context is for a pseudo-element or anonymous box,
  // the relevant atom.
  RefPtr<nsAtom> mPseudoTag;

  // mBits stores a number of things:
  //  - It records (using the style struct bits) which structs are
  //    inherited from the parent context or owned by the rule node (i.e.,
  //    not owned by the style context).
  //  - It also stores the additional bits listed at the top of
  //    nsStyleStruct.h.
  uint64_t                mBits;
};

} // namespace mozilla

#endif
