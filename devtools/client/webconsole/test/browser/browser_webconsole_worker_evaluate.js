/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// When the debugger is paused in a worker thread, console evaluations should
// be performed in that worker's selected frame.

"use strict";

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-evaluate-worker.html";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  await openDebugger();
  const toolbox = hud.toolbox;
  const dbg = createDebuggerContext(toolbox);

  execute(hud, "pauseInWorker(42)");

  await waitForPaused(dbg);
  await openConsole();

  await executeAndWaitForMessage(hud, "data", "42", ".result");
  ok(true, "Evaluated console message in worker thread");
});
