// Bug 1673574 - Improve Console logging for mixed content auto upgrading
"use strict";

const testPath = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

let seenAutoUpgradeMessage = false;

const kTestURI =
  testPath + "file_mixed_content_auto_upgrade_display_console.html";

add_task(async function () {
  // A longer timeout is necessary for this test than the plain mochitests
  // due to opening a new tab with the web console.
  requestLongerTimeout(4);

  // Enable HTTPS-Only Mode and register console-listener
  await SpecialPowers.pushPrefEnv({
    set: [
      ["security.mixed_content.upgrade_display_content", true],
      ["security.mixed_content.upgrade_display_content.image", true],
    ],
  });
  Services.console.registerListener(on_auto_upgrade_message);

  BrowserTestUtils.startLoadingURIString(gBrowser.selectedBrowser, kTestURI);

  await BrowserTestUtils.waitForCondition(() => seenAutoUpgradeMessage);

  Services.console.unregisterListener(on_auto_upgrade_message);
});

function on_auto_upgrade_message(msgObj) {
  const message = msgObj.message;

  // The console message is:
  // "Mixed Content: Upgrading insecure display request
  // ‘http://example.com/file_mixed_content_auto_upgrade_display_console.jpg’ to use ‘https’"

  if (!message.includes("Mixed Content:")) {
    return;
  }
  ok(
    message.includes("Upgrading insecure display request"),
    "msg includes info"
  );
  ok(
    message.includes("file_mixed_content_auto_upgrade_display_console.jpg"),
    "msg includes file"
  );
  seenAutoUpgradeMessage = true;
}
