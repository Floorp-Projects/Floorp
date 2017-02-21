/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CoalescedWheelData_h
#define mozilla_dom_CoalescedWheelData_h

#include "mozilla/MouseEvents.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {
namespace dom {

class CoalescedWheelData final
{
protected:
  typedef mozilla::layers::ScrollableLayerGuid ScrollableLayerGuid;

public:
  UniquePtr<WidgetWheelEvent> mCoalescedWheelEvent;
  ScrollableLayerGuid mGuid;
  uint64_t mInputBlockId;

  CoalescedWheelData()
    : mInputBlockId(0)
  {
  }

  void Coalesce(const WidgetWheelEvent& aEvent,
                const ScrollableLayerGuid& aGuid,
                const uint64_t& aInputBlockId);
  void Reset()
  {
    mCoalescedWheelEvent = nullptr;
  }

  bool IsEmpty()
  {
    return !mCoalescedWheelEvent;
  }

  bool CanCoalesce(const WidgetWheelEvent& aEvent,
                   const ScrollableLayerGuid& aGuid,
                   const uint64_t& aInputBlockId);

  const WidgetWheelEvent* GetCoalescedWheelEvent()
  {
    return mCoalescedWheelEvent.get();
  }

  ScrollableLayerGuid GetScrollableLayerGuid()
  {
    return mGuid;
  }

  uint64_t GetInputBlockId()
  {
    return mInputBlockId;
  }
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_CoalescedWheelData_h
