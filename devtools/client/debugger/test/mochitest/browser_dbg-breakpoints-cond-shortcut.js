/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test opening conditional panel using keyboard shortcut.
// Should access the closest breakpoint to a passed in cursorPosition.

add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html", "long");

  let cursorPosition = { line: undefined, column: undefined };

  await selectSource(dbg, "long");
  await waitForSelectedSource(dbg, "long");

  info("toggle conditional panel with shortcut: no breakpoints, default cursorPosition");
  pressKey(dbg, "toggleCondPanel");
  await waitForConditionalPanelFocus(dbg);
  ok(
    !!getConditionalPanel(dbg, 1),
    "conditional panel panel is open on line 1"
  );
  is(
    dbg.selectors.getConditionalPanelLocation().line,
    1,
    "conditional panel location is line 1"
  );
  info("close conditional panel");
  pressKey(dbg, "Escape");

  info("toggle conditional panel with shortcut: cursor on line 32, no breakpoints");
  // codemirror editor offset: cursorPosition will be line + 1, column + 1
  getCM(dbg).setCursor({ line: 31, ch: 1 });
  pressKey(dbg, "toggleCondPanel");

  await waitForConditionalPanelFocus(dbg);
  ok(
    !!getConditionalPanel(dbg, 32),
    "conditional panel panel is open on line 32"
  );
  is(
    dbg.selectors.getConditionalPanelLocation().line,
    32,
    "conditional panel location is line 32"
  );
  info("close conditional panel");
  pressKey(dbg, "Escape");

  info("add active column breakpoint on line 32 and set cursorPosition")
  await enableFirstBreakpoint(dbg);
  getCM(dbg).setCursor({ line: 31, ch: 1 });
  info("toggle conditional panel with shortcut and add condition to first breakpoint");
  setConditionalBreakpoint(dbg, "1");
  await waitForCondition(dbg, 1);
  const firstBreakpoint = findColumnBreakpoint(dbg, "long", 32, 2);
  is(firstBreakpoint.options.condition, "1", "first breakpoint created with condition using shortcut");

  info("set cursor at second breakpoint position and activate breakpoint");
  getCM(dbg).setCursor({ line: 31, ch: 25 });

  await enableSecondBreakpoint(dbg);
  info("toggle conditional panel with shortcut and add condition to second breakpoint");
  setConditionalBreakpoint(dbg, "2");
  await waitForCondition(dbg, 2);
  const secondBreakpoint = findColumnBreakpoint(dbg, "long", 32, 26);
  is(secondBreakpoint.options.condition, "2", "second breakpoint created with condition using shortcut");

  info("set cursor position near first breakpoint, toggle conditional panel and edit breakpoint");
  getCM(dbg).setCursor({ line: 31, ch: 7 });
  info("toggle conditional panel and edit condition using shortcut");
  setConditionalBreakpoint(dbg, "2");
  ok(
    !! waitForCondition(dbg, "12"),
    "breakpoint closest to cursor position has been edited"
  );

  info("close conditional panel");
  pressKey(dbg, "Escape");

  info("set cursor position near second breakpoint, toggle conditional panel and edit breakpoint");
  getCM(dbg).setCursor({ line: 31, ch: 21 });
  info("toggle conditional panel and edit condition using shortcut");
  setConditionalBreakpoint(dbg, "3");
  ok(
    !! waitForCondition(dbg, "13"),
    "breakpoint closest to cursor position has been edited"
  );
});

// from test/mochitest/browser_dbg-breakpoints-cond-source-maps.js
function getConditionalPanel(dbg, line) {
  return getCM(dbg).doc.getLineHandle(line - 1).widgets[0];
}

// from devtools browser_dbg-breakpoints-cond-source-maps.js
async function waitForConditionalPanelFocus(dbg) {
  await waitFor(() => dbg.win.document.activeElement.tagName === "TEXTAREA");
}

// from browser_dbg-breakpoints-columns.js
async function enableFirstBreakpoint(dbg) {
  getCM(dbg).setCursor({ line: 32, ch: 0 });
  await addBreakpoint(dbg, "long", 32);
  const bpMarkers = await waitForAllElements(dbg, "columnBreakpoints");

  ok(bpMarkers.length === 2, "2 column breakpoints");
  assertClass(bpMarkers[0], "active");
  assertClass(bpMarkers[1], "active", false);
}

async function enableSecondBreakpoint(dbg) {
  let bpMarkers = await waitForAllElements(dbg, "columnBreakpoints");

  bpMarkers[1].click();
  await waitForBreakpointCount(dbg, 2);

  bpMarkers = findAllElements(dbg, "columnBreakpoints");
  assertClass(bpMarkers[1], "active");
  await waitForAllElements(dbg, "breakpointItems", 2);
}

// modified method from browser_dbg-breakpoints-columns.js
// use shortcut to open conditional panel.
function setConditionalBreakpoint(dbg, condition) {
  pressKey(dbg, "toggleCondPanel");
  typeInPanel(dbg, condition);
}
