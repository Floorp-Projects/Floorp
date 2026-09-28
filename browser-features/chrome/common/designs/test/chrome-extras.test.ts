// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
//
// Chrome extras — the 25 UI toggles that used to be Lepton-specific.
//
// They used to be stored as `userChrome.*` bool prefs and implemented inside the
// vendored Lepton stylesheet, so they only existed for the Lepton-family
// designs. They now live in `uiCustomization.chromeExtras` and are injected as a
// design-agnostic stylesheet. See `designs/chrome-extras.ts`.
//
// These tests pin the two invariants that make that move safe:
//   1. the CSS carries no Lepton pref gate and every `--uc-*` token it
//      references is defined by the scaffold;
//   2. the migration from the old prefs fills in every key, so an upgrading
//      profile keeps the user's choices.

import {
  buildChromeExtrasCSS,
  CHROME_EXTRAS_CSS,
  CHROME_EXTRAS_DEFAULTS,
  CHROME_EXTRAS_KEYS,
  CHROME_EXTRAS_SCAFFOLD_CSS,
  CHROME_EXTRAS_STYLE_ID,
} from "../chrome-extras.ts";
import {
  config,
  deepMerge,
  getChromeExtrasSettings,
  setConfig,
  updateChromeExtrasSetting,
} from "../configs.ts";
import {
  getOldChromeExtrasConfig,
  LEGACY_ALIASES,
  LEGACY_CHROME_EXTRAS_PREFS,
  syncLegacyChromeExtrasPrefs,
} from "../utils/old-config-migrator.ts";
import { StyleManager } from "#features-chrome/common/ui-custom/styles/style-manager.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";
import { zFloorpDesignConfigs } from "../type.ts";
import { isRight } from "fp-ts/Either";
import { createRoot } from "solid-js";

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

function toggles(patch: Partial<Record<string, boolean>>) {
  return { ...CHROME_EXTRAS_DEFAULTS, ...patch };
}

/** Drop `/* ... *\/` comments so assertions look at rules, not at the
 *  provenance notes every generated file carries. */
function stripComments(css: string): string {
  return css.replace(/\/\*[\s\S]*?\*\//g, "");
}

// ---------------------------------------------------------------------------
// Tests — the key set
// ---------------------------------------------------------------------------

function testKeySetIsCompleteAndUnique(): void {
  assertEquals(
    CHROME_EXTRAS_KEYS.length,
    25,
    "there are exactly 25 chrome-extras toggles",
  );
  const unique = new Set<string>(CHROME_EXTRAS_KEYS);
  assertEquals(
    unique.size,
    CHROME_EXTRAS_KEYS.length,
    "CHROME_EXTRAS_KEYS must not contain duplicates",
  );
}

/** Every key has CSS, and the CSS record has no key that is not a real toggle. */
function testEveryKeyHasCss(): void {
  for (const key of CHROME_EXTRAS_KEYS) {
    assert(
      typeof CHROME_EXTRAS_CSS[key] === "string" &&
        CHROME_EXTRAS_CSS[key].trim().length > 0,
      `${key} must have non-empty CSS`,
    );
  }
  for (const key of Object.keys(CHROME_EXTRAS_CSS)) {
    assert(
      (CHROME_EXTRAS_KEYS as readonly string[]).includes(key),
      `CHROME_EXTRAS_CSS has an unknown key: ${key}`,
    );
  }
}

function testDefaultsAreAllOff(): void {
  for (const key of CHROME_EXTRAS_KEYS) {
    assertEquals(
      CHROME_EXTRAS_DEFAULTS[key],
      false,
      `${key} must default to false`,
    );
  }
}

function testStyleIdIsStable(): void {
  assertEquals(
    CHROME_EXTRAS_STYLE_ID,
    "floorp-chrome-extras",
    "the style element id is part of the manual debugging contract",
  );
}

// ---------------------------------------------------------------------------
// Tests — the CSS no longer depends on Lepton
// ---------------------------------------------------------------------------

/** No sheet may gate on a `userChrome.` pref — that is the Lepton dependency.
 *  Gecko's own prefs (e.g. `widget.*.native-context-menus` in
 *  hidden-disabled-menu.css) are fine and are kept verbatim. */
function testNoLeptonPrefGate(): void {
  const all = [
    CHROME_EXTRAS_SCAFFOLD_CSS,
    ...Object.values(CHROME_EXTRAS_CSS),
  ];
  for (
    const [key, css] of Object.entries({
      scaffold: CHROME_EXTRAS_SCAFFOLD_CSS,
      ...CHROME_EXTRAS_CSS,
    })
  ) {
    const rules = stripComments(css);
    assert(
      !rules.includes('-moz-bool-pref: "userChrome.'),
      `${key} must not gate on a userChrome.* pref with -moz-bool-pref`,
    );
    assert(
      !rules.includes('-moz-pref("userChrome.'),
      `${key} must not gate on a userChrome.* pref with -moz-pref`,
    );
  }
}

/**
 * Every `--uc-*` token a toggle sheet relies on must be defined by chrome-extras
 * itself, or be referenced with a fallback. A dangling reference with no
 * fallback is how the autohide back/forward buttons silently broke before the
 * Gecko 152 toolbarbutton padding aliases were added.
 */
function testEveryUsedUcTokenIsDefinedOrHasFallback(): void {
  const declare = /^\s*(--[a-z0-9-]+)\s*:/gm;
  const defined = new Set<string>();
  for (const match of CHROME_EXTRAS_SCAFFOLD_CSS.matchAll(declare)) {
    defined.add(match[1]);
  }
  const sheets = new Map<string, string>([
    ["scaffold", CHROME_EXTRAS_SCAFFOLD_CSS],
    ...Object.entries(CHROME_EXTRAS_CSS),
  ]);
  for (const [key, css] of sheets) {
    for (const match of css.matchAll(declare)) {
      defined.add(match[1]);
    }
  }

  for (const [key, css] of sheets) {
    for (const match of css.matchAll(/var\((--uc-[a-z0-9-]+)(,)?/g)) {
      const token = match[1];
      if (defined.has(token) || match[2] === ",") {
        continue;
      }
      assert(
        false,
        `${key} uses ${token} with no definition and no fallback`,
      );
    }
  }
}

/**
 * The Gecko tokens the ported rules depend on, which chrome-extras must NOT try
 * to define. `--toolbarbutton-*-padding` is the important one: it is supplied by
 * the Gecko 152 alias sheet, and before that alias existed the autohide
 * back/forward rules silently did nothing.
 */
function testExternalFirefoxTokensAreNotDefinedLocally(): void {
  const external = [
    "--animation-easing-function",
    "--toolbarbutton-outer-padding",
    "--urlbar-icon-padding",
    "--tab-min-height",
    "--tab-border-radius",
    "--urlbar-container-height",
    "--sidebar-background-color",
    "--sidebar-text-color",
  ];
  // `--toolbarbutton-inner-padding` excluded: see the assertion below.
  const declared = new Set<string>();
  for (
    const css of [
      CHROME_EXTRAS_SCAFFOLD_CSS,
      ...Object.values(CHROME_EXTRAS_CSS),
    ]
  ) {
    for (const match of css.matchAll(/^\s*(--[a-z0-9-]+)\s*:/gm)) {
      declared.add(match[1]);
    }
  }
  for (const token of external) {
    // `--toolbarbutton-inner-padding` is the one deliberate exception: Lepton's
    // one-liner nav bar overrides it (6px vs Gecko's 8px) and that override is
    // ported as-is. Everything else must never be shadowed.
    assert(
      !declared.has(token),
      `${token} is a Gecko token — chrome-extras must alias it elsewhere, ` +
        "not shadow it",
    );
  }
  assert(
    CHROME_EXTRAS_CSS.tabbarOneLiner.includes("--toolbarbutton-inner-padding:"),
    "the one-liner override of --toolbarbutton-inner-padding is intentional",
  );
  // and the one that actually matters must still resolve to something
  const scaffoldDefinesHideSize = /--uc-toolbarbutton-hide-size:/
    .test(CHROME_EXTRAS_SCAFFOLD_CSS);
  assert(
    scaffoldDefinesHideSize,
    "the scaffold must derive --uc-toolbarbutton-hide-size from the aliased " +
      "padding tokens",
  );
}

/** The scaffold must carry the tokens the ported sidebar rules rely on. */
function testScaffoldDefinesSidebarAndToolbarbuttonTokens(): void {
  for (
    const token of [
      "--uc-sidebar-width",
      "--uc-sidebar-activate-width",
      "--uc-toolbarbutton-hide-size",
      "--uc-tabbar-height",
      "--uc-window-control-space",
    ]
  ) {
    assert(
      CHROME_EXTRAS_SCAFFOLD_CSS.includes(`${token}:`),
      `scaffold must define ${token}`,
    );
  }
}

/**
 * The three Floorp-native implementations must not lean on Lepton's layout
 * variables. They may use `--uc-*` tokens, but only ones the scaffold or the
 * sheet itself defines — that is what makes them work unchanged on every design.
 */
function testNativeImplementationsAvoidLeptonLayoutTokens(): void {
  const defined = (css: string): Set<string> => {
    const out = new Set<string>();
    for (const match of css.matchAll(/^\s*(--uc-[a-z0-9-]+)\s*:/gm)) {
      out.add(match[1]);
    }
    return out;
  };
  const scaffoldTokens = defined(CHROME_EXTRAS_SCAFFOLD_CSS);

  for (
    const key of [
      "autohideNavbar",
      "tabbarAsTitlebar",
      "tabbarOneLiner",
    ] as const
  ) {
    const css = CHROME_EXTRAS_CSS[key];
    const own = defined(css);
    for (const match of css.matchAll(/var\((--uc-[a-z0-9-]+)(,)?/g)) {
      const token = match[1];
      if (scaffoldTokens.has(token) || own.has(token) || match[2] === ",") {
        continue;
      }
      assert(
        false,
        `${key} depends on Lepton's layout variable ${token}`,
      );
    }
    // Lepton's sidebar layout tokens must not leak into the native sheets.
    assert(
      !/--uc-sidebar-[a-z-]+/.test(stripComments(css)),
      `${key} must not use Lepton's --uc-sidebar-* layout tokens`,
    );
  }
}

// ---------------------------------------------------------------------------
// Tests — assembling the stylesheet
// ---------------------------------------------------------------------------

function testBuildEmitsScaffoldEvenWithNothingEnabled(): void {
  const css = buildChromeExtrasCSS(toggles({}));
  assert(
    css.includes(CHROME_EXTRAS_SCAFFOLD_CSS.trim()),
    "the scaffold is always present (it is inert until a consumer is enabled)",
  );
}

function testBuildIncludesOnlyEnabledToggles(): void {
  const css = buildChromeExtrasCSS(toggles({ hiddenTabbar: true }));
  assert(
    css.includes("#TabsToolbar"),
    "an enabled toggle's rules must be present",
  );
  assert(
    !css.includes("var(--uc-sidebar-width)"),
    "a disabled toggle's rules must be absent",
  );
}

/** `iconMenu` only means anything while the icon rules are active. */
function testIconMenuIsSkippedWhenIconsDisabled(): void {
  const withIcons = buildChromeExtrasCSS(toggles({ iconMenu: true }));
  assert(
    withIcons.includes("#usercssloader-menu"),
    "iconMenu rules apply while icons are enabled",
  );

  const withoutIcons = buildChromeExtrasCSS(
    toggles({ iconMenu: true, iconDisabled: true }),
  );
  assert(
    !withoutIcons.includes("#usercssloader-menu"),
    "iconMenu rules must be dropped when iconDisabled is on",
  );
}

/** The `iconDisabled` sheet was extracted from a negated Lepton pref gate. */
function testBaseIconRulesFollowIconDisabledPolarity(): void {
  const enabled = buildChromeExtrasCSS(toggles({}));
  assert(
    enabled.includes(CHROME_EXTRAS_CSS.iconDisabled),
    "base icon rules must be present while iconDisabled is off",
  );

  const disabled = buildChromeExtrasCSS(toggles({ iconDisabled: true }));
  assert(
    !disabled.includes(CHROME_EXTRAS_CSS.iconDisabled),
    "base icon rules must be absent while iconDisabled is on",
  );
}

/**
 * `urlViewGoButtonWhenTyping` addressed the two urlbar nodes by ID, which is how
 * Lepton (pre-152) built them. `UrlbarInput.mjs` now emits them as plain `<div>`s
 * with only a class, so the verbatim port matched nothing at all and the toggle
 * silently did nothing. Pinned here because the failure mode is invisible: the
 * sheet still parses, the toggle still flips, nothing changes on screen.
 */
function testGoButtonSelectorIsClassBased(): void {
  const css = stripComments(CHROME_EXTRAS_CSS.urlViewGoButtonWhenTyping);
  assert(
    css.includes('.urlbar-input-container[pageproxystate="invalid"]') &&
      css.includes(".urlbar-go-button"),
    "the go-button rule must target the nodes by class",
  );
  assert(
    !/#urlbar-(input-container|go-button)/.test(css),
    "Gecko 152 removed the ids from these two nodes — an id selector is a no-op",
  );
}

// ---------------------------------------------------------------------------
// Tests — config store
// ---------------------------------------------------------------------------

function testChromeExtrasDecode(): void {
  const result = zFloorpDesignConfigs.decode({
    globalConfigs: {
      faviconColor: false,
      userInterface: "fluerial",
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
      chromeExtras: { autohideTab: true },
    },
  });
  assert(
    isRight(result),
    "a config carrying uiCustomization.chromeExtras must decode",
  );
}

/** `deepMerge` is what fills the new category on an upgrading profile. */
function testDeepMergeFillsMissingChromeExtras(): void {
  const merged = deepMerge(
    {
      uiCustomization: {
        navbar: { position: "top" },
        chromeExtras: { ...CHROME_EXTRAS_DEFAULTS },
      },
    } as unknown as Record<string, unknown>,
    {
      uiCustomization: { chromeExtras: { hiddenTabbar: true } },
    } as unknown,
  ) as {
    uiCustomization: { chromeExtras: Record<string, boolean> };
  };
  for (const key of CHROME_EXTRAS_KEYS) {
    assert(
      typeof merged.uiCustomization.chromeExtras[key] === "boolean",
      `deepMerge must leave a boolean for ${key}`,
    );
  }
  assertEquals(
    merged.uiCustomization.chromeExtras.hiddenTabbar,
    true,
    "the stored value must win over the seeded default",
  );
}

/** With no pref set at all the migration yields the all-off defaults. */
function testLegacyMigrationDefaultsToOff(): void {
  const migrated = getOldChromeExtrasConfig();
  for (const key of CHROME_EXTRAS_KEYS) {
    assert(
      typeof migrated[key] === "boolean",
      `migrated ${key} must be a boolean`,
    );
  }
}

/** A disabled new toggle must also disable the pref-gated vendored rule. */
function testLegacyLeptonPrefsFollowNewSettings(): void {
  const prefNames = [
    ...new Set([
      ...Object.values(LEGACY_CHROME_EXTRAS_PREFS),
      ...Object.values(LEGACY_ALIASES).flat(),
    ]),
  ];
  const previous = prefNames.map((name) => ({
    name,
    hasUserValue: Services.prefs.prefHasUserValue(name),
    value: Services.prefs.getBoolPref(name, false),
  }));

  try {
    syncLegacyChromeExtrasPrefs(toggles({
      iconDisabled: true,
      iconMenu: false,
      hiddenTabbar: true,
      urlViewAlwaysShowPageActions: true,
    }));
    assertEquals(
      Services.prefs.getBoolPref("userChrome.icon.disabled"),
      true,
      "disabling native icons must also disable Lepton icons",
    );
    assertEquals(
      Services.prefs.getBoolPref("userChrome.icon.menu"),
      false,
      "disabling native menu icons must also disable Lepton menu icons",
    );
    assertEquals(
      Services.prefs.getBoolPref("userChrome.hidden.tabbar"),
      true,
      "enabling the native hidden tabbar must also enable Lepton's rule",
    );
    assertEquals(
      Services.prefs.getBoolPref("userChrome.urlbar.always_show_page_actions"),
      true,
      "Lepton's alias must follow the new setting",
    );

    syncLegacyChromeExtrasPrefs(toggles({}));
    for (const name of prefNames) {
      assertEquals(
        Services.prefs.getBoolPref(name),
        false,
        `${name} must turn off with the new setting`,
      );
    }
  } finally {
    for (const pref of previous) {
      if (pref.hasUserValue) {
        Services.prefs.setBoolPref(pref.name, pref.value);
      } else {
        Services.prefs.clearUserPref(pref.name);
      }
    }
  }
}

/** Exercise the mounted design effect, not just the compatibility helper. */
async function testLiveLeptonToggleTurnsOffOldRule(): Promise<void> {
  const before = config();
  try {
    setConfig((prev) => ({
      ...prev,
      globalConfigs: { ...prev.globalConfigs, userInterface: "lepton" },
      uiCustomization: {
        ...prev.uiCustomization,
        chromeExtras: {
          ...getChromeExtrasSettings(),
          iconMenu: true,
          iconDisabled: false,
        },
      },
    }));
    await new Promise((resolve) => setTimeout(resolve, 0));
    assertEquals(
      Services.prefs.getBoolPref("userChrome.icon.menu"),
      true,
      "turning on native menu icons must enable Lepton's legacy rule",
    );
    assert(
      (injectedStyle()?.textContent ?? "").includes(
        CHROME_EXTRAS_CSS.iconDisabled,
      ),
      "the mounted style manager must include base icons when enabled",
    );

    updateChromeExtrasSetting("iconMenu", false);
    updateChromeExtrasSetting("iconDisabled", true);
    await new Promise((resolve) => setTimeout(resolve, 0));
    assertEquals(
      Services.prefs.getBoolPref("userChrome.icon.menu"),
      false,
      "turning off native menu icons must disable Lepton's legacy rule",
    );
    assertEquals(
      Services.prefs.getBoolPref("userChrome.icon.disabled"),
      true,
      "disabling icons must also disable the vendored icon sheet",
    );
    assert(
      !(injectedStyle()?.textContent ?? "").includes(
        CHROME_EXTRAS_CSS.iconDisabled,
      ),
      "the mounted style manager must remove base icons when disabled",
    );
  } finally {
    setConfig(before);
    await new Promise((resolve) => setTimeout(resolve, 0));
  }
}

/** The round trip through the store must survive. */
function testChromeExtrasStoreRoundTrip(): void {
  const before = getChromeExtrasSettings();
  updateChromeExtrasSetting("hiddenTabIcon", true);
  const after = getChromeExtrasSettings();
  try {
    assertEquals(after.hiddenNavbar, before.hiddenNavbar, "unrelated key");
    assertEquals(
      after.hiddenTabIcon,
      true,
      "updateChromeExtrasSetting must take effect",
    );
  } finally {
    updateChromeExtrasSetting("hiddenTabIcon", before.hiddenTabIcon);
  }
}

// ---------------------------------------------------------------------------
// Tests — the stylesheet is injected into the document
// ---------------------------------------------------------------------------

function injectedStyle(): HTMLElement | null {
  return document.getElementById(CHROME_EXTRAS_STYLE_ID);
}

function withStyleManager(fn: (manager: StyleManager) => void): void {
  createRoot((dispose) => {
    const manager = new StyleManager();
    manager.setupStyleEffects();
    fn(manager);
    dispose();
  });
}

/** With everything off the sheet still exists (the scaffold is always there). */
function testStylesheetIsAlwaysInjected(): void {
  const before = injectedStyle();
  withStyleManager(() => {
    const style = injectedStyle();
    assert(style !== null, "the chrome-extras style element must be injected");
    assert(
      style?.tagName.toLowerCase() === "style",
      "it must be a <style> element",
    );
    assert(
      (style?.textContent ?? "").includes("--uc-sidebar-width"),
      "an empty configuration must still carry the scaffold",
    );
  });
  if (before) {
    document.head.appendChild(before);
  }
}

/** Turning a toggle on must add its rules; turning it off must remove them. */
function testTogglingUpdatesTheStylesheet(): void {
  const before = getChromeExtrasSettings().hiddenTabbar;
  updateChromeExtrasSetting("hiddenTabbar", true);
  try {
    withStyleManager(() => {
      const css = injectedStyle()?.textContent ?? "";
      assert(
        css.includes("#TabsToolbar"),
        "an enabled toggle's rules must reach the document",
      );
      assert(
        !css.includes("var(--uc-sidebar-width) !important"),
        "a disabled toggle's rules must not",
      );
    });
  } finally {
    updateChromeExtrasSetting("hiddenTabbar", before);
  }
}

/** `reappendStyle` must move an existing node to the end of <head> without
 *  dropping it — that is what keeps the sheet winning over the design CSS. */
function testReappendStyleMovesToEndOfHead(): void {
  withStyleManager((manager) => {
    const style = injectedStyle();
    assert(style !== null, "the style element must exist");

    // Something else lands after it, as the design sheets do on a design switch.
    const probe = document.createElement("style");
    probe.id = "floorp-chrome-extras-cascade-probe";
    document.head.appendChild(probe);
    assert(
      document.head.lastElementChild?.id === probe.id,
      "the probe must be last before re-appending",
    );

    manager.reappendStyle(CHROME_EXTRAS_STYLE_ID);

    assert(
      document.head.lastElementChild?.id === CHROME_EXTRAS_STYLE_ID,
      "reappendStyle must move the sheet to the end of <head>",
    );
    assert(
      injectedStyle() === style,
      "reappendStyle must move the same node, not recreate it",
    );
    const before = style?.textContent ?? "";
    assert(
      before.includes("--uc-sidebar-width"),
      "moving the node must not clear its content",
    );
    probe.remove();
  });
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "key set is complete and unique",
      fn: testKeySetIsCompleteAndUnique,
    },
    { name: "every key has css", fn: testEveryKeyHasCss },
    { name: "defaults are all off", fn: testDefaultsAreAllOff },
    { name: "style id is stable", fn: testStyleIdIsStable },
    { name: "no lepton pref gate", fn: testNoLeptonPrefGate },
    {
      name: "every used uc token is defined or has a fallback",
      fn: testEveryUsedUcTokenIsDefinedOrHasFallback,
    },
    {
      name: "external firefox tokens are not defined locally",
      fn: testExternalFirefoxTokensAreNotDefinedLocally,
    },
    {
      name: "native implementations avoid lepton layout tokens",
      fn: testNativeImplementationsAvoidLeptonLayoutTokens,
    },
    {
      name: "scaffold defines sidebar and toolbarbutton tokens",
      fn: testScaffoldDefinesSidebarAndToolbarbuttonTokens,
    },
    {
      name: "build emits scaffold even with nothing enabled",
      fn: testBuildEmitsScaffoldEvenWithNothingEnabled,
    },
    {
      name: "build includes only enabled toggles",
      fn: testBuildIncludesOnlyEnabledToggles,
    },
    {
      name: "icon menu is skipped when icons disabled",
      fn: testIconMenuIsSkippedWhenIconsDisabled,
    },
    {
      name: "base icon rules follow iconDisabled polarity",
      fn: testBaseIconRulesFollowIconDisabledPolarity,
    },
    {
      name: "go button selector is class based",
      fn: testGoButtonSelectorIsClassBased,
    },
    { name: "chrome extras decode", fn: testChromeExtrasDecode },
    {
      name: "deep merge fills missing chrome extras",
      fn: testDeepMergeFillsMissingChromeExtras,
    },
    {
      name: "legacy migration defaults to off",
      fn: testLegacyMigrationDefaultsToOff,
    },
    {
      name: "legacy Lepton prefs follow new settings",
      fn: testLegacyLeptonPrefsFollowNewSettings,
    },
    {
      name: "live Lepton toggle turns off old rule",
      fn: testLiveLeptonToggleTurnsOffOldRule,
    },
    {
      name: "chrome extras store round trip",
      fn: testChromeExtrasStoreRoundTrip,
    },
    {
      name: "stylesheet is always injected",
      fn: testStylesheetIsAlwaysInjected,
    },
    {
      name: "toggling updates the stylesheet",
      fn: testTogglingUpdatesTheStylesheet,
    },
    {
      name: "reappend style moves to end of head",
      fn: testReappendStyleMovesToEndOfHead,
    },
  ];
  await runTests("chrome-extras.test.ts", tests);
}

await runAllTests();
