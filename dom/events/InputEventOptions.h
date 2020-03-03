/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_InputEventOptions_h
#define mozilla_InputEventOptions_h

#include "mozilla/Attributes.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TextEvents.h"
#include "mozilla/dom/DataTransfer.h"
#include "mozilla/dom/StaticRange.h"
#include "nsString.h"
#include "nsTArray.h"

namespace mozilla {

/**
 * InputEventOptions is used by nsContentUtils::DispatchInputEvent() to specify
 * some attributes of InputEvent.  It would be nice if this was in
 * nsContentUtils.h, however, it needs to include StaticRange.h for declaring
 * this.  That would cause unnecessary dependency and makes incremental build
 * slower when you touch StaticRange.h even though most nsContentUtils.h users
 * don't use it.  Therefore, this struct is declared in separated header file
 * here.
 */
struct MOZ_STACK_CLASS InputEventOptions final {
  InputEventOptions() = default;
  explicit InputEventOptions(const InputEventOptions& aOther) = delete;
  InputEventOptions(InputEventOptions&& aOther) = default;
  explicit InputEventOptions(const nsAString& aData)
      : mData(aData), mDataTransfer(nullptr) {}
  explicit InputEventOptions(dom::DataTransfer* aDataTransfer)
      : mDataTransfer(aDataTransfer) {
    MOZ_ASSERT(mDataTransfer);
    MOZ_ASSERT(mDataTransfer->IsReadOnly());
  }
  InputEventOptions(const nsAString& aData,
                    OwningNonNullStaticRangeArray&& aTargetRanges)
      : mData(aData),
        mDataTransfer(nullptr),
        mTargetRanges(std::move(aTargetRanges)) {}
  InputEventOptions(dom::DataTransfer* aDataTransfer,
                    OwningNonNullStaticRangeArray&& aTargetRanges)
      : mDataTransfer(aDataTransfer), mTargetRanges(std::move(aTargetRanges)) {
    MOZ_ASSERT(mDataTransfer);
    MOZ_ASSERT(mDataTransfer->IsReadOnly());
  }

  nsString mData;
  dom::DataTransfer* mDataTransfer;
  OwningNonNullStaticRangeArray mTargetRanges;
};

}  // namespace mozilla

#endif  // #ifndef mozilla_InputEventOptions_h