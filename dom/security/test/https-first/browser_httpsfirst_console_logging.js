// Bug 1658924 - HTTPS-First Mode - Tests for console logging
// https://bugzilla.mozilla.org/show_bug.cgi?id=1658924
// This test makes sure that the various console messages from the HTTPS-First
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
    expectIncludes: ["Upgrading insecure request", "to use", "httpsfirst.com"],
  },
  {
    description: "Top-Level upgrade failure should get logged",
    expectLogLevel: Ci.nsIConsoleMessage.warn,
    expectIncludes: [
      "Upgrading insecure request",
      "failed",
      "httpsfirst.com",
      "Downgrading to",
    ],
  },
];

add_task(async function() {
  // A longer timeout is necessary for this test than the plain mochitests
  // due to opening a new tab with the web console.
  requestLongerTimeout(4);

  // Enable HTTPS-First Mode and register console-listener
  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_first", true]],
  });
  Services.console.registerListener(on_new_message);
  // 1. Upgrade page to https://
  await BrowserTestUtils.loadURI(
    gBrowser.selectedBrowser,
    "http://httpsfirst.com"
  );

  await BrowserTestUtils.waitForCondition(() => tests.length === 0);

  // Clean up
  Services.console.unregisterListener(on_new_message);
});

function on_new_message(msgObj) {
  const message = msgObj.message;
  const logLevel = msgObj.logLevel;

  if (message.includes("HTTPS-First Mode:")) {
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
