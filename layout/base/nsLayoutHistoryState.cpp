/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * container for information saved in session history when the document
 * is not
 */

#include "nsILayoutHistoryState.h"
#include "nsWeakReference.h"
#include "nsClassHashtable.h"
#include "nsPresState.h"
#include "mozilla/Attributes.h"

class nsLayoutHistoryState final : public nsILayoutHistoryState,
                                   public nsSupportsWeakReference
{
public:
  nsLayoutHistoryState()
    : mScrollPositionOnly(false)
  {
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSILAYOUTHISTORYSTATE

private:
  ~nsLayoutHistoryState() {}
  bool mScrollPositionOnly;

  nsClassHashtable<nsCStringHashKey,nsPresState> mStates;
};


already_AddRefed<nsILayoutHistoryState>
NS_NewLayoutHistoryState()
{
  RefPtr<nsLayoutHistoryState> state = new nsLayoutHistoryState();
  return state.forget();
}

NS_IMPL_ISUPPORTS(nsLayoutHistoryState,
                  nsILayoutHistoryState,
                  nsISupportsWeakReference)

NS_IMETHODIMP
nsLayoutHistoryState::GetHasStates(bool* aHasStates)
{
  *aHasStates = HasStates();
  return NS_OK;
}

NS_IMETHODIMP
nsLayoutHistoryState::GetKeys(uint32_t* aCount, char*** aKeys)
{
  if (!HasStates()) {
    return NS_ERROR_FAILURE;
  }

  char** keys =
    static_cast<char**>(moz_xmalloc(sizeof(char*) * mStates.Count()));
  *aCount = mStates.Count();
  *aKeys = keys;

  for (auto iter = mStates.Iter(); !iter.Done(); iter.Next()) {
    *keys = ToNewCString(iter.Key());
    keys++;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsLayoutHistoryState::GetPresState(const nsACString& aKey,
                                   float* aScrollX, float* aScrollY,
                                   bool* aAllowScrollOriginDowngrade,
                                   float* aRes, bool* aScaleToRes)
{
  nsPresState* state = GetState(nsCString(aKey));

  if (!state) {
    return NS_ERROR_FAILURE;
  }

  *aScrollX = state->GetScrollPosition().x;
  *aScrollY = state->GetScrollPosition().y;
  *aAllowScrollOriginDowngrade = state->GetAllowScrollOriginDowngrade();
  *aRes = state->GetResolution();
  *aScaleToRes = state->GetScaleToResolution();

  return NS_OK;
}

NS_IMETHODIMP
nsLayoutHistoryState::AddNewPresState(const nsACString& aKey,
                                      float aScrollX, float aScrollY,
                                      bool aAllowScrollOriginDowngrade,
                                      float aRes, bool aScaleToRes)
{
  nsPresState* newState = new nsPresState();
  newState->SetScrollState(nsPoint(aScrollX, aScrollY));
  newState->SetAllowScrollOriginDowngrade(aAllowScrollOriginDowngrade);
  newState->SetResolution(aRes);
  newState->SetScaleToResolution(aScaleToRes);

  mStates.Put(nsCString(aKey), newState);

  return NS_OK;
}

void
nsLayoutHistoryState::AddState(const nsCString& aStateKey, nsPresState* aState)
{
  mStates.Put(aStateKey, aState);
}

nsPresState*
nsLayoutHistoryState::GetState(const nsCString& aKey)
{
  nsPresState* state = nullptr;
  bool entryExists = mStates.Get(aKey, &state);

  if (entryExists && mScrollPositionOnly) {
    // Ensure any state that shouldn't be restored is removed
    state->ClearNonScrollState();
  }

  return state;
}

void
nsLayoutHistoryState::RemoveState(const nsCString& aKey)
{
  mStates.Remove(aKey);
}

bool
nsLayoutHistoryState::HasStates()
{
  return mStates.Count() != 0;
}

void
nsLayoutHistoryState::SetScrollPositionOnly(const bool aFlag)
{
  mScrollPositionOnly = aFlag;
}

void
nsLayoutHistoryState::ResetScrollState()
{
  for (auto iter = mStates.Iter(); !iter.Done(); iter.Next()) {
    nsPresState* state = iter.UserData();
    if (state) {
      state->SetScrollState(nsPoint(0, 0));
    }
  }
}
