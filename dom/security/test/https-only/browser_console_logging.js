// Bug 1625448 - HTTPS Only Mode - Tests for console logging
// https://bugzilla.mozilla.org/show_bug.cgi?id=1625448
// This test makes sure that the various console messages from the HTTPS-Only
// mode get logged to the console.
"use strict";

// Test Cases
// description:    Description of what the subtests expects.
// expectLogLevel: Expected log-level of a message.
// expectIncludes: Expected substrings the message should contain.
let tests = [
  {
    description: "Top-Level upgrade should get logged",
    expectLogLevel: Ci.nsIConsoleMessage.warn,
    expectIncludes: [
      "HTTPS-Only Mode: Upgrading insecure request",
      "to use",
      "file_console_logging.html",
    ],
  },
  {
    description: "iFrame upgrade failure should get logged",
    expectLogLevel: Ci.nsIConsoleMessage.error,
    expectIncludes: [
      "HTTPS-Only Mode: Upgrading insecure request",
      "failed",
      "file_console_logging.html",
    ],
  },
  {
    description: "WebSocket upgrade should get logged",
    expectLogLevel: Ci.nsIConsoleMessage.warn,
    expectIncludes: [
      "HTTPS-Only Mode: Upgrading insecure request",
      "to use",
      "ws://does.not.exist",
    ],
  },
  {
    description: "Sub-Resource upgrade for file_1 should get logged",
    expectLogLevel: Ci.nsIConsoleMessage.warn,
    expectIncludes: ["Upgrading insecure", "request", "file_1.jpg"],
  },
  {
    description: "Sub-Resource upgrade for file_2 should get logged",
    expectLogLevel: Ci.nsIConsoleMessage.warn,
    expectIncludes: ["Upgrading insecure", "request", "to use", "file_2.jpg"],
  },
  {
    description: "Exempt request for file_exempt should get logged",
    expectLogLevel: Ci.nsIConsoleMessage.info,
    expectIncludes: [
      "Not upgrading insecure request",
      "because it is exempt",
      "file_exempt.jpg",
    ],
  },
  {
    description: "Sub-Resource upgrade failure for file_2 should get logged",
    expectLogLevel: Ci.nsIConsoleMessage.error,
    expectIncludes: ["Upgrading insecure request", "failed", "file_2.jpg"],
  },
];

const testPathUpgradeable = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);
// DNS errors are not logged as HTTPS-Only Mode upgrade failures, so we have to
// upgrade to a domain that exists but fails.
const testPathNotUpgradeable = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://self-signed.example.com"
);
const kTestURISuccess = testPathUpgradeable + "file_console_logging.html";
const kTestURIFail = testPathNotUpgradeable + "file_console_logging.html";
const kTestURIExempt = testPathUpgradeable + "file_exempt.jpg";

const UPGRADE_DISPLAY_CONTENT =
  "security.mixed_content.upgrade_display_content";

add_task(async function () {
  // A longer timeout is necessary for this test than the plain mochitests
  // due to opening a new tab with the web console.
  requestLongerTimeout(4);

  // Enable HTTPS-Only Mode and register console-listener
  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_only_mode", true]],
  });
  Services.console.registerListener(on_new_message);
  // 1. Upgrade page to https://
  BrowserTestUtils.startLoadingURIString(
    gBrowser.selectedBrowser,
    kTestURISuccess
  );
  // 2. Make an exempt http:// request
  let xhr = new XMLHttpRequest();
  xhr.open("GET", kTestURIExempt, true);
  xhr.channel.loadInfo.httpsOnlyStatus |= Ci.nsILoadInfo.HTTPS_ONLY_EXEMPT;
  xhr.send();
  // 3. Make Websocket request
  new WebSocket("ws://does.not.exist");

  await BrowserTestUtils.waitForCondition(() => tests.length === 0);

  // Clean up
  Services.console.unregisterListener(on_new_message);
});

function on_new_message(msgObj) {
  const message = msgObj.message;
  const logLevel = msgObj.logLevel;

  // Bools about message and pref
  const isMCL2Enabled = Services.prefs.getBoolPref(UPGRADE_DISPLAY_CONTENT);
  const isHTTPSOnlyModeLog = message.includes("HTTPS-Only Mode:");
  const isMCLog = message.includes("Mixed Content:");

  // Check for messages about HTTPS-only upgrades (those should be unrelated to mixed content upgrades)
  // or for mixed content upgrades which should only occur if security.mixed_content.upgrade_display_content is enabled
  // (unrelated to https-only logs).
  if (
    (isHTTPSOnlyModeLog && !isMCLog) ||
    (isMCLog && isMCL2Enabled && !isHTTPSOnlyModeLog)
  ) {
    for (let i = 0; i < tests.length; i++) {
      const testCase = tests[i];
      // If security.mixed_content.upgrade_display_content is enabled, the mixed content control mechanism is upgrading file2.jpg
      // and HTTPS-Only mode is not failing upgrading file2.jpg, so it won't be logged.
      // so skip last test case
      if (
        testCase.description ==
          "Sub-Resource upgrade failure for file_2 should get logged" &&
        isMCL2Enabled
      ) {
        tests.splice(i, 1);
        continue;
      }
      // Check if log-level matches
      if (logLevel !== testCase.expectLogLevel) {
        continue;
      }
      // Check if all substrings are included
      if (testCase.expectIncludes.some(str => !message.includes(str))) {
        continue;
      }
      ok(true, testCase.description);
      tests.splice(i, 1);
      break;
    }
  }
}
