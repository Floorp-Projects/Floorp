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

  // nsILayoutHistoryState
  virtual void
  AddState(const nsCString& aKey, nsPresState* aState) override;
  virtual nsPresState*
  GetState(const nsCString& aKey) override;
  virtual void
  RemoveState(const nsCString& aKey) override;
  virtual bool
  HasStates() const override;
  virtual void
  SetScrollPositionOnly(const bool aFlag) override;
  virtual void
  ResetScrollState() override;

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
nsLayoutHistoryState::HasStates() const
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
