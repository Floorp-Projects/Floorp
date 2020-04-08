/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that filtering the console output when there are warning groups works as expected.

"use strict";
requestLongerTimeout(2);

const { PrefObserver } = require("devtools/client/shared/prefs");

const TEST_FILE =
  "browser/devtools/client/webconsole/test/browser/test-warning-groups.html";
const TEST_URI = "http://example.org/" + TEST_FILE;

const TRACKER_URL = "http://tracking.example.com/";
const BLOCKED_URL =
  TRACKER_URL +
  "browser/devtools/client/webconsole/test/browser/test-image.png";
const WARNING_GROUP_PREF = "devtools.webconsole.groupWarningMessages";

const { UrlClassifierTestUtils } = ChromeUtils.import(
  "resource://testing-common/UrlClassifierTestUtils.jsm"
);
UrlClassifierTestUtils.addTestTrackers();
registerCleanupFunction(function() {
  UrlClassifierTestUtils.cleanupTestTrackers();
});

pushPref("privacy.trackingprotection.enabled", true);

const CONTENT_BLOCKING_GROUP_LABEL =
  "The resource at “<URL>” was blocked because content blocking is enabled.";

add_task(async function testContentBlockingMessage() {
  // Enable persist log
  await pushPref("devtools.webconsole.persistlog", true);

  // Start with the warningGroup pref set to false.
  await pushPref(WARNING_GROUP_PREF, false);

  const hud = await openNewTabAndConsole(TEST_URI);

  info("Log a few content blocking messages and simple ones");
  let onContentBlockingWarningMessage = waitForMessage(
    hud,
    `${BLOCKED_URL}?1`,
    ".warn"
  );
  emitContentBlockedMessage(hud);
  await onContentBlockingWarningMessage;
  await logString(hud, "simple message 1");

  onContentBlockingWarningMessage = waitForMessage(
    hud,
    `${BLOCKED_URL}?2`,
    ".warn"
  );
  emitContentBlockedMessage(hud);
  await onContentBlockingWarningMessage;

  onContentBlockingWarningMessage = waitForMessage(
    hud,
    `${BLOCKED_URL}?3`,
    ".warn"
  );
  emitContentBlockedMessage(hud);
  await onContentBlockingWarningMessage;

  checkConsoleOutputForWarningGroup(hud, [
    `${BLOCKED_URL}?1`,
    `simple message 1`,
    `${BLOCKED_URL}?2`,
    `${BLOCKED_URL}?3`,
  ]);

  info("Enable the warningGroup feature pref and check warnings were grouped");
  await toggleWarningGroupPreference(hud);
  let warningGroupMessage1 = await waitFor(() =>
    findMessage(hud, CONTENT_BLOCKING_GROUP_LABEL)
  );
  is(
    warningGroupMessage1.querySelector(".warning-group-badge").textContent,
    "3",
    "The badge has the expected text"
  );

  checkConsoleOutputForWarningGroup(hud, [
    `▶︎⚠ ${CONTENT_BLOCKING_GROUP_LABEL}`,
    `simple message 1`,
  ]);

  info("Add a new warning message and check it's placed in the closed group");
  emitContentBlockedMessage(hud);
  await waitForBadgeNumber(warningGroupMessage1, "4");

  info(
    "Re-enable the warningGroup feature pref and check warnings are displayed"
  );
  await toggleWarningGroupPreference(hud);
  await waitFor(() => findMessage(hud, `${BLOCKED_URL}?4`));

  // Warning messages are displayed at the expected positions.
  checkConsoleOutputForWarningGroup(hud, [
    `${BLOCKED_URL}?1`,
    `simple message 1`,
    `${BLOCKED_URL}?2`,
    `${BLOCKED_URL}?3`,
    `${BLOCKED_URL}?4`,
  ]);

  info("Re-disable the warningGroup feature pref");
  await toggleWarningGroupPreference(hud);
  console.log("toggle successful");
  warningGroupMessage1 = await waitFor(() =>
    findMessage(hud, CONTENT_BLOCKING_GROUP_LABEL)
  );

  checkConsoleOutputForWarningGroup(hud, [
    `▶︎⚠ ${CONTENT_BLOCKING_GROUP_LABEL}`,
    `simple message 1`,
  ]);

  info("Expand the warning group");
  warningGroupMessage1.querySelector(".arrow").click();
  await waitFor(() => findMessage(hud, BLOCKED_URL));

  checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${CONTENT_BLOCKING_GROUP_LABEL}`,
    `| ${BLOCKED_URL}?1`,
    `| ${BLOCKED_URL}?2`,
    `| ${BLOCKED_URL}?3`,
    `| ${BLOCKED_URL}?4`,
    `simple message 1`,
  ]);

  info("Reload the page and wait for it to be ready");
  await reloadPage();

  // Wait for the navigation message to be displayed.
  await waitFor(() => findMessage(hud, "Navigated to"));

  info("Disable the warningGroup feature pref again");
  await toggleWarningGroupPreference(hud);

  info("Add one warning message and one simple message");
  await waitFor(() => findMessage(hud, `${BLOCKED_URL}?4`));
  onContentBlockingWarningMessage = waitForMessage(hud, BLOCKED_URL, ".warn");
  emitContentBlockedMessage(hud);
  await onContentBlockingWarningMessage;
  await logString(hud, "simple message 2");

  // nothing is grouped.
  checkConsoleOutputForWarningGroup(hud, [
    `${BLOCKED_URL}?1`,
    `simple message 1`,
    `${BLOCKED_URL}?2`,
    `${BLOCKED_URL}?3`,
    `${BLOCKED_URL}?4`,
    `Navigated to`,
    `${BLOCKED_URL}?5`,
    `simple message 2`,
  ]);

  info(
    "Enable the warningGroup feature pref to check that the group is still expanded"
  );
  await toggleWarningGroupPreference(hud);
  await waitFor(() => findMessage(hud, CONTENT_BLOCKING_GROUP_LABEL));

  checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${CONTENT_BLOCKING_GROUP_LABEL}`,
    `| ${BLOCKED_URL}?1`,
    `| ${BLOCKED_URL}?2`,
    `| ${BLOCKED_URL}?3`,
    `| ${BLOCKED_URL}?4`,
    `simple message 1`,
    `Navigated to`,
    `| ${BLOCKED_URL}?5`,
    `simple message 2`,
  ]);

  info(
    "Add a second warning and check it's placed in the second, closed, group"
  );
  const onContentBlockingWarningGroupMessage = waitForMessage(
    hud,
    CONTENT_BLOCKING_GROUP_LABEL,
    ".warn"
  );
  emitContentBlockedMessage(hud);
  const warningGroupMessage2 = (await onContentBlockingWarningGroupMessage)
    .node;
  await waitForBadgeNumber(warningGroupMessage2, "2");

  checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${CONTENT_BLOCKING_GROUP_LABEL}`,
    `| ${BLOCKED_URL}?1`,
    `| ${BLOCKED_URL}?2`,
    `| ${BLOCKED_URL}?3`,
    `| ${BLOCKED_URL}?4`,
    `simple message 1`,
    `Navigated to`,
    `▶︎⚠ ${CONTENT_BLOCKING_GROUP_LABEL}`,
    `simple message 2`,
  ]);

  info(
    "Disable the warningGroup pref and check all warning messages are visible"
  );
  await toggleWarningGroupPreference(hud);
  await waitFor(() => findMessage(hud, `${BLOCKED_URL}?6`));

  checkConsoleOutputForWarningGroup(hud, [
    `${BLOCKED_URL}?1`,
    `simple message 1`,
    `${BLOCKED_URL}?2`,
    `${BLOCKED_URL}?3`,
    `${BLOCKED_URL}?4`,
    `Navigated to`,
    `${BLOCKED_URL}?5`,
    `simple message 2`,
    `${BLOCKED_URL}?6`,
  ]);

  // Clean the pref for the next tests.
  Services.prefs.clearUserPref(WARNING_GROUP_PREF);
});

let cpt = 0;
/**
 * Emit a Content Blocking message. This is done by loading an image from an origin
 * tagged as tracker. The image is loaded with a incremented counter query parameter
 * each time so we can get the warning message.
 */
function emitContentBlockedMessage(hud) {
  const url = `${BLOCKED_URL}?${++cpt}`;
  SpecialPowers.spawn(gBrowser.selectedBrowser, [url], function(innerURL) {
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
  SpecialPowers.spawn(gBrowser.selectedBrowser, [str], function(arg) {
    content.console.log(arg);
  });
  return onMessage;
}

function waitForBadgeNumber(message, expectedNumber) {
  return waitFor(
    () =>
      message.querySelector(".warning-group-badge").textContent ==
      expectedNumber
  );
}

async function toggleWarningGroupPreference(hud) {
  info("Open the settings panel");
  const observer = new PrefObserver("");

  info("Change warning preference");
  const prefChanged = observer.once(WARNING_GROUP_PREF, () => {});

  await toggleConsoleSetting(
    hud,
    ".webconsole-console-settings-menu-item-warning-groups"
  );

  await prefChanged;
  observer.destroy();
}
