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
      "Upgrading insecure request",
      "to use",
      "file_console_logging.html",
    ],
  },
  {
    description: "Top-Level upgrade failure should get logged",
    expectLogLevel: Ci.nsIConsoleMessage.error,
    expectIncludes: [
      "Upgrading insecure request",
      "failed",
      "file_console_logging.html",
    ],
  },
  {
    description: "Sub-Resource upgrade for file_1 should get logged",
    expectLogLevel: Ci.nsIConsoleMessage.warn,
    expectIncludes: ["Upgrading insecure request", "to use", "file_1.jpg"],
  },
  {
    description: "Sub-Resource upgrade for file_2 should get logged",
    expectLogLevel: Ci.nsIConsoleMessage.warn,
    expectIncludes: ["Upgrading insecure request", "to use", "file_2.jpg"],
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
  "http://nocert.example.com"
);
const kTestURISuccess = testPathUpgradeable + "file_console_logging.html";
const kTestURIFail = testPathNotUpgradeable + "file_console_logging.html";
const kTestURIExempt = testPathUpgradeable + "file_exempt.jpg";

add_task(async function() {
  // A longer timeout is necessary for this test than the plain mochitests
  // due to opening a new tab with the web console.
  requestLongerTimeout(4);

  // Enable HTTPS-Only Mode and register console-listener
  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_only_mode", true]],
  });
  Services.console.registerListener(on_new_message);
  // 1. Upgrade page to https://
  await BrowserTestUtils.loadURI(gBrowser.selectedBrowser, kTestURISuccess);
  // 2. Make an exempt http:// request
  let xhr = new XMLHttpRequest();
  xhr.open("GET", kTestURIExempt, true);
  xhr.channel.loadInfo.httpsOnlyStatus |= Ci.nsILoadInfo.HTTPS_ONLY_EXEMPT;
  xhr.send();
  // Wait for logging of 1 and 2
  await BrowserTestUtils.waitForCondition(() => tests.length === 1);
  // 3. Failing upgrade to https://
  await BrowserTestUtils.loadURI(gBrowser.selectedBrowser, kTestURIFail);
  // Wait for logging of 3
  await BrowserTestUtils.waitForCondition(() => tests.length === 0);

  // Clean up
  Services.console.unregisterListener(on_new_message);
});

function on_new_message(msgObj) {
  const message = msgObj.message;
  const logLevel = msgObj.logLevel;

  if (message.includes("HTTPS-Only Mode:")) {
    for (let i = 0; i < tests.length; i++) {
      const testCase = tests[i];
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
