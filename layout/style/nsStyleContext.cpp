/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* the interface (to internal code) for retrieving computed style data */

#include "nsStyleContext.h"

#include "CSSVariableImageTable.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Maybe.h"

#include "nsCSSAnonBoxes.h"
#include "nsCSSPseudoElements.h"
#include "nsFontMetrics.h"
#include "nsStyleConsts.h"
#include "nsStyleStruct.h"
#include "nsStyleStructInlines.h"
#include "nsString.h"
#include "nsPresContext.h"
#include "nsIStyleRule.h"

#include "nsCOMPtr.h"
#include "nsStyleSet.h"
#include "nsIPresShell.h"

#include "nsRuleNode.h"
#include "GeckoProfiler.h"
#include "nsIDocument.h"
#include "nsPrintfCString.h"
#include "RubyUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/ArenaObjectID.h"
#include "mozilla/StyleSetHandle.h"
#include "mozilla/StyleSetHandleInlines.h"

#include "mozilla/ReflowInput.h"
#include "nsLayoutUtils.h"
#include "nsCoord.h"

// Ensure the binding function declarations in nsStyleContext.h matches
// those in ServoBindings.h.
#include "mozilla/ServoBindings.h"

using namespace mozilla;

//----------------------------------------------------------------------

#ifdef DEBUG

// Check that the style struct IDs are in the same order as they are
// in nsStyleStructList.h, since when we set up the IDs, we include
// the inherited and reset structs spearately from nsStyleStructList.h
enum DebugStyleStruct {
#define STYLE_STRUCT(name, checkdata_cb) eDebugStyleStruct_##name,
#include "nsStyleStructList.h"
#undef STYLE_STRUCT
};

#define STYLE_STRUCT(name, checkdata_cb) \
  static_assert(static_cast<int>(eDebugStyleStruct_##name) == \
                  static_cast<int>(eStyleStruct_##name), \
                "Style struct IDs are not declared in order?");
#include "nsStyleStructList.h"
#undef STYLE_STRUCT

const uint32_t nsStyleContext::sDependencyTable[] = {
#define STYLE_STRUCT(name, checkdata_cb)
#define STYLE_STRUCT_DEP(dep) NS_STYLE_INHERIT_BIT(dep) |
#define STYLE_STRUCT_END() 0,
#include "nsStyleStructList.h"
#undef STYLE_STRUCT
#undef STYLE_STRUCT_DEP
#undef STYLE_STRUCT_END
};

// Whether to perform expensive assertions in the nsStyleContext destructor.
static bool sExpensiveStyleStructAssertionsEnabled;
#endif

nsStyleContext::nsStyleContext(nsStyleContext* aParent,
                               OwningStyleContextSource&& aSource,
                               nsIAtom* aPseudoTag,
                               CSSPseudoElementType aPseudoType)
  : mParent(aParent)
  , mChild(nullptr)
  , mEmptyChild(nullptr)
  , mPseudoTag(aPseudoTag)
  , mSource(Move(aSource))
#ifdef MOZ_STYLO
  , mPresContext(nullptr)
#endif
  , mCachedResetData(nullptr)
  , mBits(((uint64_t)aPseudoType) << NS_STYLE_CONTEXT_TYPE_SHIFT)
  , mRefCnt(0)
#ifdef DEBUG
  , mFrameRefCnt(0)
  , mComputingStruct(nsStyleStructID_None)
#endif
{}

nsStyleContext::nsStyleContext(nsStyleContext* aParent,
                               nsIAtom* aPseudoTag,
                               CSSPseudoElementType aPseudoType,
                               already_AddRefed<nsRuleNode> aRuleNode,
                               bool aSkipParentDisplayBasedStyleFixup)
  : nsStyleContext(aParent, OwningStyleContextSource(Move(aRuleNode)),
                   aPseudoTag, aPseudoType)
{
#ifdef MOZ_STYLO
  mPresContext = mSource.AsGeckoRuleNode()->PresContext();
#endif

  if (aParent) {
#ifdef DEBUG
    nsRuleNode *r1 = mParent->RuleNode(), *r2 = mSource.AsGeckoRuleNode();
    while (r1->GetParent())
      r1 = r1->GetParent();
    while (r2->GetParent())
      r2 = r2->GetParent();
    NS_ASSERTION(r1 == r2, "must be in the same rule tree as parent");
#endif
  } else {
    PresContext()->PresShell()->StyleSet()->RootStyleContextAdded();
  }

  mSource.AsGeckoRuleNode()->SetUsedDirectly(); // before ApplyStyleFixups()!
  FinishConstruction();
  ApplyStyleFixups(aSkipParentDisplayBasedStyleFixup);
}

nsStyleContext::nsStyleContext(nsStyleContext* aParent,
                               nsPresContext* aPresContext,
                               nsIAtom* aPseudoTag,
                               CSSPseudoElementType aPseudoType,
                               already_AddRefed<ServoComputedValues> aComputedValues)
  : nsStyleContext(aParent, OwningStyleContextSource(Move(aComputedValues)),
                   aPseudoTag, aPseudoType)
{
#ifdef MOZ_STYLO
  mPresContext = aPresContext;
#endif

  FinishConstruction();

  // No need to call ApplyStyleFixups here, since fixups are handled by Servo when
  // producing the ServoComputedValues.
}

void
nsStyleContext::FinishConstruction()
{
  // This check has to be done "backward", because if it were written the
  // more natural way it wouldn't fail even when it needed to.
  static_assert((UINT64_MAX >> NS_STYLE_CONTEXT_TYPE_SHIFT) >=
                 static_cast<CSSPseudoElementTypeBase>(
                   CSSPseudoElementType::MAX),
                "pseudo element bits no longer fit in a uint64_t");
  MOZ_ASSERT(!mSource.IsNull());

#ifdef DEBUG
  static_assert(MOZ_ARRAY_LENGTH(nsStyleContext::sDependencyTable)
                  == nsStyleStructID_Length,
                "Number of items in dependency table doesn't match IDs");
#endif

  mNextSibling = this;
  mPrevSibling = this;
  if (mParent) {
    mParent->AddChild(this);
  }

  SetStyleBits();

  #define eStyleStruct_LastItem (nsStyleStructID_Length - 1)
  static_assert(NS_STYLE_INHERIT_MASK & NS_STYLE_INHERIT_BIT(LastItem),
                "NS_STYLE_INHERIT_MASK must be bigger, and other bits shifted");
  #undef eStyleStruct_LastItem
}

nsStyleContext::~nsStyleContext()
{
  NS_ASSERTION((nullptr == mChild) && (nullptr == mEmptyChild), "destructing context with children");
  MOZ_ASSERT(!mSource.IsServoComputedValues() || !mCachedResetData);

#ifdef DEBUG
  if (mSource.IsServoComputedValues()) {
    MOZ_ASSERT(!mCachedResetData,
               "Servo shouldn't cache reset structs in nsStyleContext");
    for (const auto* data : mCachedInheritedData.mStyleStructs) {
      MOZ_ASSERT(!data,
                 "Servo shouldn't cache inherit structs in nsStyleContext");
    }
  }

  if (sExpensiveStyleStructAssertionsEnabled) {
    // Assert that the style structs we are about to destroy are not referenced
    // anywhere else in the style context tree.  These checks are expensive,
    // which is why they are not enabled by default.
    nsStyleContext* root = this;
    while (root->mParent) {
      root = root->mParent;
    }
    root->AssertStructsNotUsedElsewhere(this,
                                        std::numeric_limits<int32_t>::max());
  } else {
    // In DEBUG builds when the pref is not enabled, we perform a more limited
    // check just of the children of this style context.
    AssertStructsNotUsedElsewhere(this, 2);
  }
#endif

  nsPresContext *presContext = PresContext();
  DebugOnly<nsStyleSet*> geckoStyleSet = presContext->PresShell()->StyleSet()->GetAsGecko();
  NS_ASSERTION(!geckoStyleSet ||
               geckoStyleSet->GetRuleTree() == mSource.AsGeckoRuleNode()->RuleTree() ||
               geckoStyleSet->IsInRuleTreeReconstruct(),
               "destroying style context from old rule tree too late");

  if (mParent) {
    mParent->RemoveChild(this);
  } else {
    presContext->StyleSet()->RootStyleContextRemoved();
  }

  // Free up our data structs.
  mCachedInheritedData.DestroyStructs(mBits, presContext);
  if (mCachedResetData) {
    mCachedResetData->Destroy(mBits, presContext);
  }

  // Free any ImageValues we were holding on to for CSS variable values.
  CSSVariableImageTable::RemoveAll(this);
}

#ifdef DEBUG
void
nsStyleContext::AssertStructsNotUsedElsewhere(
                                       nsStyleContext* aDestroyingContext,
                                       int32_t aLevels) const
{
  if (aLevels == 0) {
    return;
  }

  void* data;

  if (mBits & NS_STYLE_IS_GOING_AWAY) {
    return;
  }

  if (this != aDestroyingContext) {
    nsInheritedStyleData& destroyingInheritedData =
      aDestroyingContext->mCachedInheritedData;
#define STYLE_STRUCT_INHERITED(name_, checkdata_cb)                            \
    data = destroyingInheritedData.mStyleStructs[eStyleStruct_##name_];        \
    if (data &&                                                                \
        !(aDestroyingContext->mBits & NS_STYLE_INHERIT_BIT(name_)) &&          \
         (mCachedInheritedData.mStyleStructs[eStyleStruct_##name_] == data)) { \
      printf_stderr("style struct %p found on style context %p\n", data, this);\
      nsString url;                                                            \
      nsresult rv = PresContext()->Document()->GetURL(url);                    \
      if (NS_SUCCEEDED(rv)) {                                                  \
        printf_stderr("  in %s\n", NS_ConvertUTF16toUTF8(url).get());          \
      }                                                                        \
      MOZ_ASSERT(false, "destroying " #name_ " style struct still present "    \
                        "in style context tree");                              \
    }
#define STYLE_STRUCT_RESET(name_, checkdata_cb)

#include "nsStyleStructList.h"

#undef STYLE_STRUCT_INHERITED
#undef STYLE_STRUCT_RESET

    if (mCachedResetData) {
      nsResetStyleData* destroyingResetData =
        aDestroyingContext->mCachedResetData;
      if (destroyingResetData) {
#define STYLE_STRUCT_INHERITED(name_, checkdata_cb_)
#define STYLE_STRUCT_RESET(name_, checkdata_cb)                                \
        data = destroyingResetData->mStyleStructs[eStyleStruct_##name_];       \
        if (data &&                                                            \
            !(aDestroyingContext->mBits & NS_STYLE_INHERIT_BIT(name_)) &&      \
            (mCachedResetData->mStyleStructs[eStyleStruct_##name_] == data)) { \
          printf_stderr("style struct %p found on style context %p\n", data,   \
                        this);                                                 \
          nsString url;                                                        \
          nsresult rv = PresContext()->Document()->GetURL(url);                \
          if (NS_SUCCEEDED(rv)) {                                              \
            printf_stderr("  in %s\n", NS_ConvertUTF16toUTF8(url).get());      \
          }                                                                    \
          MOZ_ASSERT(false, "destroying " #name_ " style struct still present "\
                            "in style context tree");                          \
        }

#include "nsStyleStructList.h"

#undef STYLE_STRUCT_INHERITED
#undef STYLE_STRUCT_RESET
      }
    }
  }

  if (mChild) {
    const nsStyleContext* child = mChild;
    do {
      child->AssertStructsNotUsedElsewhere(aDestroyingContext, aLevels - 1);
      child = child->mNextSibling;
    } while (child != mChild);
  }

  if (mEmptyChild) {
    const nsStyleContext* child = mEmptyChild;
    do {
      child->AssertStructsNotUsedElsewhere(aDestroyingContext, aLevels - 1);
      child = child->mNextSibling;
    } while (child != mEmptyChild);
  }
}
#endif

void nsStyleContext::AddChild(nsStyleContext* aChild)
{
  NS_ASSERTION(aChild->mPrevSibling == aChild &&
               aChild->mNextSibling == aChild,
               "child already in a child list");

  nsStyleContext **listPtr = aChild->mSource.MatchesNoRules() ? &mEmptyChild : &mChild;
  // Explicitly dereference listPtr so that compiler doesn't have to know that mNextSibling
  // etc. don't alias with what ever listPtr points at.
  nsStyleContext *list = *listPtr;

  // Insert at the beginning of the list.  See also FindChildWithRules.
  if (list) {
    // Link into existing elements, if there are any.
    aChild->mNextSibling = list;
    aChild->mPrevSibling = list->mPrevSibling;
    list->mPrevSibling->mNextSibling = aChild;
    list->mPrevSibling = aChild;
  }
  (*listPtr) = aChild;
}

void nsStyleContext::RemoveChild(nsStyleContext* aChild)
{
  NS_PRECONDITION(nullptr != aChild && this == aChild->mParent, "bad argument");

  nsStyleContext **list = aChild->mSource.MatchesNoRules() ? &mEmptyChild : &mChild;

  if (aChild->mPrevSibling != aChild) { // has siblings
    if ((*list) == aChild) {
      (*list) = (*list)->mNextSibling;
    }
  }
  else {
    NS_ASSERTION((*list) == aChild, "bad sibling pointers");
    (*list) = nullptr;
  }

  aChild->mPrevSibling->mNextSibling = aChild->mNextSibling;
  aChild->mNextSibling->mPrevSibling = aChild->mPrevSibling;
  aChild->mNextSibling = aChild;
  aChild->mPrevSibling = aChild;
}

void
nsStyleContext::MoveTo(nsStyleContext* aNewParent)
{
  MOZ_ASSERT(aNewParent != mParent);

  // This function shouldn't be getting called if the parents have different
  // values for some flags in mBits (unless the flag is also set on this style
  // context) because if that were the case we would need to recompute those
  // bits for |this|.

#define CHECK_FLAG(bit_) \
  MOZ_ASSERT((mParent->mBits & (bit_)) == (aNewParent->mBits & (bit_)) ||     \
             (mBits & (bit_)),                                                \
             "MoveTo cannot be called if " #bit_ " value on old and new "     \
             "style context parents do not match, unless the flag is set "    \
             "on this style context");

  CHECK_FLAG(NS_STYLE_HAS_PSEUDO_ELEMENT_DATA)
  CHECK_FLAG(NS_STYLE_IN_DISPLAY_NONE_SUBTREE)
  CHECK_FLAG(NS_STYLE_HAS_TEXT_DECORATION_LINES)
  CHECK_FLAG(NS_STYLE_RELEVANT_LINK_VISITED)

#undef CHECK_FLAG

  // Assertions checking for visited style are just to avoid some tricky
  // cases we can't be bothered handling at the moment.
  MOZ_ASSERT(!IsStyleIfVisited());
  MOZ_ASSERT(!mParent->IsStyleIfVisited());
  MOZ_ASSERT(!aNewParent->IsStyleIfVisited());
  MOZ_ASSERT(!mStyleIfVisited || mStyleIfVisited->mParent == mParent);

  if (mParent->HasChildThatUsesResetStyle()) {
    aNewParent->AddStyleBit(NS_STYLE_HAS_CHILD_THAT_USES_RESET_STYLE);
  }

  mParent->RemoveChild(this);
  mParent = aNewParent;
  mParent->AddChild(this);

  if (mStyleIfVisited) {
    mStyleIfVisited->mParent->RemoveChild(mStyleIfVisited);
    mStyleIfVisited->mParent = aNewParent;
    mStyleIfVisited->mParent->AddChild(mStyleIfVisited);
  }
}

already_AddRefed<nsStyleContext>
nsStyleContext::FindChildWithRules(const nsIAtom* aPseudoTag,
                                   NonOwningStyleContextSource aSource,
                                   NonOwningStyleContextSource aSourceIfVisited,
                                   bool aRelevantLinkVisited)
{
  uint32_t threshold = 10; // The # of siblings we're willing to examine
                           // before just giving this whole thing up.

  RefPtr<nsStyleContext> result;
  nsStyleContext *list = aSource.MatchesNoRules() ? mEmptyChild : mChild;

  if (list) {
    nsStyleContext *child = list;
    do {
      if (child->mSource.AsRaw() == aSource &&
          child->mPseudoTag == aPseudoTag &&
          !child->IsStyleIfVisited() &&
          child->RelevantLinkVisited() == aRelevantLinkVisited) {
        bool match = false;
        if (!aSourceIfVisited.IsNull()) {
          match = child->GetStyleIfVisited() &&
                  child->GetStyleIfVisited()->mSource.AsRaw() == aSourceIfVisited;
        } else {
          match = !child->GetStyleIfVisited();
        }
        if (match && !(child->mBits & NS_STYLE_INELIGIBLE_FOR_SHARING)) {
          result = child;
          break;
        }
      }
      child = child->mNextSibling;
      threshold--;
      if (threshold == 0)
        break;
    } while (child != list);
  }

  if (result) {
    if (result != list) {
      // Move result to the front of the list.
      RemoveChild(result);
      AddChild(result);
    }
    result->mBits |= NS_STYLE_IS_SHARED;
  }

  return result.forget();
}

const void* nsStyleContext::StyleData(nsStyleStructID aSID)
{
  const void* cachedData = GetCachedStyleData(aSID);
  if (cachedData)
    return cachedData; // We have computed data stored on this node in the context tree.
  // Our style source will take care of it for us.
  const void* newData;
  if (mSource.IsGeckoRuleNode()) {
    newData = mSource.AsGeckoRuleNode()->GetStyleData(aSID, this, true);
    if (!nsCachedStyleData::IsReset(aSID)) {
      // always cache inherited data on the style context; the rule
      // node set the bit in mBits for us if needed.
      mCachedInheritedData.mStyleStructs[aSID] = const_cast<void*>(newData);
    }
  } else {
    newData = StyleStructFromServoComputedValues(aSID);

    // perform any remaining main thread work on the struct
    switch (aSID) {
#define STYLE_STRUCT(name_, checkdata_cb_)                                    \
      case eStyleStruct_##name_: {                                            \
        auto data = static_cast<const nsStyle##name_*>(newData);              \
        const_cast<nsStyle##name_*>(data)->FinishStyle(PresContext());        \
        break;                                                                \
      }
#include "nsStyleStructList.h"
#undef STYLE_STRUCT
      default:
        MOZ_ASSERT_UNREACHABLE("unexpected nsStyleStructID value");
        break;
    }

    // The Servo-backed StyleContextSource owns the struct.
    AddStyleBit(nsCachedStyleData::GetBitForSID(aSID));

    // XXXbholley: Unconditionally caching reset structs here defeats the memory
    // optimization where we lazily allocate mCachedResetData, so that we can avoid
    // performing an FFI call each time we want to get the style structs. We should
    // measure the tradeoffs at some point. If the FFI overhead is low and the memory
    // win significant, we should consider _always_ grabbing the struct over FFI, and
    // potentially giving mCachedInheritedData the same treatment.
    //
    // Note that there is a similar comment in the struct getters in nsStyleContext.h.
    SetStyle(aSID, const_cast<void*>(newData));
  }
  return newData;
}

// This is an evil evil function, since it forces you to alloc your own separate copy of
// style data!  Do not use this function unless you absolutely have to!  You should avoid
// this at all costs! -dwh
void*
nsStyleContext::GetUniqueStyleData(const nsStyleStructID& aSID)
{
  MOZ_ASSERT(!mSource.IsServoComputedValues(),
             "Can't COW-mutate servo values from Gecko!");

  // If we already own the struct and no kids could depend on it, then
  // just return it.  (We leak in this case if there are kids -- and this
  // function really shouldn't be called for style contexts that could
  // have kids depending on the data.  ClearStyleData would be OK, but
  // this test for no mChild or mEmptyChild doesn't catch that case.)
  const void *current = StyleData(aSID);
  if (!mChild && !mEmptyChild &&
      !(mBits & nsCachedStyleData::GetBitForSID(aSID)) &&
      GetCachedStyleData(aSID))
    return const_cast<void*>(current);

  void* result;
  nsPresContext *presContext = PresContext();
  switch (aSID) {

#define UNIQUE_CASE(c_)                                                       \
  case eStyleStruct_##c_:                                                     \
    result = new (presContext) nsStyle##c_(                                   \
      * static_cast<const nsStyle##c_ *>(current));                           \
    break;

  UNIQUE_CASE(Font)
  UNIQUE_CASE(Display)
  UNIQUE_CASE(Text)
  UNIQUE_CASE(TextReset)
  UNIQUE_CASE(Visibility)

#undef UNIQUE_CASE

  default:
    NS_ERROR("Struct type not supported.  Please find another way to do this if you can!");
    return nullptr;
  }

  SetStyle(aSID, result);
  mBits &= ~static_cast<uint64_t>(nsCachedStyleData::GetBitForSID(aSID));

  return result;
}

// This is an evil function, but less evil than GetUniqueStyleData. It
// creates an empty style struct for this nsStyleContext.
void*
nsStyleContext::CreateEmptyStyleData(const nsStyleStructID& aSID)
{
  MOZ_ASSERT(!mChild && !mEmptyChild &&
             !(mBits & nsCachedStyleData::GetBitForSID(aSID)) &&
             !GetCachedStyleData(aSID),
             "This style should not have been computed");

  void* result;
  nsPresContext* presContext = PresContext();
  switch (aSID) {
#define UNIQUE_CASE(c_) \
    case eStyleStruct_##c_: \
      result = new (presContext) nsStyle##c_(presContext); \
      break;

  UNIQUE_CASE(Border)
  UNIQUE_CASE(Padding)

#undef UNIQUE_CASE

  default:
    NS_ERROR("Struct type not supported.");
    return nullptr;
  }

  // The new struct is owned by this style context, but that we don't
  // need to clear the bit in mBits because we've asserted that at the
  // top of this function.
  SetStyle(aSID, result);
  return result;
}

void
nsStyleContext::SetStyle(nsStyleStructID aSID, void* aStruct)
{
  MOZ_ASSERT(!mSource.IsServoComputedValues(),
             "Servo shouldn't cache style structs in the style context!");
  // This method should only be called from nsRuleNode!  It is not a public
  // method!

  NS_ASSERTION(aSID >= 0 && aSID < nsStyleStructID_Length, "out of bounds");

  // NOTE:  nsCachedStyleData::GetStyleData works roughly the same way.
  // See the comments there (in nsRuleNode.h) for more details about
  // what this is doing and why.

  void** dataSlot;
  if (nsCachedStyleData::IsReset(aSID)) {
    if (!mCachedResetData) {
      mCachedResetData = new (PresContext()) nsResetStyleData;
    }
    dataSlot = &mCachedResetData->mStyleStructs[aSID];
  } else {
    dataSlot = &mCachedInheritedData.mStyleStructs[aSID];
  }
  NS_ASSERTION(!*dataSlot || (mBits & nsCachedStyleData::GetBitForSID(aSID)),
               "Going to leak style data");
  *dataSlot = aStruct;
}

static bool
ShouldSuppressLineBreak(const nsStyleContext* aContext,
                        const nsStyleDisplay* aDisplay,
                        const nsStyleContext* aParentContext,
                        const nsStyleDisplay* aParentDisplay)
{
  // The display change should only occur for "in-flow" children
  if (aDisplay->IsOutOfFlowStyle()) {
    return false;
  }
  // Display value of any anonymous box should not be touched. In most
  // cases, anonymous boxes are actually not in ruby frame, but instead,
  // some other frame with a ruby display value. Non-element pseudos
  // which represents text frames, as well as ruby pseudos are excluded
  // because we still want to set the flag for them.
  if ((aContext->GetPseudoType() == CSSPseudoElementType::InheritingAnonBox ||
       aContext->GetPseudoType() == CSSPseudoElementType::NonInheritingAnonBox) &&
      !nsCSSAnonBoxes::IsNonElement(aContext->GetPseudo()) &&
      !RubyUtils::IsRubyPseudo(aContext->GetPseudo())) {
    return false;
  }
  if (aParentContext->ShouldSuppressLineBreak()) {
    // Line break suppressing bit is propagated to any children of
    // line participants, which include inline, contents, and inline
    // ruby boxes.
    if (aParentDisplay->mDisplay == mozilla::StyleDisplay::Inline ||
        aParentDisplay->mDisplay == mozilla::StyleDisplay::Contents ||
        aParentDisplay->mDisplay == mozilla::StyleDisplay::Ruby ||
        aParentDisplay->mDisplay == mozilla::StyleDisplay::RubyBaseContainer) {
      return true;
    }
  }
  // Any descendant of ruby level containers is non-breakable, but
  // the level containers themselves are breakable. We have to check
  // the container display type against all ruby display type here
  // because any of the ruby boxes could be anonymous.
  // Note that, when certain HTML tags, e.g. form controls, have ruby
  // level container display type, they could also escape from this flag
  // while they shouldn't. However, it is generally fine since they
  // won't usually break the assertion that there is no line break
  // inside ruby, because:
  // 1. their display types, the ruby level container types, are inline-
  //    outside, which means they won't cause any forced line break; and
  // 2. they never start an inline span, which means their children, if
  //    any, won't be able to break the line its ruby ancestor lays; and
  // 3. their parent frame is always a ruby content frame (due to
  //    anonymous ruby box generation), which makes line layout suppress
  //    any optional line break around this frame.
  // However, there is one special case which is BR tag, because it
  // directly affects the line layout. This case is handled by the BR
  // frame which checks the flag of its parent frame instead of itself.
  if ((aParentDisplay->IsRubyDisplayType() &&
       aDisplay->mDisplay != mozilla::StyleDisplay::RubyBaseContainer &&
       aDisplay->mDisplay != mozilla::StyleDisplay::RubyTextContainer) ||
      // Since ruby base and ruby text may exist themselves without any
      // non-anonymous frame outside, we should also check them.
      aDisplay->mDisplay == mozilla::StyleDisplay::RubyBase ||
      aDisplay->mDisplay == mozilla::StyleDisplay::RubyText) {
    return true;
  }
  return false;
}

// Flex & grid containers blockify their children.
//  "The display value of a flex item is blockified"
//    https://drafts.csswg.org/css-flexbox-1/#flex-items
//  "The display value of a grid item is blockified"
//    https://drafts.csswg.org/css-grid/#grid-items
static bool
ShouldBlockifyChildren(const nsStyleDisplay* aStyleDisp)
{
  auto displayVal = aStyleDisp->mDisplay;
  return mozilla::StyleDisplay::Flex == displayVal ||
    mozilla::StyleDisplay::InlineFlex == displayVal ||
    mozilla::StyleDisplay::Grid == displayVal ||
    mozilla::StyleDisplay::InlineGrid == displayVal;
}

void
nsStyleContext::SetStyleBits()
{
  // Here we set up various style bits for both the Gecko and Servo paths.
  // _Only_ change the bits here.  For fixups of the computed values, you can
  // add to ApplyStyleFixups in Gecko and StyleAdjuster as part of Servo's
  // cascade.

  // See if we have any text decorations.
  // First see if our parent has text decorations.  If our parent does, then we inherit the bit.
  if (mParent && mParent->HasTextDecorationLines()) {
    AddStyleBit(NS_STYLE_HAS_TEXT_DECORATION_LINES);
  } else {
    // We might have defined a decoration.
    if (StyleTextReset()->HasTextDecorationLines()) {
      AddStyleBit(NS_STYLE_HAS_TEXT_DECORATION_LINES);
    }
  }

  if ((mParent && mParent->HasPseudoElementData()) || IsPseudoElement()) {
    AddStyleBit(NS_STYLE_HAS_PSEUDO_ELEMENT_DATA);
  }

  // Set the NS_STYLE_IN_DISPLAY_NONE_SUBTREE bit
  const nsStyleDisplay* disp = StyleDisplay();
  if ((mParent && mParent->IsInDisplayNoneSubtree()) ||
      disp->mDisplay == mozilla::StyleDisplay::None) {
    AddStyleBit(NS_STYLE_IN_DISPLAY_NONE_SUBTREE);
  }

  // Mark text combined for text-combine-upright, as needed.
  if (mPseudoTag == nsCSSAnonBoxes::mozText && mParent &&
      mParent->StyleVisibility()->mWritingMode !=
        NS_STYLE_WRITING_MODE_HORIZONTAL_TB &&
      mParent->StyleText()->mTextCombineUpright ==
        NS_STYLE_TEXT_COMBINE_UPRIGHT_ALL) {
    AddStyleBit(NS_STYLE_IS_TEXT_COMBINED);
  }
}

void
nsStyleContext::ApplyStyleFixups(bool aSkipParentDisplayBasedStyleFixup)
{
  MOZ_ASSERT(!mSource.IsServoComputedValues(),
             "Can't do Gecko style fixups on Servo values");

#define GET_UNIQUE_STYLE_DATA(name_) \
  static_cast<nsStyle##name_*>(GetUniqueStyleData(eStyleStruct_##name_))

  // CSS Inline Layout Level 3 - 3.5 Sizing Initial Letters:
  // For an N-line drop initial in a Western script, the cap-height of the
  // letter needs to be (N – 1) times the line-height, plus the cap-height
  // of the surrounding text.
  if (mPseudoTag == nsCSSPseudoElements::firstLetter) {
    const nsStyleTextReset* textReset = StyleTextReset();
    if (textReset->mInitialLetterSize != 0.0f) {
      nsStyleContext* containerSC = mParent;
      const nsStyleDisplay* containerDisp = containerSC->StyleDisplay();
      while (containerDisp->mDisplay == mozilla::StyleDisplay::Contents) {
        if (!containerSC->GetParent()) {
          break;
        }
        containerSC = containerSC->GetParent();
        containerDisp = containerSC->StyleDisplay();
      }
      nscoord containerLH =
        ReflowInput::CalcLineHeight(nullptr, containerSC, NS_AUTOHEIGHT, 1.0f);
      RefPtr<nsFontMetrics> containerFM =
        nsLayoutUtils::GetFontMetricsForStyleContext(containerSC);
      MOZ_ASSERT(containerFM, "Should have fontMetrics!!");
      nscoord containerCH = containerFM->CapHeight();
      RefPtr<nsFontMetrics> firstLetterFM =
        nsLayoutUtils::GetFontMetricsForStyleContext(this);
      MOZ_ASSERT(firstLetterFM, "Should have fontMetrics!!");
      nscoord firstLetterCH = firstLetterFM->CapHeight();
      nsStyleFont* mutableStyleFont = GET_UNIQUE_STYLE_DATA(Font);
      float invCapHeightRatio =
        mutableStyleFont->mFont.size / NSCoordToFloat(firstLetterCH);
      mutableStyleFont->mFont.size =
        NSToCoordRound(((textReset->mInitialLetterSize - 1) * containerLH +
                        containerCH) *
                       invCapHeightRatio);
    }
  }

  // Change writing mode of text frame for text-combine-upright. We use
  // style structs of the parent to avoid triggering computation before
  // we change the writing mode.
  // It is safe to look at the parent's style because we are looking at
  // inherited properties, and ::-moz-text never matches any rules.
  if (mPseudoTag == nsCSSAnonBoxes::mozText && mParent &&
      mParent->StyleVisibility()->mWritingMode !=
        NS_STYLE_WRITING_MODE_HORIZONTAL_TB &&
      mParent->StyleText()->mTextCombineUpright ==
        NS_STYLE_TEXT_COMBINE_UPRIGHT_ALL) {
    MOZ_ASSERT(!PeekStyleVisibility(), "If StyleVisibility was already "
               "computed, some properties may have been computed "
               "incorrectly based on the old writing mode value");
    nsStyleVisibility* mutableVis = GET_UNIQUE_STYLE_DATA(Visibility);
    mutableVis->mWritingMode = NS_STYLE_WRITING_MODE_HORIZONTAL_TB;
  }

  // CSS 2.1 10.1: Propagate the root element's 'direction' to the ICB.
  // (PageContentFrame/CanvasFrame etc will inherit 'direction')
  if (mPseudoTag == nsCSSAnonBoxes::viewport) {
    nsPresContext* presContext = PresContext();
    mozilla::dom::Element* docElement = presContext->Document()->GetRootElement();
    if (docElement) {
      RefPtr<nsStyleContext> rootStyle =
        presContext->StyleSet()->AsGecko()->ResolveStyleFor(docElement, nullptr);
      auto dir = rootStyle->StyleVisibility()->mDirection;
      if (dir != StyleVisibility()->mDirection) {
        nsStyleVisibility* uniqueVisibility = GET_UNIQUE_STYLE_DATA(Visibility);
        uniqueVisibility->mDirection = dir;
      }
    }
  }

  // Correct tables.
  const nsStyleDisplay* disp = StyleDisplay();
  if (disp->mDisplay == mozilla::StyleDisplay::Table) {
    // -moz-center and -moz-right are used for HTML's alignment
    // This is covering the <div align="right"><table>...</table></div> case.
    // In this case, we don't want to inherit the text alignment into the table.
    const nsStyleText* text = StyleText();

    if (text->mTextAlign == NS_STYLE_TEXT_ALIGN_MOZ_LEFT ||
        text->mTextAlign == NS_STYLE_TEXT_ALIGN_MOZ_CENTER ||
        text->mTextAlign == NS_STYLE_TEXT_ALIGN_MOZ_RIGHT)
    {
      nsStyleText* uniqueText = GET_UNIQUE_STYLE_DATA(Text);
      uniqueText->mTextAlign = NS_STYLE_TEXT_ALIGN_START;
    }
  }

  // CSS2.1 section 9.2.4 specifies fixups for the 'display' property of
  // the root element.  We can't implement them in nsRuleNode because we
  // don't want to store all display structs that aren't 'block',
  // 'inline', or 'table' in the style context tree on the off chance
  // that the root element has its style reresolved later.  So do them
  // here if needed, by changing the style data, so that other code
  // doesn't get confused by looking at the style data.
  if (!mParent &&
      // We don't want to blockify various anon boxes that just happen to not
      // inherit from anything.  So restrict blockification only to actual
      // elements, the viewport (which should be block anyway, but in SVG
      // document's isn't because we lazy-load ua.css there), and the ::backdrop
      // pseudo-element.  This last is explicitly allowed to have any specified
      // display type in the spec, but computes to a blockified display type per
      // various provisions of
      // https://fullscreen.spec.whatwg.org/#new-stacking-layer
      (!mPseudoTag ||
       mPseudoTag == nsCSSAnonBoxes::viewport ||
       mPseudoTag == nsCSSPseudoElements::backdrop)) {
    auto displayVal = disp->mDisplay;
    if (displayVal != mozilla::StyleDisplay::Contents) {
      nsRuleNode::EnsureBlockDisplay(displayVal, true);
    } else {
      // http://dev.w3.org/csswg/css-display/#transformations
      // "... a display-outside of 'contents' computes to block-level
      //  on the root element."
      displayVal = mozilla::StyleDisplay::Block;
    }
    if (displayVal != disp->mDisplay) {
      nsStyleDisplay* mutable_display = GET_UNIQUE_STYLE_DATA(Display);
      disp = mutable_display;

      // If we're in this code, then mOriginalDisplay doesn't matter
      // for purposes of the cascade (because this nsStyleDisplay
      // isn't living in the ruletree anyway), and for determining
      // hypothetical boxes it's better to have mOriginalDisplay
      // matching mDisplay here.
      mutable_display->mOriginalDisplay = mutable_display->mDisplay =
        displayVal;
    }
  }

  // Adjust the "display" values of flex and grid items (but not for raw text
  // or placeholders). CSS3 Flexbox section 4 says:
  //   # The computed 'display' of a flex item is determined
  //   # by applying the table in CSS 2.1 Chapter 9.7.
  // ...which converts inline-level elements to their block-level equivalents.
  // Any block-level element directly contained by elements with ruby display
  // values are converted to their inline-level equivalents.
  if (!aSkipParentDisplayBasedStyleFixup && mParent) {
    // Skip display:contents ancestors to reach the potential container.
    // (If there are only display:contents ancestors between this node and
    // a flex/grid container ancestor, then this node is a flex/grid item, since
    // its parent *in the frame tree* will be the flex/grid container. So we treat
    // it like a flex/grid item here.)
    nsStyleContext* containerContext = mParent;
    const nsStyleDisplay* containerDisp = containerContext->StyleDisplay();
    while (containerDisp->mDisplay == mozilla::StyleDisplay::Contents) {
      if (!containerContext->GetParent()) {
        break;
      }
      containerContext = containerContext->GetParent();
      containerDisp = containerContext->StyleDisplay();
    }
    if (ShouldBlockifyChildren(containerDisp) &&
        !nsCSSAnonBoxes::IsNonElement(GetPseudo())) {
      // NOTE: Technically, we shouldn't modify the 'display' value of
      // positioned elements, since they aren't flex/grid items. However,
      // we don't need to worry about checking for that, because if we're
      // positioned, we'll have already been through a call to
      // EnsureBlockDisplay() in nsRuleNode, so this call here won't change
      // anything. So we're OK.
      auto displayVal = disp->mDisplay;
      nsRuleNode::EnsureBlockDisplay(displayVal);
      if (displayVal != disp->mDisplay) {
        NS_ASSERTION(!disp->IsAbsolutelyPositionedStyle(),
                     "We shouldn't be changing the display value of "
                     "positioned content (and we should have already "
                     "converted its display value to be block-level...)");
        nsStyleDisplay* mutable_display = GET_UNIQUE_STYLE_DATA(Display);
        disp = mutable_display;
        mutable_display->mDisplay = displayVal;
      }
    }
  }

  // Note: This must come after the blockification above, otherwise we fail
  // the grid-item-blockifying-001.html reftest.
  if (mParent && ::ShouldSuppressLineBreak(this, disp, mParent,
                                           mParent->StyleDisplay())) {
    mBits |= NS_STYLE_SUPPRESS_LINEBREAK;
    auto displayVal = disp->mDisplay;
    nsRuleNode::EnsureInlineDisplay(displayVal);
    if (displayVal != disp->mDisplay) {
      nsStyleDisplay* mutable_display = GET_UNIQUE_STYLE_DATA(Display);
      disp = mutable_display;
      mutable_display->mDisplay = displayVal;
    }
  }
  // Suppress border/padding of ruby level containers
  if (disp->mDisplay == mozilla::StyleDisplay::RubyBaseContainer ||
      disp->mDisplay == mozilla::StyleDisplay::RubyTextContainer) {
    CreateEmptyStyleData(eStyleStruct_Border);
    CreateEmptyStyleData(eStyleStruct_Padding);
  }
  if (disp->IsRubyDisplayType()) {
    // Per CSS Ruby spec section Bidi Reordering, for all ruby boxes,
    // the 'normal' and 'embed' values of 'unicode-bidi' should compute to
    // 'isolate', and 'bidi-override' should compute to 'isolate-override'.
    const nsStyleTextReset* textReset = StyleTextReset();
    uint8_t unicodeBidi = textReset->mUnicodeBidi;
    if (unicodeBidi == NS_STYLE_UNICODE_BIDI_NORMAL ||
        unicodeBidi == NS_STYLE_UNICODE_BIDI_EMBED) {
      unicodeBidi = NS_STYLE_UNICODE_BIDI_ISOLATE;
    } else if (unicodeBidi == NS_STYLE_UNICODE_BIDI_BIDI_OVERRIDE) {
      unicodeBidi = NS_STYLE_UNICODE_BIDI_ISOLATE_OVERRIDE;
    }
    if (unicodeBidi != textReset->mUnicodeBidi) {
      nsStyleTextReset* mutableTextReset = GET_UNIQUE_STYLE_DATA(TextReset);
      mutableTextReset->mUnicodeBidi = unicodeBidi;
    }
  }

  /*
   * According to https://drafts.csswg.org/css-writing-modes-3/#block-flow:
   *
   * If a box has a different block flow direction than its containing block:
   *   * If the box has a specified display of inline, its display computes
   *     to inline-block. [CSS21]
   *   ...etc.
   */
  if (disp->mDisplay == mozilla::StyleDisplay::Inline &&
      !nsCSSAnonBoxes::IsNonElement(mPseudoTag) &&
      mParent) {
    auto cbContext = mParent;
    while (cbContext->StyleDisplay()->mDisplay == mozilla::StyleDisplay::Contents) {
      cbContext = cbContext->mParent;
    }
    MOZ_ASSERT(cbContext, "the root context can't have display:contents");
    // We don't need the full mozilla::WritingMode value (incorporating dir
    // and text-orientation) here; just the writing-mode property is enough.
    if (StyleVisibility()->mWritingMode !=
          cbContext->StyleVisibility()->mWritingMode) {
      nsStyleDisplay* mutable_display = GET_UNIQUE_STYLE_DATA(Display);
      disp = mutable_display;
      mutable_display->mOriginalDisplay = mutable_display->mDisplay =
        mozilla::StyleDisplay::InlineBlock;
    }
  }

  // Compute User Interface style, to trigger loads of cursors
  StyleUserInterface();
#undef GET_UNIQUE_STYLE_DATA
}

template<class StyleContextLike>
nsChangeHint
nsStyleContext::CalcStyleDifferenceInternal(StyleContextLike* aNewContext,
                                            uint32_t* aEqualStructs,
                                            uint32_t* aSamePointerStructs)
{
  PROFILER_LABEL("nsStyleContext", "CalcStyleDifference",
    js::ProfileEntry::Category::CSS);

  static_assert(nsStyleStructID_Length <= 32,
                "aEqualStructs is not big enough");

  *aEqualStructs = 0;

  nsChangeHint hint = nsChangeHint(0);
  NS_ENSURE_TRUE(aNewContext, hint);
  // We must always ensure that we populate the structs on the new style
  // context that are filled in on the old context, so that if we get
  // two style changes in succession, the second of which causes a real
  // style change, the PeekStyleData doesn't return null (implying that
  // nobody ever looked at that struct's data).  In other words, we
  // can't skip later structs if we get a big change up front, because
  // we could later get a small change in one of those structs that we
  // don't want to miss.

  DebugOnly<uint32_t> structsFound = 0;

  // FIXME(heycam): We should just do the comparison in
  // nsStyleVariables::CalcDifference, returning NeutralChange if there are
  // any Variables differences.
  const nsStyleVariables* thisVariables = PeekStyleVariables();
  if (thisVariables) {
    structsFound |= NS_STYLE_INHERIT_BIT(Variables);
    const nsStyleVariables* otherVariables = aNewContext->StyleVariables();
    if (thisVariables->mVariables == otherVariables->mVariables) {
      *aEqualStructs |= NS_STYLE_INHERIT_BIT(Variables);
    }
  } else {
    *aEqualStructs |= NS_STYLE_INHERIT_BIT(Variables);
  }

  DebugOnly<int> styleStructCount = 1;  // count Variables already

  // Servo's optimization to stop the cascade when there are no style changes
  // that children need to be recascade for relies on comparing all of the
  // structs, not just those that are returned from PeekStyleData, although
  // if PeekStyleData does return null we still don't want to accumulate
  // any change hints for those structs.
  bool checkUnrequestedServoStructs = mSource.IsServoComputedValues();

#define DO_STRUCT_DIFFERENCE(struct_)                                         \
  PR_BEGIN_MACRO                                                              \
    const nsStyle##struct_* this##struct_ = PeekStyle##struct_();             \
    bool unrequestedStruct;                                                   \
    if (this##struct_) {                                                      \
      unrequestedStruct = false;                                              \
      structsFound |= NS_STYLE_INHERIT_BIT(struct_);                          \
    } else if (checkUnrequestedServoStructs) {                                \
      this##struct_ =                                                         \
        Servo_GetStyle##struct_(mSource.AsServoComputedValues());             \
      unrequestedStruct = true;                                               \
    } else {                                                                  \
      unrequestedStruct = false;                                              \
    }                                                                         \
    if (this##struct_) {                                                      \
      const nsStyle##struct_* other##struct_ = aNewContext->Style##struct_(); \
      if (this##struct_ == other##struct_) {                                  \
        /* The very same struct, so we know that there will be no */          \
        /* differences.                                           */          \
        *aEqualStructs |= NS_STYLE_INHERIT_BIT(struct_);                      \
      } else {                                                                \
        nsChangeHint difference =                                             \
          this##struct_->CalcDifference(*other##struct_ EXTRA_DIFF_ARGS);     \
        if (!unrequestedStruct) {                                             \
          hint |= difference;                                                 \
        }                                                                     \
        if (!difference) {                                                    \
          *aEqualStructs |= NS_STYLE_INHERIT_BIT(struct_);                    \
        }                                                                     \
      }                                                                       \
    } else {                                                                  \
      *aEqualStructs |= NS_STYLE_INHERIT_BIT(struct_);                        \
    }                                                                         \
    styleStructCount++;                                                       \
  PR_END_MACRO

  // FIXME: The order of these DO_STRUCT_DIFFERENCE calls is no longer
  // significant.  With a small amount of effort, we could replace them with a
  // #include "nsStyleStructList.h".
#define EXTRA_DIFF_ARGS /* nothing */
  DO_STRUCT_DIFFERENCE(Display);
  DO_STRUCT_DIFFERENCE(XUL);
  DO_STRUCT_DIFFERENCE(Column);
  DO_STRUCT_DIFFERENCE(Content);
  DO_STRUCT_DIFFERENCE(UserInterface);
  DO_STRUCT_DIFFERENCE(Visibility);
  DO_STRUCT_DIFFERENCE(Outline);
  DO_STRUCT_DIFFERENCE(TableBorder);
  DO_STRUCT_DIFFERENCE(Table);
  DO_STRUCT_DIFFERENCE(UIReset);
  DO_STRUCT_DIFFERENCE(Text);
  DO_STRUCT_DIFFERENCE(List);
  DO_STRUCT_DIFFERENCE(SVGReset);
  DO_STRUCT_DIFFERENCE(SVG);
#undef EXTRA_DIFF_ARGS
#define EXTRA_DIFF_ARGS , PeekStyleVisibility()
  DO_STRUCT_DIFFERENCE(Position);
#undef EXTRA_DIFF_ARGS
#define EXTRA_DIFF_ARGS /* nothing */
  DO_STRUCT_DIFFERENCE(Font);
  DO_STRUCT_DIFFERENCE(Margin);
  DO_STRUCT_DIFFERENCE(Padding);
  DO_STRUCT_DIFFERENCE(Border);
  DO_STRUCT_DIFFERENCE(TextReset);
  DO_STRUCT_DIFFERENCE(Effects);
  DO_STRUCT_DIFFERENCE(Background);
  DO_STRUCT_DIFFERENCE(Color);
#undef EXTRA_DIFF_ARGS

#undef DO_STRUCT_DIFFERENCE

  MOZ_ASSERT(styleStructCount == nsStyleStructID_Length,
             "missing a call to DO_STRUCT_DIFFERENCE");

#ifdef DEBUG
  #define STYLE_STRUCT(name_, callback_)                                      \
    MOZ_ASSERT(!!(structsFound & NS_STYLE_INHERIT_BIT(name_)) ==              \
               !!PeekStyle##name_(),                                          \
               "PeekStyleData results must not change in the middle of "      \
               "difference calculation.");
  #include "nsStyleStructList.h"
  #undef STYLE_STRUCT
#endif

  // We check for struct pointer equality here rather than as part of the
  // DO_STRUCT_DIFFERENCE calls, since those calls can result in structs
  // we previously examined and found to be null on this style context
  // getting computed by later DO_STRUCT_DIFFERENCE calls (which can
  // happen when the nsRuleNode::ComputeXXXData method looks up another
  // struct.)  This is important for callers in RestyleManager that
  // need to know the equality or not of the final set of cached struct
  // pointers.
  *aSamePointerStructs = 0;

#define STYLE_STRUCT(name_, callback_)                                        \
  {                                                                           \
    const nsStyle##name_* data = PeekStyle##name_();                          \
    if (!data || data == aNewContext->Style##name_()) {                       \
      *aSamePointerStructs |= NS_STYLE_INHERIT_BIT(name_);                    \
    }                                                                         \
  }
#include "nsStyleStructList.h"
#undef STYLE_STRUCT

  // Note that we do not check whether this->RelevantLinkVisited() !=
  // aNewContext->RelevantLinkVisited(); we don't need to since
  // nsCSSFrameConstructor::DoContentStateChanged always adds
  // nsChangeHint_RepaintFrame for NS_EVENT_STATE_VISITED changes (and
  // needs to, since HasStateDependentStyle probably doesn't work right
  // for NS_EVENT_STATE_VISITED).  Hopefully this doesn't actually
  // expose whether links are visited to performance tests since all
  // link coloring happens asynchronously at a time when it's hard for
  // the page to measure.
  // However, we do need to compute the larger of the changes that can
  // happen depending on whether the link is visited or unvisited, since
  // doing only the one that's currently appropriate would expose which
  // links are in history to easy performance measurement.  Therefore,
  // here, we add nsChangeHint_RepaintFrame hints (the maximum for
  // things that can depend on :visited) for the properties on which we
  // call GetVisitedDependentColor.
  nsStyleContext *thisVis = GetStyleIfVisited(),
                *otherVis = aNewContext->GetStyleIfVisited();
  if (!thisVis != !otherVis) {
    // One style context has a style-if-visited and the other doesn't.
    // Presume a difference.
    hint |= nsChangeHint_RepaintFrame;
  } else if (thisVis && !NS_IsHintSubset(nsChangeHint_RepaintFrame, hint)) {
    // Bug 1364484: Update comments here and potentially remove the assertion
    // below once we return a non-null visited context in CalcStyleDifference
    // using Servo values.  The approach is becoming quite similar to Gecko.
    // We'll handle visited style differently in servo. Assert against being
    // in the parallel traversal to avoid static analysis hazards when calling
    // StyleFoo() below.
    MOZ_ASSERT(!ServoStyleSet::IsInServoTraversal());

    // Both style contexts have a style-if-visited.
    bool change = false;

    // NB: Calling Peek on |this|, not |thisVis|, since callers may look
    // at a struct on |this| without looking at the same struct on
    // |thisVis| (including this function if we skip one of these checks
    // due to change being true already or due to the old style context
    // not having a style-if-visited), but not the other way around.
#define STYLE_FIELD(name_) thisVisStruct->name_ != otherVisStruct->name_
#define STYLE_STRUCT(name_, fields_)                                    \
    if (!change && PeekStyle##name_()) {                                \
      const nsStyle##name_* thisVisStruct = thisVis->Style##name_();    \
      const nsStyle##name_* otherVisStruct = otherVis->Style##name_();  \
      if (MOZ_FOR_EACH_SEPARATED(STYLE_FIELD, (||), (), fields_)) {     \
        change = true;                                                  \
      }                                                                 \
    }
#include "nsCSSVisitedDependentPropList.h"
#undef STYLE_STRUCT
#undef STYLE_FIELD

    if (change) {
      hint |= nsChangeHint_RepaintFrame;
    }
  }

  if (hint & nsChangeHint_UpdateContainingBlock) {
    // If a struct returned nsChangeHint_UpdateContainingBlock, that
    // means that one property's influence on whether we're a containing
    // block for abs-pos or fixed-pos elements has changed.  However, we
    // only need to return the hint if the overall computation of
    // whether we establish a containing block has changed.

    // This depends on data in nsStyleDisplay, nsStyleEffects and
    // nsStyleSVGReset, so we do it here.

    // Note that it's perhaps good for this test to be last because it
    // doesn't use Peek* functions to get the structs on the old
    // context.  But this isn't a big concern because these struct
    // getters should be called during frame construction anyway.
    if (ThreadsafeStyleDisplay()->IsAbsPosContainingBlockForAppropriateFrame(this) ==
        aNewContext->ThreadsafeStyleDisplay()->
          IsAbsPosContainingBlockForAppropriateFrame(aNewContext) &&
        ThreadsafeStyleDisplay()->IsFixedPosContainingBlockForAppropriateFrame(this) ==
        aNewContext->ThreadsafeStyleDisplay()->
          IsFixedPosContainingBlockForAppropriateFrame(aNewContext)) {
      // While some styles that cause the frame to be a containing block
      // has changed, the overall result hasn't.
      hint &= ~nsChangeHint_UpdateContainingBlock;
    }
  }

  MOZ_ASSERT(NS_IsHintSubset(hint, nsChangeHint_AllHints),
             "Added a new hint without bumping AllHints?");
  return hint & ~nsChangeHint_NeutralChange;
}

nsChangeHint
nsStyleContext::CalcStyleDifference(nsStyleContext* aNewContext,
                                    uint32_t* aEqualStructs,
                                    uint32_t* aSamePointerStructs)
{
  return CalcStyleDifferenceInternal(aNewContext,
                                     aEqualStructs,
                                     aSamePointerStructs);
}

class MOZ_STACK_CLASS FakeStyleContext
{
public:
  explicit FakeStyleContext(const ServoComputedValues* aComputedValues)
    : mComputedValues(aComputedValues) {}

  mozilla::NonOwningStyleContextSource StyleSource() const {
    return mozilla::NonOwningStyleContextSource(mComputedValues);
  }

  nsStyleContext* GetStyleIfVisited() {
    // Bug 1364484: Figure out what to do here for Stylo visited values.  We can
    // get the visited computed values:
    // RefPtr<ServoComputedValues> visitedComputedValues =
    //   Servo_ComputedValues_GetVisitedStyle(mComputedValues).Consume();
    // But what's the best way to create the nsStyleContext?
    return nullptr;
  }

  #define STYLE_STRUCT(name_, checkdata_cb_)                                  \
  const nsStyle##name_ * Style##name_() {                                     \
    return Servo_GetStyle##name_(mComputedValues);                            \
  }                                                                           \
  const nsStyle##name_ * ThreadsafeStyle##name_() {                           \
    return Servo_GetStyle##name_(mComputedValues);                            \
  }
  #include "nsStyleStructList.h"
  #undef STYLE_STRUCT

private:
  const ServoComputedValues* MOZ_NON_OWNING_REF mComputedValues;
};

nsChangeHint
nsStyleContext::CalcStyleDifference(const ServoComputedValues* aNewComputedValues,
                                    uint32_t* aEqualStructs,
                                    uint32_t* aSamePointerStructs)
{
  FakeStyleContext newContext(aNewComputedValues);
  return CalcStyleDifferenceInternal(&newContext,
                                     aEqualStructs,
                                     aSamePointerStructs);
}

void
nsStyleContext::EnsureSameStructsCached(nsStyleContext* aOldContext)
{
  // NOTE(emilio): We could do better here for stylo, where we only call
  // Style##name_() because we need to run FinishStyle, but otherwise this
  // is only a bitwise or.
  //
  // We could reduce the FFI traffic we do only doing it for structs that have
  // non-trivial FinishStyle.

#define STYLE_STRUCT(name_, checkdata_cb_)                                    \
  if (aOldContext->PeekStyle##name_()) {                                      \
    Style##name_();                                                           \
  }
#include "nsStyleStructList.h"
#undef STYLE_STRUCT

#ifdef DEBUG
  if (mSource.IsServoComputedValues()) {
    auto oldMask = aOldContext->mBits & NS_STYLE_INHERIT_MASK;
    auto newMask = mBits & NS_STYLE_INHERIT_MASK;
    MOZ_ASSERT((oldMask & newMask) == oldMask,
               "Should have at least as many structs computed as the "
               "old context!");
  }
#endif
}

#ifdef DEBUG
void nsStyleContext::List(FILE* out, int32_t aIndent, bool aListDescendants)
{
  nsAutoCString str;
  // Indent
  int32_t ix;
  for (ix = aIndent; --ix >= 0; ) {
    str.AppendLiteral("  ");
  }
  str.Append(nsPrintfCString("%p(%d) parent=%p ",
                             (void*)this, mRefCnt, (void *)mParent));
  if (mPseudoTag) {
    nsAutoString  buffer;
    mPseudoTag->ToString(buffer);
    AppendUTF16toUTF8(buffer, str);
    str.Append(' ');
  }

  if (mSource.IsServoComputedValues()) {
    fprintf_stderr(out, "%s{ServoComputedValues}\n", str.get());
  } else if (mSource.IsGeckoRuleNode()) {
    fprintf_stderr(out, "%s{\n", str.get());
    str.Truncate();
    nsRuleNode* ruleNode = mSource.AsGeckoRuleNode();
    while (ruleNode) {
      nsIStyleRule *styleRule = ruleNode->GetRule();
      if (styleRule) {
        styleRule->List(out, aIndent + 1);
      }
      ruleNode = ruleNode->GetParent();
    }
    for (ix = aIndent; --ix >= 0; ) {
      str.AppendLiteral("  ");
    }
    fprintf_stderr(out, "%s}\n", str.get());
  }
  else {
    fprintf_stderr(out, "%s{}\n", str.get());
  }

  if (aListDescendants) {
    if (nullptr != mChild) {
      nsStyleContext* child = mChild;
      do {
        child->List(out, aIndent + 1, aListDescendants);
        child = child->mNextSibling;
      } while (mChild != child);
    }
    if (nullptr != mEmptyChild) {
      nsStyleContext* child = mEmptyChild;
      do {
        child->List(out, aIndent + 1, aListDescendants);
        child = child->mNextSibling;
      } while (mEmptyChild != child);
    }
  }
}
#endif

// Overloaded new operator. Initializes the memory to 0 and relies on an arena
// (which comes from the presShell) to perform the allocation.
void*
nsStyleContext::operator new(size_t sz, nsPresContext* aPresContext)
{
  // Check the recycle list first.
  return aPresContext->PresShell()->
    AllocateByObjectID(eArenaObjectID_nsStyleContext, sz);
}

// Overridden to prevent the global delete from being called, since the memory
// came out of an nsIArena instead of the global delete operator's heap.
void
nsStyleContext::Destroy()
{
  // Get the pres context.
  RefPtr<nsPresContext> presContext = PresContext();

  // Call our destructor.
  this->~nsStyleContext();

  // Don't let the memory be freed, since it will be recycled
  // instead. Don't call the global operator delete.
  presContext->PresShell()->
    FreeByObjectID(eArenaObjectID_nsStyleContext, this);
}

already_AddRefed<nsStyleContext>
NS_NewStyleContext(nsStyleContext* aParentContext,
                   nsIAtom* aPseudoTag,
                   CSSPseudoElementType aPseudoType,
                   nsRuleNode* aRuleNode,
                   bool aSkipParentDisplayBasedStyleFixup)
{
  RefPtr<nsRuleNode> node = aRuleNode;
  RefPtr<nsStyleContext> context =
    new (aRuleNode->PresContext())
    nsStyleContext(aParentContext, aPseudoTag, aPseudoType, node.forget(),
                   aSkipParentDisplayBasedStyleFixup);
  return context.forget();
}

already_AddRefed<nsStyleContext>
NS_NewStyleContext(nsStyleContext* aParentContext,
                   nsPresContext* aPresContext,
                   nsIAtom* aPseudoTag,
                   CSSPseudoElementType aPseudoType,
                   already_AddRefed<ServoComputedValues> aComputedValues)
{
  RefPtr<nsStyleContext> context =
    new (aPresContext)
    nsStyleContext(aParentContext, aPresContext, aPseudoTag, aPseudoType,
                   Move(aComputedValues));
  return context.forget();
}

nsIPresShell*
nsStyleContext::Arena()
{
  return PresContext()->PresShell();
}

template<typename Func>
static nscolor
GetVisitedDependentColorInternal(nsStyleContext* aSc, Func aColorFunc)
{
  nscolor colors[2];
  colors[0] = aColorFunc(aSc);
  if (nsStyleContext* visitedStyle = aSc->GetStyleIfVisited()) {
    colors[1] = aColorFunc(visitedStyle);
    return nsStyleContext::
      CombineVisitedColors(colors, aSc->RelevantLinkVisited());
  }
  return colors[0];
}

static nscolor
ExtractColor(nsStyleContext* aContext, const nscolor& aColor)
{
  return aColor;
}

static nscolor
ExtractColor(nsStyleContext* aContext, const StyleComplexColor& aColor)
{
  return aContext->StyleColor()->CalcComplexColor(aColor);
}

static nscolor
ExtractColor(nsStyleContext* aContext, const nsStyleSVGPaint& aPaintServer)
{
  return aPaintServer.Type() == eStyleSVGPaintType_Color
    ? aPaintServer.GetColor() : NS_RGBA(0, 0, 0, 0);
}

#define STYLE_FIELD(struct_, field_) aField == &struct_::field_ ||
#define STYLE_STRUCT(name_, fields_)                                          \
  template<> nscolor                                                          \
  nsStyleContext::GetVisitedDependentColor(                                   \
    decltype(nsStyle##name_::MOZ_ARG_1 fields_) nsStyle##name_::* aField)     \
  {                                                                           \
    MOZ_ASSERT(MOZ_FOR_EACH(STYLE_FIELD, (nsStyle##name_,), fields_) false,   \
               "Getting visited-dependent color for a field in nsStyle"#name_ \
               " which is not listed in nsCSSVisitedDependentPropList.h");    \
    return GetVisitedDependentColorInternal(this,                             \
                                            [aField](nsStyleContext* sc) {    \
      return ExtractColor(sc, sc->Style##name_()->*aField);                   \
    });                                                                       \
  }
#include "nsCSSVisitedDependentPropList.h"
#undef STYLE_STRUCT
#undef STYLE_FIELD

struct ColorIndexSet {
  uint8_t colorIndex, alphaIndex;
};

static const ColorIndexSet gVisitedIndices[2] = { { 0, 0 }, { 1, 0 } };

/* static */ nscolor
nsStyleContext::CombineVisitedColors(nscolor *aColors, bool aLinkIsVisited)
{
  if (NS_GET_A(aColors[1]) == 0) {
    // If the style-if-visited is transparent, then just use the
    // unvisited style rather than using the (meaningless) color
    // components of the visited style along with a potentially
    // non-transparent alpha value.
    aLinkIsVisited = false;
  }

  // NOTE: We want this code to have as little timing dependence as
  // possible on whether this->RelevantLinkVisited() is true.
  const ColorIndexSet &set =
    gVisitedIndices[aLinkIsVisited ? 1 : 0];

  nscolor colorColor = aColors[set.colorIndex];
  nscolor alphaColor = aColors[set.alphaIndex];
  return NS_RGBA(NS_GET_R(colorColor), NS_GET_G(colorColor),
                 NS_GET_B(colorColor), NS_GET_A(alphaColor));
}

#ifdef DEBUG
/* static */ const char*
nsStyleContext::StructName(nsStyleStructID aSID)
{
  switch (aSID) {
#define STYLE_STRUCT(name_, checkdata_cb)                                     \
    case eStyleStruct_##name_:                                                \
      return #name_;
#include "nsStyleStructList.h"
#undef STYLE_STRUCT
    default:
      return "Unknown";
  }
}

/* static */ bool
nsStyleContext::LookupStruct(const nsACString& aName, nsStyleStructID& aResult)
{
  if (false)
    ;
#define STYLE_STRUCT(name_, checkdata_cb_)                                    \
  else if (aName.EqualsLiteral(#name_))                                       \
    aResult = eStyleStruct_##name_;
#include "nsStyleStructList.h"
#undef STYLE_STRUCT
  else
    return false;
  return true;
}
#endif

void
nsStyleContext::SwapStyleData(nsStyleContext* aNewContext, uint32_t aStructs)
{
  static_assert(nsStyleStructID_Length <= 32, "aStructs is not big enough");

  for (nsStyleStructID i = nsStyleStructID_Inherited_Start;
       i < nsStyleStructID_Inherited_Start + nsStyleStructID_Inherited_Count;
       i = nsStyleStructID(i + 1)) {
    uint32_t bit = nsCachedStyleData::GetBitForSID(i);
    if (!(aStructs & bit)) {
      continue;
    }
    void*& thisData = mCachedInheritedData.mStyleStructs[i];
    void*& otherData = aNewContext->mCachedInheritedData.mStyleStructs[i];
    if (mBits & bit) {
      if (thisData == otherData) {
        thisData = nullptr;
      }
    } else if (!(aNewContext->mBits & bit) && thisData && otherData) {
      std::swap(thisData, otherData);
    }
  }

  for (nsStyleStructID i = nsStyleStructID_Reset_Start;
       i < nsStyleStructID_Reset_Start + nsStyleStructID_Reset_Count;
       i = nsStyleStructID(i + 1)) {
    uint32_t bit = nsCachedStyleData::GetBitForSID(i);
    if (!(aStructs & bit)) {
      continue;
    }
    if (!mCachedResetData) {
      mCachedResetData = new (PresContext()) nsResetStyleData;
    }
    if (!aNewContext->mCachedResetData) {
      aNewContext->mCachedResetData = new (PresContext()) nsResetStyleData;
    }
    void*& thisData = mCachedResetData->mStyleStructs[i];
    void*& otherData = aNewContext->mCachedResetData->mStyleStructs[i];
    if (mBits & bit) {
      if (thisData == otherData) {
        thisData = nullptr;
      }
    } else if (!(aNewContext->mBits & bit) && thisData && otherData) {
      std::swap(thisData, otherData);
    }
  }
}

void
nsStyleContext::ClearCachedInheritedStyleDataOnDescendants(uint32_t aStructs)
{
  if (mChild) {
    nsStyleContext* child = mChild;
    do {
      child->DoClearCachedInheritedStyleDataOnDescendants(aStructs);
      child = child->mNextSibling;
    } while (mChild != child);
  }
  if (mEmptyChild) {
    nsStyleContext* child = mEmptyChild;
    do {
      child->DoClearCachedInheritedStyleDataOnDescendants(aStructs);
      child = child->mNextSibling;
    } while (mEmptyChild != child);
  }
}

void
nsStyleContext::DoClearCachedInheritedStyleDataOnDescendants(uint32_t aStructs)
{
  NS_ASSERTION(mFrameRefCnt == 0, "frame still referencing style context");
  for (nsStyleStructID i = nsStyleStructID_Inherited_Start;
       i < nsStyleStructID_Inherited_Start + nsStyleStructID_Inherited_Count;
       i = nsStyleStructID(i + 1)) {
    uint32_t bit = nsCachedStyleData::GetBitForSID(i);
    if (aStructs & bit) {
      if (!(mBits & bit) && mCachedInheritedData.mStyleStructs[i]) {
        aStructs &= ~bit;
      } else {
        mCachedInheritedData.mStyleStructs[i] = nullptr;
      }
    }
  }

  if (mCachedResetData) {
    for (nsStyleStructID i = nsStyleStructID_Reset_Start;
         i < nsStyleStructID_Reset_Start + nsStyleStructID_Reset_Count;
         i = nsStyleStructID(i + 1)) {
      uint32_t bit = nsCachedStyleData::GetBitForSID(i);
      if (aStructs & bit) {
        if (!(mBits & bit) && mCachedResetData->mStyleStructs[i]) {
          aStructs &= ~bit;
        } else {
          mCachedResetData->mStyleStructs[i] = nullptr;
        }
      }
    }
  }

  if (aStructs == 0) {
    return;
  }

  ClearCachedInheritedStyleDataOnDescendants(aStructs);
}

void
nsStyleContext::SetIneligibleForSharing()
{
  if (mBits & NS_STYLE_INELIGIBLE_FOR_SHARING) {
    return;
  }
  mBits |= NS_STYLE_INELIGIBLE_FOR_SHARING;
  if (mChild) {
    nsStyleContext* child = mChild;
    do {
      child->SetIneligibleForSharing();
      child = child->mNextSibling;
    } while (mChild != child);
  }
  if (mEmptyChild) {
    nsStyleContext* child = mEmptyChild;
    do {
      child->SetIneligibleForSharing();
      child = child->mNextSibling;
    } while (mEmptyChild != child);
  }
}

#ifdef RESTYLE_LOGGING
nsCString
nsStyleContext::GetCachedStyleDataAsString(uint32_t aStructs)
{
  nsCString structs;
  for (nsStyleStructID i = nsStyleStructID(0);
       i < nsStyleStructID_Length;
       i = nsStyleStructID(i + 1)) {
    if (aStructs & nsCachedStyleData::GetBitForSID(i)) {
      const void* data = GetCachedStyleData(i);
      if (!structs.IsEmpty()) {
        structs.Append(' ');
      }
      structs.AppendPrintf("%s=%p", StructName(i), data);
      if (HasCachedDependentStyleData(i)) {
        structs.AppendLiteral("(dependent)");
      } else {
        structs.AppendLiteral("(owned)");
      }
    }
  }
  return structs;
}

int32_t&
nsStyleContext::LoggingDepth()
{
  static int32_t depth = 0;
  return depth;
}

void
nsStyleContext::LogStyleContextTree(int32_t aLoggingDepth, uint32_t aStructs)
{
  LoggingDepth() = aLoggingDepth;
  LogStyleContextTree(true, aStructs);
}

void
nsStyleContext::LogStyleContextTree(bool aFirst, uint32_t aStructs)
{
  nsCString structs = GetCachedStyleDataAsString(aStructs);
  if (!structs.IsEmpty()) {
    structs.Append(' ');
  }

  nsCString pseudo;
  if (mPseudoTag) {
    nsAutoString pseudoTag;
    mPseudoTag->ToString(pseudoTag);
    AppendUTF16toUTF8(pseudoTag, pseudo);
    pseudo.Append(' ');
  }

  nsCString flags;
  if (IsStyleIfVisited()) {
    flags.AppendLiteral("IS_STYLE_IF_VISITED ");
  }
  if (HasChildThatUsesGrandancestorStyle()) {
    flags.AppendLiteral("CHILD_USES_GRANDANCESTOR_STYLE ");
  }
  if (IsShared()) {
    flags.AppendLiteral("IS_SHARED ");
  }

  nsCString parent;
  if (aFirst) {
    parent.AppendPrintf("parent=%p ", mParent.get());
  }

  LOG_RESTYLE("%p(%d) %s%s%s%s",
              this, mRefCnt,
              structs.get(), pseudo.get(), flags.get(), parent.get());

  LOG_RESTYLE_INDENT();

  if (nullptr != mChild) {
    nsStyleContext* child = mChild;
    do {
      child->LogStyleContextTree(false, aStructs);
      child = child->mNextSibling;
    } while (mChild != child);
  }
  if (nullptr != mEmptyChild) {
    nsStyleContext* child = mEmptyChild;
    do {
      child->LogStyleContextTree(false, aStructs);
      child = child->mNextSibling;
    } while (mEmptyChild != child);
  }
}
#endif

#ifdef DEBUG
/* static */ void
nsStyleContext::Initialize()
{
  Preferences::AddBoolVarCache(
      &sExpensiveStyleStructAssertionsEnabled,
      "layout.css.expensive-style-struct-assertions.enabled");
}
#endif
