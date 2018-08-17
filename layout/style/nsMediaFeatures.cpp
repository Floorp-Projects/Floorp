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
#include "mozilla/LookAndFeel.h"
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

bool
Gecko_MediaFeatures_IsResourceDocument(nsIDocument* aDocument)
{
  return aDocument->IsResourceDoc();
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

void
Gecko_MediaFeatures_GetDeviceSize(nsIDocument* aDocument,
                                  nscoord* aWidth,
                                  nscoord* aHeight)
{
  nsSize size = GetDeviceSize(aDocument);
  *aWidth = size.width;
  *aHeight = size.height;
}

uint32_t
Gecko_MediaFeatures_GetColorDepth(nsIDocument* aDocument)
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
  return depth / 3;
}

float
Gecko_MediaFeatures_GetResolution(nsIDocument* aDocument)
{
  // We're returning resolution in terms of device pixels per css pixel, since
  // that is the preferred unit for media queries of resolution. This avoids
  // introducing precision error from conversion to and from less-used
  // physical units like inches.
  nsPresContext* pc = aDocument->GetPresContext();
  if (!pc) {
    return 1.;
  }

  if (pc->GetOverrideDPPX() > 0.) {
    return pc->GetOverrideDPPX();
  }

  if (nsContentUtils::ShouldResistFingerprinting(aDocument)) {
    return pc->DeviceContext()->GetFullZoom();
  }
  // Get the actual device pixel ratio, which also takes zoom into account.
  return float(AppUnitsPerCSSPixel()) / pc->DeviceContext()->AppUnitsPerDevPixel();
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

StyleDisplayMode
Gecko_MediaFeatures_GetDisplayMode(nsIDocument* aDocument)
{
  nsIDocument* rootDocument = TopDocument(aDocument);

  nsCOMPtr<nsISupports> container = rootDocument->GetContainer();
  if (nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(container)) {
    nsCOMPtr<nsIWidget> mainWidget;
    baseWindow->GetMainWidget(getter_AddRefs(mainWidget));
    if (mainWidget && mainWidget->SizeMode() == nsSizeMode_Fullscreen) {
      return StyleDisplayMode::Fullscreen;
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

  return static_cast<StyleDisplayMode>(displayMode);
}

float
Gecko_MediaFeatures_GetDevicePixelRatio(nsIDocument* aDocument)
{
  if (nsContentUtils::ShouldResistFingerprinting(aDocument)) {
    return 1.0;
  }

  nsIPresShell* presShell = aDocument->GetShell();
  if (!presShell) {
    return 1.0;
  }

  nsPresContext* pc = presShell->GetPresContext();
  if (!pc) {
    return 1.0;
  }

  return pc->CSSPixelsToDevPixels(1.0f);
}

bool
Gecko_MediaFeatures_HasSystemMetric(nsIDocument* aDocument,
                                    nsAtom* aMetric,
                                    bool aIsAccessibleFromContent)
{
  if (aIsAccessibleFromContent &&
      nsContentUtils::ShouldResistFingerprinting(aDocument)) {
    return false;
  }

  nsMediaFeatures::InitSystemMetrics();
  return sSystemMetrics->IndexOf(aMetric) != sSystemMetrics->NoIndex;
}

nsAtom*
Gecko_MediaFeatures_GetOperatingSystemVersion(nsIDocument* aDocument)
{
  if (nsContentUtils::ShouldResistFingerprinting(aDocument)) {
    return nullptr;
  }

#ifdef XP_WIN
  int32_t metricResult;
  if (NS_SUCCEEDED(
        LookAndFeel::GetInt(LookAndFeel::eIntID_OperatingSystemVersionIdentifier,
                            &metricResult))) {
    for (const auto& osVersion : kOsVersionStrings) {
      if (metricResult == osVersion.mId) {
        return *osVersion.mName;
      }
    }
  }
#endif

  return nullptr;
}

bool
Gecko_MediaFeatures_PrefersReducedMotion(nsIDocument* aDocument)
{
  if (nsContentUtils::ShouldResistFingerprinting(aDocument)) {
    return false;
  }
  return LookAndFeel::GetInt(LookAndFeel::eIntID_PrefersReducedMotion, 0) == 1;
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
    sSystemMetrics->AppendElement(nsGkAtoms::_moz_scrollbar_start_backward);
  }
  if (metricResult & LookAndFeel::eScrollArrow_StartForward) {
    sSystemMetrics->AppendElement(nsGkAtoms::_moz_scrollbar_start_forward);
  }
  if (metricResult & LookAndFeel::eScrollArrow_EndBackward) {
    sSystemMetrics->AppendElement(nsGkAtoms::_moz_scrollbar_end_backward);
  }
  if (metricResult & LookAndFeel::eScrollArrow_EndForward) {
    sSystemMetrics->AppendElement(nsGkAtoms::_moz_scrollbar_end_forward);
  }

  metricResult =
    LookAndFeel::GetInt(LookAndFeel::eIntID_ScrollSliderStyle);
  if (metricResult != LookAndFeel::eScrollThumbStyle_Normal) {
    sSystemMetrics->AppendElement(nsGkAtoms::_moz_scrollbar_thumb_proportional);
  }

  metricResult =
    LookAndFeel::GetInt(LookAndFeel::eIntID_UseOverlayScrollbars);
  if (metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::_moz_overlay_scrollbars);
  }

  metricResult =
    LookAndFeel::GetInt(LookAndFeel::eIntID_MenuBarDrag);
  if (metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::_moz_menubar_drag);
  }

  nsresult rv =
    LookAndFeel::GetInt(LookAndFeel::eIntID_WindowsDefaultTheme, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::_moz_windows_default_theme);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_MacGraphiteTheme, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::_moz_mac_graphite_theme);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_MacYosemiteTheme, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::_moz_mac_yosemite_theme);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_WindowsAccentColorInTitlebar, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::_moz_windows_accent_color_in_titlebar);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_DWMCompositor, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::_moz_windows_compositor);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_WindowsGlass, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::_moz_windows_glass);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_WindowsClassic, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::_moz_windows_classic);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_TouchEnabled, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::_moz_touch_enabled);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_SwipeAnimationEnabled,
                           &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::_moz_swipe_animation_enabled);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_GTKCSDAvailable,
                           &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::_moz_gtk_csd_available);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_GTKCSDMinimizeButton,
                           &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::_moz_gtk_csd_minimize_button);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_GTKCSDMaximizeButton,
                           &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::_moz_gtk_csd_maximize_button);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::eIntID_GTKCSDCloseButton,
                           &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::_moz_gtk_csd_close_button);
  }

  metricResult =
    LookAndFeel::GetInt(LookAndFeel::eIntID_SystemUsesDarkTheme);
  if (metricResult) {
    sSystemMetrics->AppendElement(nsGkAtoms::_moz_system_dark_theme);
  }
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
