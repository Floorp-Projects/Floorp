/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* the features that media queries can test */

#include "nsMediaFeatures.h"
#include "nsGkAtoms.h"
#include "nsCSSKeywords.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsCSSProps.h"
#include "nsCSSValue.h"
#ifdef XP_WIN
#include "mozilla/LookAndFeel.h"
#endif
#include "nsDeviceContext.h"
#include "nsIBaseWindow.h"
#include "nsIDocShell.h"
#include "nsIDocument.h"
#include "nsIWidget.h"
#include "nsContentUtils.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/StyleSheetInlines.h"

using namespace mozilla;

static nsTArray<RefPtr<nsAtom>>* sSystemMetrics = nullptr;

#ifdef XP_WIN
// Cached theme identifier for the moz-windows-theme media query.
static uint8_t sWinThemeId = LookAndFeel::eWindowsTheme_Generic;
#endif

static const nsCSSKTableEntry kOrientationKeywords[] = {
  { eCSSKeyword_portrait,                 StyleOrientation::Portrait },
  { eCSSKeyword_landscape,                StyleOrientation::Landscape },
  { eCSSKeyword_UNKNOWN,                  -1 }
};

static const nsCSSKTableEntry kScanKeywords[] = {
  { eCSSKeyword_progressive,              StyleScan::Progressive },
  { eCSSKeyword_interlace,                StyleScan::Interlace },
  { eCSSKeyword_UNKNOWN,                  -1 }
};

static const nsCSSKTableEntry kDisplayModeKeywords[] = {
  { eCSSKeyword_browser,                 StyleDisplayMode::Browser },
  { eCSSKeyword_minimal_ui,              StyleDisplayMode::MinimalUi },
  { eCSSKeyword_standalone,              StyleDisplayMode::Standalone },
  { eCSSKeyword_fullscreen,              StyleDisplayMode::Fullscreen },
  { eCSSKeyword_UNKNOWN,                 -1 }
};

#ifdef XP_WIN
struct WindowsThemeName {
  LookAndFeel::WindowsTheme mId;
  nsStaticAtom** mName;
};

// Windows theme identities used in the -moz-windows-theme media query.
const WindowsThemeName kThemeStrings[] = {
  { LookAndFeel::eWindowsTheme_Aero,       &nsGkAtoms::aero },
  { LookAndFeel::eWindowsTheme_AeroLite,   &nsGkAtoms::aero_lite },
  { LookAndFeel::eWindowsTheme_LunaBlue,   &nsGkAtoms::luna_blue },
  { LookAndFeel::eWindowsTheme_LunaOlive,  &nsGkAtoms::luna_olive },
  { LookAndFeel::eWindowsTheme_LunaSilver, &nsGkAtoms::luna_silver },
  { LookAndFeel::eWindowsTheme_Royale,     &nsGkAtoms::royale },
  { LookAndFeel::eWindowsTheme_Zune,       &nsGkAtoms::zune },
  { LookAndFeel::eWindowsTheme_Generic,    &nsGkAtoms::generic_ }
};

struct OperatingSystemVersionInfo {
  LookAndFeel::OperatingSystemVersion mId;
  nsStaticAtom** mName;
};

// Os version identities used in the -moz-os-version media query.
const OperatingSystemVersionInfo kOsVersionStrings[] = {
  { LookAndFeel::eOperatingSystemVersion_Windows7,  &nsGkAtoms::windows_win7 },
  { LookAndFeel::eOperatingSystemVersion_Windows8,  &nsGkAtoms::windows_win8 },
  { LookAndFeel::eOperatingSystemVersion_Windows10, &nsGkAtoms::windows_win10 }
};
#endif

// A helper for four features below
static nsSize
GetSize(nsIDocument* aDocument)
{
  nsPresContext* pc = aDocument->GetPresContext();

  // Per spec, return a 0x0 viewport if we're not being rendered. See:
  //
  //  * https://github.com/w3c/csswg-drafts/issues/571
  //  * https://github.com/whatwg/html/issues/1813
  //
  if (!pc) {
    return { };
  }

  if (pc->IsRootPaginatedDocument()) {
    // We want the page size, including unprintable areas and margins.
    //
    // FIXME(emilio, bug 1414600): Not quite!
    return pc->GetPageSize();
  }

  return pc->GetVisibleArea().Size();
}

static void
GetWidth(nsIDocument* aDocument, const nsMediaFeature*,
         nsCSSValue& aResult)
{
  nsSize size = GetSize(aDocument);
  aResult.SetFloatValue(CSSPixel::FromAppUnits(size.width), eCSSUnit_Pixel);
}

static void
GetHeight(nsIDocument* aDocument, const nsMediaFeature*,
          nsCSSValue& aResult)
{
  nsSize size = GetSize(aDocument);
  aResult.SetFloatValue(CSSPixel::FromAppUnits(size.height), eCSSUnit_Pixel);
}

static bool
IsDeviceSizePageSize(nsIDocument* aDocument)
{
  nsIDocShell* docShell = aDocument->GetDocShell();
  if (!docShell) {
    return false;
  }
  return docShell->GetDeviceSizeIsPageSize();
}

// A helper for three features below.
static nsSize
GetDeviceSize(nsIDocument* aDocument)
{
  if (nsContentUtils::ShouldResistFingerprinting(aDocument) ||
      IsDeviceSizePageSize(aDocument)) {
    return GetSize(aDocument);
  }

  nsPresContext* pc = aDocument->GetPresContext();
  // NOTE(emilio): We should probably figure out how to return an appropriate
  // device size here, though in a multi-screen world that makes no sense
  // really.
  if (!pc) {
    return { };
  }

  if (pc->IsRootPaginatedDocument()) {
    // We want the page size, including unprintable areas and margins.
    // XXX The spec actually says we want the "page sheet size", but
    // how is that different?
    return pc->GetPageSize();
  }

  nsSize size;
  pc->DeviceContext()->GetDeviceSurfaceDimensions(size.width, size.height);
  return size;
}

static void
GetDeviceWidth(nsIDocument* aDocument, const nsMediaFeature*,
               nsCSSValue& aResult)
{
  nsSize size = GetDeviceSize(aDocument);
  aResult.SetFloatValue(CSSPixel::FromAppUnits(size.width), eCSSUnit_Pixel);
}

static void
GetDeviceHeight(nsIDocument* aDocument, const nsMediaFeature*,
                nsCSSValue& aResult)
{
  nsSize size = GetDeviceSize(aDocument);
  aResult.SetFloatValue(CSSPixel::FromAppUnits(size.height), eCSSUnit_Pixel);
}

static void
GetOrientation(nsIDocument* aDocument, const nsMediaFeature*,
               nsCSSValue& aResult)
{
  nsSize size = GetSize(aDocument);
  // Per spec, square viewports should be 'portrait'
  auto orientation = size.width > size.height
    ? StyleOrientation::Landscape : StyleOrientation::Portrait;
  aResult.SetEnumValue(orientation);
}

static void
GetDeviceOrientation(nsIDocument* aDocument, const nsMediaFeature*,
                     nsCSSValue& aResult)
{
  nsSize size = GetDeviceSize(aDocument);
  // Per spec, square viewports should be 'portrait'
  auto orientation = size.width > size.height
    ? StyleOrientation::Landscape : StyleOrientation::Portrait;
  aResult.SetEnumValue(orientation);
}

static void
GetIsResourceDocument(nsIDocument* aDocument, const nsMediaFeature*,
                      nsCSSValue& aResult)
{
  aResult.SetIntValue(aDocument->IsResourceDoc() ? 1 : 0, eCSSUnit_Integer);
}

// Helper for two features below
static void
MakeArray(const nsSize& aSize, nsCSSValue& aResult)
{
  RefPtr<nsCSSValue::Array> a = nsCSSValue::Array::Create(2);

  a->Item(0).SetIntValue(aSize.width, eCSSUnit_Integer);
  a->Item(1).SetIntValue(aSize.height, eCSSUnit_Integer);

  aResult.SetArrayValue(a, eCSSUnit_Array);
}

static void
GetAspectRatio(nsIDocument* aDocument, const nsMediaFeature*,
               nsCSSValue& aResult)
{
  MakeArray(GetSize(aDocument), aResult);
}

static void
GetDeviceAspectRatio(nsIDocument* aDocument, const nsMediaFeature*,
                     nsCSSValue& aResult)
{
  MakeArray(GetDeviceSize(aDocument), aResult);
}

static nsDeviceContext*
GetDeviceContextFor(nsIDocument* aDocument)
{
  nsPresContext* pc = aDocument->GetPresContext();
  if (!pc) {
    return nullptr;
  }

  // It would be nice to call nsLayoutUtils::GetDeviceContextForScreenInfo here,
  // except for two things:  (1) it can flush, and flushing is bad here, and (2)
  // it doesn't really get us consistency in multi-monitor situations *anyway*.
  return pc->DeviceContext();
}

static void
GetColor(nsIDocument* aDocument, const nsMediaFeature*,
         nsCSSValue& aResult)
{
  // Use depth of 24 when resisting fingerprinting, or when we're not being
  // rendered.
  uint32_t depth = 24;

  if (!nsContentUtils::ShouldResistFingerprinting(aDocument)) {
    if (nsDeviceContext* dx = GetDeviceContextFor(aDocument)) {
      // FIXME: On a monochrome device, return 0!
      dx->GetDepth(depth);
    }
  }

  // The spec says to use bits *per color component*, so divide by 3,
  // and round down, since the spec says to use the smallest when the
  // color components differ.
  depth /= 3;
  aResult.SetIntValue(int32_t(depth), eCSSUnit_Integer);
}

static void
GetColorIndex(nsIDocument* aDocument, const nsMediaFeature*,
              nsCSSValue& aResult)
{
  // We should return zero if the device does not use a color lookup
  // table.  Stuart says that our handling of displays with 8-bit
  // color is bad enough that we never change the lookup table to
  // match what we're trying to display, so perhaps we should always
  // return zero.  Given that there isn't any better information
  // exposed, we don't have much other choice.
  aResult.SetIntValue(0, eCSSUnit_Integer);
}

static void
GetMonochrome(nsIDocument* aDocument, const nsMediaFeature*,
              nsCSSValue& aResult)
{
  // For color devices we should return 0.
  // FIXME: On a monochrome device, return the actual color depth, not
  // 0!
  aResult.SetIntValue(0, eCSSUnit_Integer);
}

static void
GetResolution(nsIDocument* aDocument, const nsMediaFeature*,
              nsCSSValue& aResult)
{
  // We're returning resolution in terms of device pixels per css pixel, since
  // that is the preferred unit for media queries of resolution. This avoids
  // introducing precision error from conversion to and from less-used
  // physical units like inches.

  float dppx = 1.;

  if (nsDeviceContext* dx = GetDeviceContextFor(aDocument)) {
    if (nsContentUtils::ShouldResistFingerprinting(aDocument)) {
      dppx = dx->GetFullZoom();
    } else {
      // Get the actual device pixel ratio, which also takes zoom into account.
      dppx =
        float(nsPresContext::AppUnitsPerCSSPixel()) / dx->AppUnitsPerDevPixel();
    }
  }

  aResult.SetFloatValue(dppx, eCSSUnit_Pixel);
}

static void
GetScan(nsIDocument* aDocument, const nsMediaFeature*,
        nsCSSValue& aResult)
{
  // Since Gecko doesn't support the 'tv' media type, the 'scan'
  // feature is never present.
  aResult.Reset();
}

static nsIDocument*
TopDocument(nsIDocument* aDocument)
{
  nsIDocument* current = aDocument;
  while (nsIDocument* parent = current->GetParentDocument()) {
    current = parent;
  }
  return current;
}

static void
GetDisplayMode(nsIDocument* aDocument, const nsMediaFeature*,
               nsCSSValue& aResult)
{
  nsIDocument* rootDocument = TopDocument(aDocument);

  nsCOMPtr<nsISupports> container = rootDocument->GetContainer();
  if (nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(container)) {
    nsCOMPtr<nsIWidget> mainWidget;
    baseWindow->GetMainWidget(getter_AddRefs(mainWidget));
    if (mainWidget && mainWidget->SizeMode() == nsSizeMode_Fullscreen) {
      aResult.SetEnumValue(StyleDisplayMode::Fullscreen);
      return;
    }
  }

  static_assert(nsIDocShell::DISPLAY_MODE_BROWSER == static_cast<int32_t>(StyleDisplayMode::Browser) &&
                nsIDocShell::DISPLAY_MODE_MINIMAL_UI == static_cast<int32_t>(StyleDisplayMode::MinimalUi) &&
                nsIDocShell::DISPLAY_MODE_STANDALONE == static_cast<int32_t>(StyleDisplayMode::Standalone) &&
                nsIDocShell::DISPLAY_MODE_FULLSCREEN == static_cast<int32_t>(StyleDisplayMode::Fullscreen),
                "nsIDocShell display modes must mach nsStyleConsts.h");

  uint32_t displayMode = nsIDocShell::DISPLAY_MODE_BROWSER;
  if (nsIDocShell* docShell = rootDocument->GetDocShell()) {
    docShell->GetDisplayMode(&displayMode);
  }

  aResult.SetEnumValue(static_cast<StyleDisplayMode>(displayMode));
}

static void
GetGrid(nsIDocument* aDocument, const nsMediaFeature*, nsCSSValue& aResult)
{
  // Gecko doesn't support grid devices (e.g., ttys), so the 'grid'
  // feature is always 0.
  aResult.SetIntValue(0, eCSSUnit_Integer);
}

static void
GetDevicePixelRatio(nsIDocument* aDocument, const nsMediaFeature*,
                    nsCSSValue& aResult)
{
  if (nsContentUtils::ShouldResistFingerprinting(aDocument)) {
    aResult.SetFloatValue(1.0, eCSSUnit_Number);
    return;
  }

  nsIPresShell* presShell = aDocument->GetShell();
  if (!presShell) {
    aResult.SetFloatValue(1.0, eCSSUnit_Number);
    return;
  }

  nsPresContext* pc = presShell->GetPresContext();
  if (!pc) {
    aResult.SetFloatValue(1.0, eCSSUnit_Number);
    return;
  }

  float ratio = pc->CSSPixelsToDevPixels(1.0f);
  aResult.SetFloatValue(ratio, eCSSUnit_Number);
}

static void
GetTransform3d(nsIDocument* aDocument, const nsMediaFeature*,
               nsCSSValue& aResult)
{
  // Gecko supports 3d transforms, so this feature is always 1.
  aResult.SetIntValue(1, eCSSUnit_Integer);
}

static bool
HasSystemMetric(nsAtom* aMetric)
{
  nsMediaFeatures::InitSystemMetrics();
  return sSystemMetrics->IndexOf(aMetric) != sSystemMetrics->NoIndex;
}

#ifdef XP_WIN
static uint8_t
GetWindowsThemeIdentifier()
{
  nsMediaFeatures::InitSystemMetrics();
  return sWinThemeId;
}
#endif

static void
GetSystemMetric(nsIDocument* aDocument, const nsMediaFeature* aFeature,
                nsCSSValue& aResult)
{
  aResult.Reset();

  const bool isAccessibleFromContentPages =
    !(aFeature->mReqFlags & nsMediaFeature::eUserAgentAndChromeOnly);

  MOZ_ASSERT(!isAccessibleFromContentPages ||
             *aFeature->mName == nsGkAtoms::_moz_touch_enabled);

  if (isAccessibleFromContentPages &&
      nsContentUtils::ShouldResistFingerprinting(aDocument)) {
    // If "privacy.resistFingerprinting" is enabled, then we simply don't
    // return any system-backed media feature values. (No spoofed values
    // returned.)
    return;
  }

  MOZ_ASSERT(aFeature->mValueType == nsMediaFeature::eBoolInteger,
             "unexpected type");

  nsAtom* metricAtom = *aFeature->mData.mMetric;
  bool hasMetric = HasSystemMetric(metricAtom);
  aResult.SetIntValue(hasMetric ? 1 : 0, eCSSUnit_Integer);
}

static void
GetWindowsTheme(nsIDocument* aDocument, const nsMediaFeature* aFeature,
                nsCSSValue& aResult)
{
  aResult.Reset();

  MOZ_ASSERT(aFeature->mReqFlags & nsMediaFeature::eUserAgentAndChromeOnly);
  if (nsContentUtils::ShouldResistFingerprinting(aDocument)) {
    return;
  }

#ifdef XP_WIN
  uint8_t windowsThemeId = GetWindowsThemeIdentifier();

  // Classic mode should fail to match.
  if (windowsThemeId == LookAndFeel::eWindowsTheme_Classic)
    return;

  // Look up the appropriate theme string
  for (const auto& theme : kThemeStrings) {
    if (windowsThemeId == theme.mId) {
      aResult.SetAtomIdentValue((*theme.mName)->ToAddRefed());
      break;
    }
  }
#endif
}

static void
GetOperatingSystemVersion(nsIDocument* aDocument, const nsMediaFeature* aFeature,
                         nsCSSValue& aResult)
{
  aResult.Reset();

  MOZ_ASSERT(aFeature->mReqFlags & nsMediaFeature::eUserAgentAndChromeOnly);
  if (nsContentUtils::ShouldResistFingerprinting(aDocument)) {
    return;
  }

#ifdef XP_WIN
  int32_t metricResult;
  if (NS_SUCCEEDED(
        LookAndFeel::GetInt(LookAndFeel::eIntID_OperatingSystemVersionIdentifier,
                            &metricResult))) {
    for (const auto& osVersion : kOsVersionStrings) {
      if (metricResult == osVersion.mId) {
        aResult.SetAtomIdentValue((*osVersion.mName)->ToAddRefed());
        break;
      }
    }
  }
#endif
}

static void
GetIsGlyph(nsIDocument* aDocument, const nsMediaFeature* aFeature,
           nsCSSValue& aResult)
{
  MOZ_ASSERT(aFeature->mReqFlags & nsMediaFeature::eUserAgentAndChromeOnly);
  aResult.SetIntValue(aDocument->IsSVGGlyphsDocument() ? 1 : 0, eCSSUnit_Integer);
}

/* static */ void
nsMediaFeatures::InitSystemMetrics()
{
  if (sSystemMetrics)
    return;

  MOZ_ASSERT(NS_IsMainThread());

  sSystemMetrics = new nsTArray<RefPtr<nsAtom>>;

  /***************************************************************************
   * ANY METRICS ADDED HERE SHOULD ALSO BE ADDED AS MEDIA QUERIES BELOW      *
   ***************************************************************************/

  int32_t metricResult =
    LookAndFeel::GetInt(LookAndFeel::eIntID_ScrollArrowStyle);
  if (metricResult & LookAndFeel::eScrollArrow_StartBackward) {
    sSystemMetrics->AppendElement(nsGkAtoms::scrollbar_start_backward);
  }
  if (metricResult & LookAndFeel::eScrollArrow_StartForward) {
    sSystemMetrics->AppendElement(nsGkAtoms::scrollbar_start_forward);
  }
  if (metricResult & LookAndFeel::eScrollArrow_EndBackward) {
    sSystemMetrics->AppendElement(nsGkAtoms::scrollbar_end_backward);
  }
  if (metricResult & LookAndFeel::eScrollArrow_EndForward) {
    sSystemMetrics->AppendElement(nsGkAtoms::scrollbar_end_forward);
  }

  metricResult =
    LookAndFeel::GetInt(LookAndFeel::eIntID_ScrollSliderStyle);
  if (metricResult != LookAndFeel::eScrollThumbStyle_Normal) {
    sSystemMetrics->AppendElement(nsGkAtoms::scrollbar_thumb_proportional);
  }

  metricResult =
    LookAndFeel::GetInt(LookAndFeel::eIntID_UseOverlayScrollbars);
  if (metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::overlay_scrollbars);
  }

  metricResult =
    LookAndFeel::GetInt(LookAndFeel::eIntID_MenuBarDrag);
  if (metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::menubar_drag);
  }

  nsresult rv =
    LookAndFeel::GetInt(LookAndFeel::eIntID_WindowsDefaultTheme, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::windows_default_theme);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_MacGraphiteTheme, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::mac_graphite_theme);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_MacYosemiteTheme, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::mac_yosemite_theme);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_WindowsAccentColorInTitlebar, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::windows_accent_color_in_titlebar);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_DWMCompositor, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::windows_compositor);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_WindowsGlass, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::windows_glass);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_WindowsClassic, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::windows_classic);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_TouchEnabled, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::touch_enabled);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_SwipeAnimationEnabled,
                           &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::swipe_animation_enabled);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_GTKCSDAvailable,
                           &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::gtk_csd_available);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_GTKCSDMinimizeButton,
                           &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::gtk_csd_minimize_button);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_GTKCSDMaximizeButton,
                           &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::gtk_csd_maximize_button);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_GTKCSDCloseButton,
                           &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::gtk_csd_close_button);
  }

#ifdef XP_WIN
  if (NS_SUCCEEDED(
        LookAndFeel::GetInt(LookAndFeel::eIntID_WindowsThemeIdentifier,
                            &metricResult))) {
    sWinThemeId = metricResult;
    switch (metricResult) {
      case LookAndFeel::eWindowsTheme_Aero:
        sSystemMetrics->AppendElement(nsGkAtoms::windows_theme_aero);
        break;
      case LookAndFeel::eWindowsTheme_AeroLite:
        sSystemMetrics->AppendElement(nsGkAtoms::windows_theme_aero_lite);
        break;
      case LookAndFeel::eWindowsTheme_LunaBlue:
        sSystemMetrics->AppendElement(nsGkAtoms::windows_theme_luna_blue);
        break;
      case LookAndFeel::eWindowsTheme_LunaOlive:
        sSystemMetrics->AppendElement(nsGkAtoms::windows_theme_luna_olive);
        break;
      case LookAndFeel::eWindowsTheme_LunaSilver:
        sSystemMetrics->AppendElement(nsGkAtoms::windows_theme_luna_silver);
        break;
      case LookAndFeel::eWindowsTheme_Royale:
        sSystemMetrics->AppendElement(nsGkAtoms::windows_theme_royale);
        break;
      case LookAndFeel::eWindowsTheme_Zune:
        sSystemMetrics->AppendElement(nsGkAtoms::windows_theme_zune);
        break;
      case LookAndFeel::eWindowsTheme_Generic:
        sSystemMetrics->AppendElement(nsGkAtoms::windows_theme_generic);
        break;
    }
  }
#endif
}

/* static */ void
nsMediaFeatures::FreeSystemMetrics()
{
  delete sSystemMetrics;
  sSystemMetrics = nullptr;
}

/* static */ void
nsMediaFeatures::Shutdown()
{
  FreeSystemMetrics();
}

/*
 * Adding new media features requires (1) adding the new feature to this
 * array, with appropriate entries (and potentially any new code needed
 * to support new types in these entries and (2) ensuring that either
 * nsPresContext::MediaFeatureValuesChanged or
 * nsPresContext::PostMediaFeatureValuesChangedEvent is called when the
 * value that would be returned by the entry's mGetter changes.
 */

/* static */ const nsMediaFeature
nsMediaFeatures::features[] = {
  {
    &nsGkAtoms::width,
    nsMediaFeature::eMinMaxAllowed,
    nsMediaFeature::eLength,
    nsMediaFeature::eNoRequirements,
    { nullptr },
    GetWidth
  },
  {
    &nsGkAtoms::height,
    nsMediaFeature::eMinMaxAllowed,
    nsMediaFeature::eLength,
    nsMediaFeature::eNoRequirements,
    { nullptr },
    GetHeight
  },
  {
    &nsGkAtoms::deviceWidth,
    nsMediaFeature::eMinMaxAllowed,
    nsMediaFeature::eLength,
    nsMediaFeature::eNoRequirements,
    { nullptr },
    GetDeviceWidth
  },
  {
    &nsGkAtoms::deviceHeight,
    nsMediaFeature::eMinMaxAllowed,
    nsMediaFeature::eLength,
    nsMediaFeature::eNoRequirements,
    { nullptr },
    GetDeviceHeight
  },
  {
    &nsGkAtoms::orientation,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eEnumerated,
    nsMediaFeature::eNoRequirements,
    { kOrientationKeywords },
    GetOrientation
  },
  {
    &nsGkAtoms::aspectRatio,
    nsMediaFeature::eMinMaxAllowed,
    nsMediaFeature::eIntRatio,
    nsMediaFeature::eNoRequirements,
    { nullptr },
    GetAspectRatio
  },
  {
    &nsGkAtoms::deviceAspectRatio,
    nsMediaFeature::eMinMaxAllowed,
    nsMediaFeature::eIntRatio,
    nsMediaFeature::eNoRequirements,
    { nullptr },
    GetDeviceAspectRatio
  },
  {
    &nsGkAtoms::color,
    nsMediaFeature::eMinMaxAllowed,
    nsMediaFeature::eInteger,
    nsMediaFeature::eNoRequirements,
    { nullptr },
    GetColor
  },
  {
    &nsGkAtoms::colorIndex,
    nsMediaFeature::eMinMaxAllowed,
    nsMediaFeature::eInteger,
    nsMediaFeature::eNoRequirements,
    { nullptr },
    GetColorIndex
  },
  {
    &nsGkAtoms::monochrome,
    nsMediaFeature::eMinMaxAllowed,
    nsMediaFeature::eInteger,
    nsMediaFeature::eNoRequirements,
    { nullptr },
    GetMonochrome
  },
  {
    &nsGkAtoms::resolution,
    nsMediaFeature::eMinMaxAllowed,
    nsMediaFeature::eResolution,
    nsMediaFeature::eNoRequirements,
    { nullptr },
    GetResolution
  },
  {
    &nsGkAtoms::scan,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eEnumerated,
    nsMediaFeature::eNoRequirements,
    { kScanKeywords },
    GetScan
  },
  {
    &nsGkAtoms::grid,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eBoolInteger,
    nsMediaFeature::eNoRequirements,
    { nullptr },
    GetGrid
  },
  {
    &nsGkAtoms::displayMode,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eEnumerated,
    nsMediaFeature::eNoRequirements,
    { kDisplayModeKeywords },
    GetDisplayMode
  },

  // Webkit extensions that we support for de-facto web compatibility
  // -webkit-{min|max}-device-pixel-ratio (controlled with its own pref):
  {
    &nsGkAtoms::devicePixelRatio,
    nsMediaFeature::eMinMaxAllowed,
    nsMediaFeature::eFloat,
    nsMediaFeature::eHasWebkitPrefix |
      nsMediaFeature::eWebkitDevicePixelRatioPrefEnabled,
    { nullptr },
    GetDevicePixelRatio
  },
  // -webkit-transform-3d:
  {
    &nsGkAtoms::transform_3d,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eBoolInteger,
    nsMediaFeature::eHasWebkitPrefix,
    { nullptr },
    GetTransform3d
  },

  // Mozilla extensions
  {
    &nsGkAtoms::_moz_device_pixel_ratio,
    nsMediaFeature::eMinMaxAllowed,
    nsMediaFeature::eFloat,
    nsMediaFeature::eNoRequirements,
    { nullptr },
    GetDevicePixelRatio
  },
  {
    &nsGkAtoms::_moz_device_orientation,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eEnumerated,
    nsMediaFeature::eNoRequirements,
    { kOrientationKeywords },
    GetDeviceOrientation
  },
  {
    &nsGkAtoms::_moz_is_resource_document,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eBoolInteger,
    nsMediaFeature::eNoRequirements,
    { nullptr },
    GetIsResourceDocument
  },
  {
    &nsGkAtoms::_moz_scrollbar_start_backward,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eBoolInteger,
    nsMediaFeature::eUserAgentAndChromeOnly,
    { &nsGkAtoms::scrollbar_start_backward },
    GetSystemMetric
  },
  {
    &nsGkAtoms::_moz_scrollbar_start_forward,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eBoolInteger,
    nsMediaFeature::eUserAgentAndChromeOnly,
    { &nsGkAtoms::scrollbar_start_forward },
    GetSystemMetric
  },
  {
    &nsGkAtoms::_moz_scrollbar_end_backward,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eBoolInteger,
    nsMediaFeature::eUserAgentAndChromeOnly,
    { &nsGkAtoms::scrollbar_end_backward },
    GetSystemMetric
  },
  {
    &nsGkAtoms::_moz_scrollbar_end_forward,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eBoolInteger,
    nsMediaFeature::eUserAgentAndChromeOnly,
    { &nsGkAtoms::scrollbar_end_forward },
    GetSystemMetric
  },
  {
    &nsGkAtoms::_moz_scrollbar_thumb_proportional,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eBoolInteger,
    nsMediaFeature::eUserAgentAndChromeOnly,
    { &nsGkAtoms::scrollbar_thumb_proportional },
    GetSystemMetric
  },
  {
    &nsGkAtoms::_moz_overlay_scrollbars,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eBoolInteger,
    nsMediaFeature::eUserAgentAndChromeOnly,
    { &nsGkAtoms::overlay_scrollbars },
    GetSystemMetric
  },
  {
    &nsGkAtoms::_moz_windows_default_theme,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eBoolInteger,
    nsMediaFeature::eUserAgentAndChromeOnly,
    { &nsGkAtoms::windows_default_theme },
    GetSystemMetric
  },
  {
    &nsGkAtoms::_moz_mac_graphite_theme,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eBoolInteger,
    nsMediaFeature::eUserAgentAndChromeOnly,
    { &nsGkAtoms::mac_graphite_theme },
    GetSystemMetric
  },
  {
    &nsGkAtoms::_moz_mac_yosemite_theme,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eBoolInteger,
    nsMediaFeature::eUserAgentAndChromeOnly,
    { &nsGkAtoms::mac_yosemite_theme },
    GetSystemMetric
  },
  {
    &nsGkAtoms::_moz_windows_accent_color_in_titlebar,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eBoolInteger,
    nsMediaFeature::eUserAgentAndChromeOnly,
    { &nsGkAtoms::windows_accent_color_in_titlebar },
    GetSystemMetric
  },
  {
    &nsGkAtoms::_moz_windows_compositor,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eBoolInteger,
    nsMediaFeature::eUserAgentAndChromeOnly,
    { &nsGkAtoms::windows_compositor },
    GetSystemMetric
  },
  {
    &nsGkAtoms::_moz_windows_classic,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eBoolInteger,
    nsMediaFeature::eUserAgentAndChromeOnly,
    { &nsGkAtoms::windows_classic },
    GetSystemMetric
  },
  {
    &nsGkAtoms::_moz_windows_glass,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eBoolInteger,
    nsMediaFeature::eUserAgentAndChromeOnly,
    { &nsGkAtoms::windows_glass },
    GetSystemMetric
  },
  {
    &nsGkAtoms::_moz_touch_enabled,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eBoolInteger,
    // FIXME(emilio): Restrict (or remove?) when bug 1035774 lands.
    nsMediaFeature::eNoRequirements,
    { &nsGkAtoms::touch_enabled },
    GetSystemMetric
  },
  {
    &nsGkAtoms::_moz_menubar_drag,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eBoolInteger,
    nsMediaFeature::eUserAgentAndChromeOnly,
    { &nsGkAtoms::menubar_drag },
    GetSystemMetric
  },
  {
    &nsGkAtoms::_moz_windows_theme,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eIdent,
    nsMediaFeature::eUserAgentAndChromeOnly,
    { nullptr },
    GetWindowsTheme
  },
  {
    &nsGkAtoms::_moz_os_version,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eIdent,
    nsMediaFeature::eUserAgentAndChromeOnly,
    { nullptr },
    GetOperatingSystemVersion
  },

  {
    &nsGkAtoms::_moz_swipe_animation_enabled,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eBoolInteger,
    nsMediaFeature::eUserAgentAndChromeOnly,
    { &nsGkAtoms::swipe_animation_enabled },
    GetSystemMetric
  },

  {
    &nsGkAtoms::_moz_gtk_csd_available,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eBoolInteger,
    nsMediaFeature::eUserAgentAndChromeOnly,
    { &nsGkAtoms::gtk_csd_available },
    GetSystemMetric
  },

  {
    &nsGkAtoms::_moz_gtk_csd_minimize_button,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eBoolInteger,
    nsMediaFeature::eUserAgentAndChromeOnly,
    { &nsGkAtoms::gtk_csd_minimize_button },
    GetSystemMetric
  },

  {
    &nsGkAtoms::_moz_gtk_csd_maximize_button,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eBoolInteger,
    nsMediaFeature::eUserAgentAndChromeOnly,
    { &nsGkAtoms::gtk_csd_maximize_button },
    GetSystemMetric
  },

  {
    &nsGkAtoms::_moz_gtk_csd_close_button,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eBoolInteger,
    nsMediaFeature::eUserAgentAndChromeOnly,
    { &nsGkAtoms::gtk_csd_close_button },
    GetSystemMetric
  },

  // Internal -moz-is-glyph media feature: applies only inside SVG glyphs.
  // Internal because it is really only useful in the user agent anyway
  //  and therefore not worth standardizing.
  {
    &nsGkAtoms::_moz_is_glyph,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eBoolInteger,
    nsMediaFeature::eUserAgentAndChromeOnly,
    { nullptr },
    GetIsGlyph
  },
  // Null-mName terminator:
  {
    nullptr,
    nsMediaFeature::eMinMaxAllowed,
    nsMediaFeature::eInteger,
    nsMediaFeature::eNoRequirements,
    { nullptr },
    nullptr
  },
};
