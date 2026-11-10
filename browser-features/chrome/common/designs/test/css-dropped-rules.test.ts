// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
//
// Every rule in Floorp's chrome CSS must survive Gecko's CSS parser.
//
// When Gecko removes a pseudo-class, pseudo-element or at-rule, a rule that
// uses it outside a forgiving `:is()` / `:where()` list is dropped as a whole,
// silently (e.g. `:-moz-lwtheme` on Gecko 159, see lwtheme-selectors.test.ts).
// This test parses each source in the running browser and fails on any rule
// that does not reach the CSSOM, with file:line and the offending selector.
//
// Sources (css-sources.ts): all chrome CSS files, the vendored Lepton and
// fluerial skins, the string exports of `*.css.ts` modules and the CSS
// template literals of other TS/TSX files.
//
// Parse mode: a constructed CSSStyleSheet of the chrome window. Its URL is
// chrome://, so chrome-only syntax (@-moz-document, chrome-only
// pseudo-classes) is accepted, as it is for the chrome `<style>` sheets
// Floorp injects. The sheet is never adopted, so nothing is applied to the
// window. Sources Floorp also registers as AGENT sheets are re-parsed at
// that level (UA-only syntax is valid there); a rule counts as kept when
// either level keeps it.
//
// Rules that are already dead on the pinned Gecko are listed in
// KNOWN_DROPPED_RULES with a reason; only new ones fail.
//
// Not covered: `@media` / `@supports` conditions that parse but are always
// false (Lepton keeps `(-moz-bool-pref: …)` twins for older Firefox on
// purpose), invalid arguments silently removed from a forgiving `:is()`
// list, and invalid declarations.

import { collectCssSources, type CssSource } from "./css-sources.ts";
import {
  closeAgentParser,
  type DropReport,
  findDroppedRules,
  isStylesheet,
  type ParseMode,
  splitSelectorList,
  walkCss,
} from "./css-rules.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

/**
 * Sources that Floorp registers as AGENT sheets through nsIStyleSheetService
 * (designs/browser-design-element.tsx, registerContentStyleSheets).
 */
const AGENT_SHEET_SOURCES = new Set([
  "browser-features/skin/lepton/css/leptonContent.css",
  "browser-features/chrome/common/designs/utils/tab-color-like-toolbar.css.ts#TAB_COLOR_LIKE_TOOLBAR_CSS",
]);

/**
 * Baseline of rules Gecko 159 already drops, keyed by `label selector`.
 * Vendored Lepton is not edited here (update_lepton.yml re-syncs it); fix
 * upstream or in a follow-up and delete the entry.
 */
const KNOWN_DROPPED_RULES: Record<string, string> = {
  "browser-features/skin/lepton/css/leptonChrome.css .tab-label-container::beforee":
    "SUSPECT: Lepton typo for ::before; the tab label transition never applies",
  "browser-features/skin/lepton/css/leptonContent.css @-moz-document unobservable-document()":
    "SUSPECT: Gecko removed unobservable-document(); Lepton's opt-in PDF dark mode (userContent.page.dark_mode.pdf) is dead",
  "browser-features/skin/lepton/css/leptonContent.css ::-moz-focus-inner":
    "Gecko removed ::-moz-focus-inner along with the inner focus border it styled",
};

function modesFor(source: CssSource): ParseMode[] {
  return AGENT_SHEET_SOURCES.has(source.label)
    ? ["chrome", "agent"]
    : ["chrome"];
}

function describeDrops(source: CssSource, report: DropReport): string[] {
  return report.dropped.filter((d) =>
    !(`${source.label} ${d.prelude}` in KNOWN_DROPPED_RULES)
  ).map((d) => {
    const where = `${source.label}:${d.line + source.lineOffset}`;
    const what = d.kind === "style"
      ? `style rule "${d.prelude}"`
      : `@${d.prelude.replace(/^@/, "")}`;
    const offenders = d.offenders.length
      ? `; offending selector(s): ${
        d.offenders.map((o) => `"${o}"`).join(", ")
      }`
      : "";
    return `${where}: dropped ${what}${offenders}${
      d.detail ? ` (${d.detail})` : ""
    }`;
  });
}

function testWalkerSelfCheck(): void {
  const walked = walkCss(
    '@namespace xul url("x");\n' +
      "@media (x) {\n  @-moz-document url-prefix(chrome:) {\n" +
      "    :root[lwtheme] #a { color: red; }\n  }\n}\n" +
      "/* #c { } */\n" +
      '#b { content: "{"; color: red; &:is([lwtheme]) .c { color: blue; } }\n' +
      "@keyframes k { from { opacity: 0 } to { opacity: 1 } }",
  );
  assertEquals(
    walked.blocks.map((b) => `${b.kind}:${b.prelude}@${b.line}`).join(" ; "),
    "group:@media (x)@2 ; group:@-moz-document url-prefix(chrome:)@3 ; " +
      "style::root[lwtheme] #a@4 ; style:#b@8 ; " +
      "style:&:is([lwtheme]) .c@8 ; leaf:@keyframes k@9",
    "walker must find nested at-rules, nested style rules and their lines",
  );
  assertEquals(walked.statements.length, 1, "walker must see @namespace");
  assert(walked.balanced && walked.stray === "", "walker must balance");
  assert(isStylesheet("#a { color: red }"), "a rule is a stylesheet");
  assert(!isStylesheet("New Stack {n}"), "text is not a stylesheet");
  assert(!isStylesheet("#a { color: red;"), "a fragment is not a stylesheet");
  assertEquals(
    splitSelectorList(':is(a, b) c, d[x="1,2"], e').join(" | "),
    ':is(a, b) c | d[x="1,2"] | e',
    "selector lists split at top-level commas only",
  );
}

function testProbeSelfCheck(): void {
  const ok = findDroppedRules(
    '@namespace xul url("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul");\n' +
      "@-moz-document url-prefix(about:home) { body { color: red } }\n" +
      '@media (-moz-bool-pref: "x") { :root { color: red } }\n' +
      "xul|toolbar:-moz-window-inactive { color: red }\n" +
      "#a { color: red; @media (min-width: 1px) { color: blue } &:hover {} }\n" +
      ":root:is(:-moz-lwtheme, [lwtheme]) { color: red }\n" +
      "@keyframes k { from { opacity: 0 } to { opacity: 1 } }\n" +
      "@supports selector(:has(a)) { #b { color: red } }",
  );
  assertEquals(
    ok.dropped.length,
    0,
    `valid chrome CSS must not be reported: ${JSON.stringify(ok.dropped)}`,
  );
  assertEquals(ok.styleRules, 7, "every style rule must be probed");

  const bad = findDroppedRules(
    "#ok { color: red }\n" +
      ":root:not([lwtheme]):not(:-moz-lwtheme) { color: red }\n" +
      "#a, #b::-moz-no-such-thing, #c { color: red }\n" +
      "@media (x) { #d:-moz-gone { color: red } }\n" +
      "#e { &:-moz-gone2 .f { color: red } }\n" +
      "@-moz-no-such-rule { #g { color: red } }\n" +
      "@keyframes k { from { opacity: 0 } bogus { opacity: 1 } }",
  );
  assertEquals(
    bad.dropped.map((d) => `${d.line}:${d.prelude}`).join(" ; "),
    "2::root:not([lwtheme]):not(:-moz-lwtheme) ; " +
      "3:#a, #b::-moz-no-such-thing, #c ; 4:#d:-moz-gone ; " +
      "5:&:-moz-gone2 .f ; 6:@-moz-no-such-rule ; 7:@keyframes k",
    "Gecko must drop exactly the invalid rules",
  );
  assertEquals(
    bad.dropped[1].offenders.join(" | "),
    "#b::-moz-no-such-thing",
    "the offending selector of a list must be named",
  );
}

function testAgentModeSelfCheck(): void {
  const css = ":-moz-native-anonymous { color: red }";
  assertEquals(
    findDroppedRules(css, ["chrome"]).dropped.length,
    1,
    "a chrome-level sheet must reject the UA-only :-moz-native-anonymous",
  );
  assertEquals(
    findDroppedRules(css, ["chrome", "agent"]).dropped.length,
    0,
    "an AGENT sheet must accept :-moz-native-anonymous",
  );
  assertEquals(
    findDroppedRules(
      "@-moz-document unobservable-document() { #a { color: red } }",
      ["chrome", "agent"],
    ).dropped.length,
    1,
    "Gecko 159 has no unobservable-document(), not even in AGENT sheets",
  );
}

function testSourcesAreDiscovered(): void {
  const sources = collectCssSources(isStylesheet);
  const labels = new Set(sources.map((s) => s.label));
  const files = new Set(sources.map((s) => s.file));
  for (
    const label of [
      "browser-features/skin/lepton/css/leptonChrome.css",
      "browser-features/skin/lepton/css/leptonContent.css",
      "browser-features/skin/fluerial/css/fluerial.css",
      "browser-features/chrome/common/designs/browser.css",
      "browser-features/chrome/common/designs/utils/lepton-compat-152.css.ts#GECKO_152_COLOR_FIX_CSS",
      "browser-features/chrome/common/designs/utils/gecko-152-var-aliases.css.ts#GECKO_152_VAR_ALIASES_CSS",
      "browser-features/chrome/common/designs/utils/tab-color-like-toolbar.css.ts#TAB_COLOR_LIKE_TOOLBAR_CSS",
    ]
  ) {
    assert(labels.has(label), `CSS source must be discovered: ${label}`);
  }
  for (
    const file of [
      "browser-features/chrome/common/context-menu/style.ts",
      "browser-features/chrome/common/browser-tab-color/index.ts",
      "browser-features/chrome/common/tabbar/multirow-tabbar/multirow-tabbar.tsx",
    ]
  ) {
    assert(files.has(file), `CSS template literal must be found in ${file}`);
  }
}

function testNoRuleIsDropped(): void {
  const failures: string[] = [];
  const seen = new Set<string>();
  let styleRules = 0;
  let lwthemeRules = 0;
  for (const source of collectCssSources(isStylesheet)) {
    const report = findDroppedRules(source.text, modesFor(source));
    styleRules += report.styleRules;
    lwthemeRules += report.selectors.filter((s) => s.includes("lwtheme"))
      .length;
    failures.push(...describeDrops(source, report));
    for (const d of report.dropped) seen.add(`${source.label} ${d.prelude}`);
  }
  assertEquals(
    failures.length,
    0,
    `Gecko's CSS parser drops these rules:\n  ${failures.join("\n  ")}\n`,
  );
  const stale = Object.keys(KNOWN_DROPPED_RULES).filter((k) => !seen.has(k));
  if (stale.length) {
    console.warn(
      `[css-dropped-rules] KNOWN_DROPPED_RULES entries no longer needed: ${
        stale.join(" | ")
      }`,
    );
  }
  // leptonChrome.css alone has ~160 theme-state rules (lwtheme-selectors).
  assert(
    lwthemeRules >= 150,
    `expected the theme-state rules to be checked; got ${lwthemeRules}`,
  );
  // The sources held ~7.4k style rules in total on 2026-11-10.
  assert(
    styleRules > 6_000,
    `expected to check every rule; got ${styleRules}`,
  );
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "walker self-check", fn: testWalkerSelfCheck },
    { name: "probe self-check", fn: testProbeSelfCheck },
    { name: "AGENT-level parse self-check", fn: testAgentModeSelfCheck },
    { name: "sources are discovered", fn: testSourcesAreDiscovered },
    { name: "no rule is dropped by Gecko", fn: testNoRuleIsDropped },
  ];
  try {
    await runTests("css-dropped-rules.test.ts", tests);
  } finally {
    closeAgentParser();
  }
}

await runAllTests();
