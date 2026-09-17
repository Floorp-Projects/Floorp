// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

/**
 * Color-consistency integration test for the Floorp design / theme system.
 *
 * Regression target: on Gecko 152 (Project Nova) the legacy
 * `--toolbar-bgcolor` and the new `--toolbar-background-color` DIVERGE
 * (e.g. default-theme dark: `#171717` vs `rgb(43,42,51)`). If a chrome
 * surface reads the wrong token, the surfaces no longer share one color and
 * the bar visually "floats" off the toolbar. Before the Gecko 152 fix this
 * happened for `lepton × system-auto dark`: the nav-bar rendered
 * `rgb(43,42,51)` (read from `--toolbar-background-color`) while the
 * surrounding toolbars tracked the legacy token — the surfaces split.
 *
 * TWO INVARIANTS are verified across the DESIGN × THEME matrix:
 *
 * 1. "One surface" (PRIMARY — every design, every theme):
 *    `#nav-bar` == `#PersonalToolbar` == `#panel-sidebar-box` ==
 *    `#nora-statusbar`. This is the invariant the Gecko 152 fix restored:
 *    all chrome bars must share the SAME background color. It does NOT
 *    require them to match the selected tab — that is a per-design choice.
 *
 * 2. "Tab tracks toolbar" (SECONDARY — `color_like_toolbar` designs only):
 *    For `lepton`, `photon`, `fluerial` the selected tab is aligned to the
 *    toolbar color (Lepton's `userChrome.tab.color_like_toolbar = true`),
 *    so the selected tab must be CLOSE to the bar color. `protonfix` sets
 *    `color_like_toolbar = false` and `proton` is stock Firefox, so both
 *    legitimately let the tab "float" — they are exempt.
 *
 * The test switches the active design live by rewriting the
 * `floorp.design.configs` pref (observed synchronously by the design
 * system's pref observer, which re-runs the Solid effects that re-register
 * AGENT_SHEETs and re-paint — no restart needed) and toggles light/dark via
 * `ui.systemUsesDarkTheme` (which overrides `prefers-color-scheme`, the very
 * condition that reproduced the bug).
 *
 * NOTE on equality: the colocated harness `assertEquals` uses reference
 * equality, and `getComputedStyle().backgroundColor` returns strings that may
 * differ in form (`rgba(r,g,b,1)` vs `rgb(r,g,b)`), so colors are parsed to
 * RGB tuples and compared numerically with tolerances chosen from measured
 * good-state distributions.
 */

import {
  assert,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

type RgbTuple = readonly [number, number, number];

type DesignName = "fluerial" | "lepton" | "photon" | "protonfix" | "proton";

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

/** Designs covered by the color matrix. */
const DESIGNS: readonly DesignName[] = [
  "lepton",
  "photon",
  "protonfix",
  "fluerial",
  "proton",
] as const;

/**
 * Designs where the selected tab is aligned to the toolbar color
 * (Lepton `userChrome.tab.color_like_toolbar = true`). For these, the
 * secondary "tab tracks toolbar" invariant applies. `protonfix` sets it
 * `false` and `proton` is stock Firefox — both are exempt.
 */
const TAB_TRACKS_TOOLBAR_DESIGNS: readonly DesignName[] = [
  "lepton",
  "photon",
  "fluerial",
] as const;

/**
 * Tolerance for the PRIMARY "one surface" invariant. The Gecko 152 fix makes
 * all bar surfaces resolve to the exact same token, so this can be tight.
 * A small slack absorbs sub-pixel/rounding in computed colors.
 */
const SURFACE_TOLERANCE = 2;

/**
 * Tolerance for the SECONDARY "tab tracks toolbar" invariant. Lepton aligns
 * the tab to the toolbar but via alpha compositing / lwt-accent blending, so
 * a tint gap is normal (measured: around 20 in current light Lepton-family
 * builds). The primary one-surface invariant above remains the strict
 * regression check for the Gecko 152 toolbar token split.
 */
const TAB_TRACKS_TOLERANCE = 22;

/** Elements whose background should share one color (the "one surface"
 * invariant). `[selector, humanLabel]`. */
const SURFACE_ELEMENTS: readonly [string, string][] = [
  ["#nav-bar", "nav-bar"],
  ["#PersonalToolbar", "PersonalToolbar"],
  ["#panel-sidebar-box", "panel-sidebar-box"],
  ["#nora-statusbar", "nora-statusbar"],
];

/** Selectors that may yield the selected tab's background, tried in order. */
const SELECTED_TAB_SELECTORS: readonly string[] = [
  ".tabbrowser-tab[selected] > .tab-stack > .tab-content",
  ".tabbrowser-tab[visuallyselected] > .tab-stack > .tab-content",
  ".tabbrowser-tab:is([selected],[multiselected]) > .tab-stack > .tab-content",
  ".tabbrowser-tab[selected] > .tab-stack > .tab-background",
  ".tabbrowser-tab[visuallyselected] > .tab-stack > .tab-background",
  ".tabbrowser-tab:is([selected],[multiselected]) > .tab-stack > .tab-background",
  "#tabbrowser-tabs .tab-background[selected]",
];

const SELECTED_TAB_RELATIVE_SELECTORS: readonly string[] = [
  ":scope > .tab-stack > .tab-content",
  ":scope > .tab-stack > .tab-background:is([selected], [multiselected])",
  ":scope > .tab-stack > .tab-background",
];

/** Original dark pref value, captured once so we can restore it. -1 = unset. */
let originalSystemUsesDarkTheme = -1;

// ---------------------------------------------------------------------------
// Color parsing / comparison helpers
// ---------------------------------------------------------------------------

/**
 * Parse a CSS color string into an `[r, g, b]` tuple. Supports `rgb(...)`,
 * `rgba(...)` (alpha composited over an opaque white background), and
 * `#rgb` / `#rrggbb` hex. Returns `null` for transparent / unparseable.
 */
function parseRgbColor(raw: string): RgbTuple | null {
  const value = raw.trim().toLowerCase();
  if (value === "" || value === "transparent" || value === "rgba(0, 0, 0, 0)") {
    return null;
  }

  // #rrggbb or #rgb
  const hexMatch = value.match(/^#([0-9a-f]{3}|[0-9a-f]{6})$/);
  if (hexMatch) {
    const digits = hexMatch[1];
    let r: number, g: number, b: number;
    if (digits.length === 3) {
      r = parseInt(digits[0] + digits[0], 16);
      g = parseInt(digits[1] + digits[1], 16);
      b = parseInt(digits[2] + digits[2], 16);
    } else {
      r = parseInt(digits.slice(0, 2), 16);
      g = parseInt(digits.slice(2, 4), 16);
      b = parseInt(digits.slice(4, 6), 16);
    }
    return [r, g, b];
  }

  // rgb(r, g, b) / rgba(r, g, b, a) — also tolerate space-separated form
  const rgbMatch = value.match(
    /^rgba?\(\s*([\d.]+)\s*[,\s]\s*([\d.]+)\s*[,\s]\s*([\d.]+)\s*(?:[,/]\s*([\d.]+)\s*)?\)$/,
  );
  if (rgbMatch) {
    const r = Math.round(parseFloat(rgbMatch[1]));
    const g = Math.round(parseFloat(rgbMatch[2]));
    const b = Math.round(parseFloat(rgbMatch[3]));
    const a = rgbMatch[4] !== undefined ? parseFloat(rgbMatch[4]) : 1;
    if (a <= 0) return null;
    if (a >= 1) return [r, g, b];
    // Composite over opaque white to get a comparable opaque color.
    return [
      Math.round(r * a + 255 * (1 - a)),
      Math.round(g * a + 255 * (1 - a)),
      Math.round(b * a + 255 * (1 - a)),
    ];
  }

  return null;
}

/** True when each channel of `a` and `b` is within `tolerance`. */
function colorsApproxEqual(
  a: RgbTuple,
  b: RgbTuple,
  tolerance: number,
): boolean {
  return (
    Math.abs(a[0] - b[0]) <= tolerance &&
    Math.abs(a[1] - b[1]) <= tolerance &&
    Math.abs(a[2] - b[2]) <= tolerance
  );
}

/** Human-readable `rgb(r, g, b)` form for assertion messages. */
function describeColor(color: RgbTuple | null): string {
  return color === null
    ? "(transparent/unparseable)"
    : `rgb(${color[0]}, ${color[1]}, ${color[2]})`;
}

// ---------------------------------------------------------------------------
// DOM access helpers
// ---------------------------------------------------------------------------

/** Read the computed background color of the first element matching
 * `selector`, as an RGB tuple. Returns `null` if the element is absent or
 * the color is transparent/unparseable. */
function readBgColor(selector: string): RgbTuple | null {
  const el = document.querySelector(selector);
  if (!el) return null;
  const style = globalThis.getComputedStyle(el);
  if (!style) return null;
  const raw = style.getPropertyValue("background-color");
  return parseRgbColor(raw);
}

/** Read the selected tab's background color. Tries several selectors. */
function getSelectedTabBgColor(): RgbTuple | null {
  const selectedTab = (globalThis as { gBrowser?: { selectedTab?: Element } })
    .gBrowser?.selectedTab;
  if (selectedTab) {
    for (const selector of SELECTED_TAB_RELATIVE_SELECTORS) {
      const el = selectedTab.querySelector(selector);
      if (!el) continue;
      const style = globalThis.getComputedStyle(el);
      if (!style) continue;
      const color = parseRgbColor(style.getPropertyValue("background-color"));
      if (color) return color;
    }
  }

  for (const selector of SELECTED_TAB_SELECTORS) {
    const color = readBgColor(selector);
    if (color) return color;
  }
  return null;
}

// ---------------------------------------------------------------------------
// Async wait helpers (mirrors cssVariablesUI.test.ts)
// ---------------------------------------------------------------------------

function sleep(ms: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

async function waitForCondition(
  predicate: () => boolean,
  timeoutMs = 5000,
  intervalMs = 50,
): Promise<boolean> {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    if (predicate()) return true;
    await sleep(intervalMs);
  }
  return predicate();
}

/**
 * Wait until the color returned by `getColor` stops changing (two consecutive
 * identical reads) or the timeout elapses. Used after a design/theme switch
 * to avoid reading a color mid-transition. Returns the last color read.
 */
async function waitForColorStable(
  getColor: () => RgbTuple | null,
  timeoutMs = 4000,
  intervalMs = 80,
): Promise<RgbTuple | null> {
  let prev = getColor();
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    await sleep(intervalMs);
    const next = getColor();
    if (
      prev !== null &&
      next !== null &&
      colorsApproxEqual(prev, next, SURFACE_TOLERANCE)
    ) {
      return next;
    }
    prev = next;
  }
  return prev;
}

// ---------------------------------------------------------------------------
// Design / theme switching
// ---------------------------------------------------------------------------

/** The active design name, read live from the pref. */
function getActiveDesignFromPref(): DesignName | null {
  try {
    const raw = Services.prefs.getStringPref("floorp.design.configs", "");
    if (!raw) return null;
    const parsed = JSON.parse(raw) as {
      globalConfigs?: { userInterface?: unknown };
    };
    const ui = parsed?.globalConfigs?.userInterface;
    if (
      ui === "fluerial" ||
      ui === "lepton" ||
      ui === "photon" ||
      ui === "protonfix" ||
      ui === "proton"
    ) {
      return ui;
    }
  } catch {
    // ignore — fall through
  }
  return null;
}

/**
 * Switch the active design live by rewriting the `floorp.design.configs` pref.
 * The design system observes this pref (`configs.ts: updateConfigFromPref`)
 * and re-runs its Solid createEffects to re-register AGENT_SHEETs and
 * re-paint — so this works without relying on the Solid setter being wired
 * into the test context. Returns false if the switch could not be confirmed.
 *
 * We go through the pref rather than `gFloorp.designs.setInterface()` because
 * the setter updates the Solid signal, whose `createEffect` writes the pref
 * back asynchronously; in the colocated-test context that reactive chain is
 * not reliably flushed. Writing the pref directly is observed synchronously
 * by the pref observer in `configs.ts`, which is the more robust path.
 */
async function setActiveDesign(name: DesignName): Promise<boolean> {
  try {
    const raw = Services.prefs.getStringPref("floorp.design.configs", "");
    const parsed = raw ? JSON.parse(raw) : {};
    if (!parsed || typeof parsed !== "object") {
      return false;
    }
    parsed.globalConfigs = parsed.globalConfigs ?? {};
    parsed.globalConfigs.userInterface = name;
    Services.prefs.setStringPref(
      "floorp.design.configs",
      JSON.stringify(parsed),
    );
  } catch (e) {
    console.error("[color-consistency] failed to switch design:", e);
    return false;
  }

  const reflected = await waitForCondition(
    () => getActiveDesignFromPref() === name,
    5000,
    50,
  );
  if (!reflected) return false;

  // Let the AGENT_SHEET re-registration + paint settle on the nav-bar.
  await waitForColorStable(() => readBgColor("#nav-bar"), 4000, 80);
  return true;
}

/**
 * Force light or dark scheme via `ui.systemUsesDarkTheme`. Returns whether the
 * scheme actually responds (detected by `#nav-bar` resolving to a
 * non-transparent computed background color after the flip). When the pref
 * has no observable effect this returns false so callers can skip the
 * corresponding case instead of failing flakily.
 *
 * NOTE: we intentionally read the **computed** background-color of `#nav-bar`
 * rather than the **specified** value of `--toolbar-bgcolor` on `:root`.
 * The Gecko 152 alias sheet sets `--toolbar-bgcolor:
 * var(--toolbar-background-color)` on `:root`; `getComputedStyle` returns the
 * raw specified string `var(--toolbar-background-color)` which is non-empty
 * even when `--toolbar-background-color` itself is undefined (Gecko < 152).
 * Reading the resolved `background-color` on an actual element avoids this
 * false positive.
 */
async function setSystemDark(isDark: boolean): Promise<boolean> {
  try {
    Services.prefs.setIntPref("ui.systemUsesDarkTheme", isDark ? 1 : 0);
  } catch {
    return false;
  }

  // Give the media-query-driven compat layer time to re-resolve, then
  // check that the nav-bar actually painted a non-transparent background.
  const stable = await waitForColorStable(
    () => readBgColor("#nav-bar"),
    4000,
    80,
  );
  return stable !== null;
}

// ---------------------------------------------------------------------------
// Surface snapshot
// ---------------------------------------------------------------------------

interface SurfaceReadings {
  surfaces: { label: string; color: RgbTuple | null }[];
  selectedTab: RgbTuple | null;
}

function readAllSurfaces(): SurfaceReadings {
  const surfaces = SURFACE_ELEMENTS.map(([selector, label]) => ({
    label,
    color: readBgColor(selector),
  }));
  return { surfaces, selectedTab: getSelectedTabBgColor() };
}

// ---------------------------------------------------------------------------
// Invariant assertions
// ---------------------------------------------------------------------------

/**
 * PRIMARY invariant: every present chrome surface reads the SAME color.
 * This is what the Gecko 152 fix restored — nav-bar, PersonalToolbar,
 * panel-sidebar and status bar must all share one background. Missing
 * elements are skipped; if too few surfaces are present to compare, the
 * case is skipped rather than failed.
 */
function assertOneSurface(label: string, reading: SurfaceReadings): void {
  const present = reading.surfaces.filter((s) => s.color !== null);
  assert(
    present.length >= 2,
    `${label}: need at least 2 surfaces to compare, found ${present.length} — invariant not testable`,
  );

  const reference = present[0]!.color!;
  for (const surface of present) {
    assert(
      colorsApproxEqual(surface.color!, reference, SURFACE_TOLERANCE),
      `${label}: ${surface.label} (${
        describeColor(surface.color)
      }) does not match ${present[0]!.label} (${
        describeColor(reference)
      }) — chrome surfaces diverged (regression: bars no longer share one color)`,
    );
  }
}

/**
 * SECONDARY invariant: for designs with `color_like_toolbar = true`, the
 * selected tab must be CLOSE to the bar surface color (Lepton aligns the tab
 * to the toolbar, but via blending so a small tint gap is normal). Skipped
 * when no selected-tab color is readable.
 */
function assertTabTracksToolbar(label: string, reading: SurfaceReadings): void {
  if (reading.selectedTab === null) return;
  const navBar = reading.surfaces.find((s) => s.label === "nav-bar")?.color;
  if (!navBar) return;
  assert(
    colorsApproxEqual(navBar, reading.selectedTab, TAB_TRACKS_TOLERANCE),
    `${label}: nav-bar (${
      describeColor(navBar)
    }) is too far from selected tab (${
      describeColor(reading.selectedTab)
    }) — tab should track the toolbar for this design (color_like_toolbar)`,
  );
}

function designTracksTab(design: DesignName): boolean {
  return (TAB_TRACKS_TOOLBAR_DESIGNS as readonly string[]).includes(design);
}

// ---------------------------------------------------------------------------
// Per-design per-theme case
// ---------------------------------------------------------------------------

interface CaseOutcome {
  label: string;
  ran: boolean;
  error: string | null;
}

async function runMatrixCase(
  design: DesignName,
  isDark: boolean,
): Promise<CaseOutcome> {
  const schemeLabel = isDark ? "dark" : "light";
  const caseLabel = `${design}/${schemeLabel}`;

  const switched = await setActiveDesign(design);
  if (!switched) {
    return { label: caseLabel, ran: false, error: null };
  }

  const schemeApplied = await setSystemDark(isDark);
  if (!schemeApplied) {
    // The scheme pref had no observable effect in this environment (e.g. a
    // built-in Light/Dark theme is pinned). Skip rather than fail flakily.
    return { label: caseLabel, ran: false, error: null };
  }

  // Re-stabilize after the scheme change, then snapshot.
  await waitForColorStable(() => readBgColor("#nav-bar"), 4000, 80);
  const reading = readAllSurfaces();

  try {
    // PRIMARY: every design — bars share one color.
    assertOneSurface(caseLabel, reading);

    // SECONDARY: tab tracks toolbar, only for color_like_toolbar designs.
    if (designTracksTab(design)) {
      assertTabTracksToolbar(caseLabel, reading);
    }
    return { label: caseLabel, ran: true, error: null };
  } catch (e) {
    return {
      label: caseLabel,
      ran: true,
      error: e instanceof Error ? e.message : String(e),
    };
  }
}

// ---------------------------------------------------------------------------
// Setup / teardown of the dark pref
// ---------------------------------------------------------------------------

function captureOriginalDarkPref(): void {
  try {
    if (Services.prefs.prefHasUserValue("ui.systemUsesDarkTheme")) {
      originalSystemUsesDarkTheme = Services.prefs.getIntPref(
        "ui.systemUsesDarkTheme",
      );
    }
  } catch {
    originalSystemUsesDarkTheme = -1;
  }
}

function restoreOriginalDarkPref(): void {
  try {
    if (originalSystemUsesDarkTheme >= 0) {
      Services.prefs.setIntPref(
        "ui.systemUsesDarkTheme",
        originalSystemUsesDarkTheme,
      );
    } else {
      Services.prefs.clearUserPref("ui.systemUsesDarkTheme");
    }
  } catch (e) {
    console.error("[color-consistency] failed to restore dark pref:", e);
  }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

async function testDesignMatrix(): Promise<void> {
  captureOriginalDarkPref();

  // Remember the design we started on so we can restore it at the end.
  const startDesign = getActiveDesignFromPref();

  const outcomes: CaseOutcome[] = [];
  let ran = 0;
  let skipped = 0;
  const failures: string[] = [];

  try {
    for (const design of DESIGNS) {
      for (const isDark of [false, true] as const) {
        const outcome = await runMatrixCase(design, isDark);
        outcomes.push(outcome);
        if (!outcome.ran) {
          skipped++;
        } else if (outcome.error) {
          failures.push(outcome.error);
        } else {
          ran++;
        }
      }
    }
  } finally {
    restoreOriginalDarkPref();
    if (startDesign) {
      await setActiveDesign(startDesign);
    }
  }

  // We must actually exercise at least the primary Lepton-family cases —
  // otherwise the test would silently pass when the environment skips them all
  // (e.g. the scheme pref has no effect anywhere). Require that at least the
  // regression-relevant lepton cases ran.
  const leptonRan = outcomes.some(
    (o) => o.label.startsWith("lepton/") && o.ran,
  );
  assert(
    leptonRan,
    `design matrix did not exercise any lepton cases (skipped ${skipped}/${outcomes.length}) — regression coverage lost; env cannot toggle ui.systemUsesDarkTheme`,
  );

  if (failures.length > 0) {
    throw new Error(
      `design × theme matrix failures (ran ${ran}, skipped ${skipped}): ${
        failures.join(" | ")
      }`,
    );
  }
}

/**
 * A fast structural sanity check that doesn't require switching anything —
 * the surfaces present under the currently active design should already share
 * one color. This catches the regression even in environments where the full
 * matrix cannot run (no scheme pref effect), and validates the user's actual
 * current configuration.
 */
async function testCurrentStateSharesOneSurface(): Promise<void> {
  await waitForColorStable(() => readBgColor("#nav-bar"), 4000, 80);
  const reading = readAllSurfaces();
  const active = getActiveDesignFromPref();

  const present = reading.surfaces.filter((s) => s.color !== null);
  if (present.length < 2) return; // not enough surfaces to compare — skip

  assertOneSurface(`current(${active ?? "?"})`, reading);
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "current state shares one surface color",
      fn: testCurrentStateSharesOneSurface,
    },
    { name: "design × theme matrix is color-consistent", fn: testDesignMatrix },
  ];
  await runTests("color-consistency.test.ts", tests);
}
