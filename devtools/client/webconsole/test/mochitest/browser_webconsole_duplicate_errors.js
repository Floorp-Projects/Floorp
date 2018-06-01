/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that exceptions thrown by content don't show up twice in the Web
// Console. See Bug 582201.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/mochitest/test-duplicate-error.html";

add_task(async function() {
  // On e10s, the exception is triggered in child process
  // and is ignored by test harness
  if (!Services.appinfo.browserTabsRemoteAutostart) {
    expectUncaughtException();
  }
  const hud = await openNewTabAndConsole(TEST_URI);

  await waitFor(() => findMessage(hud, "fooDuplicateError1", ".message.error"));

  const errorMessages = hud.outputNode.querySelectorAll(".message.error");
  is(errorMessages.length, 1, "There's only one error message for fooDuplicateError1");
  is(errorMessages[0].querySelector(".message-repeats"), null,
    "There is no repeat bubble on the error message");
});
