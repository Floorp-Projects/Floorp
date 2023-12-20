/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_ACTIVERESOURCE
#define MOZILLA_LAYERS_ACTIVERESOURCE

#include "nsExpirationTracker.h"

namespace mozilla::layers {

/**
 * See ActiveResourceTracker below.
 */
class ActiveResource {
 public:
  virtual void NotifyInactive() = 0;
  nsExpirationState* GetExpirationState() { return &mExpirationState; }
  bool IsActivityTracked() { return mExpirationState.IsTracked(); }

 private:
  nsExpirationState mExpirationState;
};

/**
 * A convenience class on top of nsExpirationTracker
 */
class ActiveResourceTracker final
    : public nsExpirationTracker<ActiveResource, 3> {
 public:
  ActiveResourceTracker(uint32_t aExpirationCycle, const char* aName,
                        nsIEventTarget* aEventTarget)
      : nsExpirationTracker(aExpirationCycle, aName, aEventTarget) {}

  void NotifyExpired(ActiveResource* aResource) override {
    RemoveObject(aResource);
    aResource->NotifyInactive();
  }
};

}  // namespace mozilla::layers

#endif
