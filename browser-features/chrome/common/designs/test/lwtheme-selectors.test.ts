// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
//
// Regression tests for chrome CSS that branches on the lightweight-theme
// state.
//
// Gecko 159 no longer has the `:-moz-lwtheme` pseudo-class; the `[lwtheme]`
// attribute on the root is the only signal. An unknown pseudo-class makes the
// whole selector invalid unless it sits in a forgiving selector list
// (`:is()` / `:where()`). `:not()` is not forgiving, so a selector such as
// `:root:not([lwtheme]):not(:-moz-lwtheme)` drops the entire rule, and so does
// a bare `#nav-bar:-moz-lwtheme`. Several no-theme rules were silently dead on
// Floorp 12.21.1 for this reason.
//
// The sources come from css-sources.ts (all chrome CSS/TS outside test
// directories, plus the vendored skins), so new files and a re-synced
// leptonChrome.css are covered without editing this list. That every
// theme-state rule actually parses is checked, together with every other
// rule, by css-dropped-rules.test.ts.

import { GECKO_152_COLOR_FIX_CSS } from "../utils/lepton-compat-152.css.ts";
import { blankCssComments, findKey, RAW_CSS, RAW_TS } from "./css-sources.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

/** Block comments and `//` line comments removed (not `chrome://` URLs). */
function stripTsComments(text: string): string {
  return blankCssComments(text).replace(/(^|[^:"'`\\])\/\/[^\n]*/g, "$1");
}

/**
 * Every `:-moz-lwtheme` occurrence that is NOT inside a forgiving `:is()` /
 * `:where()` list, with a little surrounding context. Single pass, so it is
 * cheap enough for the vendored leptonChrome.css.
 */
function unforgivingMozLwthemeUses(text: string): string[] {
  const offenders: string[] = [];
  const stack: string[] = [];
  const re = /:([\w-]+)\(|[(){}]|:-moz-lwtheme(?![-\w(])/g;
  for (const m of text.matchAll(re)) {
    const token = m[0];
    if (token.startsWith(":-moz-lwtheme")) {
      if (!stack.some((name) => name === "is" || name === "where")) {
        const at = m.index ?? 0;
        offenders.push(
          text.slice(Math.max(0, at - 50), at + 20).replace(/\s+/g, " "),
        );
      }
    } else if (m[1] !== undefined) {
      stack.push(m[1]);
    } else if (token === "(") {
      stack.push("(");
    } else if (token === ")") {
      stack.pop();
    } else {
      stack.length = 0;
    }
  }
  return offenders;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

function testScannerSelfCheck(): void {
  assertEquals(
    unforgivingMozLwthemeUses(
      ":root:not([lwtheme]):not(:-moz-lwtheme) {} #a:-moz-lwtheme {}",
    ).length,
    2,
    "scanner must flag :-moz-lwtheme inside :not() and bare uses",
  );
  assertEquals(
    unforgivingMozLwthemeUses(
      ":root:is(:-moz-lwtheme, [lwtheme]) {} " +
        ":root:is(:not([lwtheme]), :not(:-moz-lwtheme)) {} " +
        ":where(:-moz-lwtheme) {}",
    ).length,
    0,
    "scanner must accept :-moz-lwtheme inside forgiving :is()/:where()",
  );
}

function testSourcesAreDiscovered(): void {
  findKey(RAW_CSS, "skin/lepton/css/leptonChrome.css");
  findKey(RAW_CSS, "common/designs/browser.css");
  findKey(RAW_CSS, "ui-custom/styles/css/options/delete-border.css");
  findKey(RAW_TS, "common/browser-tab-color/index.ts");
  findKey(RAW_TS, "common/designs/utils/lepton-compat-152.css.ts");
  findKey(RAW_TS, "common/designs/utils/tab-color-like-toolbar.css.ts");
}

function testNoUnforgivingMozLwtheme(): void {
  const offenders: string[] = [];
  for (const [path, css] of Object.entries(RAW_CSS)) {
    for (const o of unforgivingMozLwthemeUses(blankCssComments(css))) {
      offenders.push(`${path}: ${o}`);
    }
  }
  for (const [path, ts] of Object.entries(RAW_TS)) {
    if (!ts.includes(":-moz-lwtheme")) continue;
    for (const o of unforgivingMozLwthemeUses(stripTsComments(ts))) {
      offenders.push(`${path}: ${o}`);
    }
  }
  assertEquals(
    offenders.length,
    0,
    `:-moz-lwtheme must only appear inside :is()/:where() — the ` +
      `pseudo-class no longer exists and the whole rule is dropped; found: ` +
      offenders.join(" | "),
  );
}

/**
 * The no-theme dialog background pin must actually apply when no LWT is
 * loaded. Before the fix its selector was invalid, so this value never
 * reached dialogs.
 */
function testNoThemeDialogPinApplies(): void {
  const root = document.documentElement;
  const hadLwtheme = root.getAttribute("lwtheme");
  const style = document.createElement("style");
  const probe = document.createElement("dialog");
  root.removeAttribute("lwtheme");
  style.textContent = GECKO_152_COLOR_FIX_CSS;
  document.head.appendChild(style);
  root.appendChild(probe);
  try {
    const computed = globalThis.getComputedStyle(probe);
    assert(computed, "computed style for the dialog probe must exist");
    const value = computed.getPropertyValue("--in-content-page-background")
      .trim();
    const dark =
      globalThis.matchMedia("(prefers-color-scheme: dark)")?.matches === true;
    assertEquals(
      value,
      dark ? "rgb(31, 30, 38)" : "rgb(255, 255, 255)",
      "the no-theme --in-content-page-background pin must apply to dialogs",
    );
  } finally {
    probe.remove();
    style.remove();
    if (hadLwtheme !== null) root.setAttribute("lwtheme", hadLwtheme);
  }
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "scanner self-check", fn: testScannerSelfCheck },
    { name: "sources are discovered by glob", fn: testSourcesAreDiscovered },
    {
      name: "no :-moz-lwtheme outside forgiving :is()/:where()",
      fn: testNoUnforgivingMozLwtheme,
    },
    {
      name: "no-theme dialog background pin applies",
      fn: testNoThemeDialogPinApplies,
    },
  ];
  await runTests("lwtheme-selectors.test.ts", tests);
}

await runAllTests();
