/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProxyAccessibleWrap.h"
#include "nsPersistentProperties.h"

using namespace mozilla::a11y;

ProxyAccessibleWrap::ProxyAccessibleWrap(ProxyAccessible* aProxy)
  : AccessibleWrap(nullptr, nullptr)
{
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

void
ProxyAccessibleWrap::Shutdown()
{
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

already_AddRefed<nsIPersistentProperties>
ProxyAccessibleWrap::Attributes()
{
  RefPtr<nsPersistentProperties> attributes = new nsPersistentProperties();
  nsAutoString unused;
  AutoTArray<Attribute, 10> attrs;
  Proxy()->Attributes(&attrs);
  for (size_t i = 0; i < attrs.Length(); i++) {
    attributes->SetStringProperty(
      attrs.ElementAt(i).Name(), attrs.ElementAt(i).Value(), unused);
  }

  return attributes.forget();
}

uint32_t
ProxyAccessibleWrap::ChildCount() const
{
  return Proxy()->ChildrenCount();
}

void
ProxyAccessibleWrap::ScrollTo(uint32_t aHow) const
{
  Proxy()->ScrollTo(aHow);
}

// Other

void
ProxyAccessibleWrap::SetTextContents(const nsAString& aText)
{
  Proxy()->ReplaceText(PromiseFlatString(aText));
}

void
ProxyAccessibleWrap::GetTextContents(nsAString& aText)
{
  nsAutoString text;
  Proxy()->TextSubstring(0, -1, text);
  aText.Assign(text);
}

bool
ProxyAccessibleWrap::GetSelectionBounds(int32_t* aStartOffset,
                                        int32_t* aEndOffset)
{
  nsAutoString unused;
  return Proxy()->SelectionBoundsAt(0, unused, aStartOffset, aEndOffset);
}

mozilla::java::GeckoBundle::LocalRef
ProxyAccessibleWrap::ToBundle()
{
  ProxyAccessible* proxy = Proxy();
  if (!proxy) {
    return nullptr;
  }

  int32_t parentID = proxy->Parent() ?
    WrapperFor(proxy->Parent())->VirtualViewID() : 0;

  nsAutoString name;
  proxy->Name(name);

  nsAutoString value;
  proxy->Value(value);

  nsAutoString viewIdResourceName;
  proxy->DOMNodeID(viewIdResourceName);

  nsCOMPtr<nsIPersistentProperties> attributes = Attributes();

  auto childCount = proxy->ChildrenCount();
  nsTArray<int32_t> children(childCount);
  for (uint32_t i = 0; i < childCount; i++) {
    auto child = WrapperFor(proxy->ChildAt(i));
    children.AppendElement(child->VirtualViewID());
  }

  return CreateBundle(parentID,
                      proxy->Role(),
                      proxy->State(),
                      name,
                      value,
                      viewIdResourceName,
                      proxy->Bounds(),
                      proxy->CurValue(),
                      proxy->MinValue(),
                      proxy->MaxValue(),
                      proxy->Step(),
                      attributes,
                      children);
}
