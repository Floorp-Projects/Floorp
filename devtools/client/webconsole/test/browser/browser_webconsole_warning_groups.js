/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that warning messages can be grouped, per navigation and category, and that
// interacting with these groups works as expected.

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

pushPref("privacy.trackingprotection.enabled", true);
pushPref("devtools.webconsole.groupWarningMessages", true);

const CONTENT_BLOCKING_GROUP_LABEL =
  "The resource at “<URL>” was blocked because content blocking is enabled.";

add_task(async function testContentBlockingMessage() {
  // Enable groupWarning and persist log
  await pushPref("devtools.webconsole.persistlog", true);

  const hud = await openNewTabAndConsole(TEST_URI);

  info(
    "Log a tracking protection message to check a single message isn't grouped"
  );
  let onContentBlockingWarningMessage = waitForMessage(
    hud,
    BLOCKED_URL,
    ".warn"
  );
  emitContentBlockedMessage(hud);
  let { node } = await onContentBlockingWarningMessage;
  is(
    node.querySelector(".warning-indent"),
    null,
    "The message has the expected style"
  );
  is(
    node.querySelector(".indent").getAttribute("data-indent"),
    "0",
    "The message has the expected indent"
  );

  info("Log a simple message");
  await logString(hud, "simple message 1");

  info(
    "Log a second tracking protection message to check that it causes the grouping"
  );
  let onContentBlockingWarningGroupMessage = waitForMessage(
    hud,
    CONTENT_BLOCKING_GROUP_LABEL,
    ".warn"
  );
  emitContentBlockedMessage(hud);
  ({ node } = await onContentBlockingWarningGroupMessage);
  is(
    node.querySelector(".warning-group-badge").textContent,
    "2",
    "The badge has the expected text"
  );

  checkConsoleOutputForWarningGroup(hud, [
    `▶︎⚠ ${CONTENT_BLOCKING_GROUP_LABEL}`,
    `simple message 1`,
  ]);

  info("Log another simple message");
  await logString(hud, "simple message 2");

  checkConsoleOutputForWarningGroup(hud, [
    `▶︎⚠ ${CONTENT_BLOCKING_GROUP_LABEL}`,
    `simple message 1`,
    `simple message 2`,
  ]);

  info(
    "Log a third tracking protection message to check that the badge updates"
  );
  emitContentBlockedMessage(hud);
  await waitFor(
    () => node.querySelector(".warning-group-badge").textContent == "3"
  );

  checkConsoleOutputForWarningGroup(hud, [
    `▶︎⚠ ${CONTENT_BLOCKING_GROUP_LABEL}`,
    `simple message 1`,
    `simple message 2`,
  ]);

  info("Open the group");
  node.querySelector(".arrow").click();
  await waitFor(() => findMessage(hud, BLOCKED_URL));

  checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${CONTENT_BLOCKING_GROUP_LABEL}`,
    `| ${BLOCKED_URL}?1`,
    `| ${BLOCKED_URL}?2`,
    `| ${BLOCKED_URL}?3`,
    `simple message 1`,
    `simple message 2`,
  ]);

  info(
    "Log a new tracking protection message to check it appears inside the group"
  );
  onContentBlockingWarningMessage = waitForMessage(hud, BLOCKED_URL, ".warn");
  emitContentBlockedMessage(hud);
  await onContentBlockingWarningMessage;
  ok(true, "The new tracking protection message is displayed");

  checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${CONTENT_BLOCKING_GROUP_LABEL}`,
    `| ${BLOCKED_URL}?1`,
    `| ${BLOCKED_URL}?2`,
    `| ${BLOCKED_URL}?3`,
    `| ${BLOCKED_URL}?4`,
    `simple message 1`,
    `simple message 2`,
  ]);

  info("Reload the page and wait for it to be ready");
  await reloadPage();

  // Also wait for the navigation message to be displayed.
  await waitFor(() => findMessage(hud, "Navigated to"));

  info("Log a tracking protection message to check it is not grouped");
  onContentBlockingWarningMessage = waitForMessage(hud, BLOCKED_URL, ".warn");
  emitContentBlockedMessage(hud);
  await onContentBlockingWarningMessage;

  await logString(hud, "simple message 3");

  checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${CONTENT_BLOCKING_GROUP_LABEL}`,
    `| ${BLOCKED_URL}?1`,
    `| ${BLOCKED_URL}?2`,
    `| ${BLOCKED_URL}?3`,
    `| ${BLOCKED_URL}?4`,
    `simple message 1`,
    `simple message 2`,
    "Navigated to",
    `${BLOCKED_URL}?5`,
    `simple message 3`,
  ]);

  info(
    "Log a second tracking protection message to check that it causes the grouping"
  );
  onContentBlockingWarningGroupMessage = waitForMessage(
    hud,
    CONTENT_BLOCKING_GROUP_LABEL,
    ".warn"
  );
  emitContentBlockedMessage(hud);
  ({ node } = await onContentBlockingWarningGroupMessage);
  is(
    node.querySelector(".warning-group-badge").textContent,
    "2",
    "The badge has the expected text"
  );

  checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${CONTENT_BLOCKING_GROUP_LABEL}`,
    `| ${BLOCKED_URL}?1`,
    `| ${BLOCKED_URL}?2`,
    `| ${BLOCKED_URL}?3`,
    `| ${BLOCKED_URL}?4`,
    `simple message 1`,
    `simple message 2`,
    `Navigated to`,
    `▶︎⚠ ${CONTENT_BLOCKING_GROUP_LABEL}`,
    `simple message 3`,
  ]);

  info("Check that opening this group works");
  node.querySelector(".arrow").click();
  await waitFor(() => findMessages(hud, BLOCKED_URL).length === 6);

  checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${CONTENT_BLOCKING_GROUP_LABEL}`,
    `| ${BLOCKED_URL}?1`,
    `| ${BLOCKED_URL}?2`,
    `| ${BLOCKED_URL}?3`,
    `| ${BLOCKED_URL}?4`,
    `simple message 1`,
    `simple message 2`,
    `Navigated to`,
    `▼︎⚠ ${CONTENT_BLOCKING_GROUP_LABEL}`,
    `| ${BLOCKED_URL}?5`,
    `| ${BLOCKED_URL}?6`,
    `simple message 3`,
  ]);

  info("Check that closing this group works, and let the other one open");
  node.querySelector(".arrow").click();
  await waitFor(() => findMessages(hud, BLOCKED_URL).length === 4);

  checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${CONTENT_BLOCKING_GROUP_LABEL}`,
    `| ${BLOCKED_URL}?1`,
    `| ${BLOCKED_URL}?2`,
    `| ${BLOCKED_URL}?3`,
    `| ${BLOCKED_URL}?4`,
    `simple message 1`,
    `simple message 2`,
    `Navigated to`,
    `▶︎⚠ ${CONTENT_BLOCKING_GROUP_LABEL}`,
    `simple message 3`,
  ]);

  info(
    "Log a third tracking protection message to check that the badge updates"
  );
  emitContentBlockedMessage(hud);
  await waitFor(
    () => node.querySelector(".warning-group-badge").textContent == "3"
  );

  checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${CONTENT_BLOCKING_GROUP_LABEL}`,
    `| ${BLOCKED_URL}?1`,
    `| ${BLOCKED_URL}?2`,
    `| ${BLOCKED_URL}?3`,
    `| ${BLOCKED_URL}?4`,
    `simple message 1`,
    `simple message 2`,
    `Navigated to`,
    `▶︎⚠ ${CONTENT_BLOCKING_GROUP_LABEL}`,
    `simple message 3`,
  ]);
});

let cpt = 0;
/**
 * Emit a Content Blocking message. This is done by loading an image from an origin
 * tagged as tracker. The image is loaded with a incremented counter query parameter
 * each time so we can get the warning message.
 */
function emitContentBlockedMessage() {
  const url = `${BLOCKED_URL}?${++cpt}`;
  ContentTask.spawn(gBrowser.selectedBrowser, url, function(innerURL) {
    content.wrappedJSObject.loadImage(innerURL);
  });
}

/**
 * Log a string from the content page.
 *
 * @param {WebConsole} hud
 * @param {String} str
 */
function logString(hud, str) {
  const onMessage = waitForMessage(hud, str);
  ContentTask.spawn(gBrowser.selectedBrowser, str, function(arg) {
    content.console.log(arg);
  });
  return onMessage;
}
