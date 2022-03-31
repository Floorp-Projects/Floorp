/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// /!\ This test is currently skiped and fails in many ways /!\

// Tests that the source tree works with sources having query strings

"use strict";

add_task(async function() {
  const dbg = await initDebugger(
    "doc-sources-querystring.html",
    "simple1.js?x=1",
    "simple1.js?x=2"
  );

  // Expand nodes and make sure the two sources appear.
  await waitForSourcesInSourceTree(dbg, [], { noExpand: true });
  await clickElement(dbg, "sourceDirectoryLabel", 3);
  await waitForSourcesInSourceTree(dbg, ["simple1.js?x=1", "simple1.js?x=2"], {
    noExpand: true,
  });

  is(getLabel(dbg, 4), "simple1.js?x=1", "simple1.js?x=1 exists");
  is(getLabel(dbg, 5), "simple1.js?x=2", "simple1.js?x=2 exists");

  await selectSource(dbg, "simple1.js?x=1");
  const tab = findElement(dbg, "activeTab");
  is(tab.innerText, "simple1.js?x=1", "Tab label is simple1.js?x=1");
  await addBreakpoint(dbg, "simple1.js?x=1", 6);
  assertBreakpointHeading(dbg, "simple1.js?x=1", 0);

  // pretty print the source and check the tab text
  clickElement(dbg, "prettyPrintButton");
  await waitForSource(dbg, "simple1.js?x=1:formatted");

  const prettyTab = findElement(dbg, "activeTab");
  is(prettyTab.innerText, "simple1.js?x=1", "Tab label is simple1.js?x=1");
  // /!\ this test is skiped and this assertion fails:
  ok(prettyTab.querySelector("img.prettyPrint"));
  assertBreakpointHeading(dbg, "simple1.js?x=1", 0);

  // assert quick open works with queries
  pressKey(dbg, "quickOpen");
  type(dbg, "simple1.js?x");
  // /!\ this test is skiped and this assertion fails:
  ok(findElement(dbg, "resultItems")[0].innerText.includes("simple.js?x=1"));
});

function getLabel(dbg, index) {
  return findElement(dbg, "sourceNode", index)
    .textContent.trim()
    .replace(/^[\s\u200b]*/g, "");
}

function assertBreakpointHeading(dbg, label, index) {
  const breakpointHeading = findAllElements(dbg, "breakpointHeadings")[index]
    .innerText;
  is(breakpointHeading, label, `Breakpoint heading is ${label}`);
}
