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
const IFRAME_TEST_URI = `http://example.org/document-builder.sjs?html=<iframe src="${encodeURI(IFRAME_TEST_COM_URI)}"></iframe><body>`;

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
