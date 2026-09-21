// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import movePageInsideSearchbarCSS from "../styles/css/options/move_page_inside_searchbar.css?inline";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

function getBrowserContainerRule(): CSSStyleRule {
  const sheet = new CSSStyleSheet();
  sheet.replaceSync(movePageInsideSearchbarCSS);

  const rule = Array.from(sheet.cssRules).find((candidate) =>
    candidate instanceof CSSStyleRule &&
    candidate.selectorText === ".browserContainer"
  );

  assert(rule instanceof CSSStyleRule, "browser container rule should parse");
  return rule;
}

function getGridAreaRows(rule: CSSStyleRule): string[][] {
  const value = rule.style.getPropertyValue("grid-template-areas");
  const rows = Array.from(
    value.matchAll(/"([^"]+)"/g),
    (match) => match[1].trim().split(/\s+/),
  );

  assert(rows.length > 0, "grid-template-areas should contain named rows");
  return rows;
}

function testFindbarPrecedesBrowserContent(): void {
  const rows = getGridAreaRows(getBrowserContainerRule());

  assertEquals(
    rows.map((row) => row[2]).join(","),
    [
      "findbar",
      "notificationbox",
      "rdm-toolbar",
      "browserstack",
      "devtools-bottom-splitter",
      "devtools-bottom",
    ].join(","),
    "the central grid column should put the findbar above browser content",
  );
}

function testDevToolsAreasRemainIntact(): void {
  const rows = getGridAreaRows(getBrowserContainerRule());

  assert(
    rows.slice(0, 4).every((row) =>
      row.length === 5 &&
      row[0] === "devtools-side-start" &&
      row[1] === "devtools-side-start-splitter" &&
      row[3] === "devtools-side-end-splitter" &&
      row[4] === "devtools-side-end"
    ),
    "side-docked DevTools should continue to span every content row",
  );
  assert(
    rows[4].every((area) => area === "devtools-bottom-splitter"),
    "the bottom DevTools splitter should still span the full grid width",
  );
  assert(
    rows[5].every((area) => area === "devtools-bottom"),
    "bottom-docked DevTools should still span the full grid width",
  );
}

function testRdmAndContentRowSizingRemainIntact(): void {
  const value = getBrowserContainerRule().style.getPropertyValue(
    "grid-template-rows",
  );

  assert(
    value.includes("var(--rdm-toolbar-height, 0)"),
    "Responsive Design Mode should retain its collapsible toolbar row",
  );
  assert(
    value.includes("minmax(var(--content-area-min-size), 1fr)"),
    "the browser stack should retain the flexible content row",
  );
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "findbar precedes browser content in the Firefox 156 grid",
      fn: testFindbarPrecedesBrowserContent,
    },
    {
      name: "DevTools grid areas remain intact",
      fn: testDevToolsAreasRemainIntact,
    },
    {
      name: "RDM and browser content retain their row sizing",
      fn: testRdmAndContentRowSizingRemainIntact,
    },
  ];

  await runTests("movePageInsideSearchbar.test.ts", tests);
}
