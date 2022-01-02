/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layout_PrintTranslator_h
#define mozilla_layout_PrintTranslator_h

#include <istream>

#include "DrawEventRecorder.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Filters.h"
#include "mozilla/gfx/RecordedEvent.h"

class nsDeviceContext;

namespace mozilla {
namespace layout {

using gfx::DrawTarget;
using gfx::FilterNode;
using gfx::GradientStops;
using gfx::NativeFontResource;
using gfx::Path;
using gfx::RecordedDependentSurface;
using gfx::ReferencePtr;
using gfx::ScaledFont;
using gfx::SourceSurface;
using gfx::Translator;
using gfx::UnscaledFont;

class PrintTranslator final : public Translator {
 public:
  explicit PrintTranslator(nsDeviceContext* aDeviceContext);

  bool TranslateRecording(PRFileDescStream& aRecording);

  DrawTarget* LookupDrawTarget(ReferencePtr aRefPtr) final {
    DrawTarget* result = mDrawTargets.GetWeak(aRefPtr);
    MOZ_ASSERT(result);
    return result;
  }

  Path* LookupPath(ReferencePtr aRefPtr) final {
    Path* result = mPaths.GetWeak(aRefPtr);
    MOZ_ASSERT(result);
    return result;
  }

  SourceSurface* LookupSourceSurface(ReferencePtr aRefPtr) final {
    SourceSurface* result = mSourceSurfaces.GetWeak(aRefPtr);
    MOZ_ASSERT(result);
    return result;
  }

  FilterNode* LookupFilterNode(ReferencePtr aRefPtr) final {
    FilterNode* result = mFilterNodes.GetWeak(aRefPtr);
    MOZ_ASSERT(result);
    return result;
  }

  already_AddRefed<GradientStops> LookupGradientStops(
      ReferencePtr aRefPtr) final {
    return mGradientStops.Get(aRefPtr);
  }

  ScaledFont* LookupScaledFont(ReferencePtr aRefPtr) final {
    ScaledFont* result = mScaledFonts.GetWeak(aRefPtr);
    MOZ_ASSERT(result);
    return result;
  }

  UnscaledFont* LookupUnscaledFont(ReferencePtr aRefPtr) final {
    UnscaledFont* result = mUnscaledFonts.GetWeak(aRefPtr);
    MOZ_ASSERT(result);
    return result;
  }

  NativeFontResource* LookupNativeFontResource(uint64_t aKey) final {
    NativeFontResource* result = mNativeFontResources.GetWeak(aKey);
    MOZ_ASSERT(result);
    return result;
  }

  void AddDrawTarget(ReferencePtr aRefPtr, DrawTarget* aDT) final {
    mDrawTargets.InsertOrUpdate(aRefPtr, RefPtr{aDT});
  }

  void AddPath(ReferencePtr aRefPtr, Path* aPath) final {
    mPaths.InsertOrUpdate(aRefPtr, RefPtr{aPath});
  }

  void AddSourceSurface(ReferencePtr aRefPtr, SourceSurface* aSurface) final {
    mSourceSurfaces.InsertOrUpdate(aRefPtr, RefPtr{aSurface});
  }

  void AddFilterNode(ReferencePtr aRefPtr, FilterNode* aFilter) final {
    mFilterNodes.InsertOrUpdate(aRefPtr, RefPtr{aFilter});
  }

  void AddGradientStops(ReferencePtr aRefPtr, GradientStops* aStops) final {
    mGradientStops.InsertOrUpdate(aRefPtr, RefPtr{aStops});
  }

  void AddScaledFont(ReferencePtr aRefPtr, ScaledFont* aScaledFont) final {
    mScaledFonts.InsertOrUpdate(aRefPtr, RefPtr{aScaledFont});
  }

  void AddUnscaledFont(ReferencePtr aRefPtr,
                       UnscaledFont* aUnscaledFont) final {
    mUnscaledFonts.InsertOrUpdate(aRefPtr, RefPtr{aUnscaledFont});
  }

  void AddNativeFontResource(uint64_t aKey,
                             NativeFontResource* aScaledFontResouce) final {
    mNativeFontResources.InsertOrUpdate(aKey, RefPtr{aScaledFontResouce});
  }

  void RemoveDrawTarget(ReferencePtr aRefPtr) final {
    mDrawTargets.Remove(aRefPtr);
  }

  void RemovePath(ReferencePtr aRefPtr) final { mPaths.Remove(aRefPtr); }

  void RemoveSourceSurface(ReferencePtr aRefPtr) final {
    mSourceSurfaces.Remove(aRefPtr);
  }

  void RemoveFilterNode(ReferencePtr aRefPtr) final {
    mFilterNodes.Remove(aRefPtr);
  }

  void RemoveGradientStops(ReferencePtr aRefPtr) final {
    mGradientStops.Remove(aRefPtr);
  }

  void RemoveScaledFont(ReferencePtr aRefPtr) final {
    mScaledFonts.Remove(aRefPtr);
  }

  void RemoveUnscaledFont(ReferencePtr aRefPtr) final {
    mUnscaledFonts.Remove(aRefPtr);
  }

  already_AddRefed<DrawTarget> CreateDrawTarget(
      ReferencePtr aRefPtr, const gfx::IntSize& aSize,
      gfx::SurfaceFormat aFormat) final;

  mozilla::gfx::DrawTarget* GetReferenceDrawTarget() final { return mBaseDT; }

 private:
  RefPtr<nsDeviceContext> mDeviceContext;
  RefPtr<DrawTarget> mBaseDT;

  nsRefPtrHashtable<nsPtrHashKey<void>, DrawTarget> mDrawTargets;
  nsRefPtrHashtable<nsPtrHashKey<void>, Path> mPaths;
  nsRefPtrHashtable<nsPtrHashKey<void>, SourceSurface> mSourceSurfaces;
  nsRefPtrHashtable<nsPtrHashKey<void>, FilterNode> mFilterNodes;
  nsRefPtrHashtable<nsPtrHashKey<void>, GradientStops> mGradientStops;
  nsRefPtrHashtable<nsPtrHashKey<void>, ScaledFont> mScaledFonts;
  nsRefPtrHashtable<nsPtrHashKey<void>, UnscaledFont> mUnscaledFonts;
  nsRefPtrHashtable<nsUint64HashKey, NativeFontResource> mNativeFontResources;
};

}  // namespace layout
}  // namespace mozilla

#endif  // mozilla_layout_PrintTranslator_h
