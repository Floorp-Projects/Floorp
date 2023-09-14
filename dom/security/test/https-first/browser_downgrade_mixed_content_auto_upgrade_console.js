// Bug 1673574 - Improve Console logging for mixed content auto upgrading
"use strict";

const testPath = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://httpsfirst.com"
);

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

const kTestURI = testPath + "file_mixed_content_auto_upgrade.html";

add_task(async function () {
  // A longer timeout is necessary for this test than the plain mochitests
  // due to opening a new tab with the web console.
  requestLongerTimeout(4);

  // Enable ML2 and HTTPS-First Mode and register console-listener
  await SpecialPowers.pushPrefEnv({
    set: [
      ["security.mixed_content.upgrade_display_content", true],
      ["dom.security.https_first", true],
    ],
  });
  Services.console.registerListener(on_new_message);
  // 1. Upgrade page to https://
  let promiseLoaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  BrowserTestUtils.startLoadingURIString(gBrowser.selectedBrowser, kTestURI);
  await promiseLoaded;

  await BrowserTestUtils.waitForCondition(() => tests.length === 0);

  // Clean up
  Services.console.unregisterListener(on_new_message);
});

function on_new_message(msgObj) {
  const message = msgObj.message;
  const logLevel = msgObj.logLevel;

  // The console message is:
  // Should only show HTTPS-First messages

  if (message.includes("Mixed Content:")) {
    ok(
      !message.includes("Upgrading insecure display request"),
      "msg included a mixed content upgrade"
    );
  }
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
