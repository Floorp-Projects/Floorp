/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layout_InlineTranslator_h
#define mozilla_layout_InlineTranslator_h

#include <istream>

#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Filters.h"
#include "mozilla/gfx/RecordedEvent.h"
#include "nsRefPtrHashtable.h"

namespace mozilla {
namespace gfx {

using gfx::Translator;
using gfx::ReferencePtr;
using gfx::DrawTarget;
using gfx::Path;
using gfx::SourceSurface;
using gfx::FilterNode;
using gfx::GradientStops;
using gfx::ScaledFont;
using gfx::NativeFontResource;

class InlineTranslator final : public Translator
{
public:
  explicit InlineTranslator(DrawTarget* aDT, void* aFontContext = nullptr);

  bool TranslateRecording(char *, size_t len);

  DrawTarget* LookupDrawTarget(ReferencePtr aRefPtr) final override
  {
    DrawTarget* result = mDrawTargets.GetWeak(aRefPtr);
    MOZ_ASSERT(result);
    return result;
  }

  Path* LookupPath(ReferencePtr aRefPtr) final override
  {
    Path* result = mPaths.GetWeak(aRefPtr);
    MOZ_ASSERT(result);
    return result;
  }

  SourceSurface* LookupSourceSurface(ReferencePtr aRefPtr) final override
  {
    SourceSurface* result = mSourceSurfaces.GetWeak(aRefPtr);
    MOZ_ASSERT(result);
    return result;
  }

  FilterNode* LookupFilterNode(ReferencePtr aRefPtr) final override
  {
    FilterNode* result = mFilterNodes.GetWeak(aRefPtr);
    MOZ_ASSERT(result);
    return result;
  }

  GradientStops* LookupGradientStops(ReferencePtr aRefPtr) final override
  {
    GradientStops* result =  mGradientStops.GetWeak(aRefPtr);
    MOZ_ASSERT(result);
    return result;
  }

  ScaledFont* LookupScaledFont(ReferencePtr aRefPtr) final override
  {
    ScaledFont* result = mScaledFonts.GetWeak(aRefPtr);
    MOZ_ASSERT(result);
    return result;
  }

  UnscaledFont* LookupUnscaledFont(ReferencePtr aRefPtr) override final
  {
    UnscaledFont* result = mUnscaledFonts.GetWeak(aRefPtr);
    MOZ_ASSERT(result);
    return result;
  }

  virtual UnscaledFont* LookupUnscaledFontByIndex(size_t index) override final
  {
    UnscaledFont* result = mUnscaledFontTable[index];
    return result;
  }

  NativeFontResource* LookupNativeFontResource(uint64_t aKey) final override
  {
    NativeFontResource* result = mNativeFontResources.GetWeak(aKey);
    MOZ_ASSERT(result);
    return result;
  }

  void AddDrawTarget(ReferencePtr aRefPtr, DrawTarget *aDT) final override
  {
    mDrawTargets.Put(aRefPtr, aDT);
  }

  void AddPath(ReferencePtr aRefPtr, Path *aPath) final override
  {
    mPaths.Put(aRefPtr, aPath);
  }

  void AddSourceSurface(ReferencePtr aRefPtr, SourceSurface *aSurface) final override
  {
    mSourceSurfaces.Put(aRefPtr, aSurface);
  }

  void AddFilterNode(ReferencePtr aRefPtr, FilterNode *aFilter) final override
  {
    mFilterNodes.Put(aRefPtr, aFilter);
  }

  void AddGradientStops(ReferencePtr aRefPtr, GradientStops *aStops) final override
  {
    mGradientStops.Put(aRefPtr, aStops);
  }

  void AddScaledFont(ReferencePtr aRefPtr, ScaledFont *aScaledFont) final override
  {
    mScaledFonts.Put(aRefPtr, aScaledFont);
  }

  void AddUnscaledFont(ReferencePtr aRefPtr, UnscaledFont *aUnscaledFont) final override
  {
    mUnscaledFontTable.push_back(aUnscaledFont);
    mUnscaledFonts.Put(aRefPtr, aUnscaledFont);
  }

  void AddNativeFontResource(uint64_t aKey,
                             NativeFontResource *aScaledFontResouce) final override
  {
    mNativeFontResources.Put(aKey, aScaledFontResouce);
  }

  void RemoveDrawTarget(ReferencePtr aRefPtr) final override
  {
    mDrawTargets.Remove(aRefPtr);
  }

  void RemovePath(ReferencePtr aRefPtr) final override
  {
    mPaths.Remove(aRefPtr);
  }

  void RemoveSourceSurface(ReferencePtr aRefPtr) final override
  {
    mSourceSurfaces.Remove(aRefPtr);
  }

  void RemoveFilterNode(ReferencePtr aRefPtr) final override
  {
    mFilterNodes.Remove(aRefPtr);
  }

  void RemoveGradientStops(ReferencePtr aRefPtr) final override
  {
    mGradientStops.Remove(aRefPtr);
  }

  void RemoveScaledFont(ReferencePtr aRefPtr) final override
  {
    mScaledFonts.Remove(aRefPtr);
  }

  void RemoveUnscaledFont(ReferencePtr aRefPtr) final override
  {
    mUnscaledFonts.Remove(aRefPtr);
  }


  already_AddRefed<DrawTarget> CreateDrawTarget(ReferencePtr aRefPtr,
                                                const gfx::IntSize &aSize,
                                                gfx::SurfaceFormat aFormat) final override;

  mozilla::gfx::DrawTarget* GetReferenceDrawTarget() final override { return mBaseDT; }

  void* GetFontContext() final override { return mFontContext; }

private:
  RefPtr<DrawTarget> mBaseDT;
  void*              mFontContext;

  std::vector<RefPtr<UnscaledFont>> mUnscaledFontTable;
  nsRefPtrHashtable<nsPtrHashKey<void>, DrawTarget> mDrawTargets;
  nsRefPtrHashtable<nsPtrHashKey<void>, Path> mPaths;
  nsRefPtrHashtable<nsPtrHashKey<void>, SourceSurface> mSourceSurfaces;
  nsRefPtrHashtable<nsPtrHashKey<void>, FilterNode> mFilterNodes;
  nsRefPtrHashtable<nsPtrHashKey<void>, GradientStops> mGradientStops;
  nsRefPtrHashtable<nsPtrHashKey<void>, ScaledFont> mScaledFonts;
  nsRefPtrHashtable<nsPtrHashKey<void>, UnscaledFont> mUnscaledFonts;
  nsRefPtrHashtable<nsUint64HashKey, NativeFontResource> mNativeFontResources;
};

} // namespace gfx
} // namespace mozilla

#endif // mozilla_layout_InlineTranslator_h
