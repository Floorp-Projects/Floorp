/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/GeckoStyleContext.h"

#include "nsStyleConsts.h"
#include "nsStyleStruct.h"
#include "nsPresContext.h"
#include "nsRuleNode.h"
#include "nsStyleContextInlines.h"

using namespace mozilla;

GeckoStyleContext::GeckoStyleContext(nsStyleContext* aParent,
                                     nsIAtom* aPseudoTag,
                                     CSSPseudoElementType aPseudoType,
                                     already_AddRefed<nsRuleNode> aRuleNode,
                                     bool aSkipParentDisplayBasedStyleFixup)
  : nsStyleContext(aParent, OwningStyleContextSource(Move(aRuleNode)),
                   aPseudoTag, aPseudoType)
  , mChild(nullptr)
  , mEmptyChild(nullptr)
{
  mBits |= NS_STYLE_CONTEXT_IS_GECKO;

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


void
GeckoStyleContext::AddChild(GeckoStyleContext* aChild)
{
  NS_ASSERTION(aChild->mPrevSibling == aChild &&
               aChild->mNextSibling == aChild,
               "child already in a child list");

  GeckoStyleContext **listPtr = aChild->mSource.MatchesNoRules() ? &mEmptyChild : &mChild;
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
GeckoStyleContext::RemoveChild(GeckoStyleContext* aChild)
{
  NS_PRECONDITION(nullptr != aChild && this == aChild->mParent, "bad argument");

  GeckoStyleContext **list = aChild->mSource.MatchesNoRules() ? &mEmptyChild : &mChild;

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
                                   NonOwningStyleContextSource aSource,
                                   NonOwningStyleContextSource aSourceIfVisited,
                                   bool aRelevantLinkVisited)
{
  uint32_t threshold = 10; // The # of siblings we're willing to examine
                           // before just giving this whole thing up.

  RefPtr<GeckoStyleContext> result;
  GeckoStyleContext *list = aSource.MatchesNoRules() ? mEmptyChild : mChild;

  if (list) {
    GeckoStyleContext *child = list;
    do {
      if (child->mSource.AsRaw() == aSource &&
          child->mPseudoTag == aPseudoTag &&
          !child->IsStyleIfVisited() &&
          child->RelevantLinkVisited() == aRelevantLinkVisited) {
        bool match = false;
        if (!aSourceIfVisited.IsNull()) {
          match = child->GetStyleIfVisited() &&
                  child->GetStyleIfVisited()->AsGecko()->mSource.AsRaw() == aSourceIfVisited;
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

