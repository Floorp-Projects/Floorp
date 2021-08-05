/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "DWriteSettings.h"

#include "mozilla/DataMutex.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/gfx/gfxVars.h"

using namespace mozilla;
using namespace mozilla::gfx;

static std::atomic<Float> sClearTypeLevel{1.0f};
static std::atomic<Float> sEnhancedContrast{1.0f};
static std::atomic<Float> sGamma{2.2f};
static Atomic<DWRITE_PIXEL_GEOMETRY> sPixelGeometry;
static Atomic<DWRITE_RENDERING_MODE> sRenderingMode;
static Atomic<DWRITE_MEASURING_MODE> sMeasuringMode;
static std::atomic<Float> sGDIGamma{1.4f};
StaticDataMutex<StaticRefPtr<IDWriteRenderingParams>> sStandardRenderingParams(
    "StandardRenderingParams");
StaticDataMutex<StaticRefPtr<IDWriteRenderingParams>> sGDIRenderingParams(
    "GDIRenderingParams");

static void ClearStandardRenderingParams() {
  auto lockedParams = sStandardRenderingParams.Lock();
  lockedParams.ref() = nullptr;
}

static void ClearGDIRenderingParams() {
  auto lockedParams = sGDIRenderingParams.Lock();
  lockedParams.ref() = nullptr;
}

static void UpdateClearTypeLevel() {
  sClearTypeLevel = gfxVars::SystemTextClearTypeLevel();
}
static void ClearTypeLevelVarUpdated() {
  UpdateClearTypeLevel();
  ClearStandardRenderingParams();
  ClearGDIRenderingParams();
}

static void UpdateEnhancedContrast() {
  sEnhancedContrast = gfxVars::SystemTextEnhancedContrast();
}
static void EnhancedContrastVarUpdated() {
  UpdateEnhancedContrast();
  ClearStandardRenderingParams();
}

static void UpdateGamma() { sGamma = gfxVars::SystemTextGamma(); }
static void GammaVarUpdated() {
  UpdateGamma();
  ClearStandardRenderingParams();
}

static void UpdateGDIGamma() { sGDIGamma = gfxVars::SystemGDIGamma(); }
static void GDIGammaVarUpdated() {
  UpdateGDIGamma();
  ClearGDIRenderingParams();
}

static void UpdatePixelGeometry() {
  sPixelGeometry =
      static_cast<DWRITE_PIXEL_GEOMETRY>(gfxVars::SystemTextPixelGeometry());
  Factory::SetBGRSubpixelOrder(sPixelGeometry == DWRITE_PIXEL_GEOMETRY_BGR);
}
static void PixelGeometryVarUpdated() {
  UpdatePixelGeometry();
  ClearStandardRenderingParams();
  ClearGDIRenderingParams();
}

static void UpdateRenderingMode() {
  sRenderingMode =
      static_cast<DWRITE_RENDERING_MODE>(gfxVars::SystemTextRenderingMode());
  switch (sRenderingMode) {
    case DWRITE_RENDERING_MODE_ALIASED:
    case DWRITE_RENDERING_MODE_CLEARTYPE_GDI_CLASSIC:
      sMeasuringMode = DWRITE_MEASURING_MODE_GDI_CLASSIC;
      break;
    case DWRITE_RENDERING_MODE_CLEARTYPE_GDI_NATURAL:
      sMeasuringMode = DWRITE_MEASURING_MODE_GDI_NATURAL;
      break;
    default:
      sMeasuringMode = DWRITE_MEASURING_MODE_NATURAL;
      break;
  }
}
static void RenderingModeVarUpdated() {
  UpdateRenderingMode();
  ClearStandardRenderingParams();
}

DWriteSettings::DWriteSettings(bool aUseGDISettings)
    : mUseGDISettings(aUseGDISettings) {}

/* static */
void DWriteSettings::Initialize() {
  UpdateClearTypeLevel();
  gfxVars::SetSystemTextClearTypeLevelListener(ClearTypeLevelVarUpdated);

  UpdateEnhancedContrast();
  gfxVars::SetSystemTextEnhancedContrastListener(EnhancedContrastVarUpdated);

  UpdateGamma();
  gfxVars::SetSystemTextGammaListener(GammaVarUpdated);

  UpdateGDIGamma();
  gfxVars::SetSystemGDIGammaListener(GDIGammaVarUpdated);

  UpdateRenderingMode();
  gfxVars::SetSystemTextRenderingModeListener(RenderingModeVarUpdated);

  UpdatePixelGeometry();
  gfxVars::SetSystemTextPixelGeometryListener(PixelGeometryVarUpdated);
}

/* static */
DWriteSettings& DWriteSettings::Get(bool aGDISettings) {
  DWriteSettings* settings;
  if (aGDISettings) {
    static DWriteSettings* sGDISettings =
        new DWriteSettings(/* aUseGDISettings */ true);
    settings = sGDISettings;
  } else {
    static DWriteSettings* sStandardSettings =
        new DWriteSettings(/* aUseGDISettings */ false);
    settings = sStandardSettings;
  }
  return *settings;
}

Float DWriteSettings::ClearTypeLevel() { return sClearTypeLevel; }

Float DWriteSettings::EnhancedContrast() {
  return mUseGDISettings ? 0.0f : sEnhancedContrast.load();
}

Float DWriteSettings::Gamma() { return mUseGDISettings ? sGDIGamma : sGamma; }

DWRITE_PIXEL_GEOMETRY DWriteSettings::PixelGeometry() { return sPixelGeometry; }

DWRITE_RENDERING_MODE DWriteSettings::RenderingMode() {
  return mUseGDISettings ? DWRITE_RENDERING_MODE_GDI_CLASSIC : sRenderingMode;
}

DWRITE_MEASURING_MODE DWriteSettings::MeasuringMode() {
  return mUseGDISettings ? DWRITE_MEASURING_MODE_GDI_CLASSIC : sMeasuringMode;
}

already_AddRefed<IDWriteRenderingParams> DWriteSettings::RenderingParams() {
  auto lockedParams = mUseGDISettings ? sGDIRenderingParams.Lock()
                                      : sStandardRenderingParams.Lock();
  if (!lockedParams.ref()) {
    RefPtr<IDWriteRenderingParams> params;
    HRESULT hr = Factory::GetDWriteFactory()->CreateCustomRenderingParams(
        Gamma(), EnhancedContrast(), ClearTypeLevel(), PixelGeometry(),
        RenderingMode(), getter_AddRefs(params));
    if (SUCCEEDED(hr)) {
      lockedParams.ref() = params.forget();
    } else {
      gfxWarning() << "Failed to create DWrite custom rendering params.";
    }
  }

  return do_AddRef(lockedParams.ref());
}
