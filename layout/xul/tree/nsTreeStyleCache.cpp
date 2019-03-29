/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTreeStyleCache.h"
#include "mozilla/dom/Element.h"
#include "mozilla/ServoStyleSet.h"
#include "nsPresContextInlines.h"

using namespace mozilla;

nsTreeStyleCache::Transition::Transition(DFAState aState, nsAtom* aSymbol)
    : mState(aState), mInputSymbol(aSymbol) {}

bool nsTreeStyleCache::Transition::operator==(const Transition& aOther) const {
  return aOther.mState == mState && aOther.mInputSymbol == mInputSymbol;
}

uint32_t nsTreeStyleCache::Transition::Hash() const {
  // Make a 32-bit integer that combines the low-order 16 bits of the state and
  // the input symbol.
  uint32_t hb = mState << 16;
  uint32_t lb = (NS_PTR_TO_UINT32(mInputSymbol.get()) << 16) >> 16;
  return hb + lb;
}

// The ComputedStyle cache impl
ComputedStyle* nsTreeStyleCache::GetComputedStyle(
    nsPresContext* aPresContext, nsIContent* aContent, ComputedStyle* aStyle,
    nsCSSAnonBoxPseudoStaticAtom* aPseudoElement, const AtomArray& aInputWord) {
  MOZ_ASSERT(nsCSSAnonBoxes::IsTreePseudoElement(aPseudoElement));

  uint32_t count = aInputWord.Length();

  // Go ahead and init the transition table.
  if (!mTransitionTable) {
    // Automatic miss. Build the table
    mTransitionTable = new TransitionTable();
  }

  // The first transition is always made off the supplied pseudo-element.
  Transition transition(0, aPseudoElement);
  DFAState currState = mTransitionTable->Get(transition);

  if (!currState) {
    // We had a miss. Make a new state and add it to our hash.
    currState = mNextState;
    mNextState++;
    mTransitionTable->Put(transition, currState);
  }

  for (uint32_t i = 0; i < count; i++) {
    Transition transition(currState, aInputWord[i]);
    currState = mTransitionTable->Get(transition);

    if (!currState) {
      // We had a miss. Make a new state and add it to our hash.
      currState = mNextState;
      mNextState++;
      mTransitionTable->Put(transition, currState);
    }
  }

  // We're in a final state.
  // Look up our ComputedStyle for this state.
  ComputedStyle* result = nullptr;
  if (mCache) {
    result = mCache->GetWeak(currState);
  }
  if (!result) {
    // We missed the cache. Resolve this pseudo-style.
    RefPtr<ComputedStyle> newResult =
        aPresContext->StyleSet()->ResolveXULTreePseudoStyle(
            aContent->AsElement(), aPseudoElement, aStyle, aInputWord);

    // Normally we rely on nsFrame::Init / RestyleManager to call this, but
    // these are weird and don't use a frame, yet ::-moz-tree-twisty definitely
    // pokes at list-style-image.
    newResult->StartImageLoads(*aPresContext->Document());

    // Even though xul-tree pseudos are defined in nsCSSAnonBoxList, nothing has
    // ever treated them as an anon box, and they don't ever get boxes anyway.
    //
    // This is really weird, and probably nothing really relies on the result of
    // these assert, but it's here just to avoid changing them accidentally.
    MOZ_ASSERT(newResult->GetPseudoType() == PseudoStyleType::XULTree);
    MOZ_ASSERT(!newResult->IsAnonBox());
    MOZ_ASSERT(!newResult->IsPseudoElement());

    // Put the ComputedStyle in our table, transferring the owning reference to
    // the table.
    if (!mCache) {
      mCache = new ComputedStyleCache();
    }
    result = newResult.get();
    mCache->Put(currState, newResult.forget());
  }

  return result;
}
