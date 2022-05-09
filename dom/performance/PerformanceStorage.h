/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PerformanceStorage_h
#define mozilla_dom_PerformanceStorage_h

#include "nsISupportsImpl.h"

class nsIHttpChannel;
class nsITimedChannel;

namespace mozilla::dom {

class PerformanceTimingData;

class PerformanceStorage {
 public:
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  virtual void AddEntry(nsIHttpChannel* aChannel,
                        nsITimedChannel* aTimedChannel) = 0;
  virtual void AddEntry(const nsString& entryName,
                        const nsString& initiatorType,
                        UniquePtr<PerformanceTimingData>&& aData) = 0;

 protected:
  virtual ~PerformanceStorage() = default;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_PerformanceStorage_h
