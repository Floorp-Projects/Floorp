/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function getScrollTop(dbg) {
  return getCM(dbg).doc.scrollTop;
}

async function waitForMatch(dbg, { matchIndex, count }) {
  await waitForState(
    dbg,
    state => {
      const result = dbg.selectors.getFileSearchResults(state);
      return result.matchIndex == matchIndex && result.count == count;
    },
    "wait for match"
  );
}

add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html");

  // Make sure that we can set a breakpoint on a line out of the
  // viewport, and that pausing there scrolls the editor to it.
  let longSrc = findSource(dbg, "long.js");
  await addBreakpoint(dbg, longSrc, 66);
  invokeInTab("testModel");
  await waitForPaused(dbg);

  const pauseScrollTop = getScrollTop(dbg);

  log("1. adding a breakpoint should not scroll the editor");
  getCM(dbg).scrollTo(0, 0);
  await addBreakpoint(dbg, longSrc, 11);
  is(getScrollTop(dbg), 0, "scroll position");

  log("2. searching should jump to the match");
  pressKey(dbg, "fileSearch");
  type(dbg, "check");
  await waitForMatch(dbg, { matchIndex: 0, count: 2 });
  const matchScrollTop = getScrollTop(dbg);
  ok(pauseScrollTop != matchScrollTop, "did not jump to debug line");
});
