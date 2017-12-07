/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FilterNodeCapture.h"

namespace mozilla {
namespace gfx {

struct Setter
{
  Setter(FilterNode* aNode, DrawTarget* aDT, bool aInputsChanged)
    : mNode{aNode}, mIndex{0}, mDT{aDT}, mInputsChanged{aInputsChanged} {}
  template<typename T>
  void match(T& aValue) { mNode->SetAttribute(mIndex, aValue); }

  FilterNode* mNode;
  uint32_t mIndex;
  DrawTarget* mDT;
  bool mInputsChanged;
};

template<>
void
Setter::match<std::vector<Float>>(std::vector<Float>& aValue)
{
  mNode->SetAttribute(mIndex, aValue.data(), aValue.size());
}

template<>
void
Setter::match<RefPtr<SourceSurface>>(RefPtr<SourceSurface>& aSurface)
{
  if (!mInputsChanged) {
    return;
  }
  mNode->SetInput(mIndex, aSurface);
}

template<>
void
Setter::match<RefPtr<FilterNode>>(RefPtr<FilterNode>& aNode)
{
  RefPtr<FilterNode> node = aNode;
  if (node->GetBackendType() == FilterBackend::FILTER_BACKEND_CAPTURE) {
    FilterNodeCapture* captureNode = static_cast<FilterNodeCapture*>(node.get());
    node = captureNode->Validate(mDT);
  }
  if (!mInputsChanged) {
    return;
  }

  mNode->SetInput(mIndex, node);
}

RefPtr<FilterNode>
FilterNodeCapture::Validate(DrawTarget* aDT)
{
  if (!mFilterNodeInternal) {
    mFilterNodeInternal = aDT->CreateFilter(mType);
  }

  if (!mFilterNodeInternal) {
    return nullptr;
  }

  Setter setter(mFilterNodeInternal, aDT, mInputsChanged);

  for (auto attribute : mAttributes)
  {
    setter.mIndex = attribute.first;
    // Variant's matching doesn't seem to compile to terribly efficient code,
    // this is probably fine since this happens on the paint thread, if ever
    // needed it would be fairly simple to write something more optimized.
    attribute.second.match(setter);
  }
  mAttributes.clear();

  for (auto input : mInputs) {
    setter.mIndex = input.first;
    input.second.match(setter);
  }
  mInputsChanged = false;

  return mFilterNodeInternal.get();
}

void
FilterNodeCapture::SetAttribute(uint32_t aIndex, const Float *aValues, uint32_t aSize)
{
  std::vector<Float> vec(aSize);
  memcpy(vec.data(), aValues, sizeof(Float) * aSize);
  AttributeValue att(std::move(vec));
  auto result = mAttributes.insert({ aIndex, att });
  if (!result.second) {
    result.first->second = att;
  }
}

}
}
