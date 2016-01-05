/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layout_PrintTranslator_h
#define mozilla_layout_PrintTranslator_h

#include <istream>

#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Filters.h"
#include "mozilla/gfx/RecordedEvent.h"
#include "mozilla/unused.h"
#include "nsRefPtrHashtable.h"

class nsDeviceContext;

namespace mozilla {
namespace layout {

using gfx::Translator;
using gfx::ReferencePtr;
using gfx::DrawTarget;
using gfx::Path;
using gfx::SourceSurface;
using gfx::FilterNode;
using gfx::GradientStops;
using gfx::ScaledFont;

class PrintTranslator final : public Translator
{
public:
  explicit PrintTranslator(nsDeviceContext* aDeviceContext);

  bool TranslateRecording(std::istream& aRecording);

  DrawTarget* LookupDrawTarget(ReferencePtr aRefPtr) final
  {
    return mDrawTargets.GetWeak(aRefPtr);
  }

  Path* LookupPath(ReferencePtr aRefPtr) final
  {
    return mPaths.GetWeak(aRefPtr);
  }

  SourceSurface* LookupSourceSurface(ReferencePtr aRefPtr) final
  {
    return mSourceSurfaces.GetWeak(aRefPtr);
  }

  FilterNode* LookupFilterNode(ReferencePtr aRefPtr) final
  {
    return mFilterNodes.GetWeak(aRefPtr);
  }

  GradientStops* LookupGradientStops(ReferencePtr aRefPtr) final
  {
    return mGradientStops.GetWeak(aRefPtr);
  }

  ScaledFont* LookupScaledFont(ReferencePtr aRefPtr) final
  {
    return mScaledFonts.GetWeak(aRefPtr);
  }

  void AddDrawTarget(ReferencePtr aRefPtr, DrawTarget *aDT) final
  {
    mDrawTargets.Put(aRefPtr, aDT);
  }

  void AddPath(ReferencePtr aRefPtr, Path *aPath) final
  {
    mPaths.Put(aRefPtr, aPath);
  }

  void AddSourceSurface(ReferencePtr aRefPtr, SourceSurface *aSurface) final
  {
    mSourceSurfaces.Put(aRefPtr, aSurface);
  }

  void AddFilterNode(ReferencePtr aRefPtr, FilterNode *aFilter) final
  {
    mFilterNodes.Put(aRefPtr, aFilter);
  }

  void AddGradientStops(ReferencePtr aRefPtr, GradientStops *aStops) final
  {
    mGradientStops.Put(aRefPtr, aStops);
  }

  void AddScaledFont(ReferencePtr aRefPtr, ScaledFont *aScaledFont) final
  {
    mScaledFonts.Put(aRefPtr, aScaledFont);
    Unused << mSavedScaledFonts.PutEntry(aScaledFont);
  }

  void RemoveDrawTarget(ReferencePtr aRefPtr) final
  {
    mDrawTargets.Remove(aRefPtr);
  }

  void RemovePath(ReferencePtr aRefPtr) final
  {
    mPaths.Remove(aRefPtr);
  }

  void RemoveSourceSurface(ReferencePtr aRefPtr) final
  {
    mSourceSurfaces.Remove(aRefPtr);
  }

  void RemoveFilterNode(ReferencePtr aRefPtr) final
  {
    mFilterNodes.Remove(aRefPtr);
  }

  void RemoveGradientStops(ReferencePtr aRefPtr) final
  {
    mGradientStops.Remove(aRefPtr);
  }

  void RemoveScaledFont(ReferencePtr aRefPtr) final
  {
    mScaledFonts.Remove(aRefPtr);
  }

  void ClearSavedFonts() { mSavedScaledFonts.Clear(); }

  already_AddRefed<DrawTarget> CreateDrawTarget(ReferencePtr aRefPtr,
                                                const gfx::IntSize &aSize,
                                                gfx::SurfaceFormat aFormat) final;

  mozilla::gfx::DrawTarget* GetReferenceDrawTarget() final { return mBaseDT; }

  mozilla::gfx::FontType GetDesiredFontType() final;

private:
  RefPtr<nsDeviceContext> mDeviceContext;
  RefPtr<DrawTarget> mBaseDT;

  nsRefPtrHashtable<nsPtrHashKey<void>, DrawTarget> mDrawTargets;
  nsRefPtrHashtable<nsPtrHashKey<void>, Path> mPaths;
  nsRefPtrHashtable<nsPtrHashKey<void>, SourceSurface> mSourceSurfaces;
  nsRefPtrHashtable<nsPtrHashKey<void>, FilterNode> mFilterNodes;
  nsRefPtrHashtable<nsPtrHashKey<void>, GradientStops> mGradientStops;
  nsRefPtrHashtable<nsPtrHashKey<void>, ScaledFont> mScaledFonts;

  // We keep an extra reference to each scaled font, because they currently
  // always get removed immediately. These can be cleared using ClearSavedFonts,
  // when we know that things have been flushed to the print device.
  nsTHashtable<nsRefPtrHashKey<ScaledFont>> mSavedScaledFonts;
};

} // namespace layout
} // namespace mozilla

#endif // mozilla_layout_PrintTranslator_h
