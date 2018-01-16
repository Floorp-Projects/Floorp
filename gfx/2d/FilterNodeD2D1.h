/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_FILTERNODED2D1_H_
#define MOZILLA_GFX_FILTERNODED2D1_H_

#include "2D.h"
#include "Filters.h"
#include <vector>
#include <d2d1_1.h>
#include <cguid.h>

namespace mozilla {
namespace gfx {

class FilterNodeD2D1 : public FilterNode
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(FilterNodeD2D1, override)

  static already_AddRefed<FilterNode> Create(ID2D1DeviceContext *aDC, FilterType aType);

  FilterNodeD2D1(ID2D1Effect *aEffect, FilterType aType)
    : mEffect(aEffect)
    , mType(aType)
  {
    InitUnmappedProperties();
  }

  virtual FilterBackend GetBackendType() { return FILTER_BACKEND_DIRECT2D1_1; }

  virtual void SetInput(uint32_t aIndex, SourceSurface *aSurface);
  virtual void SetInput(uint32_t aIndex, FilterNode *aFilter);

  virtual void SetAttribute(uint32_t aIndex, uint32_t aValue);
  virtual void SetAttribute(uint32_t aIndex, Float aValue);
  virtual void SetAttribute(uint32_t aIndex, const Point &aValue);
  virtual void SetAttribute(uint32_t aIndex, const Matrix5x4 &aValue);
  virtual void SetAttribute(uint32_t aIndex, const Point3D &aValue);
  virtual void SetAttribute(uint32_t aIndex, const Size &aValue);
  virtual void SetAttribute(uint32_t aIndex, const IntSize &aValue);
  virtual void SetAttribute(uint32_t aIndex, const Color &aValue);
  virtual void SetAttribute(uint32_t aIndex, const Rect &aValue);
  virtual void SetAttribute(uint32_t aIndex, const IntRect &aValue);
  virtual void SetAttribute(uint32_t aIndex, bool aValue);
  virtual void SetAttribute(uint32_t aIndex, const Float *aValues, uint32_t aSize);
  virtual void SetAttribute(uint32_t aIndex, const IntPoint &aValue);
  virtual void SetAttribute(uint32_t aIndex, const Matrix &aValue);

  // Called by DrawTarget before it draws our OutputEffect, and recursively
  // by the filter nodes that have this filter as one of their inputs. This
  // gives us a chance to convert any input surfaces to the target format for
  // the DrawTarget that we will draw to.
  virtual void WillDraw(DrawTarget *aDT);

  virtual ID2D1Effect* MainEffect() { return mEffect.get(); }
  virtual ID2D1Effect* InputEffect() { return mEffect.get(); }
  virtual ID2D1Effect* OutputEffect() { return mEffect.get(); }

protected:
  friend class DrawTargetD2D1;
  friend class DrawTargetD2D;
  friend class FilterNodeConvolveD2D1;

  void InitUnmappedProperties();

  RefPtr<ID2D1Effect> mEffect;
  std::vector<RefPtr<FilterNodeD2D1>> mInputFilters;
  std::vector<RefPtr<SourceSurface>> mInputSurfaces;
  FilterType mType;

private:
  using FilterNode::SetAttribute;
  using FilterNode::SetInput;
};

class FilterNodeConvolveD2D1 : public FilterNodeD2D1
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(FilterNodeConvolveD2D1, override)
  explicit FilterNodeConvolveD2D1(ID2D1DeviceContext *aDC);

  virtual void SetInput(uint32_t aIndex, FilterNode *aFilter) override;

  virtual void SetAttribute(uint32_t aIndex, uint32_t aValue) override;
  virtual void SetAttribute(uint32_t aIndex, const IntSize &aValue) override;
  virtual void SetAttribute(uint32_t aIndex, const IntPoint &aValue) override;
  virtual void SetAttribute(uint32_t aIndex, const IntRect &aValue) override;

  virtual ID2D1Effect* InputEffect() override;

private:
  using FilterNode::SetAttribute;
  using FilterNode::SetInput;

  void UpdateChain();
  void UpdateOffset();
  void UpdateSourceRect();

  RefPtr<ID2D1Effect> mExtendInputEffect;
  RefPtr<ID2D1Effect> mBorderEffect;
  ConvolveMatrixEdgeMode mEdgeMode;
  IntPoint mTarget;
  IntSize mKernelSize;
  IntRect mSourceRect;
};

class FilterNodeExtendInputAdapterD2D1 : public FilterNodeD2D1
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(FilterNodeExtendInputAdapterD2D1, override)
  FilterNodeExtendInputAdapterD2D1(ID2D1DeviceContext *aDC, FilterNodeD2D1 *aFilterNode, FilterType aType);

  virtual ID2D1Effect* InputEffect() override { return mExtendInputEffect.get(); }
  virtual ID2D1Effect* OutputEffect() override { return mWrappedFilterNode->OutputEffect(); }

private:
  RefPtr<FilterNodeD2D1> mWrappedFilterNode;
  RefPtr<ID2D1Effect> mExtendInputEffect;
};

class FilterNodePremultiplyAdapterD2D1 : public FilterNodeD2D1
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(FilterNodePremultiplyAdapterD2D1, override)
  FilterNodePremultiplyAdapterD2D1(ID2D1DeviceContext *aDC, FilterNodeD2D1 *aFilterNode, FilterType aType);

  virtual ID2D1Effect* InputEffect() override { return mPrePremultiplyEffect.get(); }
  virtual ID2D1Effect* OutputEffect() override { return mPostUnpremultiplyEffect.get(); }

private:
  RefPtr<ID2D1Effect> mPrePremultiplyEffect;
  RefPtr<ID2D1Effect> mPostUnpremultiplyEffect;
};

}
}

#endif
