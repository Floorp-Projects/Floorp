/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCCUncollectableMarker_h_
#define nsCCUncollectableMarker_h_

#include "js/TracingAPI.h"
#include "mozilla/Attributes.h"
#include "nsIObserver.h"

class nsCCUncollectableMarker MOZ_FINAL : public nsIObserver
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  /**
   * Inits a global nsCCUncollectableMarker. Should only be called once.
   */
  static nsresult Init();

  /**
   * Checks if we're collecting during a given generation
   */
  static bool InGeneration(uint32_t aGeneration)
  {
    return aGeneration && aGeneration == sGeneration;
  }

  template <class CCCallback>
  static bool InGeneration(CCCallback& aCb, uint32_t aGeneration)
  {
    return InGeneration(aGeneration) && !aCb.WantAllTraces();
  }

  static uint32_t sGeneration;

private:
  nsCCUncollectableMarker() {}

};

namespace mozilla {
namespace dom {
void TraceBlackJS(JSTracer* aTrc, uint32_t aGCNumber, bool aIsShutdownGC);
}
}

#endif
