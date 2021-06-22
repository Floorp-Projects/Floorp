/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that the debugger pauses and is automatically highlighted and selected,
// even when it hasn't been opened.

"use strict";

const IFRAME_TEST_COM_URI =
  `http://example.com/document-builder.sjs?html=` +
  encodeURI(`<script>debugger;</script>`);

// Embed the example.com test page in an example.org iframe.
const IFRAME_TEST_URI = `http://example.org/document-builder.sjs?html=` +
  encodeURI(`<script>function breakDebugger() {debugger;}</script><iframe src="${IFRAME_TEST_COM_URI}"></iframe><body>`);

add_task(async function() {
  info("Test a debugger statement from the top level document");

  // Make sure the toolbox opens with the webconsole initially selected.
  const toolbox = await initPane("doc-debugger-statements.html", "webconsole");
  
  info("Execute a debugger statement");
  const pausedRun = SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    content.wrappedJSObject.test();
  });

  const dbg = await assertDebuggerIsHighlightedAndPaused(toolbox);
  const source = findSource(dbg, "doc-debugger-statements.html");
  await assertPausedAtSourceAndLine(dbg, source.id, 16);

  await resume(dbg);
  info("Wait for the paused code to complete after resume");
  await pausedRun;

  ok(!toolbox.isHighlighted("jsdebugger"), "Debugger is no longer highlighted after resume");
});

add_task(async function() {
  info("Test a debugger statement from an iframe");

  // Make sure the toolbox opens with the webconsole initially selected.
  const toolbox = await openNewTabAndToolbox(IFRAME_TEST_URI, "webconsole");
  
  info("Reload the test page, which will trigger the debugger statement in the iframe");
  const pausedReload = refreshTab();

  const dbg = await assertDebuggerIsHighlightedAndPaused(toolbox);
  const source = findSource(dbg, IFRAME_TEST_COM_URI);
  await assertPausedAtSourceAndLine(dbg, source.id, 1);

  await resume(dbg);
  info("Wait for the paused code to complete after resume");
  await pausedReload;

  ok(!toolbox.isHighlighted("jsdebugger"), "Debugger is no longer highlighted after resume");
});

add_task(async function() {
  info("Test pausing from two distinct targets");

  // Make sure the toolbox opens with the webconsole initially selected.
  const toolbox = await openNewTabAndToolbox(IFRAME_TEST_URI, "webconsole");
  
  info("Reload the test page, which will trigger the debugger statement in the iframe");
  const pausedReload = refreshTab();

  const dbg = await assertDebuggerIsHighlightedAndPaused(toolbox);
  const topLevelThread= toolbox.commands.targetCommand.targetFront.threadFront.actorID;
  const iframeThread = dbg.selectors.getCurrentThread();
  if (isFissionEnabled()) {
    isnot(topLevelThread, iframeThread, "With fission, we get two distinct threads and could pause two times");
    ok(!dbg.selectors.getIsPaused(topLevelThread), "The top level document thread is not paused");
    ok(dbg.selectors.getIsPaused(iframeThread), "Only the iframe thread is paused");
  } else {
    is(topLevelThread, iframeThread, "Without fission, we get a unique thread and we won't pause when calling top document code");
  }
  const source = findSource(dbg, IFRAME_TEST_COM_URI);
  await assertPausedAtSourceAndLine(dbg, source.id, 1);

  info("Now execute a debugger statement in the top level target");
  const onPaused = waitForPausedThread(dbg, topLevelThread);
  const pausedTopTarget = SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    content.wrappedJSObject.breakDebugger();
  });

  if (isFissionEnabled()) {
    info("Wait for the top level target to be paused");
    await onPaused;
    // also use waitForPause to wait for UI updates
    await waitForPaused(dbg);
    ok(dbg.selectors.getIsPaused(topLevelThread), "The top level document thread is paused");
    ok(dbg.selectors.getIsPaused(iframeThread), "The iframe thread is paused");

    ok(toolbox.isHighlighted("jsdebugger"), "Debugger stays highlighted when pausing on another thread");

    info("The new paused state refers to the latest breakpoint being hit, on the top level target");
    const source2 = findSource(dbg, IFRAME_TEST_URI);
    await assertPausedAtSourceAndLine(dbg, source2.id, 1);

    info("Resume the top level target");
    await resume(dbg);

    info("Wait for top level target paused code to complete after resume");
    await pausedTopTarget;

    info("By default we stay on the last selected thread on resume and so the current thread is no longer paused");
    assertNotPaused(dbg);
    ok(toolbox.isHighlighted("jsdebugger"), "Debugger stays highlighted when resuming only the top level target");

    info("Re-select the iframe thread, which is still paused on the original breakpoint");
    dbg.actions.selectThread(getContext(dbg), iframeThread);
    await waitForPausedThread(dbg, iframeThread);
    await assertPausedAtSourceAndLine(dbg, source.id, 1);

    info("Resume the iframe target");
    await resume(dbg);
    assertNotPaused(dbg);

    info("Wait for the paused code in the iframe to complete after resume");
    await pausedReload;

    ok(!toolbox.isHighlighted("jsdebugger"), "Debugger is no longer highlighted after resuming all the paused targets");
  } else {
    info("Without fission, the iframe thread is the same as top document and doesn't pause. So wait for its resolution.");
    await pausedTopTarget;
  }

  info("Resume the last paused thread");
  await resume(dbg);
  assertNotPaused(dbg);

  info("Wait for the paused code in the iframe to complete after resume");
  await pausedReload;

  ok(!toolbox.isHighlighted("jsdebugger"), "Debugger is no longer highlighted after resuming all the paused targets");


});

async function assertDebuggerIsHighlightedAndPaused(toolbox) {
  info("Wait for the debugger to be automatically selected on pause");
  await waitUntil(() => toolbox.currentToolId == "jsdebugger");
  ok(true, "Debugger selected");

  // Wait for the debugger to finish loading.
  await toolbox.getPanelWhenReady("jsdebugger");

  // And to be fully paused
  const dbg = createDebuggerContext(toolbox);
  await waitForPaused(dbg);

  ok(toolbox.isHighlighted("jsdebugger"), "Debugger is highlighted");

  return dbg;
}
