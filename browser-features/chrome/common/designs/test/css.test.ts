// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { getCSSFromConfig } from "../utils/css.ts";
import { navBarBackgroundColorCSS } from "../utils/css.ts";
import {
  assert,
  assertEquals,
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
  const hasChromeStyles = (result.chromeStyles?.length ?? 0) > 0 ||
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

function testLeptonHasExpectedStyleEntries(): void {
  const result = getCSSFromConfig(makeConfig("lepton"));
  // Production: chromeStyles = [leptonChrome, leptonContent] (2).
  // Dev:        chromeStylesRaw = [leptonChrome, LEPTON_COMPAT, navBar] (3).
  // Both branches must carry Lepton's chrome + content sheets; the Gecko 152
  // compat layer and the nav-bar color override are layered on as raw CSS.
  const chromeCount = result.chromeStyles?.length ??
    result.chromeStylesRaw?.length ?? 0;
  assert(
    chromeCount >= 2,
    `lepton should expose at least Lepton chrome + content sheets, got ${chromeCount}`,
  );
  assert(
    (result.chromeStylesRaw?.length ?? 0) >= 1,
    "lepton should always carry inline raw chrome CSS (compat + navBar)",
  );
}

function getChromeInlineCss(
  ui: TFloorpDesignConfigs["globalConfigs"]["userInterface"],
): string {
  return getCSSFromConfig(makeConfig(ui)).chromeStylesRaw?.join("\n") ?? "";
}

function testLeptonPhotonProtonfixNavBarCssIncludesPersonalToolbar(): void {
  // Each theme must actually WIRE the shared nav-bar CSS into its
  // chromeStylesRaw (content assertions are then made against
  // `navBarBackgroundColorCSS` directly, to avoid false matches from the
  // vendored Lepton sheet which legitimately contains many selectors).
  for (const theme of ["lepton", "photon", "protonfix"] as const) {
    const css = getChromeInlineCss(theme);
    assert(
      css.includes("#PersonalToolbar"),
      `${theme} navBar CSS should style #PersonalToolbar`,
    );
    assert(
      css.includes("--tab-selected-bgcolor"),
      `${theme} navBar CSS should follow selected tab color in the no-theme case`,
    );
  }
}

/**
 * Regression for Issue #2489: under a third-party LWT the nav-bar /
 * PersonalToolbar must not follow the (often lighter) selected-tab color —
 * otherwise the bar "floats" off the toolbar. The navBar CSS must therefore
 * branch on the LWT signal.
 *
 * The signal is the `[lwtheme]` ATTRIBUTE only. Gecko 152 silently drops any
 * selector that places the `:-moz-lwtheme` pseudo-class inside `:not()` —
 * which previously invalidated the entire no-theme rule and left the default
 * and built-in (compact-light / compact-dark) themes unstyled, because those
 * built-in themes do not set `[lwtheme]` at all. Only third-party LWTs
 * (community colorways, add-on themes) set `[lwtheme]`.
 */
function testNavBarCssIsLwtAware(): void {
  // The nav-bar CSS is shared across the Lepton family. Assert it is wired into
  // every theme's chromeStylesRaw first...
  for (const theme of ["lepton", "photon", "protonfix"] as const) {
    assert(
      getChromeInlineCss(theme).includes("nav-bar") === false ||
        getChromeInlineCss(theme).includes("--floorp-chrome-surface-color"),
      `${theme} should wire the shared nav-bar surface CSS`,
    );
  }

  // ...then make every CONTENT assertion against `navBarBackgroundColorCSS`
  // directly. The vendored leptonChrome.css legitimately uses
  // `:not(:-moz-lwtheme)` (~150 occurrences) and `:root[lwtheme]` selectors of
  // its own, so testing the merged `chromeStylesRaw` string would produce false
  // positives/negatives. The regression we care about lives ONLY in
  // navBarBackgroundColorCSS.
  const css = navBarBackgroundColorCSS;

  // The no-theme branch must use the [lwtheme] attribute, and must NOT place
  // :-moz-lwtheme inside :not() (Gecko 152 drops such rules entirely).
  assert(
    /:root:not\(\[lwtheme\]\)\s+#nav-bar/.test(css),
    `no-theme navBar CSS must use :root:not([lwtheme]) #nav-bar`,
  );
  assert(
    /:root:not\(\[lwtheme\]\)\s+#PersonalToolbar/.test(css),
    `no-theme navBar CSS must use :root:not([lwtheme]) #PersonalToolbar`,
  );
  assert(
    !/:not\(:-moz-lwtheme\)/.test(css),
    `navBar CSS must not use :not(:-moz-lwtheme) — Gecko 152 drops the whole rule`,
  );

  // The LWT branch must expose the toolbar surface token via the [lwtheme]
  // attribute without forcing an opaque fill on the toolbar elements. LWT
  // background artwork is drawn on <body>, so #nav-bar / #PersonalToolbar must
  // keep Firefox's native translucent toolbar painting.
  assert(
    /:root\[lwtheme\]\s*\{/.test(css),
    `navBar CSS must expose a root-scoped toolbar surface token under LWT`,
  );
  assert(
    !/:root\[lwtheme\]\s+#(?:nav-bar|PersonalToolbar)/.test(css),
    `LWT navBar CSS must not force an opaque fill on #nav-bar or #PersonalToolbar`,
  );

  // The LWT branch must read the toolbar color, NOT --tab-selected-bgcolor.
  // The LWT rule is root-scoped:
  //   `:root[lwtheme] { --floorp-chrome-surface-color: ... }`.
  const lwtBlockMatch = css.match(
    /:root\[lwtheme\]\s*\{[\s\S]*?\}/,
  );
  const lwtBlock = lwtBlockMatch ? lwtBlockMatch[0] : "";
  assert(
    lwtBlock !== "",
    `navBar CSS must have a root-scoped [lwtheme] token block`,
  );
  assert(
    lwtBlock.includes("--floorp-chrome-surface-color"),
    `LWT navBar branch must expose --floorp-chrome-surface-color for downstream consumers`,
  );
  assert(
    lwtBlock.includes("--toolbar-bgcolor"),
    `LWT navBar branch must read --toolbar-bgcolor (the token the LWT toolbar / selected tab track)`,
  );
  assert(
    lwtBlock.includes("--toolbar-background-color"),
    `LWT navBar branch must read --toolbar-background-color as fallback`,
  );
  assert(
    !lwtBlock.includes("--tab-selected-bgcolor"),
    `LWT navBar branch must NOT read --tab-selected-bgcolor (that is what floats the bar off the toolbar under LWT)`,
  );

  // CRITICAL ordering check: on Gecko 152 the legacy --toolbar-bgcolor and the
  // new --toolbar-background-color DIVERGE (e.g. default-theme dark:
  // --toolbar-bgcolor = #171717, --toolbar-background-color = rgb(43,42,51)).
  // Firefox 152 still paints the selected .tab-background from --toolbar-bgcolor,
  // and Lepton's color_like_toolbar unsets --tab-selected-bgcolor so the tab
  // resolves to --toolbar-bgcolor. The nav-bar must therefore prefer
  // --toolbar-bgcolor OVER --toolbar-background-color, or the bar will not
  // match the selected tab. The CSS may wrap the nested var() across lines, so
  // assert via a whitespace-tolerant regex that --toolbar-bgcolor is the outer
  // (preferred) token and --toolbar-background-color the inner (fallback).
  assert(
    /var\(\s*--toolbar-bgcolor\s*,\s*var\(\s*--toolbar-background-color\s*\)\s*\)/
      .test(lwtBlock),
    `LWT navBar branch must prefer --toolbar-bgcolor over --toolbar-background-color`,
  );

  // The no-theme branch SHOULD read --tab-selected-bgcolor (Lepton aligns the
  // tab to the toolbar in this case, so the tab color IS the toolbar color).
  const noThemeBlockMatch = css.match(
    /:root:not\(\[lwtheme\]\)\s+#nav-bar,[\s\S]*?\}/,
  );
  const noThemeBlock = noThemeBlockMatch ? noThemeBlockMatch[0] : "";
  assert(
    noThemeBlock !== "",
    `navBar CSS must have a :not([lwtheme]) selector group for #nav-bar`,
  );
  assert(
    noThemeBlock.includes("--tab-selected-bgcolor"),
    `no-theme navBar branch must read --tab-selected-bgcolor (Lepton makes the tab track the toolbar)`,
  );
  // Same divergence-avoidance ordering as the LWT branch: --toolbar-bgcolor
  // must come BEFORE --toolbar-background-color so the bar matches the
  // selected tab under system-theme = auto / dark mode. Whitespace-tolerant
  // regex because the CSS wraps the nested var() across lines.
  assert(
    /var\(\s*--toolbar-bgcolor\s*,\s*var\(\s*--toolbar-background-color\s*\)\s*\)/
      .test(noThemeBlock),
    `no-theme navBar branch must prefer --toolbar-bgcolor over --toolbar-background-color so the bar matches the selected tab on Gecko 152`,
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
  const hasContentStyles = (result.styles?.length ?? 0) > 0 ||
    (result.stylesRaw?.length ?? 0) > 0;
  assert(
    hasContentStyles,
    "photon should have content styles (styles or stylesRaw)",
  );
}

function testPhotonHasChromeStyles(): void {
  const result = getCSSFromConfig(makeConfig("photon"));
  const hasChromeStyles = (result.chromeStyles?.length ?? 0) > 0 ||
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

function testProtonfixNoUseTabColorAsToolbarColor(): void {
  const result = getCSSFromConfig(makeConfig("protonfix"));
  assertEquals(
    result.useTabColorAsToolbarColor,
    undefined,
    "protonfix should not set useTabColorAsToolbarColor (conflicts with userjs color_like_toolbar=false)",
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
    {
      name: "lepton has expected style entries",
      fn: testLeptonHasExpectedStyleEntries,
    },
    {
      name: "lepton/photon/protonfix navBar CSS includes PersonalToolbar",
      fn: testLeptonPhotonProtonfixNavBarCssIncludesPersonalToolbar,
    },
    {
      name: "lepton/photon/protonfix navBar CSS is LWT-aware",
      fn: testNavBarCssIsLwtAware,
    },
    // photon
    { name: "photon returns userjs", fn: testPhotonReturnsUserjs },
    { name: "photon has content styles", fn: testPhotonHasContentStyles },
    { name: "photon has chrome styles", fn: testPhotonHasChromeStyles },
    // protonfix
    { name: "protonfix returns userjs", fn: testProtonfixReturnsUserjs },
    {
      name: "protonfix no useTabColorAsToolbarColor",
      fn: testProtonfixNoUseTabColorAsToolbarColor,
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
    // Additional tests
    {
      name: "fluerial has iconBasePath in dev",
      fn: testFluerialHasIconBasePathInDev,
    },
    {
      name: "lepton has iconBasePath in dev",
      fn: testLeptonHasIconBasePathInDev,
    },
    {
      name: "photon has both chrome and content styles",
      fn: testPhotonHasBothStyleTypes,
    },
    {
      name: "protonfix has both chrome and content styles",
      fn: testProtonfixHasBothStyleTypes,
    },
    {
      name: "userjs content is non-empty for themes that have it",
      fn: testUserjsContentNonEmpty,
    },
    {
      name: "production themes can combine chromeStyles and chromeStylesRaw",
      fn: testProductionThemesCanCombineChromeStylesAndRaw,
    },
  ]);
}

// ---------------------------------------------------------------------------
// Additional Tests — FCSS edge cases and invariants
// ---------------------------------------------------------------------------

function testFluerialHasIconBasePathInDev(): void {
  const result = getCSSFromConfig(makeConfig("fluerial"));
  // In dev mode, iconBasePath should be present
  // In production, it's undefined (chrome:// URLs don't need it)
  const hasIconPath = result.iconBasePath !== undefined;
  assert(
    hasIconPath || result.chromeStyles !== undefined,
    "fluerial should have iconBasePath in dev or chromeStyles in prod",
  );
}

function testLeptonHasIconBasePathInDev(): void {
  const result = getCSSFromConfig(makeConfig("lepton"));
  const hasIconPath = result.iconBasePath !== undefined;
  assert(
    hasIconPath || result.chromeStyles !== undefined,
    "lepton should have iconBasePath in dev or chromeStyles in prod",
  );
}

function testPhotonHasBothStyleTypes(): void {
  const result = getCSSFromConfig(makeConfig("photon"));
  // Photon has both chrome styles (chromeStyles/chromeStylesRaw)
  // and content styles (styles/stylesRaw)
  const hasChromeStyles = (result.chromeStyles?.length ?? 0) > 0 ||
    (result.chromeStylesRaw?.length ?? 0) > 0;
  const hasContentStyles = (result.styles?.length ?? 0) > 0 ||
    (result.stylesRaw?.length ?? 0) > 0;

  assertEquals(
    hasChromeStyles && hasContentStyles,
    true,
    "photon should have both chrome and content styles",
  );
}

function testProtonfixHasBothStyleTypes(): void {
  const result = getCSSFromConfig(makeConfig("protonfix"));
  // Protonfix also has both chrome and content styles
  const hasChromeStyles = (result.chromeStyles?.length ?? 0) > 0 ||
    (result.chromeStylesRaw?.length ?? 0) > 0;
  const hasContentStyles = (result.styles?.length ?? 0) > 0 ||
    (result.stylesRaw?.length ?? 0) > 0;

  assertEquals(
    hasChromeStyles && hasContentStyles,
    true,
    "protonfix should have both chrome and content styles",
  );
}

function testUserjsContentNonEmpty(): void {
  const themesWithUserjs: Array<
    TFloorpDesignConfigs["globalConfigs"]["userInterface"]
  > = ["lepton", "photon", "protonfix"];

  for (const theme of themesWithUserjs) {
    const result = getCSSFromConfig(makeConfig(theme));
    assertEquals(
      result.userjs !== null && result.userjs!.length > 0,
      true,
      `${theme} should have non-empty userjs`,
    );
  }
}

function testProductionThemesCanCombineChromeStylesAndRaw(): void {
  if (import.meta.env.DEV) {
    return;
  }

  const themes: Array<
    TFloorpDesignConfigs["globalConfigs"]["userInterface"]
  > = ["fluerial", "lepton", "photon", "protonfix"];

  for (const theme of themes) {
    const result = getCSSFromConfig(makeConfig(theme));
    const hasChromeStyles = (result.chromeStyles?.length ?? 0) > 0;
    const hasChromeStylesRaw = (result.chromeStylesRaw?.length ?? 0) > 0;

    assert(
      hasChromeStyles || hasChromeStylesRaw,
      `${theme} should expose chromeStyles and/or chromeStylesRaw in production`,
    );
  }
}
