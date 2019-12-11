/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that warning groups are not created outside console.group.

"use strict";
requestLongerTimeout(2);

const TEST_FILE =
  "browser/devtools/client/webconsole/test/browser/test-warning-groups.html";
const TEST_URI = "http://example.org/" + TEST_FILE;

const TRACKER_URL = "http://tracking.example.com/";
const BLOCKED_URL =
  TRACKER_URL +
  "browser/devtools/client/webconsole/test/browser/test-image.png";

const { UrlClassifierTestUtils } = ChromeUtils.import(
  "resource://testing-common/UrlClassifierTestUtils.jsm"
);
UrlClassifierTestUtils.addTestTrackers();
registerCleanupFunction(function() {
  UrlClassifierTestUtils.cleanupTestTrackers();
});

// Tracking protection preferences
pushPref("privacy.trackingprotection.enabled", true);

const CONTENT_BLOCKING_GROUP_LABEL =
  "The resource at “<URL>” was blocked because content blocking is enabled.";

add_task(async function testContentBlockingMessage() {
  // Enable groupWarning and persist log
  await pushPref("devtools.webconsole.groupWarningMessages", true);

  const hud = await openNewTabAndConsole(TEST_URI);

  info("Log a console.group");
  const onGroupMessage = waitForMessage(hud, "myGroup");
  let onInGroupMessage = waitForMessage(hud, "log in group");
  ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    content.wrappedJSObject.console.group("myGroup");
    content.wrappedJSObject.console.log("log in group");
  });
  const { node: consoleGroupMessageNode } = await onGroupMessage;
  await onInGroupMessage;

  checkConsoleOutputForWarningGroup(hud, [`▼ myGroup`, `| log in group`]);

  info(
    "Log a tracking protection message to check a single message isn't grouped"
  );
  const now = Date.now();
  let onContentBlockingWarningMessage = waitForMessage(
    hud,
    BLOCKED_URL,
    ".warn"
  );
  emitContentBlockedMessage(now);
  await onContentBlockingWarningMessage;

  checkConsoleOutputForWarningGroup(hud, [
    `▼ myGroup`,
    `| log in group`,
    `| ${BLOCKED_URL}?${now}-1`,
  ]);

  info("Collapse the console.group");
  consoleGroupMessageNode.querySelector(".arrow").click();
  await waitFor(() => !findMessage(hud, "log in group"));

  checkConsoleOutputForWarningGroup(hud, [`▶︎ myGroup`]);

  info("Expand the console.group");
  consoleGroupMessageNode.querySelector(".arrow").click();
  await waitFor(() => findMessage(hud, "log in group"));

  checkConsoleOutputForWarningGroup(hud, [
    `▼ myGroup`,
    `| log in group`,
    `| ${BLOCKED_URL}?${now}-1`,
  ]);

  info(
    "Log a second tracking protection message to check that it causes the grouping"
  );
  const onContentBlockingWarningGroupMessage = waitForMessage(
    hud,
    CONTENT_BLOCKING_GROUP_LABEL,
    ".warn"
  );
  emitContentBlockedMessage(now);
  const { node: warningGroupNode } = await onContentBlockingWarningGroupMessage;

  checkConsoleOutputForWarningGroup(hud, [
    `▶︎⚠ ${CONTENT_BLOCKING_GROUP_LABEL}`,
    `▼ myGroup`,
    `| log in group`,
  ]);

  info("Open the warning group");
  warningGroupNode.querySelector(".arrow").click();
  await waitFor(() => findMessage(hud, BLOCKED_URL));

  checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${CONTENT_BLOCKING_GROUP_LABEL}`,
    `| ${BLOCKED_URL}?${now}-1`,
    `| ${BLOCKED_URL}?${now}-2`,
    `▼ myGroup`,
    `| log in group`,
  ]);

  info(
    "Log a new tracking protection message to check it appears inside the group"
  );
  onContentBlockingWarningMessage = waitForMessage(hud, BLOCKED_URL, ".warn");
  emitContentBlockedMessage(now);
  await onContentBlockingWarningMessage;
  ok(true, "The new tracking protection message is displayed");

  checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${CONTENT_BLOCKING_GROUP_LABEL}`,
    `| ${BLOCKED_URL}?${now}-1`,
    `| ${BLOCKED_URL}?${now}-2`,
    `| ${BLOCKED_URL}?${now}-3`,
    `▼ myGroup`,
    `| log in group`,
  ]);

  info("Log a simple message to check if it goes into the console.group");
  onInGroupMessage = waitForMessage(hud, "log in group");
  ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    content.wrappedJSObject.console.log("second log in group");
  });
  await onInGroupMessage;

  checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${CONTENT_BLOCKING_GROUP_LABEL}`,
    `| ${BLOCKED_URL}?${now}-1`,
    `| ${BLOCKED_URL}?${now}-2`,
    `| ${BLOCKED_URL}?${now}-3`,
    `▼ myGroup`,
    `| log in group`,
    `| second log in group`,
  ]);

  info("Collapse the console.group");
  consoleGroupMessageNode.querySelector(".arrow").click();
  await waitFor(() => !findMessage(hud, "log in group"));

  checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${CONTENT_BLOCKING_GROUP_LABEL}`,
    `| ${BLOCKED_URL}?${now}-1`,
    `| ${BLOCKED_URL}?${now}-2`,
    `| ${BLOCKED_URL}?${now}-3`,
    `▶︎ myGroup`,
  ]);

  info("Close the warning group");
  warningGroupNode.querySelector(".arrow").click();
  await waitFor(() => !findMessage(hud, BLOCKED_URL));

  checkConsoleOutputForWarningGroup(hud, [
    `▶︎⚠ ${CONTENT_BLOCKING_GROUP_LABEL}`,
    `▶︎ myGroup`,
  ]);

  info("Open the console group");
  consoleGroupMessageNode.querySelector(".arrow").click();
  await waitFor(() => findMessage(hud, "log in group"));

  checkConsoleOutputForWarningGroup(hud, [
    `▶︎⚠ ${CONTENT_BLOCKING_GROUP_LABEL}`,
    `▼ myGroup`,
    `| log in group`,
    `| second log in group`,
  ]);

  info("Collapse the console.group");
  consoleGroupMessageNode.querySelector(".arrow").click();
  await waitFor(() => !findMessage(hud, "log in group"));

  checkConsoleOutputForWarningGroup(hud, [
    `▶︎⚠ ${CONTENT_BLOCKING_GROUP_LABEL}`,
    `▶︎ myGroup`,
  ]);

  info("Open the warning group");
  warningGroupNode.querySelector(".arrow").click();
  await waitFor(() => findMessage(hud, BLOCKED_URL));

  checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${CONTENT_BLOCKING_GROUP_LABEL}`,
    `| ${BLOCKED_URL}?${now}-1`,
    `| ${BLOCKED_URL}?${now}-2`,
    `| ${BLOCKED_URL}?${now}-3`,
    `▶︎ myGroup`,
  ]);
});

let cpt = 0;
/**
 * Emit a Content Blocking message. This is done by loading an image from an origin
 * tagged as tracker. The image is loaded with a incremented counter query parameter
 * each time so we can get the warning message.
 */
function emitContentBlockedMessage(prefix) {
  const url = `${BLOCKED_URL}?${prefix}-${++cpt}`;
  ContentTask.spawn(gBrowser.selectedBrowser, url, function(innerURL) {
    content.wrappedJSObject.loadImage(innerURL);
  });
}
