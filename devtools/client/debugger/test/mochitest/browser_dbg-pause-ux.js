/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

add_task(async function () {
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

  const cm = getCM(dbg);
  await waitFor(
    () => cm.getSelection() == "check",
    "Wait for actual selection in CodeMirror"
  );
  is(
    cm.getCursor().line,
    26,
    "The line of first check occurence in long.js is selected (this is zero-based)"
  );
  // The column is the end of "check", so after 'k'
  is(
    cm.getCursor().ch,
    51,
    "The column of first check occurence in long.js is selected (this is zero-based)"
  );

  const matchScrollTop = getScrollTop(dbg);
  ok(pauseScrollTop != matchScrollTop, "did not jump to debug line");
});

function getScrollTop(dbg) {
  return getCM(dbg).doc.scrollTop;
}
