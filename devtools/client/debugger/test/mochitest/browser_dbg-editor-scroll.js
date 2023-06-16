/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that the editor keeps proper scroll position per document
// while also moving to the correct location upon pause/breakpoint selection

"use strict";

requestLongerTimeout(2);

add_task(async function () {
  // This test runs too slowly on linux debug. I'd like to figure out
  // which is the slowest part of this and make it run faster, but to
  // fix a frequent failure allow a longer timeout.
  const dbg = await initDebugger("doc-editor-scroll.html");

  // Set the initial breakpoint.
  await selectSource(dbg, "simple1.js");
  await addBreakpoint(dbg, "simple1.js", 26);

  const cm = getCM(dbg);

  info("Open long file, scroll down to line below the fold");
  await selectSource(dbg, "long.js");
  cm.scrollTo(0, 600);

  info("Ensure vertical scroll is the same after switching documents");
  await selectSource(dbg, "simple1.js");
  is(cm.getScrollInfo().top, 0);
  await selectSource(dbg, "long.js");
  is(cm.getScrollInfo().top, 600);

  info("Trigger a pause, click on a frame, ensure the right line is selected");
  invokeInTab("doNamedEval");
  await waitForPaused(dbg);
  findElement(dbg, "frame", 1).focus();
  await clickElement(dbg, "frame", 1);
  ok(cm.getScrollInfo().top != 0, "frame scrolled down to correct location");

  info("Navigating while paused, goes to the correct location");
  await selectSource(dbg, "long.js");
  is(cm.getScrollInfo().top, 600);

  info("Open new source, ensure it's at 0 scroll");
  await selectSource(dbg, "frames.js");
  is(cm.getScrollInfo().top, 0);
});
