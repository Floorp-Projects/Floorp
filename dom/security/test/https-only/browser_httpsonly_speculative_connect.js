"use strict";

const TEST_PATH_HTTP = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.org"
);

let console_messages = [
  {
    description: "Speculative Connection should get logged",
    expectLogLevel: Ci.nsIConsoleMessage.warn,
    expectIncludes: [
      "Upgrading insecure speculative TCP connection",
      "to use",
      "example.org",
      "file_httpsonly_speculative_connect.html",
    ],
  },
  {
    description: "Upgrade should get logged",
    expectLogLevel: Ci.nsIConsoleMessage.warn,
    expectIncludes: [
      "Upgrading insecure request",
      "to use",
      "example.org",
      "file_httpsonly_speculative_connect.html",
    ],
  },
];

function on_new_console_messages(msgObj) {
  const message = msgObj.message;
  const logLevel = msgObj.logLevel;

  if (message.includes("HTTPS-Only Mode:")) {
    for (let i = 0; i < console_messages.length; i++) {
      const testCase = console_messages[i];
      // Check if log-level matches
      if (logLevel !== testCase.expectLogLevel) {
        continue;
      }
      // Check if all substrings are included
      if (testCase.expectIncludes.some(str => !message.includes(str))) {
        continue;
      }
      ok(true, testCase.description);
      console_messages.splice(i, 1);
      break;
    }
  }
}

add_task(async function () {
  requestLongerTimeout(4);

  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_only_mode", true]],
  });
  Services.console.registerListener(on_new_console_messages);

  let promiseLoaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  await BrowserTestUtils.startLoadingURIString(
    gBrowser.selectedBrowser,
    `${TEST_PATH_HTTP}file_httpsonly_speculative_connect.html`
  );
  await promiseLoaded;

  await BrowserTestUtils.waitForCondition(() => console_messages.length === 0);

  Services.console.unregisterListener(on_new_console_messages);
});
