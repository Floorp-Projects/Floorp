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

import {
  GECKO_152_COLOR_FIX_CSS,
  LEPTON_COMPAT_152_CSS,
} from "../utils/lepton-compat-152.css.ts";
import { navBarBackgroundColorCSS } from "../utils/css.ts";
import {
  FLUERIAL_TAB_CORNER_CSS,
  TAB_COLOR_LIKE_TOOLBAR_CSS,
} from "../utils/tab-color-like-toolbar.css.ts";
import browserCSS from "../browser.css?inline";
import panelSidebarCSS from "../../panel-sidebar/components/style.css?inline";
import deleteBorderCSS from "../../ui-custom/styles/css/options/delete-border.css?inline";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

const SOURCES: ReadonlyArray<readonly [string, string]> = [
  ["GECKO_152_COLOR_FIX_CSS", GECKO_152_COLOR_FIX_CSS],
  ["LEPTON_COMPAT_152_CSS", LEPTON_COMPAT_152_CSS],
  ["navBarBackgroundColorCSS", navBarBackgroundColorCSS],
  ["TAB_COLOR_LIKE_TOOLBAR_CSS", TAB_COLOR_LIKE_TOOLBAR_CSS],
  ["FLUERIAL_TAB_CORNER_CSS", FLUERIAL_TAB_CORNER_CSS],
  ["designs/browser.css", browserCSS],
  ["panel-sidebar/components/style.css", panelSidebarCSS],
  ["ui-custom delete-border.css", deleteBorderCSS],
];

const BLOCK_AT_RULES = ["@media", "@supports", "@layer", "@container"];

function stripComments(css: string): string {
  return css.replace(/\/\*[\s\S]*?\*\//g, "");
}

/** Index of the `}` matching the `{` at `open`. */
function findBlockEnd(css: string, open: number): number {
  let depth = 0;
  for (let i = open; i < css.length; i++) {
    if (css[i] === "{") depth++;
    else if (css[i] === "}" && --depth === 0) return i;
  }
  return css.length;
}

/**
 * Style-rule selectors (top level and inside conditional/layer at-rules) that
 * mention the lightweight-theme state.
 */
function lwthemeSelectors(css: string): string[] {
  const out: string[] = [];
  const walk = (text: string): void => {
    let i = 0;
    while (i < text.length) {
      const open = text.indexOf("{", i);
      if (open === -1) return;
      const prelude = text.slice(i, open).replace(/^[\s;}]+/, "").trim();
      const close = findBlockEnd(text, open);
      if (prelude.startsWith("@")) {
        if (BLOCK_AT_RULES.some((name) => prelude.startsWith(name))) {
          walk(text.slice(open + 1, close));
        }
      } else if (prelude.includes("lwtheme")) {
        out.push(prelude.replace(/\s+/g, " "));
      }
      i = close + 1;
    }
  };
  walk(stripComments(css));
  return out;
}

/**
 * Every `:-moz-lwtheme` occurrence that is NOT inside a forgiving `:is()` /
 * `:where()` list, with a little surrounding context.
 */
function unforgivingMozLwthemeUses(css: string): string[] {
  const text = stripComments(css);
  const offenders: string[] = [];
  for (const match of text.matchAll(/:-moz-lwtheme(?![-\w])/g)) {
    const stack: string[] = [];
    for (let i = 0; i < (match.index ?? 0); i++) {
      if (text[i] === "(") {
        const name = /:([\w-]+)$/.exec(text.slice(Math.max(0, i - 32), i));
        stack.push(name ? name[1] : "(");
      } else if (text[i] === ")") {
        stack.pop();
      } else if (text[i] === "{" || text[i] === "}") {
        stack.length = 0;
      }
    }
    if (!stack.some((name) => name === "is" || name === "where")) {
      const at = match.index ?? 0;
      offenders.push(
        text.slice(Math.max(0, at - 50), at + 20).replace(/\s+/g, " "),
      );
    }
  }
  return offenders;
}

/** Whether Gecko keeps a rule with this selector (an invalid one is dropped). */
function selectorParses(selector: string): boolean {
  const style = document.createElement("style");
  style.textContent = `${selector} {}`;
  document.head.appendChild(style);
  try {
    return style.sheet?.cssRules.length === 1;
  } finally {
    style.remove();
  }
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
  assert(
    !selectorParses(":root:not([lwtheme]):not(:-moz-lwtheme)"),
    "Gecko 159 must reject :-moz-lwtheme inside :not() (the bug this guards)",
  );
}

function testNoUnforgivingMozLwtheme(): void {
  for (const [label, css] of SOURCES) {
    const offenders = unforgivingMozLwthemeUses(css);
    assertEquals(
      offenders.length,
      0,
      `${label} must not use :-moz-lwtheme outside :is()/:where() — the ` +
        `pseudo-class no longer exists and the whole rule is dropped; ` +
        `found: ${offenders.join(" | ")}`,
    );
  }
}

function testLwthemeSelectorsParse(): void {
  let checked = 0;
  for (const [label, css] of SOURCES) {
    for (const selector of lwthemeSelectors(css)) {
      checked++;
      assert(
        selectorParses(selector),
        `${label}: Gecko drops the rule with selector "${selector}"`,
      );
    }
  }
  assert(
    checked >= 10,
    `expected to check the theme-state rules; got ${checked}`,
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
    {
      name: "no :-moz-lwtheme outside forgiving :is()/:where()",
      fn: testNoUnforgivingMozLwtheme,
    },
    {
      name: "every theme-state selector parses in Gecko",
      fn: testLwthemeSelectorsParse,
    },
    {
      name: "no-theme dialog background pin applies",
      fn: testNoThemeDialogPinApplies,
    },
  ];
  await runTests("lwtheme-selectors.test.ts", tests);
}

await runAllTests();
