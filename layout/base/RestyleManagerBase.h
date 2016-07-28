/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_RestyleManagerBase_h
#define mozilla_RestyleManagerBase_h

#include "nsChangeHint.h"

namespace mozilla {

class ServoRestyleManager;
class RestyleManager;

/**
 * Class for sharing data and logic common to both RestyleManager and
 * ServoRestyleManager.
 */
class RestyleManagerBase
{
protected:
  explicit RestyleManagerBase(nsPresContext* aPresContext);

public:
  typedef mozilla::dom::Element Element;

  // Get an integer that increments every time we process pending restyles.
  // The value is never 0.
  uint32_t GetRestyleGeneration() const { return mRestyleGeneration; }

  // Get an integer that increments every time there is a style change
  // as a result of a change to the :hover content state.
  uint32_t GetHoverGeneration() const { return mHoverGeneration; }

  bool ObservingRefreshDriver() const { return mObservingRefreshDriver; }

  void SetObservingRefreshDriver(bool aObserving) {
      mObservingRefreshDriver = aObserving;
  }

  void Disconnect() { mPresContext = nullptr; }

  static nsCString RestyleHintToString(nsRestyleHint aHint);

protected:
  void ContentStateChangedInternal(Element* aElement,
                                   EventStates aStateMask,
                                   nsChangeHint* aOutChangeHint,
                                   nsRestyleHint* aOutRestyleHint);

  bool IsDisconnected() { return mPresContext == nullptr; }

  void IncrementHoverGeneration() {
    ++mHoverGeneration;
  }

  void IncrementRestyleGeneration() {
    if (++mRestyleGeneration == 0) {
      // Keep mRestyleGeneration from being 0, since that's what
      // nsPresContext::GetRestyleGeneration returns when it no
      // longer has a RestyleManager.
      ++mRestyleGeneration;
    }
  }

  nsPresContext* PresContext() const {
    MOZ_ASSERT(mPresContext);
    return mPresContext;
  }

  nsCSSFrameConstructor* FrameConstructor() const {
    return PresContext()->FrameConstructor();
  }

  inline bool IsGecko() const {
    return !IsServo();
  }

  inline bool IsServo() const {
#ifdef MOZ_STYLO
    return PresContext()->StyleSet()->IsServo();
#else
    return false;
#endif
  }

private:
  nsPresContext* mPresContext; // weak, can be null after Disconnect().
  uint32_t mRestyleGeneration;
  uint32_t mHoverGeneration;
  // True if we're already waiting for a refresh notification.
  bool mObservingRefreshDriver;
};

} // namespace mozilla

#endif
