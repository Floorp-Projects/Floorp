/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

function getScrollTop(dbg) {
  return getCM(dbg).doc.scrollTop;
}

async function waitForMatch(dbg, { matchIndex, count }) {
  await waitForState(
    dbg,
    state => {
      const result = dbg.selectors.getFileSearchResults();
      return result.matchIndex == matchIndex && result.count == count;
    },
    "wait for match"
  );
}

add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html");

  // Make sure that we can set a breakpoint on a line out of the
  // viewport, and that pausing there scrolls the editor to it.
  const longSrc = findSource(dbg, "long.js");
  await selectSource(dbg, "long.js");
  await addBreakpoint(dbg, longSrc, 66);
  invokeInTab("testModel");
  await waitForPaused(dbg, "long.js");

  const pauseScrollTop = getScrollTop(dbg);

  info("1. adding a breakpoint should not scroll the editor");
  getCM(dbg).scrollTo(0, 0);
  await addBreakpoint(dbg, longSrc, 11);
  is(getScrollTop(dbg), 0, "scroll position");

  info("2. searching should jump to the match");
  pressKey(dbg, "fileSearch");
  type(dbg, "check");
  await waitForMatch(dbg, { matchIndex: 0, count: 2 });
  const matchScrollTop = getScrollTop(dbg);
  ok(pauseScrollTop != matchScrollTop, "did not jump to debug line");
});
