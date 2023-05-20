/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests shows that breakpoints per url feature works in the debugger.

"use strict";

add_task(async function () {
  // This test fails with server side target switching disabled
  if (!isFissionEnabled() && !isEveryFrameTargetEnabled()) {
    return;
  }

  const dbg = await initDebugger(
    "doc_dbg-same-source-distinct-threads.html",
    "same-script.js"
  );

  // There is one source but 3 source actors. One per same-script.js instance,
  // one <script> tag, one worker and one <script> tag in the iframe.
  const source = findSource(dbg, "same-script.js");
  await waitUntil(
    () => dbg.selectors.getSourceActorsForSource(source.id).length == 3
  );

  info("Add a breakpoint to the same-script.js from the iframe");
  await selectSource(dbg, source);
  await addBreakpoint(dbg, source, 5);

  info("Click in the page of the top-level document");
  BrowserTestUtils.synthesizeMouseAtCenter(
    "body",
    {},
    gBrowser.selectedBrowser
  );

  await waitForPaused(dbg);

  is(
    dbg.selectors.getSelectedSource().id,
    source.id,
    "The current selected source is the `same-script.js` from the main thread"
  );

  info("Assert that the breakpoint pauses on line 5");
  assertPausedAtSourceAndLine(dbg, source.id, 5);

  // This fails at the moment as there is no visible breakpoint on this line
  info("Assert that there is a breakpoint displayed on line 5");
  await assertBreakpoint(dbg, 5);

  info("Check that only one breakpoint currently exists");
  is(dbg.selectors.getBreakpointCount(), 1, "One breakpoint exists");

  await resume(dbg);

  info(
    "Trigger the breakpoint in the worker thread to make sure it hit the breakpoint"
  );

  invokeInTab("postMessageToWorker");
  await waitForPaused(dbg);

  is(
    dbg.selectors.getSelectedSource().id,
    source.id,
    "The current selected source is the `same-script.js` from the worker thread"
  );

  info("Assert that the breakpoint pauses on line 5");
  assertPausedAtSourceAndLine(dbg, source.id, 5);

  info("Assert that there is a breakpoint dispalyed on line 5");
  await assertBreakpoint(dbg, 5);

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
    source.id,
    "The current selected source is the `same-script.js` from the iframe"
  );

  info("Assert that the breakpoint pauses on line 5");
  assertPausedAtSourceAndLine(dbg, source.id, 5);

  info("Assert that there is a breakpoint displayed on line 5");
  await assertBreakpoint(dbg, 5);

  await resume(dbg);
  assertNotPaused(dbg);

  await removeBreakpoint(dbg, source.id, 5);

  await dbg.toolbox.closeToolbox();
});
