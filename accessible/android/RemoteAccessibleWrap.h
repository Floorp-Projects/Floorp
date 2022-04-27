/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. *
 */

#ifndef MOZILLA_A11Y_RemoteAccessibleWrap_h
#define MOZILLA_A11Y_RemoteAccessibleWrap_h

#include "AccessibleWrap.h"
#include "DocAccessibleParent.h"

namespace mozilla {
namespace a11y {

/**
 * A wrapper for Accessible proxies. The public methods here should be
 * overriden from AccessibleWrap or its super classes. This gives us an
 * abstraction layer so SessionAccessibility doesn't have to distinguish between
 * a local or remote accessibles. NOTE: This shouldn't be regarded as a full
 * Accessible implementation.
 */
class RemoteAccessibleWrap : public AccessibleWrap {
 public:
  explicit RemoteAccessibleWrap(RemoteAccessible* aProxy);

  virtual void Shutdown() override;

  // LocalAccessible

  virtual already_AddRefed<AccAttributes> Attributes() override;

  virtual uint32_t ChildCount() const override;

  virtual LocalAccessible* LocalChildAt(uint32_t aIndex) const override;

  virtual ENameValueFlag Name(nsString& aName) const override;

  virtual void Description(nsString& aDescription) const override;

  virtual void Value(nsString& aValue) const override;

  virtual uint64_t State() override;

  virtual LayoutDeviceIntRect Bounds() const override;

  MOZ_CAN_RUN_SCRIPT
  virtual void ScrollTo(uint32_t aHow) const override;

  virtual uint8_t ActionCount() const override;

  virtual bool DoAction(uint8_t aIndex) const override;

  // AccessibleWrap

  virtual void SetTextContents(const nsAString& aText) override;

  virtual void GetTextContents(nsAString& aText) override;

  virtual bool GetSelectionBounds(int32_t* aStartOffset,
                                  int32_t* aEndOffset) override;

  virtual bool PivotTo(int32_t aGranularity, bool aForward,
                       bool aInclusive) override;

  virtual void NavigateText(int32_t aGranularity, int32_t aStartOffset,
                            int32_t aEndOffset, bool aForward,
                            bool aSelect) override;

  virtual void SetSelection(int32_t aStart, int32_t aEnd) override;

  virtual void Cut() override;

  virtual void Copy() override;

  virtual void Paste() override;

  virtual void WrapperDOMNodeID(nsString& aDOMNodeID) override;

  virtual role WrapperRole() override;

  virtual AccessibleWrap* WrapperParent() override;

  virtual bool WrapperRangeInfo(double* aCurVal, double* aMinVal,
                                double* aMaxVal, double* aStep) override;
};

}  // namespace a11y
}  // namespace mozilla

#endif
