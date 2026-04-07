// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { getCSSFromConfig } from "../utils/css.ts";
import {
  assertEquals,
  assert,
  runTests,
} from "../../../test/utils/test_harness.ts";
import type { TFloorpDesignConfigs } from "../type.ts";

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/** Build a minimal valid config overriding only globalConfigs.userInterface */
function makeConfig(
  ui: TFloorpDesignConfigs["globalConfigs"]["userInterface"],
): TFloorpDesignConfigs {
  return {
    globalConfigs: {
      faviconColor: false,
      userInterface: ui,
      appliedUserJs: "",
    },
    tabbar: {
      tabbarStyle: "horizontal",
      tabbarPosition: "default",
      multiRowTabBar: { maxRowEnabled: false, maxRow: 3 },
    },
    tab: {
      tabScroll: { enabled: false, reverse: false, wrap: false },
      tabMinHeight: 30,
      tabMinWidth: 76,
      tabPinTitle: false,
      tabDubleClickToClose: false,
      tabOpenPosition: -1,
    },
    uiCustomization: {
      navbar: { position: "top", searchBarTop: false },
      display: {
        disableFullscreenNotification: false,
        deleteBrowserBorder: false,
      },
      special: {
        optimizeForTreeStyleTab: false,
        hideForwardBackwardButton: false,
        stgLikeWorkspaces: false,
      },
      multirowTab: { newtabInsideEnabled: false },
      bookmarkBar: { focusExpand: false, position: "top" },
      qrCode: { disableButton: false },
    },
  };
}

// ---------------------------------------------------------------------------
// Tests — fluerial theme
// ---------------------------------------------------------------------------

function testFluerialReturnsUserjsNull(): void {
  const result = getCSSFromConfig(makeConfig("fluerial"));
  assertEquals(result.userjs, null, "fluerial should have null userjs");
}

function testFluerialHasUseTabColorAsToolbarColor(): void {
  const result = getCSSFromConfig(makeConfig("fluerial"));
  assertEquals(
    result.useTabColorAsToolbarColor,
    true,
    "fluerial should set useTabColorAsToolbarColor to true",
  );
}

function testFluerialHasStylesOrRaw(): void {
  const result = getCSSFromConfig(makeConfig("fluerial"));
  const hasChromeStyles =
    (result.chromeStyles?.length ?? 0) > 0 ||
    (result.chromeStylesRaw?.length ?? 0) > 0;
  assert(
    hasChromeStyles,
    "fluerial should have chromeStyles or chromeStylesRaw",
  );
}

// ---------------------------------------------------------------------------
// Tests — lepton theme
// ---------------------------------------------------------------------------

function testLeptonReturnsUserjs(): void {
  const result = getCSSFromConfig(makeConfig("lepton"));
  assert(result.userjs !== null, "lepton should have non-null userjs");
  assert(
    result.userjs!.length > 0,
    "lepton userjs should be a non-empty string",
  );
}

function testLeptonNoUseTabColorAsToolbarColor(): void {
  const result = getCSSFromConfig(makeConfig("lepton"));
  assertEquals(
    result.useTabColorAsToolbarColor,
    undefined,
    "lepton should not set useTabColorAsToolbarColor",
  );
}

function testLeptonHasTwoStyleEntries(): void {
  const result = getCSSFromConfig(makeConfig("lepton"));
  const count =
    (result.chromeStyles?.length ?? 0) || (result.chromeStylesRaw?.length ?? 0);
  assertEquals(
    count,
    2,
    "lepton should have 2 style entries (chrome + content)",
  );
}

// ---------------------------------------------------------------------------
// Tests — photon theme
// ---------------------------------------------------------------------------

function testPhotonReturnsUserjs(): void {
  const result = getCSSFromConfig(makeConfig("photon"));
  assert(result.userjs !== null, "photon should have non-null userjs");
}

function testPhotonHasContentStyles(): void {
  const result = getCSSFromConfig(makeConfig("photon"));
  const hasContentStyles =
    (result.styles?.length ?? 0) > 0 || (result.stylesRaw?.length ?? 0) > 0;
  assert(
    hasContentStyles,
    "photon should have content styles (styles or stylesRaw)",
  );
}

function testPhotonHasChromeStyles(): void {
  const result = getCSSFromConfig(makeConfig("photon"));
  const hasChromeStyles =
    (result.chromeStyles?.length ?? 0) > 0 ||
    (result.chromeStylesRaw?.length ?? 0) > 0;
  assert(
    hasChromeStyles,
    "photon should have chrome styles (chromeStyles or chromeStylesRaw)",
  );
}

// ---------------------------------------------------------------------------
// Tests — protonfix theme
// ---------------------------------------------------------------------------

function testProtonfixReturnsUserjs(): void {
  const result = getCSSFromConfig(makeConfig("protonfix"));
  assert(result.userjs !== null, "protonfix should have non-null userjs");
}

function testProtonfixHasUseTabColorAsToolbarColor(): void {
  const result = getCSSFromConfig(makeConfig("protonfix"));
  assertEquals(
    result.useTabColorAsToolbarColor,
    true,
    "protonfix should set useTabColorAsToolbarColor to true",
  );
}

// ---------------------------------------------------------------------------
// Tests — proton theme (default Firefox)
// ---------------------------------------------------------------------------

function testProtonReturnsNullUserjs(): void {
  const result = getCSSFromConfig(makeConfig("proton"));
  assertEquals(result.userjs, null, "proton should have null userjs");
}

function testProtonNoStyles(): void {
  const result = getCSSFromConfig(makeConfig("proton"));
  assertEquals(result.styles, undefined, "proton should have no styles");
  assertEquals(
    result.chromeStyles,
    undefined,
    "proton should have no chromeStyles",
  );
}

// ---------------------------------------------------------------------------
// Tests — structural invariants
// ---------------------------------------------------------------------------

function testAllThemesReturnNonNullResult(): void {
  const themes: TFloorpDesignConfigs["globalConfigs"]["userInterface"][] = [
    "fluerial",
    "lepton",
    "photon",
    "protonfix",
    "proton",
  ];
  for (const theme of themes) {
    const result = getCSSFromConfig(makeConfig(theme));
    assert(
      result !== null && result !== undefined,
      `${theme} returned null/undefined`,
    );
    assert(
      typeof result.userjs === "string" || result.userjs === null,
      `${theme} userjs should be string or null`,
    );
  }
}

function testProductionPathsUseChromeProtocol(): void {
  // In production mode, styles should use chrome:// URLs
  // We verify the structural pattern: if chromeStyles exist, they should be chrome:// URLs
  const result = getCSSFromConfig(makeConfig("lepton"));
  if (result.chromeStyles) {
    for (const url of result.chromeStyles) {
      assert(
        url.startsWith("chrome://noraneko-skin/content/"),
        `production chrome style should use chrome:// URL: ${url}`,
      );
    }
  }
  if (result.styles) {
    for (const url of result.styles) {
      assert(
        url.startsWith("chrome://noraneko-skin/content/"),
        `production content style should use chrome:// URL: ${url}`,
      );
    }
  }
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  await runTests("css.test.ts", [
    // fluerial
    { name: "fluerial returns null userjs", fn: testFluerialReturnsUserjsNull },
    {
      name: "fluerial has useTabColorAsToolbarColor",
      fn: testFluerialHasUseTabColorAsToolbarColor,
    },
    { name: "fluerial has styles", fn: testFluerialHasStylesOrRaw },
    // lepton
    { name: "lepton returns userjs", fn: testLeptonReturnsUserjs },
    {
      name: "lepton no useTabColorAsToolbarColor",
      fn: testLeptonNoUseTabColorAsToolbarColor,
    },
    { name: "lepton has 2 style entries", fn: testLeptonHasTwoStyleEntries },
    // photon
    { name: "photon returns userjs", fn: testPhotonReturnsUserjs },
    { name: "photon has content styles", fn: testPhotonHasContentStyles },
    { name: "photon has chrome styles", fn: testPhotonHasChromeStyles },
    // protonfix
    { name: "protonfix returns userjs", fn: testProtonfixReturnsUserjs },
    {
      name: "protonfix has useTabColorAsToolbarColor",
      fn: testProtonfixHasUseTabColorAsToolbarColor,
    },
    // proton
    { name: "proton returns null userjs", fn: testProtonReturnsNullUserjs },
    { name: "proton no styles", fn: testProtonNoStyles },
    // structural
    {
      name: "all themes return non-null result",
      fn: testAllThemesReturnNonNullResult,
    },
    {
      name: "production paths use chrome:// protocol",
      fn: testProductionPathsUseChromeProtocol,
    },
  ]);
}
