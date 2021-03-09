/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * container for information saved in session history when the document
 * is not
 */

#include "nsILayoutHistoryState.h"
#include "nsWeakReference.h"
#include "mozilla/PresState.h"
#include "mozilla/Attributes.h"
#include "mozilla/UniquePtr.h"
#include "nsTHashMap.h"

using namespace mozilla;

class nsLayoutHistoryState final : public nsILayoutHistoryState,
                                   public nsSupportsWeakReference {
 public:
  nsLayoutHistoryState() : mScrollPositionOnly(false) {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSILAYOUTHISTORYSTATE

 private:
  ~nsLayoutHistoryState() = default;
  bool mScrollPositionOnly;

  nsTHashMap<nsCString, UniquePtr<PresState>> mStates;
};

already_AddRefed<nsILayoutHistoryState> NS_NewLayoutHistoryState() {
  RefPtr<nsLayoutHistoryState> state = new nsLayoutHistoryState();
  return state.forget();
}

NS_IMPL_ISUPPORTS(nsLayoutHistoryState, nsILayoutHistoryState,
                  nsISupportsWeakReference)

NS_IMETHODIMP
nsLayoutHistoryState::GetHasStates(bool* aHasStates) {
  *aHasStates = HasStates();
  return NS_OK;
}

NS_IMETHODIMP
nsLayoutHistoryState::GetKeys(nsTArray<nsCString>& aKeys) {
  if (!HasStates()) {
    return NS_ERROR_FAILURE;
  }

  aKeys.SetCapacity(mStates.Count());
  for (auto iter = mStates.Iter(); !iter.Done(); iter.Next()) {
    aKeys.AppendElement(iter.Key());
  }

  return NS_OK;
}

NS_IMETHODIMP
nsLayoutHistoryState::GetPresState(const nsACString& aKey, float* aScrollX,
                                   float* aScrollY,
                                   bool* aAllowScrollOriginDowngrade,
                                   float* aRes) {
  PresState* state = GetState(nsCString(aKey));

  if (!state) {
    return NS_ERROR_FAILURE;
  }

  *aScrollX = state->scrollState().x;
  *aScrollY = state->scrollState().y;
  *aAllowScrollOriginDowngrade = state->allowScrollOriginDowngrade();
  *aRes = state->resolution();

  return NS_OK;
}

NS_IMETHODIMP
nsLayoutHistoryState::AddNewPresState(const nsACString& aKey, float aScrollX,
                                      float aScrollY,
                                      bool aAllowScrollOriginDowngrade,
                                      float aRes) {
  UniquePtr<PresState> newState = NewPresState();
  newState->scrollState() = nsPoint(aScrollX, aScrollY);
  newState->allowScrollOriginDowngrade() = aAllowScrollOriginDowngrade;
  newState->resolution() = aRes;

  mStates.InsertOrUpdate(nsCString(aKey), std::move(newState));

  return NS_OK;
}

void nsLayoutHistoryState::AddState(const nsCString& aStateKey,
                                    UniquePtr<PresState> aState) {
  mStates.InsertOrUpdate(aStateKey, std::move(aState));
}

PresState* nsLayoutHistoryState::GetState(const nsCString& aKey) {
  auto statePtr = mStates.Lookup(aKey);
  if (!statePtr) {
    return nullptr;
  }
  PresState* state = statePtr->get();

  if (mScrollPositionOnly) {
    // Ensure any state that shouldn't be restored is removed
    state->contentData() = void_t();
    state->disabledSet() = false;
  }

  return state;
}

void nsLayoutHistoryState::RemoveState(const nsCString& aKey) {
  mStates.Remove(aKey);
}

bool nsLayoutHistoryState::HasStates() { return mStates.Count() != 0; }

void nsLayoutHistoryState::SetScrollPositionOnly(const bool aFlag) {
  mScrollPositionOnly = aFlag;
}

void nsLayoutHistoryState::ResetScrollState() {
  for (auto iter = mStates.Iter(); !iter.Done(); iter.Next()) {
    PresState* state = iter.Data().get();
    if (state) {
      state->scrollState() = nsPoint(0, 0);
    }
  }
}

void nsLayoutHistoryState::GetContents(bool* aScrollPositionOnly,
                                       nsTArray<nsCString>& aKeys,
                                       nsTArray<mozilla::PresState>& aStates) {
  *aScrollPositionOnly = mScrollPositionOnly;
  aKeys.SetCapacity(mStates.Count());
  aStates.SetCapacity(mStates.Count());
  for (auto iter = mStates.Iter(); !iter.Done(); iter.Next()) {
    aKeys.AppendElement(iter.Key());
    aStates.AppendElement(*(iter.Data().get()));
  }
}

void nsLayoutHistoryState::Reset() {
  mScrollPositionOnly = false;
  mStates.Clear();
}

namespace mozilla {
UniquePtr<PresState> NewPresState() {
  return MakeUnique<PresState>(
      /* contentData */ mozilla::void_t(),
      /* scrollState */ nsPoint(0, 0),
      /* allowScrollOriginDowngrade */ true,
      /* resolution */ 1.0,
      /* disabledSet */ false,
      /* disabled */ false,
      /* droppedDown */ false);
}
}  // namespace mozilla
