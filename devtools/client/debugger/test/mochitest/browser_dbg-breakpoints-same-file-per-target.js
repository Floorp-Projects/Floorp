/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests shows that breakpoints per url feature is broken in the debugger.
// Currently breakpoints are working per thread
// See Bug 1740202

"use strict";

const IS_BREAKPOINTS_PER_URL_SUPPORTED = false;

add_task(async function() {
  // This test fails with server side target switching disabled
  if (
    (!isFissionEnabled() || !isServerTargetSwitchingEnabled()) &&
    !isEveryFrameTargetEnabled()
  ) {
    return;
  }

  const dbg = await initDebugger("doc_dbg-same-source-distinct-threads.html");

  // Wait for all the sources to be available, so we make sure we have
  // the same-script.js files from all the threads.
  await waitUntil(() => dbg.selectors.getSourceCount() == 4);

  info("Add a breakpoint to the same-script.js from the iframe");
  const frameSource = findSourceInThread(
    dbg,
    "same-script.js",
    "Iframe test page"
  );
  await selectSource(dbg, frameSource);
  await addBreakpoint(dbg, frameSource, 5);

  info("Click in the page of the top-level document");
  BrowserTestUtils.synthesizeMouseAtCenter(
    "body",
    {},
    gBrowser.selectedBrowser
  );

  await waitForPaused(dbg);

  const mainThreadSource = findSourceInThread(
    dbg,
    "same-script.js",
    "Main Thread"
  );

  is(
    dbg.selectors.getSelectedSource().id,
    mainThreadSource.id,
    "The current selected source is the `same-script.js` from the main thread"
  );

  info("Assert that the breakpoint pauses on line 5");
  assertPausedAtSourceAndLine(dbg, mainThreadSource.id, 5);

  if (IS_BREAKPOINTS_PER_URL_SUPPORTED) {
    // This fails at the moment as there is no visible breakpoint on this line
    info("Assert that there is a breakpoint displayed on line 5");
    await assertBreakpoint(dbg, 5);
  }

  info("Check that only one breakpoint currently exists");
  is(dbg.selectors.getBreakpointCount(), 1, "One breakpoint exists");

  await resume(dbg);

  info(
    "Trigger the breakpoint in the worker thread to make sure it hit the breakpoint"
  );

  invokeInTab("postMessageToWorker");
  await waitForPaused(dbg);

  const workerSource = findSourceInThread(
    dbg,
    "same-script.js",
    EXAMPLE_URL + "same-script.js"
  );

  is(
    dbg.selectors.getSelectedSource().id,
    workerSource.id,
    "The current selected source is the `same-script.js` from the worker thread"
  );

  info("Assert that the breakpoint pauses on line 5");
  assertPausedAtSourceAndLine(dbg, workerSource.id, 5);

  if (IS_BREAKPOINTS_PER_URL_SUPPORTED) {
    // This fails at the moment as there is no visible breakpoint on this line
    info("Assert that there is a breakpoint dispalyed on line 5");
    await assertBreakpoint(dbg, 5);
  }

  info("Check that only one breakpoint still currently exists");
  is(dbg.selectors.getBreakpointCount(), 1, "One breakpoint exists");

  await resume(dbg);

  info("Trigger breakpoint in the iframe to make sure it hits the breakpoint");

  const iframeBrowsingContext = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    async () => {
      return content.document.querySelector("iframe").browsingContext;
    }
  );

  BrowserTestUtils.synthesizeMouseAtCenter("body", {}, iframeBrowsingContext);

  await waitForPaused(dbg);

  is(
    dbg.selectors.getSelectedSource().id,
    frameSource.id,
    "The current selected source is the `same-script.js` from the iframe"
  );

  info("Assert that the breakpoint pauses on line 5");
  assertPausedAtSourceAndLine(dbg, frameSource.id, 5);

  info("Assert that there is a breakpoint displayed on line 5");
  await assertBreakpoint(dbg, 5);

  await resume(dbg);
  assertNotPaused(dbg);

  await removeBreakpoint(dbg, frameSource.id, 5);

  await dbg.toolbox.closeToolbox();
});
