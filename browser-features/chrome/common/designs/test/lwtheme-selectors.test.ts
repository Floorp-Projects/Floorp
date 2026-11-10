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
// The sources are discovered with import.meta.glob (all chrome CSS/TS outside
// test directories, plus the vendored skins) so new files and a re-synced
// leptonChrome.css are covered without editing this list.

import { GECKO_152_COLOR_FIX_CSS } from "../utils/lepton-compat-152.css.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

const RAW_CSS: Record<string, string> = import.meta.glob<string>(
  [
    "../../../**/*.css",
    "@nora/skin/**/*.css",
    "!**/test/**",
    "!**/node_modules/**",
  ],
  { query: "?raw", import: "default", eager: true },
);
const RAW_TS: Record<string, string> = import.meta.glob<string>(
  [
    "../../../**/*.{ts,tsx,mts}",
    "!**/test/**",
    "!**/*.test.ts",
    "!**/node_modules/**",
  ],
  { query: "?raw", import: "default", eager: true },
);

type Found = { ancestors: string[]; selector: string };

function stripCssComments(text: string): string {
  return text.replace(/\/\*[\s\S]*?\*\//g, "");
}

/** Strip block comments and `//` line comments (but not `chrome://` URLs). */
function stripTsComments(text: string): string {
  return stripCssComments(text).replace(/(^|[^:"'`\\])\/\/[^\n]*/g, "$1");
}

/** Backtick template literals of a TS source, `${…}` replaced by `0`. */
function templateLiterals(ts: string): string[] {
  const out: string[] = [];
  for (const m of ts.matchAll(/`((?:\\[\s\S]|\$\{[^}]*\}|[^`\\])*)`/g)) {
    out.push(m[1].replace(/\$\{[^}]*\}/g, "0"));
  }
  return out;
}

/** Index of the `}` matching the `{` at `open` (quote aware). */
function findBlockEnd(css: string, open: number): number {
  let depth = 0;
  let quote = "";
  for (let i = open; i < css.length; i++) {
    const c = css[i];
    if (quote) {
      if (c === "\\") i++;
      else if (c === quote) quote = "";
    } else if (c === '"' || c === "'") {
      quote = c;
    } else if (c === "{") {
      depth++;
    } else if (c === "}" && --depth === 0) {
      return i;
    }
  }
  return css.length;
}

/**
 * Every style-rule selector that mentions the lightweight-theme state,
 * including rules nested in at-rules (@media, @supports, @layer,
 * @-moz-document, …) and nested style rules. `ancestors` holds the enclosing
 * style-rule selectors needed to re-parse a nested rule.
 */
function lwthemeSelectors(css: string): Found[] {
  const out: Found[] = [];
  const walk = (text: string, ancestors: string[]): void => {
    let segStart = 0;
    let parens = 0;
    let quote = "";
    for (let i = 0; i < text.length; i++) {
      const c = text[i];
      if (quote) {
        if (c === "\\") i++;
        else if (c === quote) quote = "";
        continue;
      }
      if (c === '"' || c === "'") quote = c;
      else if (c === "(" || c === "[") parens++;
      else if (c === ")" || c === "]") parens--;
      else if (parens === 0 && c === ";") segStart = i + 1;
      else if (parens === 0 && c === "{") {
        const prelude = text.slice(segStart, i).replace(/\s+/g, " ").trim();
        const close = findBlockEnd(text, i);
        const body = text.slice(i + 1, close);
        if (prelude.startsWith("@")) {
          walk(body, ancestors);
        } else {
          if (prelude.includes("lwtheme")) {
            out.push({ ancestors, selector: prelude });
          }
          walk(body, [...ancestors, prelude]);
        }
        i = close;
        segStart = close + 1;
      }
    }
  };
  walk(stripCssComments(css), []);
  return out;
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

/**
 * Parse every found selector (re-nested under its ancestors) in a
 * system-principal <style> and return the ones Gecko drops.
 */
function droppedSelectors(found: Found[]): Found[] {
  if (found.length === 0) return [];
  const style = document.createElement("style");
  style.textContent = found
    .map(({ ancestors, selector }, i) =>
      ancestors.map((a) => `${a} {`).join("") +
      `${selector} { --lwprobe-${i}: 1; }` +
      "}".repeat(ancestors.length)
    )
    .join("\n");
  document.head.appendChild(style);
  try {
    const alive = new Set<number>();
    const walk = (rules: CSSRuleList): void => {
      for (const rule of Array.from(rules)) {
        if (rule instanceof CSSStyleRule) {
          for (const prop of Array.from(rule.style)) {
            if (prop.startsWith("--lwprobe-")) {
              alive.add(Number(prop.slice(10)));
            }
          }
        }
        const nested = (rule as CSSRule & { cssRules?: CSSRuleList }).cssRules;
        if (nested) walk(nested);
      }
    };
    const sheet = style.sheet;
    assert(sheet, "probe stylesheet must exist");
    walk(sheet.cssRules);
    return found.filter((_, i) => !alive.has(i));
  } finally {
    style.remove();
  }
}

/**
 * Glob keys come back either relative to this file (`../utils/x.ts`) or
 * root-absolute (`/link-features-chrome/common/…`, `@nora/skin` files).
 * Normalize them to a path under browser-features/chrome (or skin) so they
 * can be matched by suffix.
 */
function normalizeKey(key: string): string {
  if (!key.startsWith(".")) return key;
  const parts = ["common", "designs", "test"];
  for (const segment of key.split("/")) {
    if (segment === "..") parts.pop();
    else if (segment !== ".") parts.push(segment);
  }
  return `/${parts.join("/")}`;
}

function findKey(record: Record<string, string>, suffix: string): string {
  const key = Object.keys(record).find((k) =>
    normalizeKey(k).endsWith(`/${suffix}`)
  );
  assert(key, `glob must include ${suffix}`);
  return key;
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
  const nested = lwthemeSelectors(
    "@media (x) { @-moz-document url-prefix(chrome:) { " +
      ":root[lwtheme] #a { color: red; } } }\n" +
      "#b { color: red; &:is([lwtheme]) .c { color: blue; } }",
  );
  assertEquals(
    nested.map((f) => f.selector).join(" ; "),
    ":root[lwtheme] #a ; &:is([lwtheme]) .c",
    "selector walker must descend into at-rules and nested style rules",
  );
  assertEquals(
    droppedSelectors([
      { ancestors: [], selector: ":root:not([lwtheme]):not(:-moz-lwtheme)" },
      { ancestors: [], selector: ":root:not([lwtheme])" },
      { ancestors: ["#b"], selector: "&:is([lwtheme]) .c" },
    ]).map((f) => f.selector).join(" ; "),
    ":root:not([lwtheme]):not(:-moz-lwtheme)",
    "Gecko 159 must reject only :-moz-lwtheme inside :not()",
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
    for (const o of unforgivingMozLwthemeUses(stripCssComments(css))) {
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

function testLwthemeSelectorsParse(): void {
  let checked = 0;
  const failures: string[] = [];
  const check = (label: string, css: string): void => {
    const found = lwthemeSelectors(css);
    checked += found.length;
    for (const f of droppedSelectors(found)) {
      failures.push(`${label}: "${f.selector}"`);
    }
  };
  for (const [path, css] of Object.entries(RAW_CSS)) {
    if (css.includes("lwtheme")) check(path, css);
  }
  for (const [path, ts] of Object.entries(RAW_TS)) {
    if (!ts.includes("lwtheme")) continue;
    for (const literal of templateLiterals(stripTsComments(ts))) {
      if (literal.includes("lwtheme")) check(path, literal);
    }
  }
  assertEquals(
    failures.length,
    0,
    `Gecko drops these theme-state rules: ${failures.join(" | ")}`,
  );
  // leptonChrome.css alone has ~160 theme-state rules.
  assert(
    checked >= 150,
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
    { name: "sources are discovered by glob", fn: testSourcesAreDiscovered },
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
