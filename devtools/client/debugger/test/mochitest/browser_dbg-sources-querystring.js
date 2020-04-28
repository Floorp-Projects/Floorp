/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that the source tree works.

add_task(async function() {
  const dbg = await initDebugger("doc-sources-querystring.html", "simple1.js?x=1", "simple1.js?x=2");
  const {
    selectors: { getSelectedSource },
    getState
  } = dbg;

  // Expand nodes and make sure more sources appear.
  await assertSourceCount(dbg, 2);
  await clickElement(dbg, "sourceDirectoryLabel", 2);

  const labels = [getLabel(dbg, 4), getLabel(dbg, 3)];
  is(
    getLabel(dbg, 3),
    "simple1.js?x=1",
    "simple1.js?x=1 exists"
  );
  is(
    getLabel(dbg, 4),
    "simple1.js?x=2",
    "simple1.js?x=2 exists"
  );

  const source = findSource(dbg, "simple1.js?x=1");
  await selectSource(dbg, source);
  const tab = findElement(dbg, "activeTab");
  is(tab.innerText, "simple1.js?x=1", "Tab label is simple1.js?x=1");
  await addBreakpoint(dbg, "simple1.js?x=1", 6);
  assertBreakpointHeading(dbg, "simple1.js?x=1", 2);

  // pretty print the source and check the tab text
  clickElement(dbg, "prettyPrintButton");
  await waitForSource(dbg, "simple1.js?x=1:formatted");

  const prettyTab = findElement(dbg, "activeTab");
  is(prettyTab.innerText, "simple1.js?x=1", "Tab label is simple1.js?x=1");
  ok(prettyTab.querySelector("img.prettyPrint"));
  assertBreakpointHeading(dbg, "simple1.js?x=1", 2);

  // assert quick open works with queries
  pressKey(dbg, "quickOpen");
  type(dbg, "simple1.js?x");
  ok(findElement(dbg, "resultItems")[0].innerText.includes("simple.js?x=1"));
});

function getLabel(dbg, index) {
  return findElement(dbg, "sourceNode", index)
    .textContent.trim()
    .replace(/^[\s\u200b]*/g, "");
}

function assertBreakpointHeading(dbg, label, index) {
  const breakpointHeading = findElement(dbg, "breakpointItem", index).innerText;
  is(breakpointHeading, label, `Breakpoint heading is ${label}`);
}
