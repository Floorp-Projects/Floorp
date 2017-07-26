/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/GeckoStyleContext.h"

#include "CSSVariableImageTable.h"
#include "nsStyleConsts.h"
#include "nsStyleStruct.h"
#include "nsPresContext.h"
#include "nsRuleNode.h"
#include "nsStyleContextInlines.h"
#include "nsIFrame.h"
#include "nsLayoutUtils.h"
#include "mozilla/ReflowInput.h"
#include "RubyUtils.h"

using namespace mozilla;

#ifdef DEBUG
// Whether to perform expensive assertions in the nsStyleContext destructor.
static bool sExpensiveStyleStructAssertionsEnabled;

/* static */ void
GeckoStyleContext::Initialize()
{
  Preferences::AddBoolVarCache(
      &sExpensiveStyleStructAssertionsEnabled,
      "layout.css.expensive-style-struct-assertions.enabled");
}
#endif

GeckoStyleContext::GeckoStyleContext(GeckoStyleContext* aParent,
                                     nsIAtom* aPseudoTag,
                                     CSSPseudoElementType aPseudoType,
                                     already_AddRefed<nsRuleNode> aRuleNode,
                                     bool aSkipParentDisplayBasedStyleFixup)
  : nsStyleContext(aParent, aPseudoTag, aPseudoType)
  , mCachedResetData(nullptr)
  , mRefCnt(0)
  , mChild(nullptr)
  , mEmptyChild(nullptr)
  , mRuleNode(Move(aRuleNode))
#ifdef DEBUG
  , mComputingStruct(nsStyleStructID_None)
#endif
{
  mBits |= NS_STYLE_CONTEXT_IS_GECKO;

  if (aParent) {
#ifdef DEBUG
    nsRuleNode *r1 = mParent->RuleNode(), *r2 = mRuleNode;
    while (r1->GetParent())
      r1 = r1->GetParent();
    while (r2->GetParent())
      r2 = r2->GetParent();
    NS_ASSERTION(r1 == r2, "must be in the same rule tree as parent");
#endif
  } else {
    PresContext()->PresShell()->StyleSet()->RootStyleContextAdded();
  }

  mRuleNode->SetUsedDirectly(); // before ApplyStyleFixups()!
  // FinishConstruction() calls AddChild which needs these
  // to be initialized!
  mNextSibling = this;
  mPrevSibling = this;

  FinishConstruction();
  ApplyStyleFixups(aSkipParentDisplayBasedStyleFixup);
}

// Overloaded new operator. Initializes the memory to 0 and relies on an arena
// (which comes from the presShell) to perform the allocation.
void*
GeckoStyleContext::operator new(size_t sz, nsPresContext* aPresContext)
{
  MOZ_ASSERT(sz == sizeof(GeckoStyleContext));
  // Check the recycle list first.
  return aPresContext->PresShell()->
    AllocateByObjectID(eArenaObjectID_GeckoStyleContext, sz);
}

// Overridden to prevent the global delete from being called, since the memory
// came out of an nsIArena instead of the global delete operator's heap.
void
GeckoStyleContext::Destroy()
{
  // Get the pres context.
  RefPtr<nsPresContext> presContext = PresContext();
  // Call our destructor.
  this->~GeckoStyleContext();
  // Don't let the memory be freed, since it will be recycled
  // instead. Don't call the global operator delete.
  presContext->PresShell()->
    FreeByObjectID(eArenaObjectID_GeckoStyleContext, this);
}

GeckoStyleContext::~GeckoStyleContext()
{
  nsPresContext *presContext = PresContext();
#ifdef DEBUG
  NS_ASSERTION(HasNoChildren(), "destructing context with children");
  if (sExpensiveStyleStructAssertionsEnabled) {
    // Assert that the style structs we are about to destroy are not referenced
    // anywhere else in the style context tree.  These checks are expensive,
    // which is why they are not enabled by default.
    GeckoStyleContext* root = this;
    while (root->GetParent()) {
      root = root->GetParent();
    }
    root->AssertStructsNotUsedElsewhere(this,
                                        std::numeric_limits<int32_t>::max());
  } else {
    // In DEBUG builds when the pref is not enabled, we perform a more limited
    // check just of the children of this style context.
    this->AssertStructsNotUsedElsewhere(this, 2);
  }

  nsStyleSet* geckoStyleSet = presContext->PresShell()->StyleSet()->GetAsGecko();
  NS_ASSERTION(!geckoStyleSet ||
               geckoStyleSet->GetRuleTree() == AsGecko()->RuleNode()->RuleTree() ||
               geckoStyleSet->IsInRuleTreeReconstruct(),
               "destroying style context from old rule tree too late");
#endif

  if (mParent) {
    mParent->AsGecko()->RemoveChild(this);
  } else {
    presContext->StyleSet()->RootStyleContextRemoved();
  }

  // Free up our data structs.
  DestroyCachedStructs(presContext);
  CSSVariableImageTable::RemoveAll(this);
}

void
GeckoStyleContext::AddChild(GeckoStyleContext* aChild)
{
  NS_ASSERTION(aChild->mPrevSibling == aChild &&
               aChild->mNextSibling == aChild,
               "child already in a child list");

  GeckoStyleContext **listPtr = aChild->mRuleNode->IsRoot() ? &mEmptyChild : &mChild;
  if (const nsRuleNode* source = aChild->mRuleNode) {
    if (source->IsRoot()) {
      listPtr = &mEmptyChild;
    }
  }

  // Explicitly dereference listPtr so that compiler doesn't have to know that mNextSibling
  // etc. don't alias with what ever listPtr points at.
  GeckoStyleContext *list = *listPtr;

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

void
GeckoStyleContext::MoveTo(GeckoStyleContext* aNewParent)
{
  MOZ_ASSERT(aNewParent != mParent);

  // This function shouldn't be getting called if the parents have different
  // values for some flags in mBits (unless the flag is also set on this style
  // context) because if that were the case we would need to recompute those
  // bits for |this|.

#define CHECK_FLAG(bit_) \
  MOZ_ASSERT((mParent->AsGecko()->mBits & (bit_)) ==                          \
               (aNewParent->mBits & (bit_)) || (mBits & (bit_)),              \
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
  auto* styleIfVisited = GetStyleIfVisited();
  MOZ_ASSERT(!styleIfVisited || styleIfVisited->mParent == mParent);

  if (mParent->HasChildThatUsesResetStyle()) {
    aNewParent->AddStyleBit(NS_STYLE_HAS_CHILD_THAT_USES_RESET_STYLE);
  }

  mParent->RemoveChild(this);
  mParent = aNewParent;
  mParent->AddChild(this);

  if (styleIfVisited) {
    styleIfVisited->mParent->RemoveChild(styleIfVisited);
    styleIfVisited->mParent = aNewParent;
    styleIfVisited->mParent->AddChild(styleIfVisited);
  }
}

void
GeckoStyleContext::RemoveChild(GeckoStyleContext* aChild)
{
  NS_PRECONDITION(nullptr != aChild && this == aChild->mParent, "bad argument");

  MOZ_ASSERT(aChild->mRuleNode, "child context should have rule node");
  GeckoStyleContext **list = aChild->mRuleNode->IsRoot() ? &mEmptyChild : &mChild;

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

#ifdef DEBUG
void
GeckoStyleContext::ListDescendants(FILE* out, int32_t aIndent)
{
  if (nullptr != mChild) {
    GeckoStyleContext* child = mChild;
    do {
      child->List(out, aIndent + 1, true);
      child = child->mNextSibling;
    } while (mChild != child);
  }
  if (nullptr != mEmptyChild) {
    GeckoStyleContext* child = mEmptyChild;
    do {
      child->List(out, aIndent + 1, true);
      child = child->mNextSibling;
    } while (mEmptyChild != child);
  }
}
#endif

void
GeckoStyleContext::ClearCachedInheritedStyleDataOnDescendants(uint32_t aStructs)
{
  if (mChild) {
    GeckoStyleContext* child = mChild;
    do {
      child->DoClearCachedInheritedStyleDataOnDescendants(aStructs);
      child = child->mNextSibling;
    } while (mChild != child);
  }
  if (mEmptyChild) {
    GeckoStyleContext* child = mEmptyChild;
    do {
      child->DoClearCachedInheritedStyleDataOnDescendants(aStructs);
      child = child->mNextSibling;
    } while (mEmptyChild != child);
  }
}

void
GeckoStyleContext::DoClearCachedInheritedStyleDataOnDescendants(uint32_t aStructs)
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

already_AddRefed<GeckoStyleContext>
GeckoStyleContext::FindChildWithRules(const nsIAtom* aPseudoTag,
                                   nsRuleNode* aSource,
                                   nsRuleNode* aSourceIfVisited,
                                   bool aRelevantLinkVisited)
{
  uint32_t threshold = 10; // The # of siblings we're willing to examine
                           // before just giving this whole thing up.

  RefPtr<GeckoStyleContext> result;
  MOZ_ASSERT(aSource);
  GeckoStyleContext *list = aSource->IsRoot() ? mEmptyChild : mChild;

  if (list) {
    GeckoStyleContext *child = list;
    do {
      if (child->RuleNode() == aSource &&
          child->mPseudoTag == aPseudoTag &&
          !child->IsStyleIfVisited() &&
          child->RelevantLinkVisited() == aRelevantLinkVisited) {
        bool match = false;
        if (aSourceIfVisited) {
          match = child->GetStyleIfVisited() &&
                  child->GetStyleIfVisited()->RuleNode() == aSourceIfVisited;
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



// This is an evil evil function, since it forces you to alloc your own separate copy of
// style data!  Do not use this function unless you absolutely have to!  You should avoid
// this at all costs! -dwh
void*
GeckoStyleContext::GetUniqueStyleData(const nsStyleStructID& aSID)
{
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
  UNIQUE_CASE(Position)
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
GeckoStyleContext::CreateEmptyStyleData(const nsStyleStructID& aSID)
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
GeckoStyleContext::SetIneligibleForSharing()
{
  if (mBits & NS_STYLE_INELIGIBLE_FOR_SHARING) {
    return;
  }
  mBits |= NS_STYLE_INELIGIBLE_FOR_SHARING;
  if (mChild) {
    GeckoStyleContext* child = mChild;
    do {
      child->SetIneligibleForSharing();
      child = child->mNextSibling;
    } while (mChild != child);
  }
  if (mEmptyChild) {
    GeckoStyleContext* child = mEmptyChild;
    do {
      child->SetIneligibleForSharing();
      child = child->mNextSibling;
    } while (mEmptyChild != child);
  }
}

#ifdef RESTYLE_LOGGING
nsCString
GeckoStyleContext::GetCachedStyleDataAsString(uint32_t aStructs)
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
GeckoStyleContext::LoggingDepth()
{
  static int32_t depth = 0;
  return depth;
}

void
GeckoStyleContext::LogStyleContextTree(int32_t aLoggingDepth, uint32_t aStructs)
{
  LoggingDepth() = aLoggingDepth;
  LogStyleContextTree(true, aStructs);
}

void
GeckoStyleContext::LogStyleContextTree(bool aFirst, uint32_t aStructs)
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
    GeckoStyleContext* child = mChild;
    do {
      child->LogStyleContextTree(false, aStructs);
      child = child->mNextSibling;
    } while (mChild != child);
  }
  if (nullptr != mEmptyChild) {
    GeckoStyleContext* child = mEmptyChild;
    do {
      child->LogStyleContextTree(false, aStructs);
      child = child->mNextSibling;
    } while (mEmptyChild != child);
  }
}
#endif

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

void
nsStyleContext::SetStyleBits()
{
  // Here we set up various style bits for both the Gecko and Servo paths.
  // _Only_ change the bits here.  For fixups of the computed values, you can
  // add to ApplyStyleFixups in Gecko and StyleAdjuster as part of Servo's
  // cascade.

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

#ifdef DEBUG
void
GeckoStyleContext::AssertStructsNotUsedElsewhere(
                                       GeckoStyleContext* aDestroyingContext,
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
    const GeckoStyleContext* child = mChild;
    do {
      child->AssertStructsNotUsedElsewhere(aDestroyingContext, aLevels - 1);
      child = child->mNextSibling;
    } while (child != mChild);
  }

  if (mEmptyChild) {
    const GeckoStyleContext* child = mEmptyChild;
    do {
      child->AssertStructsNotUsedElsewhere(aDestroyingContext, aLevels - 1);
      child = child->mNextSibling;
    } while (child != mEmptyChild);
  }
}
#endif


void
GeckoStyleContext::ApplyStyleFixups(bool aSkipParentDisplayBasedStyleFixup)
{
#define GET_UNIQUE_STYLE_DATA(name_) \
  static_cast<nsStyle##name_*>(GetUniqueStyleData(eStyleStruct_##name_))

  // CSS Inline Layout Level 3 - 3.5 Sizing Initial Letters:
  // For an N-line drop initial in a Western script, the cap-height of the
  // letter needs to be (N – 1) times the line-height, plus the cap-height
  // of the surrounding text.
  if (mPseudoTag == nsCSSPseudoElements::firstLetter) {
    const nsStyleTextReset* textReset = StyleTextReset();
    if (textReset->mInitialLetterSize != 0.0f) {
      GeckoStyleContext* containerSC = GetParent();
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

  // Fixup the "justify-items: auto" value based on our parent style here if
  // needed.
  //
  // Note that this only pulls a unique struct in the case the parent has the
  // "legacy" modifier (which is not the default), and the computed value would
  // change as a result.
  //
  // We check the parent first just to avoid unconditionally pulling the
  // nsStylePosition struct on every style context.
  if (mParent &&
      mParent->StylePosition()->mJustifyItems & NS_STYLE_JUSTIFY_LEGACY &&
      StylePosition()->mSpecifiedJustifyItems == NS_STYLE_JUSTIFY_AUTO &&
      StylePosition()->mJustifyItems != mParent->StylePosition()->mJustifyItems) {
    nsStylePosition* uniquePosition = GET_UNIQUE_STYLE_DATA(Position);
    uniquePosition->mJustifyItems = mParent->StylePosition()->mJustifyItems;
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
    GeckoStyleContext* containerContext = GetParent();
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
    auto cbContext = GetParent();
    while (cbContext->StyleDisplay()->mDisplay == mozilla::StyleDisplay::Contents) {
      cbContext = cbContext->GetParent();
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

bool
GeckoStyleContext::HasNoChildren() const
{
  return (nullptr == mChild) && (nullptr == mEmptyChild);
}

void
GeckoStyleContext::SetStyle(nsStyleStructID aSID, void* aStruct)
{
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


const void*
GeckoStyleContext::StyleData(nsStyleStructID aSID)
{
  const void* cachedData = GetCachedStyleData(aSID);
  if (cachedData)
    return cachedData; // We have computed data stored on this node in the context tree.
  // Our style source will take care of it for us.
  const void* newData = AsGecko()->RuleNode()->GetStyleData(aSID, this->AsGecko(), true);
  if (!nsCachedStyleData::IsReset(aSID)) {
    // always cache inherited data on the style context; the rule
    // node set the bit in mBits for us if needed.
    mCachedInheritedData.mStyleStructs[aSID] = const_cast<void*>(newData);
  }

  return newData;
}

void
GeckoStyleContext::DestroyCachedStructs(nsPresContext* aPresContext)
{
  mCachedInheritedData.DestroyStructs(mBits, aPresContext);
  if (mCachedResetData) {
    mCachedResetData->Destroy(mBits, aPresContext);
  }
}


void
GeckoStyleContext::SwapStyleData(GeckoStyleContext* aNewContext, uint32_t aStructs)
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
GeckoStyleContext::SetStyleIfVisited(already_AddRefed<GeckoStyleContext> aStyleIfVisited)
{
  MOZ_ASSERT(!IsStyleIfVisited(), "this context is not visited data");
  NS_ASSERTION(!mStyleIfVisited, "should only be set once");

  mStyleIfVisited = aStyleIfVisited;

  MOZ_ASSERT(mStyleIfVisited->IsStyleIfVisited(),
             "other context is visited data");
  MOZ_ASSERT(!mStyleIfVisited->GetStyleIfVisited(),
             "other context does not have visited data");
  NS_ASSERTION(GetStyleIfVisited()->GetPseudo() == GetPseudo(),
               "pseudo tag mismatch");
  if (GetParent() && GetParent()->GetStyleIfVisited()) {
    MOZ_ASSERT(GetStyleIfVisited()->GetParent() ==
                   GetParent()->GetStyleIfVisited() ||
                 GetStyleIfVisited()->GetParent() ==
                   GetParent(),
                 "parent mismatch");
  } else {
    MOZ_ASSERT(GetStyleIfVisited()->GetParent() ==
                   GetParent(),
                 "parent mismatch");
  }
}

