// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
//
// Regression tests for the Gecko 152 (Project Nova) compatibility layers.
// See `utils/gecko-152-var-aliases.css.ts` and
// `utils/lepton-compat-152.css.ts`, and Floorp Issue #2489.
//
// Background: Gecko 152 renamed a large set of chrome CSS custom properties
// (toolbar/toolbox/tab/toolbarbutton/panel tokens) and folded the
// `arrowpanel-*` family into the `panel-*` family. It also made the
// built-in-theme signals that user style sheets relied on unreliable. The
// result, without these layers, was the chrome collapsing to a single color,
// dialogs going black, and tabs not painting — especially under third-party
// LWTs.
//
// These tests pin the layers down so a future upstream Lepton sync or a
// Gecko version bump cannot silently drop the fix. They verify the alias
// *chains as real CSS fragments* (not just substring presence) and assert the
// non-destructive LWT contract that was the whole point of the rewrite.

import {
  GECKO_152_COLOR_FIX_CSS,
  LEPTON_COMPAT_152_CSS,
  LEPTON_COMPAT_CSS,
  FLOORP_ICON_PATCHES,
} from "../utils/lepton-compat-152.css.ts";
import {
  GECKO_152_RENAMED_VARS,
  GECKO_152_SYNTHESIZED_VARS,
  GECKO_152_VAR_ALIASES_CSS,
} from "../utils/gecko-152-var-aliases.css.ts";
import { getCSSFromConfig } from "../utils/css.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";
import type { TFloorpDesignConfigs } from "../type.ts";

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

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

/** Join every inline (raw) chrome stylesheet for a theme into one string. */
function getInlineChromeCss(
  ui: TFloorpDesignConfigs["globalConfigs"]["userInterface"],
): string {
  const r = getCSSFromConfig(makeConfig(ui));
  return r.chromeStylesRaw?.join("\n") ?? "";
}

/**
 * Extract the value of a `--name: value;` declaration from a CSS string.
 * Returns the raw value (trimmed) or "" when not found. Handles values that
 * span a single line; the alias layer is single-line-per-property so that is
 * sufficient.
 */
function extractVarDecl(css: string, name: string): string {
  const re = new RegExp(
    "--" + name.replace(/^--/, "") + "\\s*:\\s*([^;]+);",
  );
  const m = css.match(re);
  return m ? m[1].trim() : "";
}

/** Extract the selector of the compat rule that restores the right-sidebar
 * direction. Keeping this derived from the emitted CSS makes the selector
 * semantics below a regression test for the actual production stylesheet. */
function extractSidebarPositionEndSelector(): string {
  const match = LEPTON_COMPAT_152_CSS.match(
    /([^{}]+)\{\s*direction:\s*rtl\s*;\s*\}/,
  );
  const selector = match?.[1]?.trim() ?? "";
  assert(
    selector.includes("#sidebar-box"),
    "compat layer should provide a direction rule for #sidebar-box",
  );
  return selector;
}

function makeSidebarBox(
  positionEndValue: string | null,
): HTMLDivElement {
  const sidebarBox = document.createElement("div");
  sidebarBox.id = "sidebar-box";
  if (positionEndValue !== null) {
    sidebarBox.setAttribute("sidebar-positionend", positionEndValue);
  }
  return sidebarBox;
}

// ---------------------------------------------------------------------------
// Tests — the alias table matches the 151 -> 152 rename evidence
// ---------------------------------------------------------------------------

/**
 * The renamed-variable table must contain exactly the renames proven by the
 * Floorp-Runtime PR #45 diff. A typo or a phantom entry (the previous
 * revision listed `--toolbar-color` as a rename of `--toolbar-text-color`,
 * which is false) silently breaks components or synthesizes nonsense. Pin
 * the canonical mapping here.
 */
function testRenamedVarsTableIsCanonical(): void {
  const expected: Array<[string, string]> = [
    ["--toolbar-bgcolor", "--toolbar-background-color"],
    ["--toolbox-bgcolor", "--toolbox-background-color"],
    ["--toolbox-textcolor", "--toolbox-text-color"],
    ["--toolbox-bgcolor-inactive", "--toolbox-background-color-inactive"],
    ["--toolbox-textcolor-inactive", "--toolbox-text-color-inactive"],
    ["--tab-selected-bgcolor", "--tab-background-color-selected"],
    ["--tab-hover-background-color", "--tab-background-color-hover"],
    [
      "--toolbarbutton-hover-background",
      "--toolbarbutton-background-color-hover",
    ],
    [
      "--toolbarbutton-active-background",
      "--toolbarbutton-background-color-active",
    ],
    ["--arrowpanel-background", "--panel-background-color"],
    ["--arrowpanel-color", "--panel-text-color"],
    ["--arrowpanel-border-color", "--panel-border-color"],
    ["--panel-background", "--panel-background-color"],
  ];
  // NOTE: the colocated test harness's assertEquals uses reference equality
  // (===), which can never succeed for two independently-built arrays. Compare
  // the canonical (sorted) serializations instead.
  const sortKey = (a: readonly [string, string], b: readonly [string, string]) =>
    a[0].localeCompare(b[0]);
  const actualJson = JSON.stringify(
    [...GECKO_152_RENAMED_VARS].sort(sortKey),
  );
  const expectedJson = JSON.stringify(
    expected.sort(sortKey),
  );
  assert(
    actualJson === expectedJson,
    "GECKO_152_RENAMED_VARS must match the 151->152 evidence table exactly " +
      `(expected: ${expectedJson}, actual: ${actualJson})`,
  );
}

/** `--toolbar-color` survived 152 unchanged; it must NOT appear as a rename
 *  source (the previous revision's phantom entry). */
function testToolbarColorIsNotARenameSource(): void {
  const asLegacy = GECKO_152_RENAMED_VARS.some(([legacy]) =>
    legacy === "--toolbar-color"
  );
  assert(
    !asLegacy,
    "--toolbar-color is NOT renamed in Gecko 152 (still defined in " +
      "global-shared.css) and must not be an alias source",
  );
}

/**
 * `--toolbar-text-color` is referenced on 152 (tab.tokens.css:79) but defined
 * nowhere by Mozilla. It must be synthesized by Floorp, chained to the
 * LWT-provided text color with the surviving `--toolbar-color` as fallback.
 */
function testToolbarTextColorIsSynthesized(): void {
  const entry = GECKO_152_SYNTHESIZED_VARS.find(([n]) =>
    n === "--toolbar-text-color"
  );
  assert(entry !== undefined, "--toolbar-text-color must be synthesized");
  // The chain must prefer the LWT-provided --lwt-text-color, then fall back
  // to the surviving --toolbar-color.
  const chain = entry![1];
  assert(
    chain.includes("--lwt-text-color") && chain.includes("--toolbar-color"),
    "--toolbar-text-color chain must prefer --lwt-text-color and fall back to " +
      "--toolbar-color, got: " + chain,
  );
}

// ---------------------------------------------------------------------------
// Tests — every alias is emitted as a correct, non-cyclic CSS declaration
// ---------------------------------------------------------------------------

/** Each renamed token must be emitted as `legacy: var(new);` with no
 *  self-reference and no !important. */
function testRenamedAliasesAreEmittedCorrectly(): void {
  for (const [legacy, renamed] of GECKO_152_RENAMED_VARS) {
    const value = extractVarDecl(GECKO_152_VAR_ALIASES_CSS, legacy);
    assert(
      value !== "",
      `${legacy} must be declared in GECKO_152_VAR_ALIASES_CSS`,
    );
    assert(
      value === `var(${renamed})`,
      `${legacy} must alias exactly to var(${renamed}), got: ${value}`,
    );
    assert(
      !value.includes("!important"),
      `${legacy} alias must NOT use !important (theme/LWT values must win)`,
    );
    // No cyclic self-reference: the alias must not invoke `var(legacy)`.
    // Use a name-boundary match so `--panel-background` does not falsely
    // match against `--panel-background-color`.
    const cyclicRe = new RegExp(
      "var\\(\\s*" + legacy.replace(/[-/\\^$*+?.()|[\]{}]/g, "\\$&") +
        "(?![-a-z0-9])",
    );
    assert(
      !cyclicRe.test(value),
      `${legacy} alias must not reference itself via var(${legacy}) (cyclic), ` +
        `got: ${value}`,
    );
  }
}

/** Each synthesized token must be emitted verbatim from the table. */
function testSynthesizedAliasesAreEmittedCorrectly(): void {
  for (const [name, chain] of GECKO_152_SYNTHESIZED_VARS) {
    const value = extractVarDecl(GECKO_152_VAR_ALIASES_CSS, name);
    assertEquals(
      value,
      chain,
      `${name} must be emitted as the exact chain from the table`,
    );
  }
}

// ---------------------------------------------------------------------------
// Tests — compat layer is injected for the right themes
// ---------------------------------------------------------------------------

/** The three Lepton-backed designs must include the compat CSS inline. */
function testLeptonThemesIncludeCompatCss(): void {
  for (const theme of ["lepton", "photon", "protonfix"] as const) {
    const css = getInlineChromeCss(theme);
    assert(
      css.includes("Floorp Lepton compat"),
      `${theme} should include the Lepton Gecko 152 compat stylesheet`,
    );
  }
}

/**
 * The shared color-fix layer must reach EVERY chrome-rendering theme now,
 * including fluerial (it was previously Lepton-only, leaving fluerial users
 * exposed to the black-dialog / transparent-panel symptoms).
 */
function testAllSkinnedThemesIncludeColorFix(): void {
  for (const theme of ["fluerial", "lepton", "photon", "protonfix"] as const) {
    const css = getInlineChromeCss(theme);
    assert(
      css.includes("Floorp Gecko 152 color fix"),
      `${theme} should include the shared Gecko 152 color-fix layer`,
    );
  }
}

/** The built-in `proton` design ships no Floorp skin CSS, so neither layer
 *  should be present for it. */
function testProtonExcludesBothLayers(): void {
  const css = getInlineChromeCss("proton");
  assert(
    !css.includes("Floorp Lepton compat"),
    "proton must NOT include the Lepton compat stylesheet",
  );
  assert(
    !css.includes("Floorp Gecko 152 color fix"),
    "proton must NOT include the color-fix layer (it has no skin CSS at all)",
  );
}

// ---------------------------------------------------------------------------
// Tests — the compat layer uses robust, Gecko-152-safe detection
// ---------------------------------------------------------------------------

function testCompatAvoidsBrittleSelectors(): void {
  assert(
    !LEPTON_COMPAT_152_CSS.includes("[lwtheme-mozlightdark]"),
    "compat layer must not rely on the [lwtheme-mozlightdark] attribute " +
      "(unreliable on Gecko 152)",
  );
  assert(
    !LEPTON_COMPAT_152_CSS.includes("[builtintheme]"),
    "compat layer must not rely on the [builtintheme] attribute " +
      "(unreliable on Gecko 152)",
  );
}

function testCompatUsesRobustLwtSignals(): void {
  assert(
    LEPTON_COMPAT_152_CSS.includes(":-moz-lwtheme"),
    "compat layer should target LWT state via the stable :-moz-lwtheme " +
      "pseudo-class",
  );
  assert(
    LEPTON_COMPAT_152_CSS.includes("prefers-color-scheme"),
    "compat layer should branch light/dark via prefers-color-scheme rather " +
      "than brittle attributes",
  );
}

// ---------------------------------------------------------------------------
// Tests — the non-destructive LWT contract (the core of the rewrite)
// ---------------------------------------------------------------------------

/**
 * Regression for the bug that broke third-party LWTs: the previous revision
 * force-set `--lwt-accent-color`, `--toolbar-bgcolor` and
 * `--arrowpanel-background` with `!important` under BROAD selectors, which
 * clobbered a loaded LWT's own palette. Verify that no color-setting rule
 * in either layer uses `!important` — values must be guarded aliases or
 * scoped to the no-theme case, so a theme that provides its own value wins.
 */
function testNoColorOverrideUsesImportant(): void {
  for (const [label, css] of [
    ["LEPTON_COMPAT_152_CSS", LEPTON_COMPAT_152_CSS],
    ["GECKO_152_COLOR_FIX_CSS", GECKO_152_COLOR_FIX_CSS],
  ] as const) {
    // Find every custom-property declaration and assert none carries
    // !important. (Declarations like `stroke: transparent !important` in the
    // icon patches are fine and not in these two strings.)
    const declRe = /(--[a-z-]+\s*:\s*[^;]*!important\s*;)/gi;
    const offenders = css.match(declRe);
    assertEquals(
      offenders,
      null,
      `${label} must not set any custom property with !important ` +
        "(that is what clobbers loaded LWT palettes); found: " +
        (offenders?.join(" | ") ?? ""),
    );
  }
}

/**
 * Every rule that sets a theme/LWT-owned token (`--lwt-accent-color`,
 * `--toolbar-bgcolor`, `--arrowpanel-*`, `--in-content-page-background`) must
 * be scoped to the no-theme case (`:root:not([lwtheme]):not(:-moz-lwtheme)`)
 * — OR be a guarded `var(name, fallback)` alias. This is what lets a
 * third-party LWT keep its own palette. */
function testLwtOwnedTokensAreGuardedOrNoThemeScoped(): void {
  const lwtOwned = [
    "--lwt-accent-color",
    "--toolbar-bgcolor",
    "--arrowpanel-background",
    "--arrowpanel-color",
    "--arrowpanel-border-color",
    "--in-content-page-background",
  ];
  const combined = LEPTON_COMPAT_152_CSS + "\n" + GECKO_152_COLOR_FIX_CSS;
  for (const token of lwtOwned) {
    // Match each declaration of this token and capture the whole rule's
    // selector by grabbing text up to the preceding `}` or start of string.
    const declRe = new RegExp(
      "([\\s\\S]*?)\\{[^{}]*" +
        token.replace(/[-/\\^$*+?.()|[\]{}]/g, "\\$&") +
        "\\s*:[^;}]*[;}]",
      "g",
    );
    let m: RegExpExecArray | null;
    while ((m = declRe.exec(combined)) !== null) {
      const selectorChunk = (m[1] ?? "").trim();
      const isNoThemeScoped = selectorChunk.includes(":not([lwtheme])") ||
        selectorChunk.includes(":not(:-moz-lwtheme)");
      const declValue = m[0].slice(m[0].indexOf(token));
      const isGuardedAlias = /var\(\s*--lwt-accent-color/.test(declValue) ||
        /var\(\s*--toolbar-background-color/.test(declValue) ||
        /var\(\s*--panel-background-color/.test(declValue) ||
        /var\(\s*--panel-text-color/.test(declValue) ||
        /var\(\s*--panel-border-color/.test(declValue);
      assert(
        isNoThemeScoped || isGuardedAlias,
        `${token} must only be set in the no-theme scope or via a guarded ` +
          `alias (so a loaded LWT keeps its value); offending selector: ` +
          `"${selectorChunk}"`,
      );
    }
  }
}

/** The LWT accent must NOT be the broken cyclic `var(--lwt-accent-color,
 *  revert)` form from the previous revision (which degenerated to `revert`
 *  and let nothing through). */
function testNoCyclicLwtAccentReference(): void {
  assert(
    !/var\(\s*--lwt-accent-color\s*,\s*revert\s*\)/.test(LEPTON_COMPAT_152_CSS),
    "compat layer must not use the cyclic var(--lwt-accent-color, revert) " +
      "form — it degenerates to revert and blocks the LWT palette",
  );
}

// ---------------------------------------------------------------------------
// Tests — the two reported symptoms are directly addressed
// ---------------------------------------------------------------------------

/** "Dialog boxes show a black background" — ensure in-content / dialog
 *  background is anchored to a stable per-scheme value. */
function testCompatFixesDialogBackground(): void {
  assert(
    GECKO_152_COLOR_FIX_CSS.includes("--in-content-page-background"),
    "color-fix layer should pin --in-content-page-background for dialogs",
  );
  assert(
    GECKO_152_COLOR_FIX_CSS.includes("dialog"),
    "color-fix layer should target the dialog element",
  );
}

/** "The whole UI turns blue" / "transparent panels" — ensure the panel
 *  background gets a safe default for the no-theme case. */
function testCompatFixesPanelBackground(): void {
  assert(
    GECKO_152_COLOR_FIX_CSS.includes("--arrowpanel-background"),
    "color-fix layer should provide a default --arrowpanel-background",
  );
}

/** Gecko 152 uses [sidebar-positionend] as a boolean presence attribute. The
 *  compat selector must therefore match every present value, not only
 *  [sidebar-positionend="true"]. */
function testCompatFixesRightSidebarByAttributePresence(): void {
  const selector = extractSidebarPositionEndSelector();
  assert(
    selector.includes("[sidebar-positionend]"),
    "right-sidebar compat should target Gecko 152's attribute",
  );
  assert(
    !selector.includes('[sidebar-positionend="true"]'),
    "right-sidebar compat must use presence semantics, not value equality",
  );

  for (const value of ["", "true", "false"]) {
    assert(
      makeSidebarBox(value).matches(selector),
      `right-sidebar selector should match present value ${
        JSON.stringify(value)
      }`,
    );
  }
}

/** A left sidebar has neither position-end attribute. It must retain its
 *  normal LTR direction; older Gecko's [positionend] marker remains accepted
 *  for backward compatibility. */
function testCompatPreservesLeftAndLegacySidebarSemantics(): void {
  const selector = extractSidebarPositionEndSelector();
  assert(
    !makeSidebarBox(null).matches(selector),
    "right-sidebar compat must not match a left sidebar",
  );

  const legacyRightSidebar = makeSidebarBox(null);
  legacyRightSidebar.setAttribute("positionend", "");
  assert(
    legacyRightSidebar.matches(selector),
    "right-sidebar compat should retain the legacy presence attribute",
  );
}

// ---------------------------------------------------------------------------
// Tests — Floorp-specific icon patches are preserved out-of-vendor
// ---------------------------------------------------------------------------

function testFloorpIconPatchesPresent(): void {
  const floorpIds = [
    "#ssbPageAction-image",
    "#usercssloader-menu",
    "#unloadWebpanelMenu",
    "#changeUAWebpanelMenu",
    "#deleteWebpanelMenu",
    "#toggle_sharemode",
    "#run-ssb-contextmenu",
    "#uninstall-ssb-contextmenu",
  ];
  for (const id of floorpIds) {
    assert(
      FLOORP_ICON_PATCHES.includes(id),
      `Floorp icon patch for ${id} should be preserved in the compat layer`,
    );
  }
}

/** Bundled export is the sum of the color fix, the Lepton compat, and the
 *  icon patches. */
function testBundledCompatIsColorPlusLeptonPlusIcons(): void {
  assertEquals(
    LEPTON_COMPAT_CSS,
    GECKO_152_COLOR_FIX_CSS + "\n" + LEPTON_COMPAT_152_CSS + "\n" +
      FLOORP_ICON_PATCHES,
    "LEPTON_COMPAT_CSS must bundle the color fix + Lepton compat + icon patches",
  );
}

/** The icon patches ride along with the Lepton family only (the IDs are
 *  Lepton-scoped). */
function testLeptonThemesIncludeFloorpIconPatches(): void {
  for (const theme of ["lepton", "photon", "protonfix"] as const) {
    const css = getInlineChromeCss(theme);
    assert(
      css.includes("#usercssloader-menu"),
      `${theme} should include the Floorp icon patches`,
    );
  }
}

/** fluerial gets the color fix but NOT the Lepton icon patches. */
function testFluerialExcludesLeptonIconPatches(): void {
  const css = getInlineChromeCss("fluerial");
  assert(
    !css.includes("#usercssloader-menu"),
    "fluerial must NOT include the Lepton-scoped Floorp icon patches",
  );
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  await runTests("lepton-compat.test.ts", [
    // alias table canonicality
    { name: "renamed vars table is canonical", fn: testRenamedVarsTableIsCanonical },
    { name: "--toolbar-color is not a rename source", fn: testToolbarColorIsNotARenameSource },
    { name: "--toolbar-text-color is synthesized", fn: testToolbarTextColorIsSynthesized },
    // alias emission
    { name: "renamed aliases are emitted correctly", fn: testRenamedAliasesAreEmittedCorrectly },
    { name: "synthesized aliases are emitted correctly", fn: testSynthesizedAliasesAreEmittedCorrectly },
    // injection scope
    { name: "lepton themes include compat css", fn: testLeptonThemesIncludeCompatCss },
    { name: "all skinned themes include color fix", fn: testAllSkinnedThemesIncludeColorFix },
    { name: "proton excludes both layers", fn: testProtonExcludesBothLayers },
    // robust detection
    { name: "compat avoids brittle selectors", fn: testCompatAvoidsBrittleSelectors },
    { name: "compat uses robust lwt signals", fn: testCompatUsesRobustLwtSignals },
    // non-destructive LWT contract
    { name: "no color override uses !important", fn: testNoColorOverrideUsesImportant },
    { name: "lwt-owned tokens are guarded or no-theme scoped", fn: testLwtOwnedTokensAreGuardedOrNoThemeScoped },
    { name: "no cyclic lwt accent reference", fn: testNoCyclicLwtAccentReference },
    // symptom coverage
    { name: "compat fixes dialog background (black dialog symptom)", fn: testCompatFixesDialogBackground },
    { name: "compat fixes panel background (transparent panel symptom)", fn: testCompatFixesPanelBackground },
    {
      name: "right-sidebar compat uses attribute presence",
      fn: testCompatFixesRightSidebarByAttributePresence,
    },
    {
      name: "right-sidebar compat preserves left and legacy semantics",
      fn: testCompatPreservesLeftAndLegacySidebarSemantics,
    },
    // icon patches
    { name: "floorp icon patches present", fn: testFloorpIconPatchesPresent },
    { name: "bundled compat is color + lepton + icons", fn: testBundledCompatIsColorPlusLeptonPlusIcons },
    { name: "lepton themes include floorp icon patches", fn: testLeptonThemesIncludeFloorpIconPatches },
    { name: "fluerial excludes lepton icon patches", fn: testFluerialExcludesLeptonIconPatches },
  ]);
}

await runAllTests();
