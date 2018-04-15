/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check if an error with Unicode characters is reported correctly.

"use strict";

const TEST_URI = "data:text/html;charset=utf8,<script>\u6e2c</script>";
const EXPECTED_REPORT = "ReferenceError: \u6e2c is not defined";

add_task(async function() {
  let hud = await openNewTabAndConsole(TEST_URI);

  // On e10s, the exception is triggered in child process
  // and is ignored by test harness
  if (!Services.appinfo.browserTabsRemoteAutostart) {
    expectUncaughtException();
  }

  info("generate exception and wait for the message");

  let msg = await waitFor(() => findMessage(hud, EXPECTED_REPORT));
  ok(msg, `Message found: "${EXPECTED_REPORT}"`);
});
