/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the Web Console shows weak crypto warnings (SHA-1 Certificate)

"use strict";

const TEST_URI =
  "data:text/html;charset=utf8,<!DOCTYPE html>Web Console weak crypto warnings test";
const TEST_URI_PATH =
  "/browser/devtools/client/webconsole/test/" +
  "browser/test-certificate-messages.html";

const TRIGGER_MSG = "If you haven't seen ssl warnings yet, you won't";
const TLS_1_0_URL = "https://tls1.example.com" + TEST_URI_PATH;

const TLS_expected_message =
  "This site uses a deprecated version of TLS. " +
  "Please upgrade to TLS 1.2 or 1.3.";

registerCleanupFunction(function() {
  // Set preferences back to their original values
  Services.prefs.clearUserPref("security.tls.version.min");
  Services.prefs.clearUserPref("security.tls.version.max");
});

add_task(async function() {
  // Run with server side targets in order to avoid the restart
  // of console and error resource listeners during a client side target switching.
  // This leads to unexpected order between console and error messages
  await pushPref("devtools.target-switching.server.enabled", true);

  const hud = await openNewTabAndConsole(TEST_URI);

  info("Test TLS warnings");
  // Run with all versions enabled for this test.
  Services.prefs.setIntPref("security.tls.version.min", 1);
  Services.prefs.setIntPref("security.tls.version.max", 4);
  const onContentLog = waitForMessage(hud, TRIGGER_MSG);
  await navigateTo(TLS_1_0_URL);
  await onContentLog;

  const textContent = hud.ui.outputNode.textContent;
  ok(textContent.includes(TLS_expected_message), "TLS warning message found");

  Services.cache2.clear();
});
