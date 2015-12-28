/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * interface for container for information saved in session history when
 * the document is not
 */

#ifndef _nsILayoutHistoryState_h
#define _nsILayoutHistoryState_h

#include "nsISupports.h"
#include "nsStringFwd.h"

class nsPresState;
template<typename> struct already_AddRefed;

#define NS_ILAYOUTHISTORYSTATE_IID \
{ 0xaef27cb3, 0x4df9, 0x4eeb, \
  { 0xb0, 0xb0, 0xac, 0x56, 0xcf, 0x86, 0x1d, 0x04 } }

class nsILayoutHistoryState : public nsISupports {
 public: 
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ILAYOUTHISTORYSTATE_IID)

  /**
   * Set |aState| as the state object for |aKey|.
   * This _transfers_ownership_ of |aState| to the LayoutHistoryState.
   * It will be freed when RemoveState() is called or when the
   * LayoutHistoryState is destroyed.
   */
  virtual void AddState(const nsCString& aKey, nsPresState* aState) = 0;

  /**
   * Look up the state object for |aKey|.
   */
  virtual nsPresState* GetState(const nsCString& aKey) = 0;

  /**
   * Remove the state object for |aKey|.
   */
  virtual void RemoveState(const nsCString& aKey) = 0;

  /**
   * Check whether this history has any states in it
   */
  virtual bool HasStates() const = 0;

  /**
   * Sets whether this history can contain only scroll position history
   * or all possible history
   */
  virtual void SetScrollPositionOnly(const bool aFlag) = 0;

  /**
   * Resets nsPresState::GetScrollState of all nsPresState objects to 0,0.
   */
  virtual void ResetScrollState() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsILayoutHistoryState,
                              NS_ILAYOUTHISTORYSTATE_IID)

already_AddRefed<nsILayoutHistoryState>
NS_NewLayoutHistoryState();

#endif /* _nsILayoutHistoryState_h */

