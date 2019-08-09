/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Load a page with tracking elements that get blocked and make sure that a
// 'learn more' link shows up in the webconsole.

"use strict";
requestLongerTimeout(2);

const TEST_FILE =
  "browser/devtools/client/webconsole/test/browser/test-warning-groups.html";
const TEST_URI = "http://example.com/" + TEST_FILE;

const TRACKER_URL = "http://tracking.example.com/";
const IMG_FILE =
  "browser/devtools/client/webconsole/test/browser/test-image.png";
const TRACKER_IMG = "http://tracking.example.org/" + IMG_FILE;

const CONTENT_BLOCKING_GROUP_LABEL =
  "The resource at “<URL>” was blocked because content blocking is enabled.";

const COOKIE_BEHAVIOR_PREF = "network.cookie.cookieBehavior";
const COOKIE_BEHAVIORS = {
  // reject all third-party cookies
  REJECT_FOREIGN: 1,
  // reject all cookies
  REJECT: 2,
  // reject third-party cookies unless the eTLD already has at least one cookie
  LIMIT_FOREIGN: 3,
  // reject trackers
  REJECT_TRACKER: 4,
};

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
  const { hud, tab, win } = await openNewWindowAndConsole(
    "http://tracking.example.org/" + TEST_FILE
  );
  const now = Date.now();

  info("Test content blocking message");
  const message =
    `The resource at \u201chttp://tracking.example.com/?1&${now}\u201d ` +
    `was blocked because content blocking is enabled`;
  const onContentBlockingWarningMessage = waitForMessage(hud, message, ".warn");
  emitContentBlockingMessage(tab, `${TRACKER_URL}?1&${now}`);
  await onContentBlockingWarningMessage;

  ok(true, "The content blocking message was displayed");

  info(
    "Emit a new content blocking message to check that it causes a grouping"
  );
  const onContentBlockingWarningGroupMessage = waitForMessage(
    hud,
    CONTENT_BLOCKING_GROUP_LABEL,
    ".warn"
  );
  emitContentBlockingMessage(tab, `${TRACKER_URL}?2&${now}`);
  const { node } = await onContentBlockingWarningGroupMessage;
  is(
    node.querySelector(".warning-group-badge").textContent,
    "2",
    "The badge has the expected text"
  );

  checkConsoleOutputForWarningGroup(hud, [
    `▶︎⚠ ${CONTENT_BLOCKING_GROUP_LABEL} 2`,
  ]);

  info("Open the group");
  node.querySelector(".arrow").click();
  await waitFor(() => findMessage(hud, "http://tracking.example.com/?1"));

  checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${CONTENT_BLOCKING_GROUP_LABEL} 2`,
    `| The resource at \u201chttp://tracking.example.com/?1&${now}\u201d was blocked`,
    `| The resource at \u201chttp://tracking.example.com/?2&${now}\u201d was blocked`,
  ]);
  await win.close();
});

add_task(async function testForeignCookieBlockedMessage() {
  info("Test foreign cookie blocked message");
  // We change the pref and open a new window to ensure it will be taken into account.
  await pushPref(COOKIE_BEHAVIOR_PREF, COOKIE_BEHAVIORS.REJECT_FOREIGN);
  await testStorageAccessBlockedGrouping(
    "Request to access cookie or storage on " +
      "“<URL>” was blocked because we are blocking all third-party storage access " +
      "requests and content blocking is enabled."
  );
});

add_task(async function testLimitForeignCookieBlockedMessage() {
  info("Test unvisited eTLD foreign cookies blocked message");
  // We change the pref and open a new window to ensure it will be taken into account.
  await pushPref(COOKIE_BEHAVIOR_PREF, COOKIE_BEHAVIORS.LIMIT_FOREIGN);
  await testStorageAccessBlockedGrouping(
    "Request to access cookie or storage on " +
      "“<URL>” was blocked because we are blocking all third-party storage access " +
      "requests and content blocking is enabled."
  );
});

add_task(async function testAllCookieBlockedMessage() {
  info("Test all cookies blocked message");
  // We change the pref and open a new window to ensure it will be taken into account.
  await pushPref(COOKIE_BEHAVIOR_PREF, COOKIE_BEHAVIORS.REJECT);
  await testStorageAccessBlockedGrouping(
    "Request to access cookie or storage on " +
      "“<URL>” was blocked because we are blocking all storage access requests."
  );
});

add_task(async function testTrackerCookieBlockedMessage() {
  info("Test tracker cookie blocked message");
  // We change the pref and open a new window to ensure it will be taken into account.
  await pushPref(COOKIE_BEHAVIOR_PREF, COOKIE_BEHAVIORS.REJECT_TRACKER);
  await testStorageAccessBlockedGrouping(
    "Request to access cookie or storage on " +
      "“<URL>” was blocked because it came from a tracker and content blocking is " +
      "enabled."
  );
});

add_task(async function testCookieBlockedByPermissionMessage() {
  info("Test cookie blocked by permission message");
  // Turn off tracking protection and add a block permission on the URL.
  await pushPref("privacy.trackingprotection.enabled", false);
  const p = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
    "http://tracking.example.org/"
  );
  Services.perms.addFromPrincipal(
    p,
    "cookie",
    Ci.nsIPermissionManager.DENY_ACTION
  );

  await testStorageAccessBlockedGrouping(
    "Request to access cookies or storage on " +
      "“<URL>” was blocked because of custom cookie permission."
  );

  // Remove the custom permission.
  Services.perms.removeFromPrincipal(p, "cookie");
});

/**
 * Test that storage access blocked messages are grouped by emitting 2 messages.
 *
 * @param {String} groupLabel: The warning group label that should be created.
 *                             It should contain "<URL>".
 */
async function testStorageAccessBlockedGrouping(groupLabel) {
  const { hud, win, tab } = await openNewWindowAndConsole(TEST_URI);
  const now = Date.now();

  hud.ui.clearOutput();

  const getWarningMessage = url => groupLabel.replace("<URL>", url);

  const onStorageAccessBlockedMessage = waitForMessage(
    hud,
    getWarningMessage(`${TRACKER_IMG}?1&${now}`),
    ".warn"
  );
  emitStorageAccessBlockedMessage(tab, `${TRACKER_IMG}?1&${now}`);
  await onStorageAccessBlockedMessage;

  info(
    "Emit a new content blocking message to check that it causes a grouping"
  );

  const onContentBlockingWarningGroupMessage = waitForMessage(
    hud,
    groupLabel,
    ".warn"
  );
  emitStorageAccessBlockedMessage(tab, `${TRACKER_IMG}?2&${now}`);
  const { node } = await onContentBlockingWarningGroupMessage;
  is(
    node.querySelector(".warning-group-badge").textContent,
    "2",
    "The badge has the expected text"
  );

  checkConsoleOutputForWarningGroup(hud, [`▶︎⚠ ${groupLabel} 2`]);

  info("Open the group");
  node.querySelector(".arrow").click();
  await waitFor(() => findMessage(hud, TRACKER_IMG));

  checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${groupLabel} 2`,
    `| ${getWarningMessage(TRACKER_IMG + "?1&" + now)}`,
    `| ${getWarningMessage(TRACKER_IMG + "?2&" + now)}`,
  ]);

  hud.ui.clearOutput();
  await win.close();
}

/**
 * Emit a Content Blocking message. This is done by loading an iframe from an origin
 * tagged as tracker. The image is loaded with a incremented counter query parameter
 * each time so we can get the warning message.
 */
function emitContentBlockingMessage(tab, url) {
  ContentTask.spawn(tab.linkedBrowser, url, function(innerURL) {
    content.wrappedJSObject.loadIframe(innerURL);
  });
}

/**
 * Emit a Storage blocked message. This is done by loading an image from an origin
 * tagged as tracker. The image is loaded with a incremented counter query parameter
 * each time so we can get the warning message.
 */
function emitStorageAccessBlockedMessage(tab, url) {
  ContentTask.spawn(tab.linkedBrowser, url, async function(innerURL) {
    content.wrappedJSObject.loadImage(innerURL);
  });
}
