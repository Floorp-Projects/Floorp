/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* implementation of CSS counters (for numbering things) */

#include "nsCounterManager.h"

#include "mozilla/Likely.h"
#include "mozilla/IntegerRange.h"
#include "mozilla/PresShell.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/WritingModes.h"
#include "mozilla/dom/Element.h"
#include "nsContentUtils.h"
#include "nsIContent.h"
#include "nsIContentInlines.h"
#include "nsIFrame.h"
#include "nsContainerFrame.h"
#include "nsTArray.h"
#include "mozilla/dom/Text.h"

using namespace mozilla;

bool nsCounterUseNode::InitTextFrame(nsGenConList* aList,
                                     nsIFrame* aPseudoFrame,
                                     nsIFrame* aTextFrame) {
  nsCounterNode::InitTextFrame(aList, aPseudoFrame, aTextFrame);

  auto* counterList = static_cast<nsCounterList*>(aList);
  counterList->Insert(this);
  aPseudoFrame->AddStateBits(NS_FRAME_HAS_CSS_COUNTER_STYLE);
  // If the list is already dirty, or the node is not at the end, just start
  // with an empty string for now and when we recalculate the list we'll change
  // the value to the right one.
  if (counterList->IsDirty()) {
    return false;
  }
  if (!counterList->IsLast(this)) {
    counterList->SetDirty();
    return true;
  }
  Calc(counterList, /* aNotify = */ false);
  return false;
}

// assign the correct |mValueAfter| value to a node that has been inserted
// Should be called immediately after calling |Insert|.
void nsCounterUseNode::Calc(nsCounterList* aList, bool aNotify) {
  NS_ASSERTION(!aList->IsDirty(), "Why are we calculating with a dirty list?");
  mValueAfter = nsCounterList::ValueBefore(this);
  if (mText) {
    nsAutoString contentString;
    GetText(contentString);
    mText->SetText(contentString, aNotify);
  }
}

// assign the correct |mValueAfter| value to a node that has been inserted
// Should be called immediately after calling |Insert|.
void nsCounterChangeNode::Calc(nsCounterList* aList) {
  NS_ASSERTION(!aList->IsDirty(), "Why are we calculating with a dirty list?");
  if (IsContentBasedReset()) {
    // RecalcAll takes care of this case.
  } else if (mType == RESET || mType == SET) {
    mValueAfter = mChangeValue;
  } else {
    NS_ASSERTION(mType == INCREMENT, "invalid type");
    mValueAfter = nsCounterManager::IncrementCounter(
        nsCounterList::ValueBefore(this), mChangeValue);
  }
}

void nsCounterUseNode::GetText(nsString& aResult) {
  CounterStyle* style =
      mPseudoFrame->PresContext()->CounterStyleManager()->ResolveCounterStyle(
          mCounterStyle);
  GetText(mPseudoFrame->GetWritingMode(), style, aResult);
}

void nsCounterUseNode::GetText(WritingMode aWM, CounterStyle* aStyle,
                               nsString& aResult) {
  const bool isBidiRTL = aWM.IsBidiRTL();
  auto AppendCounterText = [&aResult, isBidiRTL](const nsAutoString& aText,
                                                 bool aIsRTL) {
    if (MOZ_LIKELY(isBidiRTL == aIsRTL)) {
      aResult.Append(aText);
    } else {
      // RLM = 0x200f, LRM = 0x200e
      const char16_t mark = aIsRTL ? 0x200f : 0x200e;
      aResult.Append(mark);
      aResult.Append(aText);
      aResult.Append(mark);
    }
  };

  if (mForLegacyBullet) {
    nsAutoString prefix;
    aStyle->GetPrefix(prefix);
    aResult.Assign(prefix);
  }

  AutoTArray<nsCounterNode*, 8> stack;
  stack.AppendElement(static_cast<nsCounterNode*>(this));

  if (mAllCounters && mScopeStart) {
    for (nsCounterNode* n = mScopeStart; n->mScopePrev; n = n->mScopeStart) {
      stack.AppendElement(n->mScopePrev);
    }
  }

  for (nsCounterNode* n : Reversed(stack)) {
    nsAutoString text;
    bool isTextRTL;
    aStyle->GetCounterText(n->mValueAfter, aWM, text, isTextRTL);
    if (!mForLegacyBullet || aStyle->IsBullet()) {
      aResult.Append(text);
    } else {
      AppendCounterText(text, isTextRTL);
    }
    if (n == this) {
      break;
    }
    aResult.Append(mSeparator);
  }

  if (mForLegacyBullet) {
    nsAutoString suffix;
    aStyle->GetSuffix(suffix);
    aResult.Append(suffix);
  }
}

static const nsIContent* GetParentContentForScope(nsIFrame* frame) {
  // We do not want elements with `display: contents` to establish scope for
  // counters. We'd like to do something like
  // `nsIFrame::GetClosestFlattenedTreeAncestorPrimaryFrame()` above, but this
  // may be called before the primary frame is set on frames.
  nsIContent* content = frame->GetContent()->GetFlattenedTreeParent();
  while (content && content->IsElement() &&
         content->AsElement()->IsDisplayContents()) {
    content = content->GetFlattenedTreeParent();
  }

  return content;
}

void nsCounterList::SetScope(nsCounterNode* aNode) {
  // This function is responsible for setting |mScopeStart| and
  // |mScopePrev| (whose purpose is described in nsCounterManager.h).
  // We do this by starting from the node immediately preceding
  // |aNode| in content tree order, which is reasonably likely to be
  // the previous element in our scope (or, for a reset, the previous
  // element in the containing scope, which is what we want).  If
  // we're not in the same scope that it is, then it's too deep in the
  // frame tree, so we walk up parent scopes until we find something
  // appropriate.

  auto setNullScopeFor = [](nsCounterNode* aNode) {
    aNode->mScopeStart = nullptr;
    aNode->mScopePrev = nullptr;
    if (aNode->IsUnitializedIncrementNode()) {
      aNode->ChangeNode()->mChangeValue = 1;
    }
  };

  if (aNode == First()) {
    setNullScopeFor(aNode);
    return;
  }

  auto didSetScopeFor = [this](nsCounterNode* aNode) {
    if (aNode->mType == nsCounterNode::USE) {
      return;
    }
    if (aNode->mScopeStart->IsContentBasedReset()) {
      mDirty = true;
    }
    if (aNode->IsUnitializedIncrementNode()) {
      aNode->ChangeNode()->mChangeValue =
          aNode->mScopeStart->IsReversed() ? -1 : 1;
    }
  };

  // If there exist an explicit RESET scope created by an ancestor or
  // the element itself, then we use that scope.
  // Otherwise, fall through to consider scopes created by siblings (and
  // their descendants) in reverse document order.
  if (aNode->mType != nsCounterNode::USE &&
      StaticPrefs::layout_css_counter_ancestor_scope_enabled()) {
    for (auto* p = aNode->mPseudoFrame; p; p = p->GetParent()) {
      // This relies on the fact that a RESET node is always the first
      // CounterNode for a frame if it has any.
      auto* counter = GetFirstNodeFor(p);
      if (!counter || counter->mType != nsCounterNode::RESET) {
        continue;
      }
      if (p == aNode->mPseudoFrame) {
        break;
      }
      aNode->mScopeStart = counter;
      aNode->mScopePrev = counter;
      for (nsCounterNode* prev = Prev(aNode); prev; prev = prev->mScopePrev) {
        if (prev->mScopeStart == counter) {
          aNode->mScopePrev =
              prev->mType == nsCounterNode::RESET ? prev->mScopePrev : prev;
          break;
        }
        if (prev->mType != nsCounterNode::RESET) {
          prev = prev->mScopeStart;
          if (!prev) {
            break;
          }
        }
      }
      didSetScopeFor(aNode);
      return;
    }
  }

  // Get the content node for aNode's rendering object's *parent*,
  // since scope includes siblings, so we want a descendant check on
  // parents. Note here that mPseudoFrame is a bit of a misnomer, as it
  // might not be a pseudo element at all, but a normal element that
  // happens to increment a counter. We want to respect the flat tree
  // here, but skipping any <slot> element that happens to contain
  // mPseudoFrame. That's why this uses GetInFlowParent() instead
  // of GetFlattenedTreeParent().
  const nsIContent* nodeContent = GetParentContentForScope(aNode->mPseudoFrame);
  for (nsCounterNode *prev = Prev(aNode), *start; prev;
       prev = start->mScopePrev) {
    // If |prev| starts a scope (because it's a real or implied
    // reset), we want it as the scope start rather than the start
    // of its enclosing scope.  Otherwise, there's no enclosing
    // scope, so the next thing in prev's scope shares its scope
    // start.
    start = (prev->mType == nsCounterNode::RESET || !prev->mScopeStart)
                ? prev
                : prev->mScopeStart;

    // |startContent| is analogous to |nodeContent| (see above).
    const nsIContent* startContent =
        GetParentContentForScope(start->mPseudoFrame);
    NS_ASSERTION(nodeContent || !startContent,
                 "null check on startContent should be sufficient to "
                 "null check nodeContent as well, since if nodeContent "
                 "is for the root, startContent (which is before it) "
                 "must be too");

    // A reset's outer scope can't be a scope created by a sibling.
    if (!(aNode->mType == nsCounterNode::RESET &&
          nodeContent == startContent) &&
        // everything is inside the root (except the case above,
        // a second reset on the root)
        (!startContent ||
         nodeContent->IsInclusiveFlatTreeDescendantOf(startContent))) {
      aNode->mScopeStart = start;
      aNode->mScopePrev = prev;
      didSetScopeFor(aNode);
      return;
    }
  }

  setNullScopeFor(aNode);
}

void nsCounterList::RecalcAll() {
  // Setup the scope and calculate the default start value for content-based
  // reversed() counters.  We need to track the last increment for each of
  // those scopes so that we can add it in an extra time at the end.
  // https://drafts.csswg.org/css-lists/#instantiating-counters
  nsTHashMap<nsPtrHashKey<nsCounterChangeNode>, int32_t> scopes;
  for (nsCounterNode* node = First(); node; node = Next(node)) {
    SetScope(node);
    if (node->IsContentBasedReset()) {
      node->ChangeNode()->mSeenSetNode = false;
      node->mValueAfter = 0;
      scopes.InsertOrUpdate(node->ChangeNode(), 0);
    } else if (node->mScopeStart && node->mScopeStart->IsContentBasedReset() &&
               !node->mScopeStart->ChangeNode()->mSeenSetNode) {
      if (node->mType == nsCounterChangeNode::INCREMENT) {
        auto incrementNegated = -node->ChangeNode()->mChangeValue;
        if (auto entry = scopes.Lookup(node->mScopeStart->ChangeNode())) {
          entry.Data() = incrementNegated;
        }
        auto* next = Next(node);
        if (next && next->mPseudoFrame == node->mPseudoFrame &&
            next->mType == nsCounterChangeNode::SET) {
          continue;
        }
        node->mScopeStart->mValueAfter += incrementNegated;
      } else if (node->mType == nsCounterChangeNode::SET) {
        node->mScopeStart->mValueAfter += node->ChangeNode()->mChangeValue;
        // We have a 'counter-set' for this scope so we're done.
        // The counter is incremented from that value for the remaining nodes.
        node->mScopeStart->ChangeNode()->mSeenSetNode = true;
      }
    }
  }

  // For all the content-based reversed() counters we found, add in the
  // incrementNegated from its last counter-increment.
  for (auto iter = scopes.ConstIter(); !iter.Done(); iter.Next()) {
    iter.Key()->mValueAfter += iter.Data();
  }

  mDirty = false;
  for (nsCounterNode* node = First(); node; node = Next(node)) {
    node->Calc(this, /* aNotify = */ true);
  }
}

static bool AddCounterChangeNode(nsCounterManager& aManager, nsIFrame* aFrame,
                                 int32_t aIndex,
                                 const nsStyleContent::CounterPair& aPair,
                                 nsCounterNode::Type aType) {
  auto* node = new nsCounterChangeNode(aFrame, aType, aPair.value, aIndex,
                                       aPair.is_reversed);
  nsCounterList* counterList = aManager.CounterListFor(aPair.name.AsAtom());
  counterList->Insert(node);
  if (!counterList->IsLast(node)) {
    // Tell the caller it's responsible for recalculating the entire list.
    counterList->SetDirty();
    return true;
  }

  // Don't call Calc() if the list is already dirty -- it'll be recalculated
  // anyway, and trying to calculate with a dirty list doesn't work.
  if (MOZ_LIKELY(!counterList->IsDirty())) {
    node->Calc(counterList);
  }
  return counterList->IsDirty();
}

static bool HasCounters(const nsStyleContent& aStyle) {
  return !aStyle.mCounterIncrement.IsEmpty() ||
         !aStyle.mCounterReset.IsEmpty() || !aStyle.mCounterSet.IsEmpty();
}

bool nsCounterManager::AddCounterChanges(nsIFrame* aFrame) {
  // For elements with 'display:list-item' we add a default
  // 'counter-increment:list-item' unless 'counter-increment' already has a
  // value for 'list-item'.
  //
  // https://drafts.csswg.org/css-lists-3/#declaring-a-list-item
  //
  // We inherit `display` for some anonymous boxes, but we don't want them to
  // increment the list-item counter.
  const bool requiresListItemIncrement =
      aFrame->StyleDisplay()->IsListItem() && !aFrame->Style()->IsAnonBox();

  const nsStyleContent* styleContent = aFrame->StyleContent();

  if (!requiresListItemIncrement && !HasCounters(*styleContent)) {
    MOZ_ASSERT(!aFrame->HasAnyStateBits(NS_FRAME_HAS_CSS_COUNTER_STYLE));
    return false;
  }

  aFrame->AddStateBits(NS_FRAME_HAS_CSS_COUNTER_STYLE);

  bool dirty = false;
  // Add in order, resets first, so all the comparisons will be optimized
  // for addition at the end of the list.
  {
    int32_t i = 0;
    for (const auto& pair : styleContent->mCounterReset.AsSpan()) {
      dirty |= AddCounterChangeNode(*this, aFrame, i++, pair,
                                    nsCounterChangeNode::RESET);
    }
  }
  bool hasListItemIncrement = false;
  {
    int32_t i = 0;
    for (const auto& pair : styleContent->mCounterIncrement.AsSpan()) {
      hasListItemIncrement |= pair.name.AsAtom() == nsGkAtoms::list_item;
      if (pair.value != 0) {
        dirty |= AddCounterChangeNode(*this, aFrame, i++, pair,
                                      nsCounterChangeNode::INCREMENT);
      }
    }
  }

  if (requiresListItemIncrement && !hasListItemIncrement) {
    RefPtr<nsAtom> atom = nsGkAtoms::list_item;
    // We use a magic value here to signal to SetScope() that it should
    // set the value to -1 or 1 depending on if the scope is reversed()
    // or not.
    auto listItemIncrement = nsStyleContent::CounterPair{
        {StyleAtom(atom.forget())}, std::numeric_limits<int32_t>::min()};
    dirty |= AddCounterChangeNode(
        *this, aFrame, styleContent->mCounterIncrement.Length(),
        listItemIncrement, nsCounterChangeNode::INCREMENT);
  }

  {
    int32_t i = 0;
    for (const auto& pair : styleContent->mCounterSet.AsSpan()) {
      dirty |= AddCounterChangeNode(*this, aFrame, i++, pair,
                                    nsCounterChangeNode::SET);
    }
  }
  return dirty;
}

nsCounterList* nsCounterManager::CounterListFor(nsAtom* aCounterName) {
  MOZ_ASSERT(aCounterName);
  return mNames.GetOrInsertNew(aCounterName);
}

void nsCounterManager::RecalcAll() {
  for (const auto& list : mNames.Values()) {
    if (list->IsDirty()) {
      list->RecalcAll();
    }
  }
}

void nsCounterManager::SetAllDirty() {
  for (const auto& list : mNames.Values()) {
    list->SetDirty();
  }
}

bool nsCounterManager::DestroyNodesFor(nsIFrame* aFrame) {
  MOZ_ASSERT(aFrame->HasAnyStateBits(NS_FRAME_HAS_CSS_COUNTER_STYLE),
             "why call me?");
  bool destroyedAny = false;
  for (const auto& list : mNames.Values()) {
    if (list->DestroyNodesFor(aFrame)) {
      destroyedAny = true;
      list->SetDirty();
    }
  }
  return destroyedAny;
}

#ifdef ACCESSIBILITY
void nsCounterManager::GetSpokenCounterText(nsIFrame* aFrame,
                                            nsAString& aText) const {
  CounterValue ordinal = 1;
  if (const auto* list = mNames.Get(nsGkAtoms::list_item)) {
    for (nsCounterNode* n = list->GetFirstNodeFor(aFrame);
         n && n->mPseudoFrame == aFrame; n = list->Next(n)) {
      if (n->mType == nsCounterNode::USE) {
        ordinal = n->mValueAfter;
        break;
      }
    }
  }
  CounterStyle* counterStyle =
      aFrame->PresContext()->CounterStyleManager()->ResolveCounterStyle(
          aFrame->StyleList()->mCounterStyle);
  nsAutoString text;
  bool isBullet;
  counterStyle->GetSpokenCounterText(ordinal, aFrame->GetWritingMode(), text,
                                     isBullet);
  if (isBullet) {
    aText = text;
    if (!counterStyle->IsNone()) {
      aText.Append(' ');
    }
  } else {
    counterStyle->GetPrefix(aText);
    aText += text;
    nsAutoString suffix;
    counterStyle->GetSuffix(suffix);
    aText += suffix;
  }
}
#endif

#if defined(DEBUG) || defined(MOZ_LAYOUT_DEBUGGER)
void nsCounterManager::Dump() const {
  printf("\n\nCounter Manager Lists:\n");
  for (const auto& entry : mNames) {
    printf("Counter named \"%s\":\n", nsAtomCString(entry.GetKey()).get());

    nsCounterList* list = entry.GetWeak();
    int32_t i = 0;
    for (nsCounterNode* node = list->First(); node; node = list->Next(node)) {
      const char* types[] = {"RESET", "INCREMENT", "SET", "USE"};
      printf(
          "  Node #%d @%p frame=%p index=%d type=%s valAfter=%d\n"
          "       scope-start=%p scope-prev=%p",
          i++, (void*)node, (void*)node->mPseudoFrame, node->mContentIndex,
          types[node->mType], node->mValueAfter, (void*)node->mScopeStart,
          (void*)node->mScopePrev);
      if (node->mType == nsCounterNode::USE) {
        nsAutoString text;
        node->UseNode()->GetText(text);
        printf(" text=%s", NS_ConvertUTF16toUTF8(text).get());
      }
      printf("\n");
    }
  }
  printf("\n\n");
}
#endif
