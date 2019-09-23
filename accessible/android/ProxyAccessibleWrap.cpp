/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProxyAccessibleWrap.h"

#include "nsPersistentProperties.h"

#include "mozilla/a11y/DocAccessiblePlatformExtParent.h"

using namespace mozilla::a11y;

ProxyAccessibleWrap::ProxyAccessibleWrap(ProxyAccessible* aProxy)
    : AccessibleWrap(nullptr, nullptr) {
  mType = eProxyType;
  mBits.proxy = aProxy;

  if (aProxy->mHasValue) {
    mStateFlags |= eHasNumericValue;
  }

  if (aProxy->mIsSelection) {
    mGenericTypes |= eSelect;
  }

  if (aProxy->mIsHyperText) {
    mGenericTypes |= eHyperText;
  }

  auto doc = reinterpret_cast<DocProxyAccessibleWrap*>(
      Proxy()->Document()->GetWrapper());
  if (doc) {
    mID = AcquireID();
    doc->AddID(mID, this);
  }
}

void ProxyAccessibleWrap::Shutdown() {
  auto doc = reinterpret_cast<DocProxyAccessibleWrap*>(
      Proxy()->Document()->GetWrapper());
  if (mID && doc) {
    doc->RemoveID(mID);
    ReleaseID(mID);
    mID = 0;
  }

  mBits.proxy = nullptr;
  mStateFlags |= eIsDefunct;
}

// Accessible

already_AddRefed<nsIPersistentProperties> ProxyAccessibleWrap::Attributes() {
  AutoTArray<Attribute, 10> attrs;
  Proxy()->Attributes(&attrs);
  return AttributeArrayToProperties(attrs);
}

uint32_t ProxyAccessibleWrap::ChildCount() const {
  return Proxy()->ChildrenCount();
}

Accessible* ProxyAccessibleWrap::GetChildAt(uint32_t aIndex) const {
  ProxyAccessible* child = Proxy()->ChildAt(aIndex);
  return child ? WrapperFor(child) : nullptr;
}

ENameValueFlag ProxyAccessibleWrap::Name(nsString& aName) const {
  Proxy()->Name(aName);
  return eNameOK;
}

void ProxyAccessibleWrap::Value(nsString& aValue) const {
  Proxy()->Value(aValue);
}

uint64_t ProxyAccessibleWrap::State() { return Proxy()->State(); }

nsIntRect ProxyAccessibleWrap::Bounds() const { return Proxy()->Bounds(); }

void ProxyAccessibleWrap::ScrollTo(uint32_t aHow) const {
  Proxy()->ScrollTo(aHow);
}

uint8_t ProxyAccessibleWrap::ActionCount() const {
  return Proxy()->ActionCount();
}

bool ProxyAccessibleWrap::DoAction(uint8_t aIndex) const {
  return Proxy()->DoAction(aIndex);
}

// Other

void ProxyAccessibleWrap::SetTextContents(const nsAString& aText) {
  Proxy()->ReplaceText(PromiseFlatString(aText));
}

void ProxyAccessibleWrap::GetTextContents(nsAString& aText) {
  nsAutoString text;
  Proxy()->TextSubstring(0, -1, text);
  aText.Assign(text);
}

bool ProxyAccessibleWrap::GetSelectionBounds(int32_t* aStartOffset,
                                             int32_t* aEndOffset) {
  nsAutoString unused;
  return Proxy()->SelectionBoundsAt(0, unused, aStartOffset, aEndOffset);
}

void ProxyAccessibleWrap::Pivot(int32_t aGranularity, bool aForward,
                                bool aInclusive) {
  Unused << Proxy()->Document()->GetPlatformExtension()->SendPivot(
      Proxy()->ID(), aGranularity, aForward, aInclusive);
}

void ProxyAccessibleWrap::ExploreByTouch(float aX, float aY) {
  Unused << Proxy()->Document()->GetPlatformExtension()->SendExploreByTouch(
      Proxy()->ID(), aX, aY);
}

void ProxyAccessibleWrap::NavigateText(int32_t aGranularity,
                                       int32_t aStartOffset, int32_t aEndOffset,
                                       bool aForward, bool aSelect) {
  Unused << Proxy()->Document()->GetPlatformExtension()->SendNavigateText(
      Proxy()->ID(), aGranularity, aStartOffset, aEndOffset, aForward, aSelect);
}

void ProxyAccessibleWrap::SetSelection(int32_t aStart, int32_t aEnd) {
  Unused << Proxy()->Document()->GetPlatformExtension()->SendSetSelection(
      Proxy()->ID(), aStart, aEnd);
}

role ProxyAccessibleWrap::WrapperRole() { return Proxy()->Role(); }

AccessibleWrap* ProxyAccessibleWrap::WrapperParent() {
  return Proxy()->Parent() ? WrapperFor(Proxy()->Parent()) : nullptr;
}

bool ProxyAccessibleWrap::WrapperRangeInfo(double* aCurVal, double* aMinVal,
                                           double* aMaxVal, double* aStep) {
  if (HasNumericValue()) {
    ProxyAccessible* proxy = Proxy();
    *aCurVal = proxy->CurValue();
    *aMinVal = proxy->MinValue();
    *aMaxVal = proxy->MaxValue();
    *aStep = proxy->Step();
    return true;
  }

  return false;
}

void ProxyAccessibleWrap::WrapperDOMNodeID(nsString& aDOMNodeID) {
  Proxy()->DOMNodeID(aDOMNodeID);
}
