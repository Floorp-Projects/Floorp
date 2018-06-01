/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CoalescedInputData_h
#define mozilla_dom_CoalescedInputData_h

#include "mozilla/UniquePtr.h"
#include "FrameMetrics.h"

namespace mozilla {
namespace dom {

template<class InputEventType>
class CoalescedInputData
{
protected:
  typedef mozilla::layers::ScrollableLayerGuid ScrollableLayerGuid;

  UniquePtr<InputEventType> mCoalescedInputEvent;
  ScrollableLayerGuid mGuid;
  uint64_t mInputBlockId;

public:
  CoalescedInputData()
    : mInputBlockId(0)
  {
  }

  void RetrieveDataFrom(CoalescedInputData& aSource)
  {
    mCoalescedInputEvent = std::move(aSource.mCoalescedInputEvent);
    mGuid = aSource.mGuid;
    mInputBlockId = aSource.mInputBlockId;
  }

  bool IsEmpty()
  {
    return !mCoalescedInputEvent;
  }

  bool CanCoalesce(const InputEventType& aEvent,
                   const ScrollableLayerGuid& aGuid,
                   const uint64_t& aInputBlockId);

  UniquePtr<InputEventType> TakeCoalescedEvent()
  {
    return std::move(mCoalescedInputEvent);
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

#endif // mozilla_dom_CoalescedInputData_h
