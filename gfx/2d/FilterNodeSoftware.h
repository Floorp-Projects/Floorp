/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MOZILLA_GFX_FILTERNODESOFTWARE_H_
#define _MOZILLA_GFX_FILTERNODESOFTWARE_H_

#include "Filters.h"
#include "mozilla/Mutex.h"
#include <vector>

namespace mozilla {
namespace gfx {

class DataSourceSurface;
class DrawTarget;
struct DrawOptions;
class FilterNodeSoftware;

/**
 * Can be attached to FilterNodeSoftware instances using
 * AddInvalidationListener. FilterInvalidated is called whenever the output of
 * the observed filter may have changed; that is, whenever cached GetOutput()
 * results (and results derived from them) need to discarded.
 */
class FilterInvalidationListener {
 public:
  virtual void FilterInvalidated(FilterNodeSoftware* aFilter) = 0;
};

/**
 * This is the base class for the software (i.e. pure CPU, non-accelerated)
 * FilterNode implementation. The software implementation is backend-agnostic,
 * so it can be used as a fallback for all DrawTarget implementations.
 */
class FilterNodeSoftware : public FilterNode,
                           public FilterInvalidationListener {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(FilterNodeSoftware, override)
  FilterNodeSoftware();
  virtual ~FilterNodeSoftware();

  // Factory method, intended to be called from DrawTarget*::CreateFilter.
  static already_AddRefed<FilterNode> Create(FilterType aType);

  // Draw the filter, intended to be called by DrawTarget*::DrawFilter.
  void Draw(DrawTarget* aDrawTarget, const Rect& aSourceRect,
            const Point& aDestPoint, const DrawOptions& aOptions);

  FilterBackend GetBackendType() override { return FILTER_BACKEND_SOFTWARE; }
  void SetInput(uint32_t aIndex, SourceSurface* aSurface) override;
  void SetInput(uint32_t aIndex, FilterNode* aFilter) override;

  virtual const char* GetName() { return "Unknown"; }

  void AddInvalidationListener(FilterInvalidationListener* aListener);
  void RemoveInvalidationListener(FilterInvalidationListener* aListener);

  // FilterInvalidationListener implementation
  void FilterInvalidated(FilterNodeSoftware* aFilter) override;

 protected:
  // The following methods are intended to be overriden by subclasses.

  /**
   * Translates a *FilterInputs enum value into an index for the
   * mInputFilters / mInputSurfaces arrays. Returns -1 for invalid inputs.
   * If somebody calls SetInput(enumValue, input) with an enumValue for which
   * InputIndex(enumValue) is -1, we abort.
   */
  virtual int32_t InputIndex(uint32_t aInputEnumIndex) { return -1; }

  /**
   * Every filter node has an output rect, which can also be infinite. The
   * output rect can depend on the values of any set attributes and on the
   * output rects of any input filters or surfaces.
   * This method returns the intersection of the filter's output rect with
   * aInRect. Filters with unconstrained output always return aInRect.
   */
  virtual IntRect GetOutputRectInRect(const IntRect& aInRect) = 0;

  /**
   * Return a surface with the rendered output which is of size aRect.Size().
   * aRect is required to be a subrect of this filter's output rect; in other
   * words, aRect == GetOutputRectInRect(aRect) must always be true.
   * May return nullptr in error conditions or for an empty aRect.
   * Implementations are not required to allocate a new surface and may even
   * pass through input surfaces unchanged.
   * Callers need to treat the returned surface as immutable.
   */
  virtual already_AddRefed<DataSourceSurface> Render(const IntRect& aRect) = 0;

  /**
   * Call RequestRect (see below) on any input filters with the desired input
   * rect, so that the input filter knows what to cache the next time it
   * renders.
   */
  virtual void RequestFromInputsForRect(const IntRect& aRect) {}

  /**
   * This method provides a caching default implementation but can be overriden
   * by subclasses that don't want to cache their output. Those classes should
   * call Render(aRect) directly from here.
   */
  virtual already_AddRefed<DataSourceSurface> GetOutput(const IntRect& aRect);

  // The following methods are non-virtual helper methods.

  /**
   * Format hints for GetInputDataSourceSurface. Some callers of
   * GetInputDataSourceSurface can handle both B8G8R8A8 and A8 surfaces, these
   * should pass CAN_HANDLE_A8 in order to avoid unnecessary conversions.
   * Callers that can only handle B8G8R8A8 surfaces pass NEED_COLOR_CHANNELS.
   */
  enum FormatHint { CAN_HANDLE_A8, NEED_COLOR_CHANNELS };

  /**
   * Returns SurfaceFormat::B8G8R8A8 or SurfaceFormat::A8, depending on the
   * current surface format and the format hint.
   */
  SurfaceFormat DesiredFormat(SurfaceFormat aCurrentFormat,
                              FormatHint aFormatHint);

  /**
   * Intended to be called by FilterNodeSoftware::Render implementations.
   * Returns a surface of size aRect.Size() or nullptr in error conditions. The
   * returned surface contains the output of the specified input filter or
   * input surface in aRect. If aRect extends beyond the input filter's output
   * rect (or the input surface's dimensions), the remaining area is filled
   * according to aEdgeMode: The default, EDGE_MODE_NONE, simply pads with
   * transparent black.
   * If non-null, the returned surface is guaranteed to be of SurfaceFormat::A8
   * or SurfaceFormat::B8G8R8A8. If aFormatHint is NEED_COLOR_CHANNELS, the
   * returned surface is guaranteed to be of SurfaceFormat::B8G8R8A8 always.
   * Each pixel row of the returned surface is guaranteed to be 16-byte aligned.
   */
  already_AddRefed<DataSourceSurface> GetInputDataSourceSurface(
      uint32_t aInputEnumIndex, const IntRect& aRect,
      FormatHint aFormatHint = CAN_HANDLE_A8,
      ConvolveMatrixEdgeMode aEdgeMode = EDGE_MODE_NONE,
      const IntRect* aTransparencyPaddedSourceRect = nullptr);

  /**
   * Returns the intersection of the input filter's or surface's output rect
   * with aInRect.
   */
  IntRect GetInputRectInRect(uint32_t aInputEnumIndex, const IntRect& aInRect);

  /**
   * Calls RequestRect on the specified input, if it's a filter.
   */
  void RequestInputRect(uint32_t aInputEnumIndex, const IntRect& aRect);

  /**
   * Calls MapRectToSource on the specified input, if it's a filter.
   */
  IntRect MapInputRectToSource(uint32_t aInputEnumIndex, const IntRect& aRect,
                               const IntRect& aMax, FilterNode* aSourceNode);

  /**
   * Returns the number of set input filters or surfaces. Needed for filters
   * which can have an arbitrary number of inputs.
   */
  size_t NumberOfSetInputs();

  /**
   * Discard the cached surface that was stored in the GetOutput default
   * implementation. Needs to be called whenever attributes or inputs are set
   * that might change the result of a Render() call.
   */
  void Invalidate();

  /**
   * Called in order to let this filter know what to cache during the next
   * GetOutput call. Expected to call RequestRect on this filter's input
   * filters.
   */
  void RequestRect(const IntRect& aRect);

  /**
   * Set input filter and clear input surface for this input index, or set
   * input surface and clear input filter. One of aSurface and aFilter should
   * be null.
   */
  void SetInput(uint32_t aIndex, SourceSurface* aSurface,
                FilterNodeSoftware* aFilter);

 protected:
  /**
   * mInputSurfaces / mInputFilters: For each input index, either a surface or
   * a filter is set, and the other is null.
   */
  std::vector<RefPtr<SourceSurface> > mInputSurfaces;
  std::vector<RefPtr<FilterNodeSoftware> > mInputFilters;

  /**
   * Weak pointers to our invalidation listeners, i.e. to those filters who
   * have this filter as an input. Invalidation listeners are required to
   * unsubscribe themselves from us when they let go of their reference to us.
   * This ensures that the pointers in this array are never stale.
   */
  std::vector<FilterInvalidationListener*> mInvalidationListeners;

  /**
   * Lock guarding mRequestedRect, mCachedRect, and mCachedOutput. All uses
   * of those members must aquire this lock.
   */
  Mutex mCacheMutex;

  /**
   * Stores the rect which we want to render and cache on the next call to
   * GetOutput.
   */
  IntRect mRequestedRect;

  /**
   * Stores our cached output.
   */
  IntRect mCachedRect;
  RefPtr<DataSourceSurface> mCachedOutput;
};

// Subclasses for specific filters.

class FilterNodeTransformSoftware : public FilterNodeSoftware {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(FilterNodeTransformSoftware, override)
  FilterNodeTransformSoftware();
  const char* GetName() override { return "Transform"; }
  using FilterNodeSoftware::SetAttribute;
  void SetAttribute(uint32_t aIndex, uint32_t aGraphicsFilter) override;
  void SetAttribute(uint32_t aIndex, const Matrix& aMatrix) override;
  IntRect MapRectToSource(const IntRect& aRect, const IntRect& aMax,
                          FilterNode* aSourceNode) override;

 protected:
  already_AddRefed<DataSourceSurface> Render(const IntRect& aRect) override;
  IntRect GetOutputRectInRect(const IntRect& aRect) override;
  int32_t InputIndex(uint32_t aInputEnumIndex) override;
  void RequestFromInputsForRect(const IntRect& aRect) override;
  IntRect SourceRectForOutputRect(const IntRect& aRect);

 private:
  Matrix mMatrix;
  SamplingFilter mSamplingFilter;
};

class FilterNodeBlendSoftware : public FilterNodeSoftware {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(FilterNodeBlendSoftware, override)
  FilterNodeBlendSoftware();
  const char* GetName() override { return "Blend"; }
  using FilterNodeSoftware::SetAttribute;
  void SetAttribute(uint32_t aIndex, uint32_t aBlendMode) override;
  IntRect MapRectToSource(const IntRect& aRect, const IntRect& aMax,
                          FilterNode* aSourceNode) override;

 protected:
  already_AddRefed<DataSourceSurface> Render(const IntRect& aRect) override;
  IntRect GetOutputRectInRect(const IntRect& aRect) override;
  int32_t InputIndex(uint32_t aInputEnumIndex) override;
  void RequestFromInputsForRect(const IntRect& aRect) override;

 private:
  BlendMode mBlendMode;
};

class FilterNodeMorphologySoftware : public FilterNodeSoftware {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(FilterNodeMorphologySoftware,
                                          override)
  FilterNodeMorphologySoftware();
  const char* GetName() override { return "Morphology"; }
  using FilterNodeSoftware::SetAttribute;
  void SetAttribute(uint32_t aIndex, const IntSize& aRadii) override;
  void SetAttribute(uint32_t aIndex, uint32_t aOperator) override;

 protected:
  already_AddRefed<DataSourceSurface> Render(const IntRect& aRect) override;
  IntRect GetOutputRectInRect(const IntRect& aRect) override;
  int32_t InputIndex(uint32_t aInputEnumIndex) override;
  void RequestFromInputsForRect(const IntRect& aRect) override;

 private:
  IntSize mRadii;
  MorphologyOperator mOperator;
};

class FilterNodeColorMatrixSoftware : public FilterNodeSoftware {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(FilterNodeColorMatrixSoftware,
                                          override)
  const char* GetName() override { return "ColorMatrix"; }
  using FilterNodeSoftware::SetAttribute;
  void SetAttribute(uint32_t aIndex, const Matrix5x4& aMatrix) override;
  void SetAttribute(uint32_t aIndex, uint32_t aAlphaMode) override;

 protected:
  already_AddRefed<DataSourceSurface> Render(const IntRect& aRect) override;
  IntRect GetOutputRectInRect(const IntRect& aRect) override;
  int32_t InputIndex(uint32_t aInputEnumIndex) override;
  void RequestFromInputsForRect(const IntRect& aRect) override;
  IntRect MapRectToSource(const IntRect& aRect, const IntRect& aMax,
                          FilterNode* aSourceNode) override;

 private:
  Matrix5x4 mMatrix;
  AlphaMode mAlphaMode;
};

class FilterNodeFloodSoftware : public FilterNodeSoftware {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(FilterNodeFloodSoftware, override)
  const char* GetName() override { return "Flood"; }
  using FilterNodeSoftware::SetAttribute;
  void SetAttribute(uint32_t aIndex, const Color& aColor) override;
  IntRect MapRectToSource(const IntRect& aRect, const IntRect& aMax,
                          FilterNode* aSourceNode) override;

 protected:
  already_AddRefed<DataSourceSurface> GetOutput(const IntRect& aRect) override;
  already_AddRefed<DataSourceSurface> Render(const IntRect& aRect) override;
  IntRect GetOutputRectInRect(const IntRect& aRect) override;

 private:
  Color mColor;
};

class FilterNodeTileSoftware : public FilterNodeSoftware {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(FilterNodeTileSoftware, override)
  const char* GetName() override { return "Tile"; }
  using FilterNodeSoftware::SetAttribute;
  void SetAttribute(uint32_t aIndex, const IntRect& aSourceRect) override;

 protected:
  already_AddRefed<DataSourceSurface> Render(const IntRect& aRect) override;
  IntRect GetOutputRectInRect(const IntRect& aRect) override;
  int32_t InputIndex(uint32_t aInputEnumIndex) override;
  void RequestFromInputsForRect(const IntRect& aRect) override;

 private:
  IntRect mSourceRect;
};

/**
 * Baseclass for the four different component transfer filters.
 */
class FilterNodeComponentTransferSoftware : public FilterNodeSoftware {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(FilterNodeComponentTransferSoftware,
                                          override)
  FilterNodeComponentTransferSoftware();

  using FilterNodeSoftware::SetAttribute;
  void SetAttribute(uint32_t aIndex, bool aDisable) override;
  IntRect MapRectToSource(const IntRect& aRect, const IntRect& aMax,
                          FilterNode* aSourceNode) override;

 protected:
  already_AddRefed<DataSourceSurface> Render(const IntRect& aRect) override;
  IntRect GetOutputRectInRect(const IntRect& aRect) override;
  int32_t InputIndex(uint32_t aInputEnumIndex) override;
  void RequestFromInputsForRect(const IntRect& aRect) override;
  virtual void GenerateLookupTable(ptrdiff_t aComponent,
                                   uint8_t aTables[4][256], bool aDisabled);
  virtual void FillLookupTable(ptrdiff_t aComponent, uint8_t aTable[256]) = 0;

  bool mDisableR;
  bool mDisableG;
  bool mDisableB;
  bool mDisableA;
};

class FilterNodeTableTransferSoftware
    : public FilterNodeComponentTransferSoftware {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(FilterNodeTableTransferSoftware,
                                          override)
  const char* GetName() override { return "TableTransfer"; }
  using FilterNodeComponentTransferSoftware::SetAttribute;
  void SetAttribute(uint32_t aIndex, const Float* aFloat,
                    uint32_t aSize) override;

 protected:
  void FillLookupTable(ptrdiff_t aComponent, uint8_t aTable[256]) override;

 private:
  void FillLookupTableImpl(std::vector<Float>& aTableValues,
                           uint8_t aTable[256]);

  std::vector<Float> mTableR;
  std::vector<Float> mTableG;
  std::vector<Float> mTableB;
  std::vector<Float> mTableA;
};

class FilterNodeDiscreteTransferSoftware
    : public FilterNodeComponentTransferSoftware {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(FilterNodeDiscreteTransferSoftware,
                                          override)
  const char* GetName() override { return "DiscreteTransfer"; }
  using FilterNodeComponentTransferSoftware::SetAttribute;
  void SetAttribute(uint32_t aIndex, const Float* aFloat,
                    uint32_t aSize) override;

 protected:
  void FillLookupTable(ptrdiff_t aComponent, uint8_t aTable[256]) override;

 private:
  void FillLookupTableImpl(std::vector<Float>& aTableValues,
                           uint8_t aTable[256]);

  std::vector<Float> mTableR;
  std::vector<Float> mTableG;
  std::vector<Float> mTableB;
  std::vector<Float> mTableA;
};

class FilterNodeLinearTransferSoftware
    : public FilterNodeComponentTransferSoftware {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(FilterNodeLinearTransformSoftware,
                                          override)
  FilterNodeLinearTransferSoftware();
  const char* GetName() override { return "LinearTransfer"; }
  using FilterNodeComponentTransferSoftware::SetAttribute;
  void SetAttribute(uint32_t aIndex, Float aValue) override;

 protected:
  void FillLookupTable(ptrdiff_t aComponent, uint8_t aTable[256]) override;

 private:
  void FillLookupTableImpl(Float aSlope, Float aIntercept, uint8_t aTable[256]);

  Float mSlopeR;
  Float mSlopeG;
  Float mSlopeB;
  Float mSlopeA;
  Float mInterceptR;
  Float mInterceptG;
  Float mInterceptB;
  Float mInterceptA;
};

class FilterNodeGammaTransferSoftware
    : public FilterNodeComponentTransferSoftware {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(FilterNodeGammaTransferSoftware,
                                          override)
  FilterNodeGammaTransferSoftware();
  const char* GetName() override { return "GammaTransfer"; }
  using FilterNodeComponentTransferSoftware::SetAttribute;
  void SetAttribute(uint32_t aIndex, Float aValue) override;

 protected:
  void FillLookupTable(ptrdiff_t aComponent, uint8_t aTable[256]) override;

 private:
  void FillLookupTableImpl(Float aAmplitude, Float aExponent, Float aOffset,
                           uint8_t aTable[256]);

  Float mAmplitudeR;
  Float mAmplitudeG;
  Float mAmplitudeB;
  Float mAmplitudeA;
  Float mExponentR;
  Float mExponentG;
  Float mExponentB;
  Float mExponentA;
  Float mOffsetR;
  Float mOffsetG;
  Float mOffsetB;
  Float mOffsetA;
};

class FilterNodeConvolveMatrixSoftware : public FilterNodeSoftware {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(FilterNodeConvolveMatrixSoftware,
                                          override)
  FilterNodeConvolveMatrixSoftware();
  const char* GetName() override { return "ConvolveMatrix"; }
  using FilterNodeSoftware::SetAttribute;
  void SetAttribute(uint32_t aIndex, const IntSize& aKernelSize) override;
  void SetAttribute(uint32_t aIndex, const Float* aMatrix,
                    uint32_t aSize) override;
  void SetAttribute(uint32_t aIndex, Float aValue) override;
  void SetAttribute(uint32_t aIndex, const Size& aKernelUnitLength) override;
  void SetAttribute(uint32_t aIndex, const IntRect& aSourceRect) override;
  void SetAttribute(uint32_t aIndex, const IntPoint& aTarget) override;
  void SetAttribute(uint32_t aIndex, uint32_t aEdgeMode) override;
  void SetAttribute(uint32_t aIndex, bool aPreserveAlpha) override;
  IntRect MapRectToSource(const IntRect& aRect, const IntRect& aMax,
                          FilterNode* aSourceNode) override;

 protected:
  already_AddRefed<DataSourceSurface> Render(const IntRect& aRect) override;
  IntRect GetOutputRectInRect(const IntRect& aRect) override;
  int32_t InputIndex(uint32_t aInputEnumIndex) override;
  void RequestFromInputsForRect(const IntRect& aRect) override;

 private:
  template <typename CoordType>
  already_AddRefed<DataSourceSurface> DoRender(const IntRect& aRect,
                                               CoordType aKernelUnitLengthX,
                                               CoordType aKernelUnitLengthY);

  IntRect InflatedSourceRect(const IntRect& aDestRect);
  IntRect InflatedDestRect(const IntRect& aSourceRect);

  IntSize mKernelSize;
  std::vector<Float> mKernelMatrix;
  Float mDivisor;
  Float mBias;
  IntPoint mTarget;
  IntRect mSourceRect;
  ConvolveMatrixEdgeMode mEdgeMode;
  Size mKernelUnitLength;
  bool mPreserveAlpha;
};

class FilterNodeDisplacementMapSoftware : public FilterNodeSoftware {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(FilterNodeDisplacementMapSoftware,
                                          override)
  FilterNodeDisplacementMapSoftware();
  const char* GetName() override { return "DisplacementMap"; }
  using FilterNodeSoftware::SetAttribute;
  void SetAttribute(uint32_t aIndex, Float aScale) override;
  void SetAttribute(uint32_t aIndex, uint32_t aValue) override;
  IntRect MapRectToSource(const IntRect& aRect, const IntRect& aMax,
                          FilterNode* aSourceNode) override;

 protected:
  already_AddRefed<DataSourceSurface> Render(const IntRect& aRect) override;
  IntRect GetOutputRectInRect(const IntRect& aRect) override;
  int32_t InputIndex(uint32_t aInputEnumIndex) override;
  void RequestFromInputsForRect(const IntRect& aRect) override;

 private:
  IntRect InflatedSourceOrDestRect(const IntRect& aDestOrSourceRect);

  Float mScale;
  ColorChannel mChannelX;
  ColorChannel mChannelY;
};

class FilterNodeTurbulenceSoftware : public FilterNodeSoftware {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(FilterNodeTurbulenceSoftware,
                                          override)
  FilterNodeTurbulenceSoftware();
  const char* GetName() override { return "Turbulence"; }
  using FilterNodeSoftware::SetAttribute;
  void SetAttribute(uint32_t aIndex, const Size& aSize) override;
  void SetAttribute(uint32_t aIndex, const IntRect& aRenderRect) override;
  void SetAttribute(uint32_t aIndex, bool aStitchable) override;
  void SetAttribute(uint32_t aIndex, uint32_t aValue) override;
  IntRect MapRectToSource(const IntRect& aRect, const IntRect& aMax,
                          FilterNode* aSourceNode) override;

 protected:
  already_AddRefed<DataSourceSurface> Render(const IntRect& aRect) override;
  IntRect GetOutputRectInRect(const IntRect& aRect) override;
  int32_t InputIndex(uint32_t aInputEnumIndex) override;

 private:
  IntRect mRenderRect;
  Size mBaseFrequency;
  uint32_t mNumOctaves;
  uint32_t mSeed;
  bool mStitchable;
  TurbulenceType mType;
};

class FilterNodeArithmeticCombineSoftware : public FilterNodeSoftware {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(FilterNodeArithmeticCombineSoftware,
                                          override)
  FilterNodeArithmeticCombineSoftware();
  const char* GetName() override { return "ArithmeticCombine"; }
  using FilterNodeSoftware::SetAttribute;
  void SetAttribute(uint32_t aIndex, const Float* aFloat,
                    uint32_t aSize) override;
  IntRect MapRectToSource(const IntRect& aRect, const IntRect& aMax,
                          FilterNode* aSourceNode) override;

 protected:
  already_AddRefed<DataSourceSurface> Render(const IntRect& aRect) override;
  IntRect GetOutputRectInRect(const IntRect& aRect) override;
  int32_t InputIndex(uint32_t aInputEnumIndex) override;
  void RequestFromInputsForRect(const IntRect& aRect) override;

 private:
  Float mK1;
  Float mK2;
  Float mK3;
  Float mK4;
};

class FilterNodeCompositeSoftware : public FilterNodeSoftware {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(FilterNodeCompositeSoftware, override)
  FilterNodeCompositeSoftware();
  const char* GetName() override { return "Composite"; }
  using FilterNodeSoftware::SetAttribute;
  void SetAttribute(uint32_t aIndex, uint32_t aOperator) override;

 protected:
  already_AddRefed<DataSourceSurface> Render(const IntRect& aRect) override;
  IntRect GetOutputRectInRect(const IntRect& aRect) override;
  int32_t InputIndex(uint32_t aInputEnumIndex) override;
  void RequestFromInputsForRect(const IntRect& aRect) override;
  IntRect MapRectToSource(const IntRect& aRect, const IntRect& aMax,
                          FilterNode* aSourceNode) override;

 private:
  CompositeOperator mOperator;
};

// Base class for FilterNodeGaussianBlurSoftware and
// FilterNodeDirectionalBlurSoftware.
class FilterNodeBlurXYSoftware : public FilterNodeSoftware {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(FilterNodeBlurXYSoftware, override)
 protected:
  already_AddRefed<DataSourceSurface> Render(const IntRect& aRect) override;
  IntRect GetOutputRectInRect(const IntRect& aRect) override;
  int32_t InputIndex(uint32_t aInputEnumIndex) override;
  IntRect InflatedSourceOrDestRect(const IntRect& aDestRect);
  void RequestFromInputsForRect(const IntRect& aRect) override;
  IntRect MapRectToSource(const IntRect& aRect, const IntRect& aMax,
                          FilterNode* aSourceNode) override;

  // Implemented by subclasses.
  virtual Size StdDeviationXY() = 0;
};

class FilterNodeGaussianBlurSoftware : public FilterNodeBlurXYSoftware {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(FilterNodeGaussianBlurSoftware,
                                          override)
  FilterNodeGaussianBlurSoftware();
  const char* GetName() override { return "GaussianBlur"; }
  using FilterNodeSoftware::SetAttribute;
  void SetAttribute(uint32_t aIndex, Float aStdDeviation) override;

 protected:
  Size StdDeviationXY() override;

 private:
  Float mStdDeviation;
};

class FilterNodeDirectionalBlurSoftware : public FilterNodeBlurXYSoftware {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(FilterNodeDirectionalBlurSoftware,
                                          override)
  FilterNodeDirectionalBlurSoftware();
  const char* GetName() override { return "DirectionalBlur"; }
  using FilterNodeSoftware::SetAttribute;
  void SetAttribute(uint32_t aIndex, Float aStdDeviation) override;
  void SetAttribute(uint32_t aIndex, uint32_t aBlurDirection) override;

 protected:
  Size StdDeviationXY() override;

 private:
  Float mStdDeviation;
  BlurDirection mBlurDirection;
};

class FilterNodeCropSoftware : public FilterNodeSoftware {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(FilterNodeCropSoftware, override)
  const char* GetName() override { return "Crop"; }
  using FilterNodeSoftware::SetAttribute;
  void SetAttribute(uint32_t aIndex, const Rect& aSourceRect) override;

 protected:
  already_AddRefed<DataSourceSurface> Render(const IntRect& aRect) override;
  IntRect GetOutputRectInRect(const IntRect& aRect) override;
  int32_t InputIndex(uint32_t aInputEnumIndex) override;
  void RequestFromInputsForRect(const IntRect& aRect) override;
  IntRect MapRectToSource(const IntRect& aRect, const IntRect& aMax,
                          FilterNode* aSourceNode) override;

 private:
  IntRect mCropRect;
};

class FilterNodePremultiplySoftware : public FilterNodeSoftware {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(FilterNodePremultiplySoftware,
                                          override)
  const char* GetName() override { return "Premultiply"; }

 protected:
  already_AddRefed<DataSourceSurface> Render(const IntRect& aRect) override;
  IntRect GetOutputRectInRect(const IntRect& aRect) override;
  int32_t InputIndex(uint32_t aInputEnumIndex) override;
  void RequestFromInputsForRect(const IntRect& aRect) override;
  IntRect MapRectToSource(const IntRect& aRect, const IntRect& aMax,
                          FilterNode* aSourceNode) override;
};

class FilterNodeUnpremultiplySoftware : public FilterNodeSoftware {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(FilterNodeUnpremultiplySoftware,
                                          override)
  const char* GetName() override { return "Unpremultiply"; }

 protected:
  already_AddRefed<DataSourceSurface> Render(const IntRect& aRect) override;
  IntRect GetOutputRectInRect(const IntRect& aRect) override;
  int32_t InputIndex(uint32_t aInputEnumIndex) override;
  void RequestFromInputsForRect(const IntRect& aRect) override;
  IntRect MapRectToSource(const IntRect& aRect, const IntRect& aMax,
                          FilterNode* aSourceNode) override;
};

class FilterNodeOpacitySoftware : public FilterNodeSoftware {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(FilterNodeOpacitySoftware, override)
  const char* GetName() override { return "Opacity"; }
  using FilterNodeSoftware::SetAttribute;
  void SetAttribute(uint32_t aIndex, Float aValue) override;
  IntRect MapRectToSource(const IntRect& aRect, const IntRect& aMax,
                          FilterNode* aSourceNode) override;

 protected:
  already_AddRefed<DataSourceSurface> Render(const IntRect& aRect) override;
  IntRect GetOutputRectInRect(const IntRect& aRect) override;
  int32_t InputIndex(uint32_t aInputEnumIndex) override;
  void RequestFromInputsForRect(const IntRect& aRect) override;

  Float mValue = 1.0f;
};

template <typename LightType, typename LightingType>
class FilterNodeLightingSoftware : public FilterNodeSoftware {
 public:
#if defined(MOZILLA_INTERNAL_API) && \
    (defined(DEBUG) || defined(FORCE_BUILD_REFCNT_LOGGING))
  // Helpers for refcounted
  const char* typeName() const override { return mTypeName; }
  size_t typeSize() const override { return sizeof(*this); }
#endif
  explicit FilterNodeLightingSoftware(const char* aTypeName);
  const char* GetName() override { return "Lighting"; }
  using FilterNodeSoftware::SetAttribute;
  void SetAttribute(uint32_t aIndex, Float) override;
  void SetAttribute(uint32_t aIndex, const Size&) override;
  void SetAttribute(uint32_t aIndex, const Point3D&) override;
  void SetAttribute(uint32_t aIndex, const Color&) override;
  IntRect MapRectToSource(const IntRect& aRect, const IntRect& aMax,
                          FilterNode* aSourceNode) override;

 protected:
  already_AddRefed<DataSourceSurface> Render(const IntRect& aRect) override;
  IntRect GetOutputRectInRect(const IntRect& aRect) override;
  int32_t InputIndex(uint32_t aInputEnumIndex) override;
  void RequestFromInputsForRect(const IntRect& aRect) override;

 private:
  template <typename CoordType>
  already_AddRefed<DataSourceSurface> DoRender(const IntRect& aRect,
                                               CoordType aKernelUnitLengthX,
                                               CoordType aKernelUnitLengthY);

  Mutex mLock;
  LightType mLight;
  LightingType mLighting;
  Float mSurfaceScale;
  Size mKernelUnitLength;
  Color mColor;
#if defined(MOZILLA_INTERNAL_API) && \
    (defined(DEBUG) || defined(FORCE_BUILD_REFCNT_LOGGING))
  const char* mTypeName;
#endif
};

}  // namespace gfx
}  // namespace mozilla

#endif  // _MOZILLA_GFX_FILTERNODESOFTWARE_H_
