/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CoalescedMouseData_h
#define mozilla_dom_CoalescedMouseData_h

#include "CoalescedInputData.h"
#include "mozilla/MouseEvents.h"
#include "nsRefreshObservers.h"

class nsRefreshDriver;

namespace mozilla::dom {

class CoalescedMouseData final : public CoalescedInputData<WidgetMouseEvent> {
 public:
  CoalescedMouseData() { MOZ_COUNT_CTOR(mozilla::dom::CoalescedMouseData); }

  ~CoalescedMouseData() { MOZ_COUNT_DTOR(mozilla::dom::CoalescedMouseData); }

  void Coalesce(const WidgetMouseEvent& aEvent,
                const ScrollableLayerGuid& aGuid,
                const uint64_t& aInputBlockId);

  bool CanCoalesce(const WidgetMouseEvent& aEvent,
                   const ScrollableLayerGuid& aGuid,
                   const uint64_t& aInputBlockId);
};

class CoalescedMouseMoveFlusher final : public CoalescedInputFlusher {
 public:
  explicit CoalescedMouseMoveFlusher(BrowserChild* aBrowserChild);

  void WillRefresh(mozilla::TimeStamp aTime) override;

 private:
  ~CoalescedMouseMoveFlusher() override;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_CoalescedMouseData_h
