/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
/* the interface (to internal code) for retrieving computed style data */

#include "CSSVariableImageTable.h"
#include "mozilla/DebugOnly.h"

#include "nsCSSAnonBoxes.h"
#include "nsStyleConsts.h"
#include "nsString.h"
#include "nsPresContext.h"
#include "nsIStyleRule.h"

#include "nsCOMPtr.h"
#include "nsStyleSet.h"
#include "nsIPresShell.h"

#include "nsRuleNode.h"
#include "nsStyleContext.h"
#include "mozilla/StyleAnimationValue.h"
#include "GeckoProfiler.h"
#include "nsIDocument.h"
#include "nsPrintfCString.h"
#include "mozilla/Preferences.h"

#ifdef DEBUG
// #define NOISY_DEBUG
#endif

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
                               nsIAtom* aPseudoTag,
                               nsCSSPseudoElements::Type aPseudoType,
                               nsRuleNode* aRuleNode,
                               bool aSkipParentDisplayBasedStyleFixup)
  : mParent(aParent)
  , mChild(nullptr)
  , mEmptyChild(nullptr)
  , mPseudoTag(aPseudoTag)
  , mRuleNode(aRuleNode)
  , mCachedResetData(nullptr)
  , mBits(((uint64_t)aPseudoType) << NS_STYLE_CONTEXT_TYPE_SHIFT)
  , mRefCnt(0)
#ifdef DEBUG
  , mFrameRefCnt(0)
  , mComputingStruct(nsStyleStructID_None)
#endif
{
  // This check has to be done "backward", because if it were written the
  // more natural way it wouldn't fail even when it needed to.
  static_assert((UINT64_MAX >> NS_STYLE_CONTEXT_TYPE_SHIFT) >=
                nsCSSPseudoElements::ePseudo_MAX,
                "pseudo element bits no longer fit in a uint64_t");
  MOZ_ASSERT(aRuleNode);

#ifdef DEBUG
  static_assert(MOZ_ARRAY_LENGTH(nsStyleContext::sDependencyTable)
                  == nsStyleStructID_Length,
                "Number of items in dependency table doesn't match IDs");
#endif

  mNextSibling = this;
  mPrevSibling = this;
  if (mParent) {
    mParent->AddRef();
    mParent->AddChild(this);
#ifdef DEBUG
    nsRuleNode *r1 = mParent->RuleNode(), *r2 = aRuleNode;
    while (r1->GetParent())
      r1 = r1->GetParent();
    while (r2->GetParent())
      r2 = r2->GetParent();
    NS_ASSERTION(r1 == r2, "must be in the same rule tree as parent");
#endif
  }

  mRuleNode->AddRef();
  mRuleNode->SetUsedDirectly(); // before ApplyStyleFixups()!

  ApplyStyleFixups(aSkipParentDisplayBasedStyleFixup);

  #define eStyleStruct_LastItem (nsStyleStructID_Length - 1)
  NS_ASSERTION(NS_STYLE_INHERIT_MASK & NS_STYLE_INHERIT_BIT(LastItem),
               "NS_STYLE_INHERIT_MASK must be bigger, and other bits shifted");
  #undef eStyleStruct_LastItem
}

nsStyleContext::~nsStyleContext()
{
  NS_ASSERTION((nullptr == mChild) && (nullptr == mEmptyChild), "destructing context with children");

  nsPresContext *presContext = mRuleNode->PresContext();
  nsStyleSet* styleSet = presContext->PresShell()->StyleSet();

  NS_ASSERTION(styleSet->GetRuleTree() == mRuleNode->RuleTree() ||
               styleSet->IsInRuleTreeReconstruct(),
               "destroying style context from old rule tree too late");

#ifdef DEBUG
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

  mRuleNode->Release();

  styleSet->NotifyStyleContextDestroyed(this);

  if (mParent) {
    mParent->RemoveChild(this);
    mParent->Release();
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
      PresContext()->Document()->GetURL(url);                                  \
      printf_stderr("  in %s\n", NS_ConvertUTF16toUTF8(url).get());            \
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
          PresContext()->Document()->GetURL(url);                              \
          printf_stderr("  in %s\n", NS_ConvertUTF16toUTF8(url).get());        \
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

  nsStyleContext **listPtr = aChild->mRuleNode->IsRoot() ? &mEmptyChild : &mChild;
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

  nsStyleContext **list = aChild->mRuleNode->IsRoot() ? &mEmptyChild : &mChild;

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
  // values for some flags in mBits, because if that were the case we would need
  // to recompute those bits for |this|. (TODO: add more flags to |mask|.)
  DebugOnly<uint64_t> mask = NS_STYLE_IN_DISPLAY_NONE_SUBTREE;
  MOZ_ASSERT((mParent->mBits & mask) == (aNewParent->mBits & mask));

  // Assertions checking for visited style are just to avoid some tricky
  // cases we can't be bothered handling at the moment.
  MOZ_ASSERT(!IsStyleIfVisited());
  MOZ_ASSERT(!mParent->IsStyleIfVisited());
  MOZ_ASSERT(!aNewParent->IsStyleIfVisited());
  MOZ_ASSERT(!mStyleIfVisited || mStyleIfVisited->mParent == mParent);

  nsStyleContext* oldParent = mParent;

  if (oldParent->HasChildThatUsesResetStyle()) {
    aNewParent->AddStyleBit(NS_STYLE_HAS_CHILD_THAT_USES_RESET_STYLE);
  }

  aNewParent->AddRef();
  mParent->RemoveChild(this);
  mParent = aNewParent;
  mParent->AddChild(this);
  oldParent->Release();

  if (mStyleIfVisited) {
    oldParent = mStyleIfVisited->mParent;
    aNewParent->AddRef();
    mStyleIfVisited->mParent->RemoveChild(mStyleIfVisited);
    mStyleIfVisited->mParent = aNewParent;
    mStyleIfVisited->mParent->AddChild(mStyleIfVisited);
    oldParent->Release();
  }
}

already_AddRefed<nsStyleContext>
nsStyleContext::FindChildWithRules(const nsIAtom* aPseudoTag, 
                                   nsRuleNode* aRuleNode,
                                   nsRuleNode* aRulesIfVisited,
                                   bool aRelevantLinkVisited)
{
  uint32_t threshold = 10; // The # of siblings we're willing to examine
                           // before just giving this whole thing up.

  nsRefPtr<nsStyleContext> result;
  nsStyleContext *list = aRuleNode->IsRoot() ? mEmptyChild : mChild;

  if (list) {
    nsStyleContext *child = list;
    do {
      if (child->mRuleNode == aRuleNode &&
          child->mPseudoTag == aPseudoTag &&
          !child->IsStyleIfVisited() &&
          child->RelevantLinkVisited() == aRelevantLinkVisited) {
        bool match = false;
        if (aRulesIfVisited) {
          match = child->GetStyleIfVisited() &&
                  child->GetStyleIfVisited()->mRuleNode == aRulesIfVisited;
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

/* static */ bool
nsStyleContext::ListContainsStyleContextThatUsesGrandancestorStyle(const nsStyleContext* aHead)
{
  if (aHead) {
    const nsStyleContext* child = aHead;
    do {
      if (child->UsesGrandancestorStyle()) {
        return true;
      }
      child = child->mNextSibling;
    } while (child != aHead);
  }

  return false;
}

bool
nsStyleContext::HasChildThatUsesGrandancestorStyle() const
{
  return ListContainsStyleContextThatUsesGrandancestorStyle(mEmptyChild) ||
         ListContainsStyleContextThatUsesGrandancestorStyle(mChild);
}

const void* nsStyleContext::GetCachedStyleData(nsStyleStructID aSID)
{
  const void* cachedData;
  if (nsCachedStyleData::IsReset(aSID)) {
    if (mCachedResetData) {
      cachedData = mCachedResetData->mStyleStructs[aSID];
    } else {
      cachedData = nullptr;
    }
  } else {
    cachedData = mCachedInheritedData.mStyleStructs[aSID];
  }
  return cachedData;
}

const void* nsStyleContext::StyleData(nsStyleStructID aSID)
{
  const void* cachedData = GetCachedStyleData(aSID);
  if (cachedData)
    return cachedData; // We have computed data stored on this node in the context tree.
  return mRuleNode->GetStyleData(aSID, this, true); // Our rule node will take care of it for us.
}

// This is an evil evil function, since it forces you to alloc your own separate copy of
// style data!  Do not use this function unless you absolutely have to!  You should avoid
// this at all costs! -dwh
void* 
nsStyleContext::GetUniqueStyleData(const nsStyleStructID& aSID)
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
#define UNIQUE_CASE(c_, ...) \
    case eStyleStruct_##c_: \
      result = new (presContext) nsStyle##c_(__VA_ARGS__); \
      break;

  UNIQUE_CASE(Border, presContext)
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
  // This method should only be called from nsRuleNode!  It is not a public
  // method!
  
  NS_ASSERTION(aSID >= 0 && aSID < nsStyleStructID_Length, "out of bounds");

  // NOTE:  nsCachedStyleData::GetStyleData works roughly the same way.
  // See the comments there (in nsRuleNode.h) for more details about
  // what this is doing and why.

  void** dataSlot;
  if (nsCachedStyleData::IsReset(aSID)) {
    if (!mCachedResetData) {
      mCachedResetData = new (mRuleNode->PresContext()) nsResetStyleData;
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
ShouldSuppressLineBreak(const nsStyleDisplay* aStyleDisplay,
                        const nsStyleContext* aContainerContext,
                        const nsStyleDisplay* aContainerDisplay)
{
  // The display change should only occur for "in-flow" children
  if (aStyleDisplay->IsOutOfFlowStyle()) {
    return false;
  }
  if (aContainerContext->ShouldSuppressLineBreak()) {
    // Line break suppressing bit is propagated to any children of line
    // participants, which include inline and inline ruby boxes.
    if (aContainerDisplay->mDisplay == NS_STYLE_DISPLAY_INLINE ||
        aContainerDisplay->mDisplay == NS_STYLE_DISPLAY_RUBY ||
        aContainerDisplay->mDisplay == NS_STYLE_DISPLAY_RUBY_BASE_CONTAINER) {
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
  if ((aContainerDisplay->IsRubyDisplayType() &&
       aStyleDisplay->mDisplay != NS_STYLE_DISPLAY_RUBY_BASE_CONTAINER &&
       aStyleDisplay->mDisplay != NS_STYLE_DISPLAY_RUBY_TEXT_CONTAINER) ||
      // Since ruby base and ruby text may exist themselves without any
      // non-anonymous frame outside, we should also check them.
      aStyleDisplay->mDisplay == NS_STYLE_DISPLAY_RUBY_BASE ||
      aStyleDisplay->mDisplay == NS_STYLE_DISPLAY_RUBY_TEXT) {
    return true;
  }
  return false;
}

void
nsStyleContext::ApplyStyleFixups(bool aSkipParentDisplayBasedStyleFixup)
{
  // See if we have any text decorations.
  // First see if our parent has text decorations.  If our parent does, then we inherit the bit.
  if (mParent && mParent->HasTextDecorationLines()) {
    mBits |= NS_STYLE_HAS_TEXT_DECORATION_LINES;
  } else {
    // We might have defined a decoration.
    const nsStyleTextReset* text = StyleTextReset();
    uint8_t decorationLine = text->mTextDecorationLine;
    if (decorationLine != NS_STYLE_TEXT_DECORATION_LINE_NONE &&
        decorationLine != NS_STYLE_TEXT_DECORATION_LINE_OVERRIDE_ALL) {
      mBits |= NS_STYLE_HAS_TEXT_DECORATION_LINES;
    }
  }

  if ((mParent && mParent->HasPseudoElementData()) || mPseudoTag) {
    mBits |= NS_STYLE_HAS_PSEUDO_ELEMENT_DATA;
  }

  // CSS 2.1 10.1: Propagate the root element's 'direction' to the ICB.
  // (PageContentFrame/CanvasFrame etc will inherit 'direction')
  if (mPseudoTag == nsCSSAnonBoxes::viewport) {
    nsPresContext* presContext = PresContext();
    mozilla::dom::Element* docElement = presContext->Document()->GetRootElement();
    if (docElement) {
      nsRefPtr<nsStyleContext> rootStyle =
        presContext->StyleSet()->ResolveStyleFor(docElement, nullptr);
      auto dir = rootStyle->StyleVisibility()->mDirection;
      if (dir != StyleVisibility()->mDirection) {
        nsStyleVisibility* uniqueVisibility =
          (nsStyleVisibility*)GetUniqueStyleData(eStyleStruct_Visibility);
        uniqueVisibility->mDirection = dir;
      }
    }
  }

  // Correct tables.
  const nsStyleDisplay* disp = StyleDisplay();
  if (disp->mDisplay == NS_STYLE_DISPLAY_TABLE) {
    // -moz-center and -moz-right are used for HTML's alignment
    // This is covering the <div align="right"><table>...</table></div> case.
    // In this case, we don't want to inherit the text alignment into the table.
    const nsStyleText* text = StyleText();
    
    if (text->mTextAlign == NS_STYLE_TEXT_ALIGN_MOZ_CENTER ||
        text->mTextAlign == NS_STYLE_TEXT_ALIGN_MOZ_RIGHT)
    {
      nsStyleText* uniqueText = (nsStyleText*)GetUniqueStyleData(eStyleStruct_Text);
      uniqueText->mTextAlign = NS_STYLE_TEXT_ALIGN_DEFAULT;
    }
  }

  // CSS2.1 section 9.2.4 specifies fixups for the 'display' property of
  // the root element.  We can't implement them in nsRuleNode because we
  // don't want to store all display structs that aren't 'block',
  // 'inline', or 'table' in the style context tree on the off chance
  // that the root element has its style reresolved later.  So do them
  // here if needed, by changing the style data, so that other code
  // doesn't get confused by looking at the style data.
  if (!mParent) {
    uint8_t displayVal = disp->mDisplay;
    if (displayVal != NS_STYLE_DISPLAY_CONTENTS) {
      nsRuleNode::EnsureBlockDisplay(displayVal, true);
    } else {
      // http://dev.w3.org/csswg/css-display/#transformations
      // "... a display-outside of 'contents' computes to block-level
      //  on the root element."
      displayVal = NS_STYLE_DISPLAY_BLOCK;
    }
    if (displayVal != disp->mDisplay) {
      nsStyleDisplay *mutable_display =
        static_cast<nsStyleDisplay*>(GetUniqueStyleData(eStyleStruct_Display));

      // If we're in this code, then mOriginalDisplay doesn't matter
      // for purposes of the cascade (because this nsStyleDisplay
      // isn't living in the ruletree anyway), and for determining
      // hypothetical boxes it's better to have mOriginalDisplay
      // matching mDisplay here.
      mutable_display->mOriginalDisplay = mutable_display->mDisplay =
        displayVal;
    }
  }

  // Adjust the "display" values of flex and grid items (but not for raw text,
  // placeholders, or table-parts). CSS3 Flexbox section 4 says:
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
    while (containerDisp->mDisplay == NS_STYLE_DISPLAY_CONTENTS) {
      if (!containerContext->GetParent()) {
        break;
      }
      containerContext = containerContext->GetParent();
      containerDisp = containerContext->StyleDisplay();
    }
    if (containerDisp->IsFlexOrGridDisplayType() &&
        GetPseudo() != nsCSSAnonBoxes::mozNonElement) {
      uint8_t displayVal = disp->mDisplay;
      // Skip table parts.
      // NOTE: This list needs to be kept in sync with
      // nsCSSFrameConstructor::FindDisplayData() -- specifically,
      // this should be the list of display-values that returns
      // FCDATA_DESIRED_PARENT_TYPE_TO_BITS from that method.
      if (NS_STYLE_DISPLAY_TABLE_CAPTION      != displayVal &&
          NS_STYLE_DISPLAY_TABLE_ROW_GROUP    != displayVal &&
          NS_STYLE_DISPLAY_TABLE_HEADER_GROUP != displayVal &&
          NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP != displayVal &&
          NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP != displayVal &&
          NS_STYLE_DISPLAY_TABLE_COLUMN       != displayVal &&
          NS_STYLE_DISPLAY_TABLE_ROW          != displayVal &&
          NS_STYLE_DISPLAY_TABLE_CELL         != displayVal) {

        // NOTE: Technically, we shouldn't modify the 'display' value of
        // positioned elements, since they aren't flex/grid items. However,
        // we don't need to worry about checking for that, because if we're
        // positioned, we'll have already been through a call to
        // EnsureBlockDisplay() in nsRuleNode, so this call here won't change
        // anything. So we're OK.
        nsRuleNode::EnsureBlockDisplay(displayVal);
        if (displayVal != disp->mDisplay) {
          NS_ASSERTION(!disp->IsAbsolutelyPositionedStyle(),
                       "We shouldn't be changing the display value of "
                       "positioned content (and we should have already "
                       "converted its display value to be block-level...)");
          nsStyleDisplay *mutable_display =
            static_cast<nsStyleDisplay*>(GetUniqueStyleData(eStyleStruct_Display));
          mutable_display->mDisplay = displayVal;
        }
      }
    }

    if (::ShouldSuppressLineBreak(disp, containerContext, containerDisp)) {
      mBits |= NS_STYLE_SUPPRESS_LINEBREAK;
      uint8_t displayVal = disp->mDisplay;
      nsRuleNode::EnsureInlineDisplay(displayVal);
      if (displayVal != disp->mDisplay) {
        nsStyleDisplay *mutable_display =
          static_cast<nsStyleDisplay*>(GetUniqueStyleData(eStyleStruct_Display));
        mutable_display->mDisplay = displayVal;
      }
    }
  }

  // Set the NS_STYLE_IN_DISPLAY_NONE_SUBTREE bit
  if ((mParent && mParent->IsInDisplayNoneSubtree()) ||
      disp->mDisplay == NS_STYLE_DISPLAY_NONE) {
    mBits |= NS_STYLE_IN_DISPLAY_NONE_SUBTREE;
  }

  // Suppress border/padding of ruby level containers
  if (disp->mDisplay == NS_STYLE_DISPLAY_RUBY_BASE_CONTAINER ||
      disp->mDisplay == NS_STYLE_DISPLAY_RUBY_TEXT_CONTAINER) {
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
    } else if (unicodeBidi == NS_STYLE_UNICODE_BIDI_OVERRIDE) {
      unicodeBidi = NS_STYLE_UNICODE_BIDI_ISOLATE_OVERRIDE;
    }
    if (unicodeBidi != textReset->mUnicodeBidi) {
      auto mutableTextReset = static_cast<nsStyleTextReset*>(
        GetUniqueStyleData(eStyleStruct_TextReset));
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
  if (disp->mDisplay == NS_STYLE_DISPLAY_INLINE && mParent) {
    // We don't need the full mozilla::WritingMode value (incorporating dir
    // and text-orientation) here; just the writing-mode property is enough.
    if (StyleVisibility()->mWritingMode !=
        mParent->StyleVisibility()->mWritingMode) {
      nsStyleDisplay *mutable_display =
        static_cast<nsStyleDisplay*>(GetUniqueStyleData(eStyleStruct_Display));
      mutable_display->mOriginalDisplay = mutable_display->mDisplay =
        NS_STYLE_DISPLAY_INLINE_BLOCK;
    }
  }

  // Compute User Interface style, to trigger loads of cursors
  StyleUserInterface();
}

nsChangeHint
nsStyleContext::CalcStyleDifference(nsStyleContext* aOther,
                                    nsChangeHint aParentHintsNotHandledForDescendants,
                                    uint32_t* aEqualStructs,
                                    uint32_t* aSamePointerStructs)
{
  PROFILER_LABEL("nsStyleContext", "CalcStyleDifference",
    js::ProfileEntry::Category::CSS);

  MOZ_ASSERT(NS_IsHintSubset(aParentHintsNotHandledForDescendants,
                             nsChangeHint_Hints_NotHandledForDescendants),
             "caller is passing inherited hints, but shouldn't be");

  static_assert(nsStyleStructID_Length <= 32,
                "aEqualStructs is not big enough");

  *aEqualStructs = 0;

  nsChangeHint hint = NS_STYLE_HINT_NONE;
  NS_ENSURE_TRUE(aOther, hint);
  // We must always ensure that we populate the structs on the new style
  // context that are filled in on the old context, so that if we get
  // two style changes in succession, the second of which causes a real
  // style change, the PeekStyleData doesn't return null (implying that
  // nobody ever looked at that struct's data).  In other words, we
  // can't skip later structs if we get a big change up front, because
  // we could later get a small change in one of those structs that we
  // don't want to miss.

  // If our rule nodes are the same, then any differences in style data
  // are already accounted for by differences on ancestors.  We know
  // this because CalcStyleDifference is always called on two style
  // contexts that point to the same element, so we know that our
  // position in the style context tree is the same and our position in
  // the rule node tree is also the same.
  // However, if there were noninherited style change hints on the
  // parent, we might produce these same noninherited hints on this
  // style context's frame due to 'inherit' values, so we do need to
  // compare.
  // (Things like 'em' units are handled by the change hint produced
  // by font-size changing, so we don't need to worry about them like
  // we worry about 'inherit' values.)
  bool compare = mRuleNode != aOther->mRuleNode;

  // If we had any change in variable values, then we'll need to examine
  // all of the other style structs too, even if the new style context has
  // the same rule node as the old one.
  const nsStyleVariables* thisVariables = PeekStyleVariables();
  if (thisVariables) {
    const nsStyleVariables* otherVariables = aOther->StyleVariables();
    if (thisVariables->mVariables == otherVariables->mVariables) {
      *aEqualStructs |= nsCachedStyleData::GetBitForSID(eStyleStruct_Variables);
    } else {
      compare = true;
    }
  } else {
    *aEqualStructs |= nsCachedStyleData::GetBitForSID(eStyleStruct_Variables);
  }

  DebugOnly<int> styleStructCount = 1;  // count Variables already

#define DO_STRUCT_DIFFERENCE(struct_)                                         \
  PR_BEGIN_MACRO                                                              \
    const nsStyle##struct_* this##struct_ = PeekStyle##struct_();             \
    if (this##struct_) {                                                      \
      const nsStyle##struct_* other##struct_ = aOther->Style##struct_();      \
      nsChangeHint maxDifference = nsStyle##struct_::MaxDifference();         \
      nsChangeHint differenceAlwaysHandledForDescendants =                    \
        nsStyle##struct_::DifferenceAlwaysHandledForDescendants();            \
      if (this##struct_ == other##struct_) {                                  \
        /* The very same struct, so we know that there will be no */          \
        /* differences.                                           */          \
        *aEqualStructs |= NS_STYLE_INHERIT_BIT(struct_);                      \
      } else if (compare ||                                                   \
                 (NS_SubtractHint(maxDifference,                              \
                                  differenceAlwaysHandledForDescendants) &    \
                  aParentHintsNotHandledForDescendants)) {                    \
        nsChangeHint difference =                                             \
            this##struct_->CalcDifference(*other##struct_);                   \
        NS_ASSERTION(NS_IsHintSubset(difference, maxDifference),              \
                     "CalcDifference() returned bigger hint than "            \
                     "MaxDifference()");                                      \
        NS_UpdateHint(hint, difference);                                      \
        if (!difference) {                                                    \
          *aEqualStructs |= NS_STYLE_INHERIT_BIT(struct_);                    \
        }                                                                     \
      } else {                                                                \
        /* We still must call CalcDifference to see if there were any */      \
        /* changes so that we can set *aEqualStructs appropriately.   */      \
        nsChangeHint difference =                                             \
            this##struct_->CalcDifference(*other##struct_);                   \
        NS_ASSERTION(NS_IsHintSubset(difference, maxDifference),              \
                     "CalcDifference() returned bigger hint than "            \
                     "MaxDifference()");                                      \
        if (!difference) {                                                    \
          *aEqualStructs |= NS_STYLE_INHERIT_BIT(struct_);                    \
        }                                                                     \
      }                                                                       \
    } else {                                                                  \
      *aEqualStructs |= NS_STYLE_INHERIT_BIT(struct_);                        \
    }                                                                         \
    styleStructCount++;                                                       \
  PR_END_MACRO

  // In general, we want to examine structs starting with those that can
  // cause the largest style change, down to those that can cause the
  // smallest.  This lets us skip later ones if we already have a hint
  // that subsumes their MaxDifference.  (As the hints get
  // finer-grained, this optimization is becoming less useful, though.)
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
  DO_STRUCT_DIFFERENCE(Quotes);
  DO_STRUCT_DIFFERENCE(SVGReset);
  DO_STRUCT_DIFFERENCE(SVG);
  DO_STRUCT_DIFFERENCE(Position);
  DO_STRUCT_DIFFERENCE(Font);
  DO_STRUCT_DIFFERENCE(Margin);
  DO_STRUCT_DIFFERENCE(Padding);
  DO_STRUCT_DIFFERENCE(Border);
  DO_STRUCT_DIFFERENCE(TextReset);
  DO_STRUCT_DIFFERENCE(Background);
  DO_STRUCT_DIFFERENCE(Color);

#undef DO_STRUCT_DIFFERENCE

  MOZ_ASSERT(styleStructCount == nsStyleStructID_Length,
             "missing a call to DO_STRUCT_DIFFERENCE");

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
    if (!data || data == aOther->Style##name_()) {                            \
      *aSamePointerStructs |= NS_STYLE_INHERIT_BIT(name_);                    \
    }                                                                         \
  }
#include "nsStyleStructList.h"
#undef STYLE_STRUCT

  // Note that we do not check whether this->RelevantLinkVisited() !=
  // aOther->RelevantLinkVisited(); we don't need to since
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
                *otherVis = aOther->GetStyleIfVisited();
  if (!thisVis != !otherVis) {
    // One style context has a style-if-visited and the other doesn't.
    // Presume a difference.
    NS_UpdateHint(hint, nsChangeHint_RepaintFrame);
  } else if (thisVis && !NS_IsHintSubset(nsChangeHint_RepaintFrame, hint)) {
    // Both style contexts have a style-if-visited.
    bool change = false;

    // NB: Calling Peek on |this|, not |thisVis|, since callers may look
    // at a struct on |this| without looking at the same struct on
    // |thisVis| (including this function if we skip one of these checks
    // due to change being true already or due to the old style context
    // not having a style-if-visited), but not the other way around.
    if (PeekStyleColor()) {
      if (thisVis->StyleColor()->mColor !=
          otherVis->StyleColor()->mColor) {
        change = true;
      }
    }

    // NB: Calling Peek on |this|, not |thisVis| (see above).
    if (!change && PeekStyleBackground()) {
      if (thisVis->StyleBackground()->mBackgroundColor !=
          otherVis->StyleBackground()->mBackgroundColor) {
        change = true;
      }
    }

    // NB: Calling Peek on |this|, not |thisVis| (see above).
    if (!change && PeekStyleBorder()) {
      const nsStyleBorder *thisVisBorder = thisVis->StyleBorder();
      const nsStyleBorder *otherVisBorder = otherVis->StyleBorder();
      NS_FOR_CSS_SIDES(side) {
        bool thisFG, otherFG;
        // Dummy initialisations to keep Valgrind/Memcheck happy.
        // See bug 1122375 comment 4.
        nscolor thisColor = NS_RGBA(0, 0, 0, 0);
        nscolor otherColor = NS_RGBA(0, 0, 0, 0);
        thisVisBorder->GetBorderColor(side, thisColor, thisFG);
        otherVisBorder->GetBorderColor(side, otherColor, otherFG);
        if (thisFG != otherFG || (!thisFG && thisColor != otherColor)) {
          change = true;
          break;
        }
      }
    }

    // NB: Calling Peek on |this|, not |thisVis| (see above).
    if (!change && PeekStyleOutline()) {
      const nsStyleOutline *thisVisOutline = thisVis->StyleOutline();
      const nsStyleOutline *otherVisOutline = otherVis->StyleOutline();
      bool haveColor;
      nscolor thisColor, otherColor;
      if (thisVisOutline->GetOutlineInitialColor() != 
            otherVisOutline->GetOutlineInitialColor() ||
          (haveColor = thisVisOutline->GetOutlineColor(thisColor)) != 
            otherVisOutline->GetOutlineColor(otherColor) ||
          (haveColor && thisColor != otherColor)) {
        change = true;
      }
    }

    // NB: Calling Peek on |this|, not |thisVis| (see above).
    if (!change && PeekStyleColumn()) {
      const nsStyleColumn *thisVisColumn = thisVis->StyleColumn();
      const nsStyleColumn *otherVisColumn = otherVis->StyleColumn();
      if (thisVisColumn->mColumnRuleColor != otherVisColumn->mColumnRuleColor ||
          thisVisColumn->mColumnRuleColorIsForeground !=
            otherVisColumn->mColumnRuleColorIsForeground) {
        change = true;
      }
    }

    // NB: Calling Peek on |this|, not |thisVis| (see above).
    if (!change && PeekStyleTextReset()) {
      const nsStyleTextReset *thisVisTextReset = thisVis->StyleTextReset();
      const nsStyleTextReset *otherVisTextReset = otherVis->StyleTextReset();
      // Dummy initialisations to keep Valgrind/Memcheck happy.
      // See bug 1122375 comment 4.
      nscolor thisVisDecColor = NS_RGBA(0, 0, 0, 0);
      nscolor otherVisDecColor = NS_RGBA(0, 0, 0, 0);
      bool thisVisDecColorIsFG, otherVisDecColorIsFG;
      thisVisTextReset->GetDecorationColor(thisVisDecColor,
                                           thisVisDecColorIsFG);
      otherVisTextReset->GetDecorationColor(otherVisDecColor,
                                            otherVisDecColorIsFG);
      if (thisVisDecColorIsFG != otherVisDecColorIsFG ||
          (!thisVisDecColorIsFG && thisVisDecColor != otherVisDecColor)) {
        change = true;
      }
    }

    // NB: Calling Peek on |this|, not |thisVis| (see above).
    if (!change && PeekStyleSVG()) {
      const nsStyleSVG *thisVisSVG = thisVis->StyleSVG();
      const nsStyleSVG *otherVisSVG = otherVis->StyleSVG();
      if (thisVisSVG->mFill != otherVisSVG->mFill ||
          thisVisSVG->mStroke != otherVisSVG->mStroke) {
        change = true;
      }
    }

    if (change) {
      NS_UpdateHint(hint, nsChangeHint_RepaintFrame);
    }
  }

  return NS_SubtractHint(hint, nsChangeHint_NeutralChange);
}

void
nsStyleContext::Mark()
{
  // Mark our rule node.
  mRuleNode->Mark();

  // Mark our children (i.e., tell them to mark their rule nodes, etc.).
  if (mChild) {
    nsStyleContext* child = mChild;
    do {
      child->Mark();
      child = child->mNextSibling;
    } while (mChild != child);
  }
  
  if (mEmptyChild) {
    nsStyleContext* child = mEmptyChild;
    do {
      child->Mark();
      child = child->mNextSibling;
    } while (mEmptyChild != child);
  }
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

  if (mRuleNode) {
    fprintf_stderr(out, "%s{\n", str.get());
    str.Truncate();
    nsRuleNode* ruleNode = mRuleNode;
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
nsStyleContext::operator new(size_t sz, nsPresContext* aPresContext) CPP_THROW_NEW
{
  // Check the recycle list first.
  return aPresContext->PresShell()->AllocateByObjectID(nsPresArena::nsStyleContext_id, sz);
}

// Overridden to prevent the global delete from being called, since the memory
// came out of an nsIArena instead of the global delete operator's heap.
void 
nsStyleContext::Destroy()
{
  // Get the pres context from our rule node.
  nsRefPtr<nsPresContext> presContext = mRuleNode->PresContext();

  // Call our destructor.
  this->~nsStyleContext();

  // Don't let the memory be freed, since it will be recycled
  // instead. Don't call the global operator delete.
  presContext->PresShell()->FreeByObjectID(nsPresArena::nsStyleContext_id, this);
}

already_AddRefed<nsStyleContext>
NS_NewStyleContext(nsStyleContext* aParentContext,
                   nsIAtom* aPseudoTag,
                   nsCSSPseudoElements::Type aPseudoType,
                   nsRuleNode* aRuleNode,
                   bool aSkipParentDisplayBasedStyleFixup)
{
  nsRefPtr<nsStyleContext> context =
    new (aRuleNode->PresContext())
    nsStyleContext(aParentContext, aPseudoTag, aPseudoType, aRuleNode,
                   aSkipParentDisplayBasedStyleFixup);
  return context.forget();
}

static inline void
ExtractAnimationValue(nsCSSProperty aProperty,
                      nsStyleContext* aStyleContext,
                      StyleAnimationValue& aResult)
{
  DebugOnly<bool> success =
    StyleAnimationValue::ExtractComputedValue(aProperty, aStyleContext,
                                              aResult);
  MOZ_ASSERT(success,
             "aProperty must be extractable by StyleAnimationValue");
}

static nscolor
ExtractColor(nsCSSProperty aProperty,
             nsStyleContext *aStyleContext)
{
  StyleAnimationValue val;
  ExtractAnimationValue(aProperty, aStyleContext, val);
  return val.GetColorValue();
}

static nscolor
ExtractColorLenient(nsCSSProperty aProperty,
                    nsStyleContext *aStyleContext)
{
  StyleAnimationValue val;
  ExtractAnimationValue(aProperty, aStyleContext, val);
  if (val.GetUnit() == StyleAnimationValue::eUnit_Color) {
    return val.GetColorValue();
  }
  return NS_RGBA(0, 0, 0, 0);
}

struct ColorIndexSet {
  uint8_t colorIndex, alphaIndex;
};

static const ColorIndexSet gVisitedIndices[2] = { { 0, 0 }, { 1, 0 } };

nscolor
nsStyleContext::GetVisitedDependentColor(nsCSSProperty aProperty)
{
  NS_ASSERTION(aProperty == eCSSProperty_color ||
               aProperty == eCSSProperty_background_color ||
               aProperty == eCSSProperty_border_top_color ||
               aProperty == eCSSProperty_border_right_color ||
               aProperty == eCSSProperty_border_bottom_color ||
               aProperty == eCSSProperty_border_left_color ||
               aProperty == eCSSProperty_outline_color ||
               aProperty == eCSSProperty__moz_column_rule_color ||
               aProperty == eCSSProperty_text_decoration_color ||
               aProperty == eCSSProperty_fill ||
               aProperty == eCSSProperty_stroke,
               "we need to add to nsStyleContext::CalcStyleDifference");

  bool isPaintProperty = aProperty == eCSSProperty_fill ||
                         aProperty == eCSSProperty_stroke;

  nscolor colors[2];
  colors[0] = isPaintProperty ? ExtractColorLenient(aProperty, this)
                              : ExtractColor(aProperty, this);

  nsStyleContext *visitedStyle = this->GetStyleIfVisited();
  if (!visitedStyle) {
    return colors[0];
  }

  colors[1] = isPaintProperty ? ExtractColorLenient(aProperty, visitedStyle)
                              : ExtractColor(aProperty, visitedStyle);

  return nsStyleContext::CombineVisitedColors(colors,
                                              this->RelevantLinkVisited());
}

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
/* static */ void
nsStyleContext::AssertStyleStructMaxDifferenceValid()
{
#define STYLE_STRUCT(name, checkdata_cb)                                     \
    MOZ_ASSERT(NS_IsHintSubset(nsStyle##name::DifferenceAlwaysHandledForDescendants(), \
                               nsStyle##name::MaxDifference()));
#include "nsStyleStructList.h"
#undef STYLE_STRUCT
}

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
      mCachedResetData = new (mRuleNode->PresContext()) nsResetStyleData;
    }
    if (!aNewContext->mCachedResetData) {
      aNewContext->mCachedResetData =
        new (mRuleNode->PresContext()) nsResetStyleData;
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
      if (HasCachedInheritedStyleData(i)) {
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
  if (UsesGrandancestorStyle()) {
    flags.AppendLiteral("USES_GRANDANCESTOR_STYLE ");
  }
  if (IsShared()) {
    flags.AppendLiteral("IS_SHARED ");
  }

  nsCString parent;
  if (aFirst) {
    parent.AppendPrintf("parent=%p ", mParent);
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
