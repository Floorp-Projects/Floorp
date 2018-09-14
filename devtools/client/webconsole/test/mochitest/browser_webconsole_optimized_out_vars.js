/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that inspecting an optimized out variable works when execution is
// paused.

"use strict";

// Import helpers for the new debugger
/* import-globals-from ../../../debugger/new/test/mochitest/helpers.js */
Services.scriptloader.loadSubScript(
    "chrome://mochitests/content/browser/devtools/client/debugger/new/test/mochitest/helpers.js",
    this);

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/mochitest/" +
                 "test-closure-optimized-out.html";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  await openDebugger();

  const toolbox = gDevTools.getToolbox(hud.target);
  const dbg = createDebuggerContext(toolbox);

  await addBreakpoint(dbg, "test-closure-optimized-out.html", 18);
  await waitForThreadEvents(dbg, "resumed");

  // Cause the debuggee to pause
  await pauseDebugger(dbg);

  await toolbox.selectTool("webconsole");

  // This is the meat of the test: evaluate the optimized out variable.
  const onMessage = waitForMessage(hud, "optimized out");
  hud.jsterm.execute("upvar");

  info("Waiting for optimized out message");
  await onMessage;

  ok(true, "Optimized out message logged");

  info("Open the debugger");
  await openDebugger();

  info("Resume");
  await resume(dbg);

  info("Remove the breakpoint");
  const source = findSource(dbg, "test-closure-optimized-out.html");
  await removeBreakpoint(dbg, source.id, 18);
});

async function pauseDebugger(dbg) {
  info("Waiting for debugger to pause");
  ContentTask.spawn(gBrowser.selectedBrowser, {}, async function() {
    const button = content.document.querySelector("button");
    button.click();
  });
  await waitForPaused(dbg);
}
