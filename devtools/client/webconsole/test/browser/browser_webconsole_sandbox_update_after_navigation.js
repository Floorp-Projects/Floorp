/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests if the JSTerm sandbox is updated when the user navigates from one
// domain to another, in order to avoid permission denied errors with a sandbox
// created for a different origin. See Bug 664688.

"use strict";

const BASE_URI =
  "browser/devtools/client/webconsole/test/browser/test-console.html";
const TEST_URI1 = "http://example.com/" + BASE_URI;
const TEST_URI2 = "http://example.org/" + BASE_URI;

add_task(async function() {
  await pushPref("devtools.webconsole.persistlog", false);

  const hud = await openNewTabAndConsole(TEST_URI1);

  await executeAndWaitForMessage(
    hud,
    "window.location.href",
    TEST_URI1,
    ".result"
  );

  // load second url
  await navigateTo(TEST_URI2);

  ok(!findMessage(hud, "Permission denied"), "no permission denied errors");

  info("wait for window.location.href after page navigation");
  await clearOutput(hud);
  await executeAndWaitForMessage(
    hud,
    "window.location.href",
    TEST_URI2,
    ".result"
  );

  ok(!findMessage(hud, "Permission denied"), "no permission denied errors");

  // Navigation clears messages. Wait for that clear to happen before
  // continuing the test or it might destroy messages we wait later on (Bug
  // 1270234).
  const promises = [hud.ui.once("messages-cleared")];
  if (isFissionEnabled()) {
    promises.push(hud.commands.targetCommand.once("switched-target"));
  }

  gBrowser.goBack();

  info("Waiting for messages-cleared event due to navigation");
  await Promise.all(promises);

  info("Messages cleared after navigation; checking location");
  await executeAndWaitForMessage(
    hud,
    "window.location.href",
    TEST_URI1,
    ".result"
  );

  ok(!findMessage(hud, "Permission denied"), "no permission denied errors");
});
