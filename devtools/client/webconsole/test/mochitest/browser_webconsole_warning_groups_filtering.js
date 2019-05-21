/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that filtering the console output when there are warning groups works as expected.

"use strict";
requestLongerTimeout(2);

const TEST_FILE =
  "browser/devtools/client/webconsole/test/mochitest/test-warning-groups.html";
const TEST_URI = "http://example.org/" + TEST_FILE;

const TRACKER_URL = "http://tracking.example.com/";
const BLOCKED_URL = TRACKER_URL +
  "browser/devtools/client/webconsole/test/mochitest/test-image.png";

const {UrlClassifierTestUtils} = ChromeUtils.import("resource://testing-common/UrlClassifierTestUtils.jsm");
UrlClassifierTestUtils.addTestTrackers();
registerCleanupFunction(function() {
  UrlClassifierTestUtils.cleanupTestTrackers();
});

pushPref("privacy.trackingprotection.enabled", true);
pushPref("devtools.webconsole.groupWarningMessages", true);

add_task(async function testContentBlockingMessage() {
  const CONTENT_BLOCKING_GROUP_LABEL = "Content blocked messages";

  // Enable groupWarning and persist log
  await pushPref("devtools.webconsole.persistlog", true);

  const hud = await openNewTabAndConsole(TEST_URI);

  info("Log a few content blocking messages and simple ones");
  let onContentBlockingWarningMessage = waitForMessage(hud, BLOCKED_URL, ".warn");
  emitStorageAccessBlockedMessage(hud);
  await onContentBlockingWarningMessage;
  await logString(hud, "simple message 1");
  let onContentBlockingWarningGroupMessage =
    waitForMessage(hud, CONTENT_BLOCKING_GROUP_LABEL, ".warn");
  emitStorageAccessBlockedMessage(hud);
  const warningGroupMessage1 = (await onContentBlockingWarningGroupMessage).node;
  await logString(hud, "simple message 2");
  emitStorageAccessBlockedMessage(hud);
  await waitForBadgeNumber(warningGroupMessage1, "3");
  emitStorageAccessBlockedMessage(hud);
  await waitForBadgeNumber(warningGroupMessage1, "4");

  info("Reload the page and wait for it to be ready");
  const onDomContentLoaded = BrowserTestUtils.waitForContentEvent(
    hud.target.tab.linkedBrowser, "DOMContentLoaded", true);
  ContentTask.spawn(gBrowser.selectedBrowser, null, () => {
    content.location.reload();
  });
  await onDomContentLoaded;

  // Wait for the navigation message to be displayed.
  await waitFor(() => findMessage(hud, "Navigated to"));
  onContentBlockingWarningMessage = waitForMessage(hud, BLOCKED_URL, ".warn");
  emitStorageAccessBlockedMessage(hud);
  await onContentBlockingWarningMessage;
  await logString(hud, "simple message 3");
  onContentBlockingWarningGroupMessage =
    waitForMessage(hud, CONTENT_BLOCKING_GROUP_LABEL, ".warn");
  emitStorageAccessBlockedMessage(hud);
  const warningGroupMessage2 = (await onContentBlockingWarningGroupMessage).node;
  emitStorageAccessBlockedMessage(hud);
  await waitForBadgeNumber(warningGroupMessage2, "3");

  checkConsoleOutputForWarningGroup(hud, [
    `▶︎⚠ ${CONTENT_BLOCKING_GROUP_LABEL}`,
    `simple message 1`,
    `simple message 2`,
    `Navigated to`,
    `▶︎⚠ ${CONTENT_BLOCKING_GROUP_LABEL}`,
    `simple message 3`,
  ]);

  info("Filter warnings");
  await setFilterState(hud, {warn: false});
  await waitFor(() => !findMessage(hud, CONTENT_BLOCKING_GROUP_LABEL));

  checkConsoleOutputForWarningGroup(hud, [
    `simple message 1`,
    `simple message 2`,
    `Navigated to`,
    `simple message 3`,
  ]);

  info("Display warning messages again");
  await setFilterState(hud, {warn: true});
  await waitFor(() => findMessage(hud, CONTENT_BLOCKING_GROUP_LABEL));

  checkConsoleOutputForWarningGroup(hud, [
    `▶︎⚠ ${CONTENT_BLOCKING_GROUP_LABEL}`,
    `simple message 1`,
    `simple message 2`,
    `Navigated to`,
    `▶︎⚠ ${CONTENT_BLOCKING_GROUP_LABEL}`,
    `simple message 3`,
  ]);

  info("Expand the first warning group");
  findMessages(hud, CONTENT_BLOCKING_GROUP_LABEL)[0].querySelector(".arrow").click();
  await waitFor(() => findMessage(hud, BLOCKED_URL));

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

  info("Filter warnings");
  await setFilterState(hud, {warn: false});
  await waitFor(() => !findMessage(hud, CONTENT_BLOCKING_GROUP_LABEL));

  checkConsoleOutputForWarningGroup(hud, [
    `simple message 1`,
    `simple message 2`,
    `Navigated to`,
    `simple message 3`,
  ]);

  info("Display warning messages again");
  await setFilterState(hud, {warn: true});
  await waitFor(() => findMessage(hud, CONTENT_BLOCKING_GROUP_LABEL));
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

  info("Filter on warning group text");
  await setFilterState(hud, {text: CONTENT_BLOCKING_GROUP_LABEL});
  await waitFor(() => !findMessage(hud, "simple message"));
  checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${CONTENT_BLOCKING_GROUP_LABEL}`,
    `| ${BLOCKED_URL}?1`,
    `| ${BLOCKED_URL}?2`,
    `| ${BLOCKED_URL}?3`,
    `| ${BLOCKED_URL}?4`,
    `Navigated to`,
    `▶︎⚠ ${CONTENT_BLOCKING_GROUP_LABEL}`,
  ]);

  info("Open the second warning group");
  findMessages(hud, CONTENT_BLOCKING_GROUP_LABEL)[1].querySelector(".arrow").click();
  await waitFor(() => findMessage(hud, "?6"));

  checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${CONTENT_BLOCKING_GROUP_LABEL}`,
    `| ${BLOCKED_URL}?1`,
    `| ${BLOCKED_URL}?2`,
    `| ${BLOCKED_URL}?3`,
    `| ${BLOCKED_URL}?4`,
    `Navigated to`,
    `▼︎⚠ ${CONTENT_BLOCKING_GROUP_LABEL}`,
    `| ${BLOCKED_URL}?5`,
    `| ${BLOCKED_URL}?6`,
    `| ${BLOCKED_URL}?7`,
  ]);

  info("Filter on warning message text from a single warning group");
  await setFilterState(hud, {text: "/\\?(2|4)/"});
  await waitFor(() => !findMessage(hud, "?1"));
  checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${CONTENT_BLOCKING_GROUP_LABEL}`,
    `| ${BLOCKED_URL}?2`,
    `| ${BLOCKED_URL}?4`,
    `Navigated to`,
  ]);

  info("Filter on warning message text from two warning groups");
  await setFilterState(hud, {text: "/\\?(3|6|7)/"});
  await waitFor(() => findMessage(hud, "?7"));
  checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${CONTENT_BLOCKING_GROUP_LABEL}`,
    `| ${BLOCKED_URL}?3`,
    `Navigated to`,
    `▼︎⚠ ${CONTENT_BLOCKING_GROUP_LABEL}`,
    `| ${BLOCKED_URL}?6`,
    `| ${BLOCKED_URL}?7`,
  ]);

  info("Clearing text filter");
  await setFilterState(hud, {text: ""});
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
    `| ${BLOCKED_URL}?7`,
    `simple message 3`,
  ]);

  info("Filter warnings with two opened warning groups");
  await setFilterState(hud, {warn: false});
  await waitFor(() => !findMessage(hud, CONTENT_BLOCKING_GROUP_LABEL));
  checkConsoleOutputForWarningGroup(hud, [
    `simple message 1`,
    `simple message 2`,
    `Navigated to`,
    `simple message 3`,
  ]);

  info("Display warning messages again with two opened warning groups");
  await setFilterState(hud, {warn: true});
  await waitFor(() => findMessage(hud, CONTENT_BLOCKING_GROUP_LABEL));
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
    `| ${BLOCKED_URL}?7`,
    `simple message 3`,
  ]);
});

let cpt = 0;
/**
 * Emit a Content Blocking message. This is done by loading an image from an origin
 * tagged as tracker. The image is loaded with a incremented counter query parameter
 * each time so we can get the warning message.
 */
function emitStorageAccessBlockedMessage(hud) {
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

function waitForBadgeNumber(message, expectedNumber) {
  return waitFor(() =>
    message.querySelector(".warning-group-badge").textContent == expectedNumber);
}
