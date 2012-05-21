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

#define NS_ILAYOUTHISTORYSTATE_IID \
{ 0x003919e2, 0x5e6b, 0x4d76, \
 { 0xa9, 0x4f, 0xbc, 0x5d, 0x15, 0x5b, 0x1c, 0x67 } }

class nsILayoutHistoryState : public nsISupports {
 public: 
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ILAYOUTHISTORYSTATE_IID)

  /**
   * Set |aState| as the state object for |aKey|.
   * This _transfers_ownership_ of |aState| to the LayoutHistoryState.
   * It will be freed when RemoveState() is called or when the
   * LayoutHistoryState is destroyed.
   */
  NS_IMETHOD AddState(const nsCString& aKey, nsPresState* aState) = 0;

  /**
   * Look up the state object for |aKey|.
   */
  NS_IMETHOD GetState(const nsCString& aKey, nsPresState** aState) = 0;

  /**
   * Remove the state object for |aKey|.
   */
  NS_IMETHOD RemoveState(const nsCString& aKey) = 0;

  /**
   * Check whether this history has any states in it
   */
  NS_IMETHOD_(bool) HasStates() const = 0;

  /**
   * Sets whether this history can contain only scroll position history
   * or all possible history
   */
   NS_IMETHOD SetScrollPositionOnly(const bool aFlag) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsILayoutHistoryState,
                              NS_ILAYOUTHISTORYSTATE_IID)

nsresult
NS_NewLayoutHistoryState(nsILayoutHistoryState** aState);

#endif /* _nsILayoutHistoryState_h */

