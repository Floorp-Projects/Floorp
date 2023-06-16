/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that throwing uncaught errors and primitive values in workers shows a
// stack in the console.

"use strict";

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-error-worker.html";

add_task(async function () {
  await pushPref("javascript.options.asyncstack_capture_debuggee_only", false);

  const hud = await openNewTabAndConsole(TEST_URI);

  await checkMessageStack(hud, "hello", [13, 4, 3]);
  await checkMessageStack(hud, "there", [16, 4, 3]);
  await checkMessageStack(hud, "dom", [18, 4, 3]);
  await checkMessageStack(hud, "worker2", [6, 3, 3]);
});
