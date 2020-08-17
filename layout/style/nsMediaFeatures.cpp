/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* the features that media queries can test */

#include "nsMediaFeatures.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsCSSProps.h"
#include "nsCSSValue.h"
#include "mozilla/LookAndFeel.h"
#include "nsDeviceContext.h"
#include "nsIBaseWindow.h"
#include "nsIDocShell.h"
#include "nsIPrintSettings.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/DocumentInlines.h"
#include "nsIWidget.h"
#include "nsContentUtils.h"
#include "mozilla/RelativeLuminanceUtils.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/StyleSheetInlines.h"
#include "mozilla/GeckoBindings.h"
#include "PreferenceSheet.h"
#include "nsGlobalWindowOuter.h"

using namespace mozilla;
using mozilla::dom::Document;

static nsTArray<const nsStaticAtom*>* sSystemMetrics = nullptr;

// A helper for four features below
static nsSize GetSize(const Document* aDocument) {
  nsPresContext* pc = aDocument->GetPresContext();

  // Per spec, return a 0x0 viewport if we're not being rendered. See:
  //
  //  * https://github.com/w3c/csswg-drafts/issues/571
  //  * https://github.com/whatwg/html/issues/1813
  //
  if (!pc) {
    return {};
  }

  if (pc->IsRootPaginatedDocument()) {
    // We want the page size, including unprintable areas and margins.
    //
    // FIXME(emilio, bug 1414600): Not quite!
    return pc->GetPageSize();
  }

  return pc->GetVisibleArea().Size();
}

// A helper for three features below.
static nsSize GetDeviceSize(const Document* aDocument) {
  if (nsContentUtils::ShouldResistFingerprinting(aDocument)) {
    return GetSize(aDocument);
  }

  // Media queries in documents in an RDM pane should use the simulated
  // device size.
  Maybe<CSSIntSize> deviceSize =
      nsGlobalWindowOuter::GetRDMDeviceSize(*aDocument);
  if (deviceSize.isSome()) {
    return CSSPixel::ToAppUnits(deviceSize.value());
  }

  nsPresContext* pc = aDocument->GetPresContext();
  // NOTE(emilio): We should probably figure out how to return an appropriate
  // device size here, though in a multi-screen world that makes no sense
  // really.
  if (!pc) {
    return {};
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

bool Gecko_MediaFeatures_IsResourceDocument(const Document* aDocument) {
  return aDocument->IsResourceDoc();
}

static nsDeviceContext* GetDeviceContextFor(const Document* aDocument) {
  nsPresContext* pc = aDocument->GetPresContext();
  if (!pc) {
    return nullptr;
  }

  // It would be nice to call nsLayoutUtils::GetDeviceContextForScreenInfo here,
  // except for two things:  (1) it can flush, and flushing is bad here, and (2)
  // it doesn't really get us consistency in multi-monitor situations *anyway*.
  return pc->DeviceContext();
}

void Gecko_MediaFeatures_GetDeviceSize(const Document* aDocument,
                                       nscoord* aWidth, nscoord* aHeight) {
  nsSize size = GetDeviceSize(aDocument);
  *aWidth = size.width;
  *aHeight = size.height;
}

uint32_t Gecko_MediaFeatures_GetMonochromeBitsPerPixel(
    const Document* aDocument) {
  // The default bits per pixel for a monochrome device. We could propagate this
  // further to nsIPrintSettings, but Gecko doesn't actually know this value
  // from the hardware, so it seems silly to do so.
  static constexpr uint32_t kDefaultMonochromeBpp = 8;

  nsPresContext* pc = aDocument->GetPresContext();
  if (!pc) {
    return 0;
  }
  nsIPrintSettings* ps = pc->GetPrintSettings();
  if (!ps) {
    return 0;
  }
  bool color = true;
  ps->GetPrintInColor(&color);
  return color ? 0 : kDefaultMonochromeBpp;
}

uint32_t Gecko_MediaFeatures_GetColorDepth(const Document* aDocument) {
  if (Gecko_MediaFeatures_GetMonochromeBitsPerPixel(aDocument) != 0) {
    // If we're a monochrome device, then the color depth is zero.
    return 0;
  }

  // Use depth of 24 when resisting fingerprinting, or when we're not being
  // rendered.
  uint32_t depth = 24;

  if (!nsContentUtils::ShouldResistFingerprinting(aDocument)) {
    if (nsDeviceContext* dx = GetDeviceContextFor(aDocument)) {
      dx->GetDepth(depth);
    }
  }

  // The spec says to use bits *per color component*, so divide by 3,
  // and round down, since the spec says to use the smallest when the
  // color components differ.
  return depth / 3;
}

float Gecko_MediaFeatures_GetResolution(const Document* aDocument) {
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
  return float(AppUnitsPerCSSPixel()) /
         pc->DeviceContext()->AppUnitsPerDevPixel();
}

static const Document* TopDocument(const Document* aDocument) {
  const Document* current = aDocument;
  while (const Document* parent = current->GetInProcessParentDocument()) {
    current = parent;
  }
  return current;
}

StyleDisplayMode Gecko_MediaFeatures_GetDisplayMode(const Document* aDocument) {
  const Document* rootDocument = TopDocument(aDocument);

  nsCOMPtr<nsISupports> container = rootDocument->GetContainer();
  if (nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(container)) {
    nsCOMPtr<nsIWidget> mainWidget;
    baseWindow->GetMainWidget(getter_AddRefs(mainWidget));
    if (mainWidget && mainWidget->SizeMode() == nsSizeMode_Fullscreen) {
      return StyleDisplayMode::Fullscreen;
    }
  }

  static_assert(nsIDocShell::DISPLAY_MODE_BROWSER ==
                        static_cast<int32_t>(StyleDisplayMode::Browser) &&
                    nsIDocShell::DISPLAY_MODE_MINIMAL_UI ==
                        static_cast<int32_t>(StyleDisplayMode::MinimalUi) &&
                    nsIDocShell::DISPLAY_MODE_STANDALONE ==
                        static_cast<int32_t>(StyleDisplayMode::Standalone) &&
                    nsIDocShell::DISPLAY_MODE_FULLSCREEN ==
                        static_cast<int32_t>(StyleDisplayMode::Fullscreen),
                "nsIDocShell display modes must mach nsStyleConsts.h");

  nsIDocShell* docShell = rootDocument->GetDocShell();
  if (!docShell) {
    return StyleDisplayMode::Browser;
  }
  return static_cast<StyleDisplayMode>(docShell->GetDisplayMode());
}

bool Gecko_MediaFeatures_HasSystemMetric(const Document* aDocument,
                                         nsAtom* aMetric,
                                         bool aIsAccessibleFromContent) {
  if (aIsAccessibleFromContent &&
      nsContentUtils::ShouldResistFingerprinting(aDocument)) {
    return false;
  }

  nsMediaFeatures::InitSystemMetrics();
  return sSystemMetrics->IndexOf(aMetric) != sSystemMetrics->NoIndex;
}

nsAtom* Gecko_MediaFeatures_GetOperatingSystemVersion(
    const Document* aDocument) {
  using OperatingSystemVersion = LookAndFeel::OperatingSystemVersion;

  if (nsContentUtils::ShouldResistFingerprinting(aDocument)) {
    return nullptr;
  }

  int32_t metricResult;
  if (NS_FAILED(LookAndFeel::GetInt(
          LookAndFeel::IntID::OperatingSystemVersionIdentifier,
          &metricResult))) {
    return nullptr;
  }

  switch (OperatingSystemVersion(metricResult)) {
    case OperatingSystemVersion::Windows7:
      return nsGkAtoms::windows_win7;
    case OperatingSystemVersion::Windows8:
      return nsGkAtoms::windows_win8;
    case OperatingSystemVersion::Windows10:
      return nsGkAtoms::windows_win10;
    default:
      return nullptr;
  }
}

bool Gecko_MediaFeatures_PrefersReducedMotion(const Document* aDocument) {
  if (nsContentUtils::ShouldResistFingerprinting(aDocument)) {
    return false;
  }
  return LookAndFeel::GetInt(LookAndFeel::IntID::PrefersReducedMotion, 0) == 1;
}

StylePrefersColorScheme Gecko_MediaFeatures_PrefersColorScheme(
    const Document* aDocument) {
  return aDocument->PrefersColorScheme();
}

StyleContrastPref Gecko_MediaFeatures_PrefersContrast(
    const Document* aDocument, const bool aForcedColors) {
  if (nsContentUtils::ShouldResistFingerprinting(aDocument)) {
    return StyleContrastPref::NoPreference;
  }
  // Neither Linux, Windows, nor Mac have a way to indicate that low
  // contrast is prefered so the presence of an accessibility theme
  // implies that high contrast is prefered.
  //
  // Note that MacOS does not expose whether or not high contrast is
  // enabled so for MacOS users this will always evaluate to
  // false. For more information and discussion see:
  // https://github.com/w3c/csswg-drafts/issues/3856#issuecomment-642313572
  // https://github.com/w3c/csswg-drafts/issues/2943
  if (!!LookAndFeel::GetInt(LookAndFeel::IntID::UseAccessibilityTheme, 0)) {
    return StyleContrastPref::More;
  }
  return StyleContrastPref::NoPreference;
}

static PointerCapabilities GetPointerCapabilities(const Document* aDocument,
                                                  LookAndFeel::IntID aID) {
  MOZ_ASSERT(aID == LookAndFeel::IntID::PrimaryPointerCapabilities ||
             aID == LookAndFeel::IntID::AllPointerCapabilities);
  MOZ_ASSERT(aDocument);

  if (nsIDocShell* docShell = aDocument->GetDocShell()) {
    // The touch-events-override happens only for the Responsive Design Mode so
    // that we don't need to care about ResistFingerprinting.
    if (docShell->GetTouchEventsOverride() ==
        nsIDocShell::TOUCHEVENTS_OVERRIDE_ENABLED) {
      return PointerCapabilities::Coarse;
    }
  }

  // The default value for Desktop is mouse-type pointer, and for Android
  // a coarse pointer.
  const PointerCapabilities kDefaultCapabilities =
#ifdef ANDROID
      PointerCapabilities::Coarse;
#else
      PointerCapabilities::Fine | PointerCapabilities::Hover;
#endif

  if (nsContentUtils::ShouldResistFingerprinting(aDocument)) {
    return kDefaultCapabilities;
  }

  int32_t intValue;
  nsresult rv = LookAndFeel::GetInt(aID, &intValue);
  if (NS_FAILED(rv)) {
    return kDefaultCapabilities;
  }

  return static_cast<PointerCapabilities>(intValue);
}

PointerCapabilities Gecko_MediaFeatures_PrimaryPointerCapabilities(
    const Document* aDocument) {
  return GetPointerCapabilities(aDocument,
                                LookAndFeel::IntID::PrimaryPointerCapabilities);
}

PointerCapabilities Gecko_MediaFeatures_AllPointerCapabilities(
    const Document* aDocument) {
  return GetPointerCapabilities(aDocument,
                                LookAndFeel::IntID::AllPointerCapabilities);
}

/* static */
void nsMediaFeatures::InitSystemMetrics() {
  if (sSystemMetrics) return;

  MOZ_ASSERT(NS_IsMainThread());

  sSystemMetrics = new nsTArray<const nsStaticAtom*>;

  /***************************************************************************
   * ANY METRICS ADDED HERE SHOULD ALSO BE ADDED AS MEDIA QUERIES BELOW      *
   ***************************************************************************/

  int32_t metricResult =
      LookAndFeel::GetInt(LookAndFeel::IntID::ScrollArrowStyle);
  if (metricResult & LookAndFeel::eScrollArrow_StartBackward) {
    sSystemMetrics->AppendElement(
        (nsStaticAtom*)nsGkAtoms::_moz_scrollbar_start_backward);
  }
  if (metricResult & LookAndFeel::eScrollArrow_StartForward) {
    sSystemMetrics->AppendElement(
        (nsStaticAtom*)nsGkAtoms::_moz_scrollbar_start_forward);
  }
  if (metricResult & LookAndFeel::eScrollArrow_EndBackward) {
    sSystemMetrics->AppendElement(
        (nsStaticAtom*)nsGkAtoms::_moz_scrollbar_end_backward);
  }
  if (metricResult & LookAndFeel::eScrollArrow_EndForward) {
    sSystemMetrics->AppendElement(
        (nsStaticAtom*)nsGkAtoms::_moz_scrollbar_end_forward);
  }

  metricResult = LookAndFeel::GetInt(LookAndFeel::IntID::ScrollSliderStyle);
  if (metricResult != LookAndFeel::eScrollThumbStyle_Normal) {
    sSystemMetrics->AppendElement(
        (nsStaticAtom*)nsGkAtoms::_moz_scrollbar_thumb_proportional);
  }

  metricResult = LookAndFeel::GetInt(LookAndFeel::IntID::UseOverlayScrollbars);
  if (metricResult) {
    sSystemMetrics->AppendElement(
        (nsStaticAtom*)nsGkAtoms::_moz_overlay_scrollbars);
  }

  metricResult = LookAndFeel::GetInt(LookAndFeel::IntID::MenuBarDrag);
  if (metricResult) {
    sSystemMetrics->AppendElement((nsStaticAtom*)nsGkAtoms::_moz_menubar_drag);
  }

  nsresult rv = LookAndFeel::GetInt(LookAndFeel::IntID::WindowsDefaultTheme,
                                    &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(
        (nsStaticAtom*)nsGkAtoms::_moz_windows_default_theme);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::IntID::MacGraphiteTheme, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(
        (nsStaticAtom*)nsGkAtoms::_moz_mac_graphite_theme);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::IntID::MacYosemiteTheme, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(
        (nsStaticAtom*)nsGkAtoms::_moz_mac_yosemite_theme);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::IntID::WindowsAccentColorInTitlebar,
                           &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(
        (nsStaticAtom*)nsGkAtoms::_moz_windows_accent_color_in_titlebar);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::IntID::DWMCompositor, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(
        (nsStaticAtom*)nsGkAtoms::_moz_windows_compositor);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::IntID::WindowsGlass, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement((nsStaticAtom*)nsGkAtoms::_moz_windows_glass);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::IntID::WindowsClassic, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(
        (nsStaticAtom*)nsGkAtoms::_moz_windows_classic);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::IntID::TouchEnabled, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement((nsStaticAtom*)nsGkAtoms::_moz_touch_enabled);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::IntID::SwipeAnimationEnabled,
                           &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(
        (nsStaticAtom*)nsGkAtoms::_moz_swipe_animation_enabled);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::IntID::GTKCSDAvailable, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(
        (nsStaticAtom*)nsGkAtoms::_moz_gtk_csd_available);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::IntID::GTKCSDHideTitlebarByDefault,
                           &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(
        (nsStaticAtom*)nsGkAtoms::_moz_gtk_csd_hide_titlebar_by_default);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::IntID::GTKCSDTransparentBackground,
                           &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(
        (nsStaticAtom*)nsGkAtoms::_moz_gtk_csd_transparent_background);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::IntID::GTKCSDMinimizeButton,
                           &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(
        (nsStaticAtom*)nsGkAtoms::_moz_gtk_csd_minimize_button);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::IntID::GTKCSDMaximizeButton,
                           &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(
        (nsStaticAtom*)nsGkAtoms::_moz_gtk_csd_maximize_button);
  }

  rv =
      LookAndFeel::GetInt(LookAndFeel::IntID::GTKCSDCloseButton, &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(
        (nsStaticAtom*)nsGkAtoms::_moz_gtk_csd_close_button);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::IntID::GTKCSDReversedPlacement,
                           &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(
        (nsStaticAtom*)nsGkAtoms::_moz_gtk_csd_reversed_placement);
  }

  rv = LookAndFeel::GetInt(LookAndFeel::IntID::SystemUsesDarkTheme,
                           &metricResult);
  if (NS_SUCCEEDED(rv) && metricResult) {
    sSystemMetrics->AppendElement(
        (nsStaticAtom*)nsGkAtoms::_moz_system_dark_theme);
  }
}

/* static */
void nsMediaFeatures::FreeSystemMetrics() {
  delete sSystemMetrics;
  sSystemMetrics = nullptr;
}

/* static */
void nsMediaFeatures::Shutdown() { FreeSystemMetrics(); }
