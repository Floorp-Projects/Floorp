/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// When the debugger is paused in a worker thread, console evaluations should
// be performed in that worker's selected frame.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/mochitest/test-evaluate-worker.html";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const {jsterm} = hud;

  await openDebugger();
  const toolbox = gDevTools.getToolbox(hud.target);
  const dbg = createDebuggerContext(toolbox);

  jsterm.execute("pauseInWorker(42)");

  await waitForPaused(dbg);
  await openConsole();

  const onMessage = waitForMessage(hud, "42");
  jsterm.execute("data");
  await onMessage;

  ok(true, "Evaluated console message in worker thread");
});
