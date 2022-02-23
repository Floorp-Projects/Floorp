/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */

#ifndef _RTCSTATSIDGENERATOR_H_
#define _RTCSTATSIDGENERATOR_H_

#include <map>

#include "mozilla/Atomics.h"
#include "mozilla/UniquePtr.h"
#include "nsISupportsImpl.h"
#include "nsString.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {
struct RTCStatsCollection;
}  // namespace dom

class RTCStatsIdGenerator {
 public:
  RTCStatsIdGenerator();
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RTCStatsIdGenerator);

  void RewriteIds(nsTArray<UniquePtr<dom::RTCStatsCollection>> aFromStats,
                  dom::RTCStatsCollection* aIntoReport);

 private:
  virtual ~RTCStatsIdGenerator(){};
  nsString Id(const nsString& aKey);
  nsString Generate();

  const uint64_t mSalt;
  uint64_t mCounter;
  std::map<nsString, nsString> mAllocated;
};

}  // namespace mozilla
#endif
