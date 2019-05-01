/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_FILTERNODECAPTURE_H_
#define MOZILLA_GFX_FILTERNODECAPTURE_H_

#include "2D.h"
#include "Filters.h"
#include <unordered_map>
#include <vector>
#include "mozilla/Variant.h"

namespace mozilla {
namespace gfx {

class FilterNodeCapture final : public FilterNode {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(FilterNodeCapture, override)

  explicit FilterNodeCapture(FilterType aType)
      : mType{aType}, mInputsChanged{true} {}

  FilterBackend GetBackendType() override { return FILTER_BACKEND_CAPTURE; }

  RefPtr<FilterNode> Validate(DrawTarget* aDT);

  template <typename T, typename C>
  void Replace(uint32_t aIndex, const T& aValue, C& aContainer) {
    // This replace function avoids generating the hash twice.
    auto result = aContainer.insert({aIndex, typename C::mapped_type(aValue)});
    if (!result.second) {
      result.first->second = typename C::mapped_type(aValue);
    }
  }

  void SetInput(uint32_t aIndex, SourceSurface* aSurface) override {
    mInputsChanged = true;
    Replace(aIndex, RefPtr<SourceSurface>(aSurface), mInputs);
  }
  void SetInput(uint32_t aIndex, FilterNode* aFilter) override {
    mInputsChanged = true;
    Replace(aIndex, RefPtr<FilterNode>(aFilter), mInputs);
  }

  using AttributeValue =
      Variant<uint32_t, Float, Point, Matrix5x4, Point3D, Size, IntSize, Color,
              Rect, IntRect, bool, std::vector<Float>, IntPoint, Matrix>;

  void SetAttribute(uint32_t aIndex, uint32_t aValue) override {
    Replace(aIndex, aValue, mAttributes);
  }
  void SetAttribute(uint32_t aIndex, Float aValue) override {
    Replace(aIndex, aValue, mAttributes);
  }
  void SetAttribute(uint32_t aIndex, const Point& aValue) override {
    Replace(aIndex, aValue, mAttributes);
  }
  void SetAttribute(uint32_t aIndex, const Matrix5x4& aValue) override {
    Replace(aIndex, aValue, mAttributes);
  }
  void SetAttribute(uint32_t aIndex, const Point3D& aValue) override {
    Replace(aIndex, aValue, mAttributes);
  }
  void SetAttribute(uint32_t aIndex, const Size& aValue) override {
    Replace(aIndex, aValue, mAttributes);
  }
  void SetAttribute(uint32_t aIndex, const IntSize& aValue) override {
    Replace(aIndex, aValue, mAttributes);
  }
  void SetAttribute(uint32_t aIndex, const Color& aValue) override {
    Replace(aIndex, aValue, mAttributes);
  }
  void SetAttribute(uint32_t aIndex, const Rect& aValue) override {
    Replace(aIndex, aValue, mAttributes);
  }
  void SetAttribute(uint32_t aIndex, const IntRect& aValue) override {
    Replace(aIndex, aValue, mAttributes);
  }
  void SetAttribute(uint32_t aIndex, bool aValue) override {
    Replace(aIndex, aValue, mAttributes);
  }
  virtual void SetAttribute(uint32_t aIndex, const Float* aValues,
                            uint32_t aSize) override;
  void SetAttribute(uint32_t aIndex, const IntPoint& aValue) override {
    Replace(aIndex, aValue, mAttributes);
  }
  void SetAttribute(uint32_t aIndex, const Matrix& aValue) override {
    Replace(aIndex, aValue, mAttributes);
  }

 private:
  FilterType mType;
  RefPtr<FilterNode> mFilterNodeInternal;

  // This only contains attributes that were set since the last validation.
  std::unordered_map<uint32_t, AttributeValue> mAttributes;

  // This always contains all inputs, so that Validate may be called on input
  // filter nodes.
  std::unordered_map<uint32_t,
                     Variant<RefPtr<SourceSurface>, RefPtr<FilterNode>>>
      mInputs;

  // This is true if SetInput was called since the last validation.
  bool mInputsChanged;
};

}  // namespace gfx
}  // namespace mozilla

#endif
