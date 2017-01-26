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
#include "nsCSSValue.h"
#ifdef XP_WIN
#include "mozilla/LookAndFeel.h"
#endif
#include "nsCSSRuleProcessor.h"
#include "nsDeviceContext.h"
#include "nsIBaseWindow.h"
#include "nsIDocument.h"
#include "nsIWidget.h"
#include "nsContentUtils.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/StyleSheetInlines.h"

using namespace mozilla;

static const nsCSSProps::KTableEntry kOrientationKeywords[] = {
  { eCSSKeyword_portrait,                 NS_STYLE_ORIENTATION_PORTRAIT },
  { eCSSKeyword_landscape,                NS_STYLE_ORIENTATION_LANDSCAPE },
  { eCSSKeyword_UNKNOWN,                  -1 }
};

static const nsCSSProps::KTableEntry kScanKeywords[] = {
  { eCSSKeyword_progressive,              NS_STYLE_SCAN_PROGRESSIVE },
  { eCSSKeyword_interlace,                NS_STYLE_SCAN_INTERLACE },
  { eCSSKeyword_UNKNOWN,                  -1 }
};

static const nsCSSProps::KTableEntry kDisplayModeKeywords[] = {
  { eCSSKeyword_browser,                 NS_STYLE_DISPLAY_MODE_BROWSER },
  { eCSSKeyword_minimal_ui,              NS_STYLE_DISPLAY_MODE_MINIMAL_UI },
  { eCSSKeyword_standalone,              NS_STYLE_DISPLAY_MODE_STANDALONE },
  { eCSSKeyword_fullscreen,              NS_STYLE_DISPLAY_MODE_FULLSCREEN },
  { eCSSKeyword_UNKNOWN,                 -1 }
};

#ifdef XP_WIN
struct WindowsThemeName {
  LookAndFeel::WindowsTheme id;
  const wchar_t* name;
};

// Windows theme identities used in the -moz-windows-theme media query.
const WindowsThemeName themeStrings[] = {
  { LookAndFeel::eWindowsTheme_Aero,       L"aero" },
  { LookAndFeel::eWindowsTheme_AeroLite,   L"aero-lite" },
  { LookAndFeel::eWindowsTheme_LunaBlue,   L"luna-blue" },
  { LookAndFeel::eWindowsTheme_LunaOlive,  L"luna-olive" },
  { LookAndFeel::eWindowsTheme_LunaSilver, L"luna-silver" },
  { LookAndFeel::eWindowsTheme_Royale,     L"royale" },
  { LookAndFeel::eWindowsTheme_Zune,       L"zune" },
  { LookAndFeel::eWindowsTheme_Generic,    L"generic" }
};

struct OperatingSystemVersionInfo {
  LookAndFeel::OperatingSystemVersion id;
  const wchar_t* name;
};

// Os version identities used in the -moz-os-version media query.
const OperatingSystemVersionInfo osVersionStrings[] = {
  { LookAndFeel::eOperatingSystemVersion_Windows7,      L"windows-win7" },
  { LookAndFeel::eOperatingSystemVersion_Windows8,      L"windows-win8" },
  { LookAndFeel::eOperatingSystemVersion_Windows10,     L"windows-win10" }
};
#endif

// A helper for four features below
static nsSize
GetSize(nsPresContext* aPresContext)
{
  nsSize size;
  if (aPresContext->IsRootPaginatedDocument())
    // We want the page size, including unprintable areas and margins.
    size = aPresContext->GetPageSize();
  else
    size = aPresContext->GetVisibleArea().Size();
  return size;
}

static void
GetWidth(nsPresContext* aPresContext, const nsMediaFeature*,
         nsCSSValue& aResult)
{
  nsSize size = GetSize(aPresContext);
  float pixelWidth = aPresContext->AppUnitsToFloatCSSPixels(size.width);
  aResult.SetFloatValue(pixelWidth, eCSSUnit_Pixel);
}

static void
GetHeight(nsPresContext* aPresContext, const nsMediaFeature*,
          nsCSSValue& aResult)
{
  nsSize size = GetSize(aPresContext);
  float pixelHeight = aPresContext->AppUnitsToFloatCSSPixels(size.height);
  aResult.SetFloatValue(pixelHeight, eCSSUnit_Pixel);
}

inline static nsDeviceContext*
GetDeviceContextFor(nsPresContext* aPresContext)
{
  // It would be nice to call
  // nsLayoutUtils::GetDeviceContextForScreenInfo here, except for two
  // things:  (1) it can flush, and flushing is bad here, and (2) it
  // doesn't really get us consistency in multi-monitor situations
  // *anyway*.
  return aPresContext->DeviceContext();
}

static bool
ShouldResistFingerprinting(nsPresContext* aPresContext)
{
  return nsContentUtils::ShouldResistFingerprinting(aPresContext->GetDocShell());
}

// A helper for three features below.
static nsSize
GetDeviceSize(nsPresContext* aPresContext)
{
  nsSize size;

  if (ShouldResistFingerprinting(aPresContext) || aPresContext->IsDeviceSizePageSize()) {
    size = GetSize(aPresContext);
  } else if (aPresContext->IsRootPaginatedDocument()) {
    // We want the page size, including unprintable areas and margins.
    // XXX The spec actually says we want the "page sheet size", but
    // how is that different?
    size = aPresContext->GetPageSize();
  } else {
    GetDeviceContextFor(aPresContext)->
      GetDeviceSurfaceDimensions(size.width, size.height);
  }
  return size;
}

static void
GetDeviceWidth(nsPresContext* aPresContext, const nsMediaFeature*,
               nsCSSValue& aResult)
{
  nsSize size = GetDeviceSize(aPresContext);
  float pixelWidth = aPresContext->AppUnitsToFloatCSSPixels(size.width);
  aResult.SetFloatValue(pixelWidth, eCSSUnit_Pixel);
}

static void
GetDeviceHeight(nsPresContext* aPresContext, const nsMediaFeature*,
                nsCSSValue& aResult)
{
  nsSize size = GetDeviceSize(aPresContext);
  float pixelHeight = aPresContext->AppUnitsToFloatCSSPixels(size.height);
  aResult.SetFloatValue(pixelHeight, eCSSUnit_Pixel);
}

static void
GetOrientation(nsPresContext* aPresContext, const nsMediaFeature*,
               nsCSSValue& aResult)
{
  nsSize size = GetSize(aPresContext);
  int32_t orientation;
  if (size.width > size.height) {
    orientation = NS_STYLE_ORIENTATION_LANDSCAPE;
  } else {
    // Per spec, square viewports should be 'portrait'
    orientation = NS_STYLE_ORIENTATION_PORTRAIT;
  }

  aResult.SetIntValue(orientation, eCSSUnit_Enumerated);
}

static void
GetDeviceOrientation(nsPresContext* aPresContext, const nsMediaFeature*,
                     nsCSSValue& aResult)
{
  nsSize size = GetDeviceSize(aPresContext);
  int32_t orientation;
  if (size.width > size.height) {
    orientation = NS_STYLE_ORIENTATION_LANDSCAPE;
  } else {
    // Per spec, square viewports should be 'portrait'
    orientation = NS_STYLE_ORIENTATION_PORTRAIT;
  }

  aResult.SetIntValue(orientation, eCSSUnit_Enumerated);
}

static void
GetIsResourceDocument(nsPresContext* aPresContext, const nsMediaFeature*,
                      nsCSSValue& aResult)
{
  nsIDocument* doc = aPresContext->Document();
  aResult.SetIntValue(doc && doc->IsResourceDoc() ? 1 : 0, eCSSUnit_Integer);
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
GetAspectRatio(nsPresContext* aPresContext, const nsMediaFeature*,
               nsCSSValue& aResult)
{
  MakeArray(GetSize(aPresContext), aResult);
}

static void
GetDeviceAspectRatio(nsPresContext* aPresContext, const nsMediaFeature*,
                     nsCSSValue& aResult)
{
  MakeArray(GetDeviceSize(aPresContext), aResult);
}

static void
GetColor(nsPresContext* aPresContext, const nsMediaFeature*,
         nsCSSValue& aResult)
{
  uint32_t depth = 24; // Use depth of 24 when resisting fingerprinting.

  if (!ShouldResistFingerprinting(aPresContext)) {
    // FIXME:  This implementation is bogus.  nsDeviceContext
    // doesn't provide reliable information (should be fixed in bug
    // 424386).
    // FIXME: On a monochrome device, return 0!
    nsDeviceContext *dx = GetDeviceContextFor(aPresContext);
    dx->GetDepth(depth);
  }

  // The spec says to use bits *per color component*, so divide by 3,
  // and round down, since the spec says to use the smallest when the
  // color components differ.
  depth /= 3;
  aResult.SetIntValue(int32_t(depth), eCSSUnit_Integer);
}

static void
GetColorIndex(nsPresContext* aPresContext, const nsMediaFeature*,
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
GetMonochrome(nsPresContext* aPresContext, const nsMediaFeature*,
              nsCSSValue& aResult)
{
  // For color devices we should return 0.
  // FIXME: On a monochrome device, return the actual color depth, not
  // 0!
  aResult.SetIntValue(0, eCSSUnit_Integer);
}

static void
GetResolution(nsPresContext* aPresContext, const nsMediaFeature*,
              nsCSSValue& aResult)
{
  float dpi = 96; // Use 96 when resisting fingerprinting.

  if (!ShouldResistFingerprinting(aPresContext)) {
    // Resolution measures device pixels per CSS (inch/cm/pixel).  We
    // return it in device pixels per CSS inches.
    dpi = float(nsPresContext::AppUnitsPerCSSInch()) /
          float(aPresContext->AppUnitsPerDevPixel());
  }

  aResult.SetFloatValue(dpi, eCSSUnit_Inch);
}

static void
GetScan(nsPresContext* aPresContext, const nsMediaFeature*,
        nsCSSValue& aResult)
{
  // Since Gecko doesn't support the 'tv' media type, the 'scan'
  // feature is never present.
  aResult.Reset();
}

static void
GetDisplayMode(nsPresContext* aPresContext, const nsMediaFeature*,
               nsCSSValue& aResult)
{
  nsCOMPtr<nsISupports> container;
  if (aPresContext) {
    // Calling GetRootPresContext() can be slow, so make sure to call it
    // just once.
    nsRootPresContext* root = aPresContext->GetRootPresContext();
    if (root && root->Document()) {
      container = root->Document()->GetContainer();
    }
  }
  nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(container);
  if (!baseWindow) {
    aResult.SetIntValue(NS_STYLE_DISPLAY_MODE_BROWSER, eCSSUnit_Enumerated);
    return;
  }
  nsCOMPtr<nsIWidget> mainWidget;
  baseWindow->GetMainWidget(getter_AddRefs(mainWidget));
  int32_t displayMode;
  nsSizeMode mode = mainWidget ? mainWidget->SizeMode() : nsSizeMode_Normal;
  // Background tabs are always in 'browser' mode for now.
  // If new modes are supported, please ensure not cause the regression in
  // Bug 1259641.
  switch (mode) {
    case nsSizeMode_Fullscreen:
      displayMode = NS_STYLE_DISPLAY_MODE_FULLSCREEN;
      break;
    default:
      displayMode = NS_STYLE_DISPLAY_MODE_BROWSER;
      break;
  }

  aResult.SetIntValue(displayMode, eCSSUnit_Enumerated);
}

static void
GetGrid(nsPresContext* aPresContext, const nsMediaFeature*,
        nsCSSValue& aResult)
{
  // Gecko doesn't support grid devices (e.g., ttys), so the 'grid'
  // feature is always 0.
  aResult.SetIntValue(0, eCSSUnit_Integer);
}

static void
GetDevicePixelRatio(nsPresContext* aPresContext, const nsMediaFeature*,
                    nsCSSValue& aResult)
{
  if (!ShouldResistFingerprinting(aPresContext)) {
    float ratio = aPresContext->CSSPixelsToDevPixels(1.0f);
    aResult.SetFloatValue(ratio, eCSSUnit_Number);
  } else {
    aResult.SetFloatValue(1.0, eCSSUnit_Number);
  }
}

static void
GetTransform3d(nsPresContext* aPresContext, const nsMediaFeature*,
               nsCSSValue& aResult)
{
  // Gecko supports 3d transforms, so this feature is always 1.
  aResult.SetIntValue(1, eCSSUnit_Integer);
}

static void
GetSystemMetric(nsPresContext* aPresContext, const nsMediaFeature* aFeature,
                nsCSSValue& aResult)
{
  aResult.Reset();
  if (ShouldResistFingerprinting(aPresContext)) {
    // If "privacy.resistFingerprinting" is enabled, then we simply don't
    // return any system-backed media feature values. (No spoofed values returned.)
    return;
  }

  MOZ_ASSERT(aFeature->mValueType == nsMediaFeature::eBoolInteger,
             "unexpected type");
  nsIAtom *metricAtom = *aFeature->mData.mMetric;
  bool hasMetric = nsCSSRuleProcessor::HasSystemMetric(metricAtom);
  aResult.SetIntValue(hasMetric ? 1 : 0, eCSSUnit_Integer);
}

static void
GetWindowsTheme(nsPresContext* aPresContext, const nsMediaFeature* aFeature,
                nsCSSValue& aResult)
{
  aResult.Reset();
  if (ShouldResistFingerprinting(aPresContext)) {
    return;
  }

#ifdef XP_WIN
  uint8_t windowsThemeId =
    nsCSSRuleProcessor::GetWindowsThemeIdentifier();

  // Classic mode should fail to match.
  if (windowsThemeId == LookAndFeel::eWindowsTheme_Classic)
    return;

  // Look up the appropriate theme string
  for (size_t i = 0; i < ArrayLength(themeStrings); ++i) {
    if (windowsThemeId == themeStrings[i].id) {
      aResult.SetStringValue(nsDependentString(themeStrings[i].name),
                             eCSSUnit_Ident);
      break;
    }
  }
#endif
}

static void
GetOperatingSystemVersion(nsPresContext* aPresContext, const nsMediaFeature* aFeature,
                         nsCSSValue& aResult)
{
  aResult.Reset();
  if (ShouldResistFingerprinting(aPresContext)) {
    return;
  }

#ifdef XP_WIN
  int32_t metricResult;
  if (NS_SUCCEEDED(
        LookAndFeel::GetInt(LookAndFeel::eIntID_OperatingSystemVersionIdentifier,
                            &metricResult))) {
    for (size_t i = 0; i < ArrayLength(osVersionStrings); ++i) {
      if (metricResult == osVersionStrings[i].id) {
        aResult.SetStringValue(nsDependentString(osVersionStrings[i].name),
                               eCSSUnit_Ident);
        break;
      }
    }
  }
#endif
}

static void
GetIsGlyph(nsPresContext* aPresContext, const nsMediaFeature* aFeature,
          nsCSSValue& aResult)
{
  aResult.SetIntValue(aPresContext->IsGlyph() ? 1 : 0, eCSSUnit_Integer);
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
    &nsGkAtoms::_moz_color_picker_available,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eBoolInteger,
    nsMediaFeature::eNoRequirements,
    { &nsGkAtoms::color_picker_available },
    GetSystemMetric
  },
  {
    &nsGkAtoms::_moz_scrollbar_start_backward,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eBoolInteger,
    nsMediaFeature::eNoRequirements,
    { &nsGkAtoms::scrollbar_start_backward },
    GetSystemMetric
  },
  {
    &nsGkAtoms::_moz_scrollbar_start_forward,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eBoolInteger,
    nsMediaFeature::eNoRequirements,
    { &nsGkAtoms::scrollbar_start_forward },
    GetSystemMetric
  },
  {
    &nsGkAtoms::_moz_scrollbar_end_backward,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eBoolInteger,
    nsMediaFeature::eNoRequirements,
    { &nsGkAtoms::scrollbar_end_backward },
    GetSystemMetric
  },
  {
    &nsGkAtoms::_moz_scrollbar_end_forward,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eBoolInteger,
    nsMediaFeature::eNoRequirements,
    { &nsGkAtoms::scrollbar_end_forward },
    GetSystemMetric
  },
  {
    &nsGkAtoms::_moz_scrollbar_thumb_proportional,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eBoolInteger,
    nsMediaFeature::eNoRequirements,
    { &nsGkAtoms::scrollbar_thumb_proportional },
    GetSystemMetric
  },
  {
    &nsGkAtoms::_moz_overlay_scrollbars,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eBoolInteger,
    nsMediaFeature::eNoRequirements,
    { &nsGkAtoms::overlay_scrollbars },
    GetSystemMetric
  },
  {
    &nsGkAtoms::_moz_windows_default_theme,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eBoolInteger,
    nsMediaFeature::eNoRequirements,
    { &nsGkAtoms::windows_default_theme },
    GetSystemMetric
  },
  {
    &nsGkAtoms::_moz_mac_graphite_theme,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eBoolInteger,
    nsMediaFeature::eNoRequirements,
    { &nsGkAtoms::mac_graphite_theme },
    GetSystemMetric
  },
  {
    &nsGkAtoms::_moz_mac_yosemite_theme,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eBoolInteger,
    nsMediaFeature::eNoRequirements,
    { &nsGkAtoms::mac_yosemite_theme },
    GetSystemMetric
  },
  {
    &nsGkAtoms::_moz_windows_compositor,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eBoolInteger,
    nsMediaFeature::eNoRequirements,
    { &nsGkAtoms::windows_compositor },
    GetSystemMetric
  },
  {
    &nsGkAtoms::_moz_windows_classic,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eBoolInteger,
    nsMediaFeature::eNoRequirements,
    { &nsGkAtoms::windows_classic },
    GetSystemMetric
  },
  {
    &nsGkAtoms::_moz_windows_glass,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eBoolInteger,
    nsMediaFeature::eNoRequirements,
    { &nsGkAtoms::windows_glass },
    GetSystemMetric
  },
  {
    &nsGkAtoms::_moz_touch_enabled,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eBoolInteger,
    nsMediaFeature::eNoRequirements,
    { &nsGkAtoms::touch_enabled },
    GetSystemMetric
  },
  {
    &nsGkAtoms::_moz_menubar_drag,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eBoolInteger,
    nsMediaFeature::eNoRequirements,
    { &nsGkAtoms::menubar_drag },
    GetSystemMetric
  },
  {
    &nsGkAtoms::_moz_windows_theme,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eIdent,
    nsMediaFeature::eNoRequirements,
    { nullptr },
    GetWindowsTheme
  },
  {
    &nsGkAtoms::_moz_os_version,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eIdent,
    nsMediaFeature::eNoRequirements,
    { nullptr },
    GetOperatingSystemVersion
  },

  {
    &nsGkAtoms::_moz_swipe_animation_enabled,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eBoolInteger,
    nsMediaFeature::eNoRequirements,
    { &nsGkAtoms::swipe_animation_enabled },
    GetSystemMetric
  },

  {
    &nsGkAtoms::_moz_physical_home_button,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eBoolInteger,
    nsMediaFeature::eNoRequirements,
    { &nsGkAtoms::physical_home_button },
    GetSystemMetric
  },

  // Internal -moz-is-glyph media feature: applies only inside SVG glyphs.
  // Internal because it is really only useful in the user agent anyway
  //  and therefore not worth standardizing.
  {
    &nsGkAtoms::_moz_is_glyph,
    nsMediaFeature::eMinMaxNotAllowed,
    nsMediaFeature::eBoolInteger,
    nsMediaFeature::eNoRequirements,
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
