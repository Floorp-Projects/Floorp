/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteAccessibleWrap.h"
#include "LocalAccessible-inl.h"

#include "mozilla/a11y/DocAccessiblePlatformExtParent.h"

using namespace mozilla::a11y;

RemoteAccessibleWrap::RemoteAccessibleWrap(RemoteAccessible* aProxy)
    : AccessibleWrap(nullptr, nullptr) {
  mType = eProxyType;
  mBits.proxy = aProxy;

  if (aProxy->HasNumericValue()) {
    mGenericTypes |= eNumericValue;
  }

  if (aProxy->IsSelect()) {
    mGenericTypes |= eSelect;
  }

  if (aProxy->IsHyperText()) {
    mGenericTypes |= eHyperText;
  }

  auto doc = reinterpret_cast<DocRemoteAccessibleWrap*>(
      Proxy()->Document()->GetWrapper());
  if (doc) {
    mID = AcquireID();
    doc->AddID(mID, this);
  }
}

void RemoteAccessibleWrap::Shutdown() {
  auto doc = reinterpret_cast<DocRemoteAccessibleWrap*>(
      Proxy()->Document()->GetWrapper());
  if (mID && doc) {
    doc->RemoveID(mID);
    ReleaseID(mID);
    mID = 0;
  }

  mBits.proxy = nullptr;
  mStateFlags |= eIsDefunct;
}

// LocalAccessible

already_AddRefed<AccAttributes> RemoteAccessibleWrap::Attributes() {
  RefPtr<AccAttributes> attrs;
  Proxy()->Attributes(&attrs);
  return attrs.forget();
}

uint32_t RemoteAccessibleWrap::ChildCount() const {
  return Proxy()->ChildCount();
}

LocalAccessible* RemoteAccessibleWrap::LocalChildAt(uint32_t aIndex) const {
  RemoteAccessible* child = Proxy()->RemoteChildAt(aIndex);
  return child ? WrapperFor(child) : nullptr;
}

ENameValueFlag RemoteAccessibleWrap::Name(nsString& aName) const {
  Proxy()->Name(aName);
  return eNameOK;
}

void RemoteAccessibleWrap::Value(nsString& aValue) const {
  Proxy()->Value(aValue);
}

uint64_t RemoteAccessibleWrap::State() { return Proxy()->State(); }

nsIntRect RemoteAccessibleWrap::Bounds() const { return Proxy()->Bounds(); }

void RemoteAccessibleWrap::ScrollTo(uint32_t aHow) const {
  Proxy()->ScrollTo(aHow);
}

uint8_t RemoteAccessibleWrap::ActionCount() const {
  return Proxy()->ActionCount();
}

bool RemoteAccessibleWrap::DoAction(uint8_t aIndex) const {
  return Proxy()->DoAction(aIndex);
}

// Other

void RemoteAccessibleWrap::SetTextContents(const nsAString& aText) {
  Proxy()->ReplaceText(PromiseFlatString(aText));
}

void RemoteAccessibleWrap::GetTextContents(nsAString& aText) {
  nsAutoString text;
  Proxy()->TextSubstring(0, -1, text);
  aText.Assign(text);
}

bool RemoteAccessibleWrap::GetSelectionBounds(int32_t* aStartOffset,
                                              int32_t* aEndOffset) {
  nsAutoString unused;
  return Proxy()->SelectionBoundsAt(0, unused, aStartOffset, aEndOffset);
}

void RemoteAccessibleWrap::PivotTo(int32_t aGranularity, bool aForward,
                                   bool aInclusive) {
  Unused << Proxy()->Document()->GetPlatformExtension()->SendPivot(
      Proxy()->ID(), aGranularity, aForward, aInclusive);
}

void RemoteAccessibleWrap::ExploreByTouch(float aX, float aY) {
  Unused << Proxy()->Document()->GetPlatformExtension()->SendExploreByTouch(
      Proxy()->ID(), aX, aY);
}

void RemoteAccessibleWrap::NavigateText(int32_t aGranularity,
                                        int32_t aStartOffset,
                                        int32_t aEndOffset, bool aForward,
                                        bool aSelect) {
  Unused << Proxy()->Document()->GetPlatformExtension()->SendNavigateText(
      Proxy()->ID(), aGranularity, aStartOffset, aEndOffset, aForward, aSelect);
}

void RemoteAccessibleWrap::SetSelection(int32_t aStart, int32_t aEnd) {
  Unused << Proxy()->Document()->GetPlatformExtension()->SendSetSelection(
      Proxy()->ID(), aStart, aEnd);
}

void RemoteAccessibleWrap::Cut() {
  Unused << Proxy()->Document()->GetPlatformExtension()->SendCut(Proxy()->ID());
}

void RemoteAccessibleWrap::Copy() {
  Unused << Proxy()->Document()->GetPlatformExtension()->SendCopy(
      Proxy()->ID());
}

void RemoteAccessibleWrap::Paste() {
  Unused << Proxy()->Document()->GetPlatformExtension()->SendPaste(
      Proxy()->ID());
}

role RemoteAccessibleWrap::WrapperRole() { return Proxy()->Role(); }

AccessibleWrap* RemoteAccessibleWrap::WrapperParent() {
  return Proxy()->RemoteParent() ? WrapperFor(Proxy()->RemoteParent())
                                 : nullptr;
}

bool RemoteAccessibleWrap::WrapperRangeInfo(double* aCurVal, double* aMinVal,
                                            double* aMaxVal, double* aStep) {
  if (HasNumericValue()) {
    RemoteAccessible* proxy = Proxy();
    *aCurVal = proxy->CurValue();
    *aMinVal = proxy->MinValue();
    *aMaxVal = proxy->MaxValue();
    *aStep = proxy->Step();
    return true;
  }

  return false;
}

void RemoteAccessibleWrap::WrapperDOMNodeID(nsString& aDOMNodeID) {
  Proxy()->DOMNodeID(aDOMNodeID);
}
