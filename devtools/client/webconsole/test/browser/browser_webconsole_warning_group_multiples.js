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
const FILE_PATH =
  "browser/devtools/client/webconsole/test/browser/test-image.png";
const CONTENT_BLOCKED_URL = TRACKER_URL + FILE_PATH;
const STORAGE_BLOCKED_URL = "http://example.com/" + FILE_PATH;

const COOKIE_BEHAVIOR_PREF = "network.cookie.cookieBehavior";
const COOKIE_BEHAVIORS_REJECT_FOREIGN = 1;

const CONTENT_BLOCKED_GROUP_LABEL =
  "The resource at “<URL>” was blocked because content blocking is enabled.";
const STORAGE_BLOCKED_GROUP_LABEL =
  "Request to access cookie or storage on “<URL>” " +
  "was blocked because we are blocking all third-party storage access requests and " +
  "content blocking is enabled.";

const { UrlClassifierTestUtils } = ChromeUtils.import(
  "resource://testing-common/UrlClassifierTestUtils.jsm"
);
UrlClassifierTestUtils.addTestTrackers();
registerCleanupFunction(function() {
  UrlClassifierTestUtils.cleanupTestTrackers();
});

pushPref("privacy.trackingprotection.enabled", true);
pushPref("devtools.webconsole.groupWarningMessages", true);

add_task(async function testContentBlockingMessage() {
  await pushPref(COOKIE_BEHAVIOR_PREF, COOKIE_BEHAVIORS_REJECT_FOREIGN);
  await pushPref("devtools.webconsole.persistlog", true);

  const hud = await openNewTabAndConsole(TEST_URI);

  info(
    "Log a tracking protection message to check a single message isn't grouped"
  );
  let onContentBlockedMessage = waitForMessage(
    hud,
    CONTENT_BLOCKED_URL,
    ".warn"
  );
  emitContentBlockingMessage(hud);
  let { node } = await onContentBlockedMessage;
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
  let onContentBlockedWarningGroupMessage = waitForMessage(
    hud,
    CONTENT_BLOCKED_GROUP_LABEL,
    ".warn"
  );
  emitContentBlockingMessage(hud);
  const {
    node: contentBlockedWarningGroupNode,
  } = await onContentBlockedWarningGroupMessage;
  is(
    contentBlockedWarningGroupNode.querySelector(".warning-group-badge")
      .textContent,
    "2",
    "The badge has the expected text"
  );

  checkConsoleOutputForWarningGroup(hud, [
    `▶︎⚠ ${CONTENT_BLOCKED_GROUP_LABEL}`,
    `simple message 1`,
  ]);

  let onStorageBlockedWarningGroupMessage = waitForMessage(
    hud,
    STORAGE_BLOCKED_URL,
    ".warn"
  );

  emitStorageAccessBlockedMessage(hud);
  ({ node } = await onStorageBlockedWarningGroupMessage);
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

  info("Log a second simple message");
  await logString(hud, "simple message 2");

  checkConsoleOutputForWarningGroup(hud, [
    `▶︎⚠ ${CONTENT_BLOCKED_GROUP_LABEL}`,
    `simple message 1`,
    `${STORAGE_BLOCKED_URL}`,
    `simple message 2`,
  ]);

  info(
    "Log a second storage blocked message to check that it creates another group"
  );
  onStorageBlockedWarningGroupMessage = waitForMessage(
    hud,
    STORAGE_BLOCKED_GROUP_LABEL,
    ".warn"
  );
  emitStorageAccessBlockedMessage(hud);
  const {
    node: storageBlockedWarningGroupNode,
  } = await onStorageBlockedWarningGroupMessage;
  is(
    storageBlockedWarningGroupNode.querySelector(".warning-group-badge")
      .textContent,
    "2",
    "The badge has the expected text"
  );

  info("Expand the second warning group");
  storageBlockedWarningGroupNode.querySelector(".arrow").click();
  await waitFor(() => findMessage(hud, STORAGE_BLOCKED_URL));

  checkConsoleOutputForWarningGroup(hud, [
    `▶︎⚠ ${CONTENT_BLOCKED_GROUP_LABEL}`,
    `simple message 1`,
    `▼︎⚠ ${STORAGE_BLOCKED_GROUP_LABEL}`,
    `| ${STORAGE_BLOCKED_URL}?3`,
    `| ${STORAGE_BLOCKED_URL}?4`,
    `simple message 2`,
  ]);

  info(
    "Add another storage blocked message to check it does go into the opened group"
  );
  let onStorageBlockedMessage = waitForMessage(
    hud,
    STORAGE_BLOCKED_URL,
    ".warn"
  );
  emitStorageAccessBlockedMessage(hud);
  await onStorageBlockedMessage;

  checkConsoleOutputForWarningGroup(hud, [
    `▶︎⚠ ${CONTENT_BLOCKED_GROUP_LABEL}`,
    `simple message 1`,
    `▼︎⚠ ${STORAGE_BLOCKED_GROUP_LABEL}`,
    `| ${STORAGE_BLOCKED_URL}?3`,
    `| ${STORAGE_BLOCKED_URL}?4`,
    `| ${STORAGE_BLOCKED_URL}?5`,
    `simple message 2`,
  ]);

  info(
    "Add a content blocked message to check the first group badge is updated"
  );
  emitContentBlockingMessage();
  await waitForBadgeNumber(contentBlockedWarningGroupNode, 3);

  info("Expand the first warning group");
  contentBlockedWarningGroupNode.querySelector(".arrow").click();
  await waitFor(() => findMessage(hud, CONTENT_BLOCKED_URL));

  checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${CONTENT_BLOCKED_GROUP_LABEL}`,
    `| ${CONTENT_BLOCKED_URL}?1`,
    `| ${CONTENT_BLOCKED_URL}?2`,
    `| ${CONTENT_BLOCKED_URL}?6`,
    `simple message 1`,
    `▼︎⚠ ${STORAGE_BLOCKED_GROUP_LABEL}`,
    `| ${STORAGE_BLOCKED_URL}?3`,
    `| ${STORAGE_BLOCKED_URL}?4`,
    `| ${STORAGE_BLOCKED_URL}?5`,
    `simple message 2`,
  ]);

  info("Reload the page and wait for it to be ready");
  await reloadPage();

  // Also wait for the navigation message to be displayed.
  await waitFor(() => findMessage(hud, "Navigated to"));

  info("Add a storage blocked message and a content blocked one");
  onStorageBlockedMessage = waitForMessage(hud, STORAGE_BLOCKED_URL, ".warn");
  emitStorageAccessBlockedMessage(hud);
  await onStorageBlockedMessage;

  onContentBlockedMessage = waitForMessage(hud, CONTENT_BLOCKED_URL, ".warn");
  emitContentBlockingMessage(hud);
  await onContentBlockedMessage;

  checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${CONTENT_BLOCKED_GROUP_LABEL}`,
    `| ${CONTENT_BLOCKED_URL}?1`,
    `| ${CONTENT_BLOCKED_URL}?2`,
    `| ${CONTENT_BLOCKED_URL}?6`,
    `simple message 1`,
    `▼︎⚠ ${STORAGE_BLOCKED_GROUP_LABEL}`,
    `| ${STORAGE_BLOCKED_URL}?3`,
    `| ${STORAGE_BLOCKED_URL}?4`,
    `| ${STORAGE_BLOCKED_URL}?5`,
    `simple message 2`,
    `Navigated to`,
    `${STORAGE_BLOCKED_URL}?7`,
    `${CONTENT_BLOCKED_URL}?8`,
  ]);

  info(
    "Add a storage blocked message and a content blocked one to create warningGroups"
  );
  onStorageBlockedWarningGroupMessage = waitForMessage(
    hud,
    STORAGE_BLOCKED_GROUP_LABEL,
    ".warn"
  );
  emitStorageAccessBlockedMessage();
  await onStorageBlockedWarningGroupMessage;

  onContentBlockedWarningGroupMessage = waitForMessage(
    hud,
    CONTENT_BLOCKED_GROUP_LABEL,
    ".warn"
  );
  emitContentBlockingMessage();
  await onContentBlockedWarningGroupMessage;

  checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${CONTENT_BLOCKED_GROUP_LABEL}`,
    `| ${CONTENT_BLOCKED_URL}?1`,
    `| ${CONTENT_BLOCKED_URL}?2`,
    `| ${CONTENT_BLOCKED_URL}?6`,
    `simple message 1`,
    `▼︎⚠ ${STORAGE_BLOCKED_GROUP_LABEL}`,
    `| ${STORAGE_BLOCKED_URL}?3`,
    `| ${STORAGE_BLOCKED_URL}?4`,
    `| ${STORAGE_BLOCKED_URL}?5`,
    `simple message 2`,
    `Navigated to`,
    `▶︎⚠ ${STORAGE_BLOCKED_GROUP_LABEL}`,
    `▶︎⚠ ${CONTENT_BLOCKED_GROUP_LABEL}`,
  ]);
});

let cpt = 0;
const now = Date.now();

/**
 * Emit a Content Blocking message. This is done by loading an image from an origin
 * tagged as tracker. The image is loaded with a incremented counter query parameter
 * each time so we can get the warning message.
 */
function emitContentBlockingMessage() {
  const url = `${CONTENT_BLOCKED_URL}?${++cpt}-${now}`;
  ContentTask.spawn(gBrowser.selectedBrowser, url, function(innerURL) {
    content.wrappedJSObject.loadImage(innerURL);
  });
}

/**
 * Emit a Storage blocked message. This is done by loading an image from a different
 * origin, with the cookier permission rejecting foreign origin cookie access.
 */
function emitStorageAccessBlockedMessage() {
  const url = `${STORAGE_BLOCKED_URL}?${++cpt}-${now}`;
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

function waitForBadgeNumber(messageNode, expectedNumber) {
  return waitFor(
    () =>
      messageNode.querySelector(".warning-group-badge").textContent ==
      expectedNumber
  );
}
