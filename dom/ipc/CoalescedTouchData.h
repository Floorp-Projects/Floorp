/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom__CoalescedTouchData_h
#define mozilla_dom__CoalescedTouchData_h

#include "CoalescedInputData.h"
#include "mozilla/TouchEvents.h"

namespace mozilla {
namespace dom {

class CoalescedTouchData final : public CoalescedInputData<WidgetTouchEvent> {
 public:
  void CreateCoalescedTouchEvent(const WidgetTouchEvent& aEvent);

  void Coalesce(const WidgetTouchEvent& aEvent,
                const ScrollableLayerGuid& aGuid, const uint64_t& aInputBlockId,
                const nsEventStatus& aApzResponse);

  bool CanCoalesce(const WidgetTouchEvent& aEvent,
                   const ScrollableLayerGuid& aGuid,
                   const uint64_t& aInputBlockId,
                   const nsEventStatus& aApzResponse);

  nsEventStatus GetApzResponse() { return mApzResponse; }

 private:
  Touch* GetTouch(int32_t aIdentifier);

  nsEventStatus mApzResponse = nsEventStatus_eIgnore;
};

class CoalescedTouchMoveFlusher final : public CoalescedInputFlusher {
 public:
  explicit CoalescedTouchMoveFlusher(BrowserChild* aBrowserChild);
  void WillRefresh(mozilla::TimeStamp aTime) override;

 private:
  ~CoalescedTouchMoveFlusher() override;
};
}  // namespace dom
};  // namespace mozilla

#endif  // mozilla_dom_CoalescedTouchData_h
