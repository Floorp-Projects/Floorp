/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PerformanceEventCounts_h___
#define mozilla_dom_PerformanceEventCounts_h___

#include "nsCOMPtr.h"
#include "nsWrapperCache.h"

namespace mozilla::dom {
class EventCounts final : public nsWrapperCache {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(EventCounts)
  NS_DECL_CYCLE_COLLECTION_NATIVE_WRAPPERCACHE_CLASS(EventCounts)

  explicit EventCounts(nsISupports* aParent);

  nsISupports* GetParentObject() const { return mParent; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

 private:
  ~EventCounts() = default;
  nsCOMPtr<nsISupports> mParent;
};
}  // namespace mozilla::dom
#endif
