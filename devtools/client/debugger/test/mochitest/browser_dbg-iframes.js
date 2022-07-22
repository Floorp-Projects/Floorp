/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test is taking too much time to complete on some hardware since
// release at https://bugzilla.mozilla.org/show_bug.cgi?id=1423158

"use strict";

requestLongerTimeout(3);

/**
 * Test debugging a page with iframes
 *  1. pause in the main thread
 *  2. pause in the iframe
 */
add_task(async function() {
  const dbg = await initDebugger("doc-iframes.html");

  // test pausing in the main thread
  await reload(dbg);
  await waitForPaused(dbg);
  await waitForLoadedSource(dbg, "doc-iframes.html");
  assertPausedAtSourceAndLine(dbg, findSource(dbg, "doc-iframes.html").id, 11);

  // test pausing in the iframe
  await resume(dbg);
  await waitForPaused(dbg);
  await waitForLoadedSource(dbg, "doc-debugger-statements.html");
  assertPausedAtSourceAndLine(
    dbg,
    findSource(dbg, "doc-debugger-statements.html").id,
    11
  );

  // test pausing in the iframe
  await resume(dbg);
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(
    dbg,
    findSource(dbg, "doc-debugger-statements.html").id,
    16
  );
  await waitFor(() => dbg.toolbox.isHighlighted("jsdebugger"));
  ok(true, "Debugger is highlighted when paused");

  if (isFissionEnabled() || isEveryFrameTargetEnabled()) {
    info("Remove the iframe and wait for resume");
    await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
      const iframe = content.document.querySelector("iframe");
      iframe.remove();
    });
    await waitForResumed(dbg);
    await waitFor(() => !dbg.toolbox.isHighlighted("jsdebugger"));
    ok(true, "Debugger is no longer highlighted when resumed");
  }
});
