/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDeviceContext.h"
#include <algorithm>  // for max
#include "gfxContext.h"
#include "gfxImageSurface.h"  // for gfxImageSurface
#include "gfxPoint.h"         // for gfxSize
#include "gfxTextRun.h"       // for gfxFontGroup
#include "mozilla/LookAndFeel.h"
#include "mozilla/gfx/PathHelpers.h"
#include "mozilla/gfx/PrintTarget.h"
#include "mozilla/Preferences.h"  // for Preferences
#include "mozilla/ProfilerMarkers.h"
#include "mozilla/Services.h"  // for GetObserverService
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/Try.h"            // for MOZ_TRY
#include "mozilla/mozalloc.h"       // for operator new
#include "mozilla/widget/Screen.h"  // for Screen
#include "nsCRT.h"                  // for nsCRT
#include "nsDebug.h"                // for NS_ASSERTION, etc
#include "nsFont.h"                 // for nsFont
#include "nsFontCache.h"            // for nsFontCache
#include "nsFontMetrics.h"          // for nsFontMetrics
#include "nsAtom.h"                 // for nsAtom, NS_Atomize
#include "nsID.h"
#include "nsIDeviceContextSpec.h"   // for nsIDeviceContextSpec
#include "nsLanguageAtomService.h"  // for nsLanguageAtomService
#include "nsIObserver.h"            // for nsIObserver, etc
#include "nsIObserverService.h"     // for nsIObserverService
#include "nsISupportsImpl.h"        // for MOZ_COUNT_CTOR, etc
#include "nsISupportsUtils.h"       // for NS_ADDREF, NS_RELEASE
#include "nsIWidget.h"              // for nsIWidget, NS_NATIVE_WINDOW
#include "nsRect.h"                 // for nsRect
#include "nsServiceManagerUtils.h"  // for do_GetService
#include "nsString.h"               // for nsDependentString
#include "nsTArray.h"               // for nsTArray, nsTArray_Impl
#include "nsThreadUtils.h"          // for NS_IsMainThread
#include "mozilla/gfx/Logging.h"
#include "mozilla/widget/ScreenManager.h"  // for ScreenManager

using namespace mozilla;
using namespace mozilla::gfx;
using mozilla::widget::ScreenManager;

nsDeviceContext::nsDeviceContext()
    : mWidth(0),
      mHeight(0),
      mAppUnitsPerDevPixel(-1),
      mAppUnitsPerDevPixelAtUnitFullZoom(-1),
      mAppUnitsPerPhysicalInch(-1),
      mFullZoom(1.0f),
      mPrintingScale(1.0f),
      mPrintingTranslate(gfxPoint(0, 0)),
      mIsCurrentlyPrintingDoc(false) {
  MOZ_ASSERT(NS_IsMainThread(), "nsDeviceContext created off main thread");
}

nsDeviceContext::~nsDeviceContext() = default;

void nsDeviceContext::SetDPI() {
  float dpi;

  // Use the printing DC to determine DPI values, if we have one.
  if (mDeviceContextSpec) {
    dpi = mDeviceContextSpec->GetDPI();
    mPrintingScale = mDeviceContextSpec->GetPrintingScale();
    mPrintingTranslate = mDeviceContextSpec->GetPrintingTranslate();
    mAppUnitsPerDevPixelAtUnitFullZoom =
        NS_lround((AppUnitsPerCSSPixel() * 96) / dpi);
  } else {
    // A value of -1 means use the maximum of 96 and the system DPI.
    // A value of 0 means use the system DPI. A positive value is used as the
    // DPI. This sets the physical size of a device pixel and thus controls the
    // interpretation of physical units.
    int32_t prefDPI = StaticPrefs::layout_css_dpi();
    if (prefDPI > 0) {
      dpi = prefDPI;
    } else if (mWidget) {
      dpi = mWidget->GetDPI();
      MOZ_ASSERT(dpi > 0);
      if (prefDPI < 0) {
        dpi = std::max(96.0f, dpi);
      }
    } else {
      dpi = 96.0f;
    }

    CSSToLayoutDeviceScale scale =
        mWidget ? mWidget->GetDefaultScale() : CSSToLayoutDeviceScale(1.0);
    MOZ_ASSERT(scale.scale > 0.0);
    mAppUnitsPerDevPixelAtUnitFullZoom =
        std::max(1, NS_lround(AppUnitsPerCSSPixel() / scale.scale));
  }

  NS_ASSERTION(dpi != -1.0, "no dpi set");

  mAppUnitsPerPhysicalInch =
      NS_lround(dpi * mAppUnitsPerDevPixelAtUnitFullZoom);
  UpdateAppUnitsForFullZoom();
}

void nsDeviceContext::Init(nsIWidget* aWidget) {
  if (mIsInitialized && mWidget == aWidget) {
    return;
  }

  // We can't assert |!mIsInitialized| here since EndSwapDocShellsForDocument
  // re-initializes nsDeviceContext objects.  We can only assert in
  // InitForPrinting (below).
  mIsInitialized = true;

  mWidget = aWidget;
  SetDPI();
}

// XXX This is only for printing. We should make that obvious in the name.
UniquePtr<gfxContext> nsDeviceContext::CreateRenderingContext() {
  return CreateRenderingContextCommon(/* not a reference context */ false);
}

UniquePtr<gfxContext> nsDeviceContext::CreateReferenceRenderingContext() {
  return CreateRenderingContextCommon(/* a reference context */ true);
}

UniquePtr<gfxContext> nsDeviceContext::CreateRenderingContextCommon(
    bool aWantReferenceContext) {
  MOZ_ASSERT(IsPrinterContext());
  MOZ_ASSERT(mWidth > 0 && mHeight > 0);

  if (NS_WARN_IF(!mPrintTarget)) {
    // Printing canceled already.
    return nullptr;
  }

  RefPtr<gfx::DrawTarget> dt;
  if (aWantReferenceContext) {
    dt = mPrintTarget->GetReferenceDrawTarget();
  } else {
    // This will be null if printing a page from the parent process.
    RefPtr<DrawEventRecorder> recorder;
    mDeviceContextSpec->GetDrawEventRecorder(getter_AddRefs(recorder));
    dt = mPrintTarget->MakeDrawTarget(gfx::IntSize(mWidth, mHeight), recorder);
  }

  if (!dt || !dt->IsValid()) {
    gfxCriticalNote << "Failed to create draw target in device context sized "
                    << mWidth << "x" << mHeight << " and pointer "
                    << hexa(mPrintTarget);
    return nullptr;
  }

  dt->AddUserData(&sDisablePixelSnapping, (void*)0x1, nullptr);

  auto pContext = MakeUnique<gfxContext>(dt);

  gfxMatrix transform;
  transform.PreTranslate(mPrintingTranslate);
  transform.PreScale(mPrintingScale, mPrintingScale);
  pContext->SetMatrixDouble(transform);
  return pContext;
}

uint32_t nsDeviceContext::GetDepth() {
  RefPtr<widget::Screen> screen = FindScreen();
  if (!screen) {
    ScreenManager& screenManager = ScreenManager::GetSingleton();
    screen = screenManager.GetPrimaryScreen();
    MOZ_ASSERT(screen);
  }
  int32_t depth = 0;
  screen->GetColorDepth(&depth);
  return uint32_t(depth);
}

dom::ScreenColorGamut nsDeviceContext::GetColorGamut() {
  RefPtr<widget::Screen> screen = FindScreen();
  if (!screen) {
    auto& screenManager = ScreenManager::GetSingleton();
    screen = screenManager.GetPrimaryScreen();
    MOZ_ASSERT(screen);
  }
  dom::ScreenColorGamut colorGamut;
  screen->GetColorGamut(&colorGamut);
  return colorGamut;
}

hal::ScreenOrientation nsDeviceContext::GetScreenOrientationType() {
  RefPtr<widget::Screen> screen = FindScreen();
  if (!screen) {
    auto& screenManager = ScreenManager::GetSingleton();
    screen = screenManager.GetPrimaryScreen();
    MOZ_ASSERT(screen);
  }
  return screen->GetOrientationType();
}

uint16_t nsDeviceContext::GetScreenOrientationAngle() {
  RefPtr<widget::Screen> screen = FindScreen();
  if (!screen) {
    auto& screenManager = ScreenManager::GetSingleton();
    screen = screenManager.GetPrimaryScreen();
    MOZ_ASSERT(screen);
  }
  return screen->GetOrientationAngle();
}

nsresult nsDeviceContext::GetDeviceSurfaceDimensions(nscoord& aWidth,
                                                     nscoord& aHeight) {
  if (IsPrinterContext()) {
    aWidth = mWidth;
    aHeight = mHeight;
  } else {
    nsRect area;
    ComputeFullAreaUsingScreen(&area);
    aWidth = area.Width();
    aHeight = area.Height();
  }

  return NS_OK;
}

nsresult nsDeviceContext::GetRect(nsRect& aRect) {
  if (IsPrinterContext()) {
    aRect.SetRect(0, 0, mWidth, mHeight);
  } else
    ComputeFullAreaUsingScreen(&aRect);

  return NS_OK;
}

nsresult nsDeviceContext::GetClientRect(nsRect& aRect) {
  if (IsPrinterContext()) {
    aRect.SetRect(0, 0, mWidth, mHeight);
  } else
    ComputeClientRectUsingScreen(&aRect);

  return NS_OK;
}

nsresult nsDeviceContext::InitForPrinting(nsIDeviceContextSpec* aDevice) {
  NS_ENSURE_ARG_POINTER(aDevice);

  MOZ_ASSERT(!mIsInitialized,
             "Only initialize once, immediately after construction");

  // We don't set mIsInitialized here. The Init() call below does that.

  mPrintTarget = aDevice->MakePrintTarget();
  if (!mPrintTarget) {
    return NS_ERROR_FAILURE;
  }

  mDeviceContextSpec = aDevice;

  Init(nullptr);

  if (!CalcPrintingSize()) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult nsDeviceContext::BeginDocument(const nsAString& aTitle,
                                        const nsAString& aPrintToFileName,
                                        int32_t aStartPage, int32_t aEndPage) {
  MOZ_DIAGNOSTIC_ASSERT(!mIsCurrentlyPrintingDoc,
                        "Mismatched BeginDocument/EndDocument calls");
  AUTO_PROFILER_MARKER_TEXT("DeviceContext Printing", LAYOUT_Printing, {},
                            "nsDeviceContext::BeginDocument"_ns);

  nsresult rv = mPrintTarget->BeginPrinting(aTitle, aPrintToFileName,
                                            aStartPage, aEndPage);

  if (NS_SUCCEEDED(rv)) {
    if (mDeviceContextSpec) {
      rv = mDeviceContextSpec->BeginDocument(aTitle, aPrintToFileName,
                                             aStartPage, aEndPage);
    }
    mIsCurrentlyPrintingDoc = true;
  }

  // Warn about any failure (except user cancelling):
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv) || rv == NS_ERROR_ABORT,
                       "nsDeviceContext::BeginDocument failed");

  return rv;
}

RefPtr<PrintEndDocumentPromise> nsDeviceContext::EndDocument() {
  MOZ_DIAGNOSTIC_ASSERT(mIsCurrentlyPrintingDoc,
                        "Mismatched BeginDocument/EndDocument calls");
  MOZ_DIAGNOSTIC_ASSERT(mPrintTarget);
  AUTO_PROFILER_MARKER_TEXT("DeviceContext Printing", LAYOUT_Printing, {},
                            "nsDeviceContext::EndDocument"_ns);

  mIsCurrentlyPrintingDoc = false;

  if (mPrintTarget) {
    auto result = mPrintTarget->EndPrinting();
    if (NS_FAILED(result)) {
      return PrintEndDocumentPromise::CreateAndReject(NS_ERROR_NOT_AVAILABLE,
                                                      __func__);
    }
    mPrintTarget->Finish();
    mPrintTarget = nullptr;
  }

  if (mDeviceContextSpec) {
    return mDeviceContextSpec->EndDocument();
  }

  return PrintEndDocumentPromise::CreateAndResolve(true, __func__);
}

nsresult nsDeviceContext::AbortDocument() {
  MOZ_DIAGNOSTIC_ASSERT(mIsCurrentlyPrintingDoc,
                        "Mismatched BeginDocument/EndDocument calls");
  AUTO_PROFILER_MARKER_TEXT("DeviceContext Printing", LAYOUT_Printing, {},
                            "nsDeviceContext::AbortDocument"_ns);

  nsresult rv = mPrintTarget->AbortPrinting();
  mIsCurrentlyPrintingDoc = false;

  if (mDeviceContextSpec) {
    Unused << mDeviceContextSpec->EndDocument();
  }

  mPrintTarget = nullptr;

  return rv;
}

nsresult nsDeviceContext::BeginPage(const IntSize& aSizeInPoints) {
  MOZ_DIAGNOSTIC_ASSERT(!mIsCurrentlyPrintingDoc || mPrintTarget,
                        "What nulled out our print target while printing?");
  AUTO_PROFILER_MARKER_TEXT("DeviceContext Printing", LAYOUT_Printing, {},
                            "nsDeviceContext::BeginPage"_ns);

  if (mDeviceContextSpec) {
    MOZ_TRY(mDeviceContextSpec->BeginPage(aSizeInPoints));
  }
  if (mPrintTarget) {
    MOZ_TRY(mPrintTarget->BeginPage(aSizeInPoints));
  }
  return NS_OK;
}

nsresult nsDeviceContext::EndPage() {
  MOZ_DIAGNOSTIC_ASSERT(!mIsCurrentlyPrintingDoc || mPrintTarget,
                        "What nulled out our print target while printing?");
  AUTO_PROFILER_MARKER_TEXT("DeviceContext Printing", LAYOUT_Printing, {},
                            "nsDeviceContext::EndPage"_ns);

  if (mPrintTarget) {
    MOZ_TRY(mPrintTarget->EndPage());
  }
  if (mDeviceContextSpec) {
    MOZ_TRY(mDeviceContextSpec->EndPage());
  }
  return NS_OK;
}

void nsDeviceContext::ComputeClientRectUsingScreen(nsRect* outRect) {
  // we always need to recompute the clientRect
  // because the window may have moved onto a different screen. In the single
  // monitor case, we only need to do the computation if we haven't done it
  // once already, and remember that we have because we're assured it won't
  // change.
  if (RefPtr<widget::Screen> screen = FindScreen()) {
    *outRect = LayoutDeviceIntRect::ToAppUnits(screen->GetAvailRect(),
                                               AppUnitsPerDevPixel());
  }
}

void nsDeviceContext::ComputeFullAreaUsingScreen(nsRect* outRect) {
  // if we have more than one screen, we always need to recompute the clientRect
  // because the window may have moved onto a different screen. In the single
  // monitor case, we only need to do the computation if we haven't done it
  // once already, and remember that we have because we're assured it won't
  // change.
  if (RefPtr<widget::Screen> screen = FindScreen()) {
    *outRect = LayoutDeviceIntRect::ToAppUnits(screen->GetRect(),
                                               AppUnitsPerDevPixel());
    mWidth = outRect->Width();
    mHeight = outRect->Height();
  }
}

//
// FindScreen
//
// Determines which screen intersects the largest area of the given surface.
//
already_AddRefed<widget::Screen> nsDeviceContext::FindScreen() {
  if (!mWidget) {
    return nullptr;
  }

  CheckDPIChange();

  if (RefPtr<widget::Screen> screen = mWidget->GetWidgetScreen()) {
    return screen.forget();
  }

  ScreenManager& screenManager = ScreenManager::GetSingleton();
  return screenManager.GetPrimaryScreen();
}

bool nsDeviceContext::CalcPrintingSize() {
  gfxSize size(mPrintTarget->GetSize());
  // For printing, CSS inches and physical inches are identical
  // so it doesn't matter which we use here
  mWidth = NSToCoordRound(size.width * AppUnitsPerPhysicalInch() /
                          POINTS_PER_INCH_FLOAT);
  mHeight = NSToCoordRound(size.height * AppUnitsPerPhysicalInch() /
                           POINTS_PER_INCH_FLOAT);

  return (mWidth > 0 && mHeight > 0);
}

bool nsDeviceContext::CheckDPIChange() {
  int32_t oldDevPixels = mAppUnitsPerDevPixelAtUnitFullZoom;
  int32_t oldInches = mAppUnitsPerPhysicalInch;

  SetDPI();

  return oldDevPixels != mAppUnitsPerDevPixelAtUnitFullZoom ||
         oldInches != mAppUnitsPerPhysicalInch;
}

bool nsDeviceContext::SetFullZoom(float aScale) {
  if (aScale <= 0) {
    MOZ_ASSERT_UNREACHABLE("Invalid full zoom value");
    return false;
  }
  int32_t oldAppUnitsPerDevPixel = mAppUnitsPerDevPixel;
  mFullZoom = aScale;
  UpdateAppUnitsForFullZoom();
  return oldAppUnitsPerDevPixel != mAppUnitsPerDevPixel;
}

static int32_t ApplyFullZoom(int32_t aUnzoomedAppUnits, float aFullZoom) {
  if (aFullZoom == 1.0f) {
    return aUnzoomedAppUnits;
  }
  return std::max(1, NSToIntRound(float(aUnzoomedAppUnits) / aFullZoom));
}

int32_t nsDeviceContext::AppUnitsPerDevPixelInTopLevelChromePage() const {
  // The only zoom that applies to chrome pages is the system zoom, if any.
  return ApplyFullZoom(mAppUnitsPerDevPixelAtUnitFullZoom,
                       LookAndFeel::SystemZoomSettings().mFullZoom);
}

void nsDeviceContext::UpdateAppUnitsForFullZoom() {
  mAppUnitsPerDevPixel =
      ApplyFullZoom(mAppUnitsPerDevPixelAtUnitFullZoom, mFullZoom);
  // adjust mFullZoom to reflect appunit rounding
  mFullZoom = float(mAppUnitsPerDevPixelAtUnitFullZoom) / mAppUnitsPerDevPixel;
}

DesktopToLayoutDeviceScale nsDeviceContext::GetDesktopToDeviceScale() {
  if (RefPtr<widget::Screen> screen = FindScreen()) {
    return screen->GetDesktopToLayoutDeviceScale();
  }
  return DesktopToLayoutDeviceScale(1.0);
}
