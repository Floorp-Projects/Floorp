/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layout_InlineTranslator_h
#define mozilla_layout_InlineTranslator_h

#include <istream>
#include <string>

#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Filters.h"
#include "mozilla/gfx/RecordedEvent.h"
#include "nsRefPtrHashtable.h"

namespace mozilla {
namespace gfx {

using gfx::DrawTarget;
using gfx::FilterNode;
using gfx::GradientStops;
using gfx::NativeFontResource;
using gfx::Path;
using gfx::ReferencePtr;
using gfx::ScaledFont;
using gfx::SourceSurface;
using gfx::Translator;

class InlineTranslator : public Translator {
 public:
  explicit InlineTranslator(DrawTarget* aDT, void* aFontContext = nullptr);

  bool TranslateRecording(char*, size_t len);

  void SetExternalSurfaces(
      nsRefPtrHashtable<nsUint64HashKey, SourceSurface>* aExternalSurfaces) {
    mExternalSurfaces = aExternalSurfaces;
  }

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

  GradientStops* LookupGradientStops(ReferencePtr aRefPtr) final {
    DebugOnly<bool> found;
    GradientStops* result = mGradientStops.GetWeak(aRefPtr
#if defined(DEBUG)
                                                   ,
                                                   &found
#endif
    );
    // GradientStops can be null in some circumstances.
    MOZ_ASSERT(found);
    return result;
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

  already_AddRefed<SourceSurface> LookupExternalSurface(
      uint64_t aKey) override {
    return mExternalSurfaces->Get(aKey);
  }

  void AddDrawTarget(ReferencePtr aRefPtr, DrawTarget* aDT) final {
    mDrawTargets.Put(aRefPtr, aDT);
  }

  void AddPath(ReferencePtr aRefPtr, Path* aPath) final {
    mPaths.Put(aRefPtr, aPath);
  }

  void AddSourceSurface(ReferencePtr aRefPtr, SourceSurface* aSurface) final {
    mSourceSurfaces.Put(aRefPtr, aSurface);
  }

  void AddFilterNode(ReferencePtr aRefPtr, FilterNode* aFilter) final {
    mFilterNodes.Put(aRefPtr, aFilter);
  }

  void AddGradientStops(ReferencePtr aRefPtr, GradientStops* aStops) final {
    mGradientStops.Put(aRefPtr, aStops);
  }

  void AddScaledFont(ReferencePtr aRefPtr, ScaledFont* aScaledFont) final {
    mScaledFonts.Put(aRefPtr, aScaledFont);
  }

  void AddUnscaledFont(ReferencePtr aRefPtr,
                       UnscaledFont* aUnscaledFont) final {
    mUnscaledFonts.Put(aRefPtr, aUnscaledFont);
  }

  void AddNativeFontResource(uint64_t aKey,
                             NativeFontResource* aScaledFontResouce) final {
    mNativeFontResources.Put(aKey, aScaledFontResouce);
  }

  void RemoveDrawTarget(ReferencePtr aRefPtr) override {
    mDrawTargets.Remove(aRefPtr);
  }

  void RemovePath(ReferencePtr aRefPtr) final { mPaths.Remove(aRefPtr); }

  void RemoveSourceSurface(ReferencePtr aRefPtr) override {
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
      gfx::SurfaceFormat aFormat) override;

  mozilla::gfx::DrawTarget* GetReferenceDrawTarget() final { return mBaseDT; }

  void* GetFontContext() final { return mFontContext; }
  std::string GetError() { return mError; }

 private:
  RefPtr<DrawTarget> mBaseDT;
  void* mFontContext;
  std::string mError;

  nsRefPtrHashtable<nsPtrHashKey<void>, DrawTarget> mDrawTargets;
  nsRefPtrHashtable<nsPtrHashKey<void>, Path> mPaths;
  nsRefPtrHashtable<nsPtrHashKey<void>, SourceSurface> mSourceSurfaces;
  nsRefPtrHashtable<nsPtrHashKey<void>, FilterNode> mFilterNodes;
  nsRefPtrHashtable<nsPtrHashKey<void>, GradientStops> mGradientStops;
  nsRefPtrHashtable<nsPtrHashKey<void>, ScaledFont> mScaledFonts;
  nsRefPtrHashtable<nsPtrHashKey<void>, UnscaledFont> mUnscaledFonts;
  nsRefPtrHashtable<nsUint64HashKey, NativeFontResource> mNativeFontResources;
  nsRefPtrHashtable<nsUint64HashKey, SourceSurface>* mExternalSurfaces;
};

}  // namespace gfx
}  // namespace mozilla

#endif  // mozilla_layout_InlineTranslator_h
