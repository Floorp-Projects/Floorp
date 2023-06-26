/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test that the debugger pauses and is automatically highlighted and selected,
// even when it hasn't been opened.

"use strict";

const IFRAME_TEST_COM_URI = `https://example.com/document-builder.sjs?html=${encodeURI(
  `<script>const a=2;\ndebugger;\nconsole.log(a);</script>`
)}`;

// Embed the example.com test page in an example.org iframe.
const IFRAME_TEST_URI = `https://example.org/document-builder.sjs?html=${encodeURI(
  `<script>function breakDebugger() {const b=3;\ndebugger;\nconsole.log(b);}</script><iframe src="${IFRAME_TEST_COM_URI}"></iframe><body>`
)}`;

add_task(async function () {
  info("Test a debugger statement from the top level document");

  // Make sure the toolbox opens with the webconsole initially selected.
  const toolbox = await initPane("doc-debugger-statements.html", "webconsole");

  info("Execute a debugger statement");
  const pausedRun = SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    function () {
      content.wrappedJSObject.test();
    }
  );

  const dbg = await assertDebuggerIsHighlightedAndPaused(toolbox);
  const source = findSource(dbg, "doc-debugger-statements.html");
  assertPausedAtSourceAndLine(dbg, source.id, 16);

  await resume(dbg);
  info("Wait for the paused code to complete after resume");
  await pausedRun;

  ok(
    !toolbox.isHighlighted("jsdebugger"),
    "Debugger is no longer highlighted after resume"
  );
});

add_task(async function () {
  info("Test a debugger statement from an iframe");

  // Make sure the toolbox opens with the webconsole initially selected.
  const toolbox = await openNewTabAndToolbox(IFRAME_TEST_URI, "webconsole");

  info(
    "Reload the test page, which will trigger the debugger statement in the iframe"
  );
  const pausedReload = reloadBrowser();

  const dbg = await assertDebuggerIsHighlightedAndPaused(toolbox);
  const source = findSource(dbg, IFRAME_TEST_COM_URI);
  assertPausedAtSourceAndLine(dbg, source.id, 2);

  await resume(dbg);
  info("Wait for the paused code to complete after resume");
  await pausedReload;

  ok(
    !toolbox.isHighlighted("jsdebugger"),
    "Debugger is no longer highlighted after resume"
  );
});

add_task(async function () {
  info("Test pausing from two distinct targets");

  // Make sure the toolbox opens with the webconsole initially selected.
  const toolbox = await openNewTabAndToolbox(IFRAME_TEST_URI, "webconsole");

  info(
    "Reload the test page, which will trigger the debugger statement in the iframe"
  );
  const pausedReload = reloadBrowser();

  const dbg = await assertDebuggerIsHighlightedAndPaused(toolbox);
  const topLevelThread =
    toolbox.commands.targetCommand.targetFront.threadFront.actorID;
  const iframeThread = dbg.selectors.getCurrentThread();
  if (isFissionEnabled() || isEveryFrameTargetEnabled()) {
    isnot(
      topLevelThread,
      iframeThread,
      "With fission/EFT, we get two distinct threads and could pause two times"
    );
    ok(
      !dbg.selectors.getIsPaused(topLevelThread),
      "The top level document thread is not paused"
    );
    ok(
      dbg.selectors.getIsPaused(iframeThread),
      "Only the iframe thread is paused"
    );
  } else {
    is(
      topLevelThread,
      iframeThread,
      "Without fission/EFT, we get a unique thread and we won't pause when calling top document code"
    );
  }
  const source = findSource(dbg, IFRAME_TEST_COM_URI);
  assertPausedAtSourceAndLine(dbg, source.id, 2);

  info("Step over to the next line");
  await stepOver(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 3);

  info("Now execute a debugger statement in the top level target");
  const onPaused = waitForPausedThread(dbg, topLevelThread);
  const pausedTopTarget = SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    function () {
      content.wrappedJSObject.breakDebugger();
    }
  );

  if (isFissionEnabled() || isEveryFrameTargetEnabled()) {
    info("Wait for the top level target to be paused");
    await onPaused;
    // also use waitForPause to wait for UI updates
    await waitForPaused(dbg);

    ok(
      dbg.selectors.getIsPaused(topLevelThread),
      "The top level document thread is paused"
    );
    ok(dbg.selectors.getIsPaused(iframeThread), "The iframe thread is paused");

    ok(
      toolbox.isHighlighted("jsdebugger"),
      "Debugger stays highlighted when pausing on another thread"
    );

    info(
      "The new paused state refers to the latest breakpoint being hit, on the top level target"
    );
    const source2 = findSource(dbg, IFRAME_TEST_URI);
    assertPausedAtSourceAndLine(dbg, source2.id, 2);

    info("Resume the top level target");
    await resume(dbg);

    info("Wait for top level target paused code to complete after resume");
    await pausedTopTarget;

    info(
      "By default we stay on the last selected thread on resume and so the current thread is no longer paused"
    );
    assertNotPaused(dbg);
    ok(
      toolbox.isHighlighted("jsdebugger"),
      "Debugger stays highlighted when resuming only the top level target"
    );

    info(
      "Re-select the iframe thread, which is still paused on the original breakpoint"
    );
    dbg.actions.selectThread(iframeThread);
    await waitForPausedThread(dbg, iframeThread);
    await waitForSelectedSource(dbg, source);
    assertPausedAtSourceAndLine(dbg, source.id, 3);

    info("Resume the iframe target");
    await resume(dbg);
    assertNotPaused(dbg);

    info("Wait for the paused code in the iframe to complete after resume");
    await pausedReload;

    await waitUntil(() => !toolbox.isHighlighted("jsdebugger"));
    ok(
      true,
      "Debugger is no longer highlighted after resuming all the paused targets"
    );
  } else {
    info(
      "Without fission/EFT, the iframe thread is the same as top document and doesn't pause. So wait for its resolution."
    );
    await pausedTopTarget;
  }

  info("Resume the last paused thread");
  await resume(dbg);
  assertNotPaused(dbg);

  info("Wait for the paused code in the iframe to complete after resume");
  await pausedReload;

  await waitUntil(() => !toolbox.isHighlighted("jsdebugger"));
  ok(
    true,
    "Debugger is no longer highlighted after resuming all the paused targets"
  );
});
