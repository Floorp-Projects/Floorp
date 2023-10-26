/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// This tests makes sure that on reload of a remote iframe, there
// are no duplicated breakpoints. The test case relates to a specific
// issue in Bug 1728587
"use strict";

add_task(async function () {
  // With fission and EFT disabled all the sources are going to be under the Main Thread
  // and this sceanrio becomes irrelevant
  if (!isFissionEnabled() && !isEveryFrameTargetEnabled()) {
    return;
  }

  const dbg = await initDebugger(
    "doc_dbg-fission-frame-sources.html",
    "simple2.js"
  );

  info(
    "Open a tab for a source in the top document to assert it doesn't disappear"
  );
  await selectSource(dbg, "simple1.js");

  info("Add breakpoint to the source (simple2.js) in the remote frame");
  await selectSource(dbg, "simple2.js");
  await addBreakpoint(dbg, "simple2.js", 3);

  is(dbg.selectors.getBreakpointCount(), 1, "Only one breakpoint exists");

  const source = findSource(dbg, "simple2.js");
  assertBreakpointsList(dbg, source);

  is(
    countTabs(dbg),
    2,
    "We see the tabs for the sources of both top and iframe documents"
  );

  const onBreakpointSet = waitForDispatch(dbg.store, "SET_BREAKPOINT");

  info("reload the iframe");
  const iframeBrowsingContext = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    function () {
      const iframe = content.document.querySelector("iframe");
      return iframe.browsingContext;
    }
  );
  await SpecialPowers.spawn(iframeBrowsingContext, [], () => {
    content.location.reload();
  });

  await onBreakpointSet;

  await waitForSelectedSource(dbg, "simple2.js");

  is(dbg.selectors.getBreakpointCount(), 1, "Only one breakpoint still exists");

  const sourceAfterReload = findSource(dbg, "simple2.js");
  assertBreakpointsList(dbg, sourceAfterReload);

  is(countTabs(dbg), 2, "We still see the tabs for the two sources");

  await removeBreakpoint(dbg, sourceAfterReload.id, 3);
});

function assertBreakpointsList(dbg, source) {
  const breakpointHeadings = findAllElements(dbg, "breakpointHeadings");
  const breakpointItems = findAllElements(dbg, "breakpointItems");

  is(
    breakpointHeadings.length,
    1,
    "The breakpoint list show one breakpoint source"
  );
  is(
    breakpointItems.length,
    1,
    "The breakpoint list shows only one breakpoint"
  );

  is(
    breakpointHeadings[0].title,
    source.url,
    "The info displayed for the breakpoint tooltip of the 1st breakpoint is correct"
  );
  is(
    breakpointHeadings[0].textContent,
    "simple2.js",
    "The info displayed for the breakpoint heading of the 1st breakpoint is correct"
  );
  is(
    breakpointItems[0].textContent,
    "return x + y;3:5",
    "The info displayed for the 1st breakpoint is correct"
  );
}
