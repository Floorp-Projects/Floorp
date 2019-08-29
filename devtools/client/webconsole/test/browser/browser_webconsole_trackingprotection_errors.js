/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Load a page with tracking elements that get blocked and make sure that a
// 'learn more' link shows up in the webconsole.

"use strict";
requestLongerTimeout(2);

const TEST_PATH = "browser/devtools/client/webconsole/test/browser/";
const TEST_FILE = TEST_PATH + "test-trackingprotection-securityerrors.html";
const TEST_URI = "http://example.com/" + TEST_FILE;
const TRACKER_URL = "http://tracking.example.org/";
const BLOCKED_URL = `\u201c${TRACKER_URL +
  TEST_PATH +
  "cookieSetter.html"}\u201d`;

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

registerCleanupFunction(function() {
  UrlClassifierTestUtils.cleanupTestTrackers();
});

pushPref("devtools.webconsole.groupWarningMessages", false);

add_task(async function testContentBlockingMessage() {
  await UrlClassifierTestUtils.addTestTrackers();

  await pushPref("privacy.trackingprotection.enabled", true);
  const hud = await openNewTabAndConsole(TRACKER_URL + TEST_FILE);

  info("Test content blocking message");
  const message = await waitFor(() =>
    findMessage(
      hud,
      `The resource at \u201chttp://tracking.example.com/\u201d was blocked because ` +
        `content blocking is enabled`
    )
  );

  await testLearnMoreClickOpenNewTab(
    message,
    "https://developer.mozilla.org/Firefox/Privacy/Tracking_Protection" +
      DOCS_GA_PARAMS
  );
});

add_task(async function testForeignCookieBlockedMessage() {
  info("Test foreign cookie blocked message");
  // Bug 1518138: GC heuristics are broken for this test, so that the test
  // ends up running out of memory. Try to work-around the problem by GCing
  // before the test begins.
  Cu.forceShrinkingGC();
  // We change the pref and open a new window to ensure it will be taken into account.
  await pushPref(COOKIE_BEHAVIOR_PREF, COOKIE_BEHAVIORS.REJECT_FOREIGN);
  const { hud, win } = await openNewWindowAndConsole(TEST_URI);
  const message = await waitFor(() =>
    findMessage(
      hud,
      `Request to access cookie or storage on ${BLOCKED_URL} was blocked because we are ` +
        `blocking all third-party storage access requests and content blocking is enabled`
    )
  );
  await testLearnMoreClickOpenNewTab(
    message,
    getStorageErrorUrl("CookieBlockedForeign")
  );
  win.close();
});

add_task(async function testLimitForeignCookieBlockedMessage() {
  info("Test unvisited eTLD foreign cookies blocked message");
  // Bug 1518138: GC heuristics are broken for this test, so that the test
  // ends up running out of memory. Try to work-around the problem by GCing
  // before the test begins.
  Cu.forceShrinkingGC();
  // We change the pref and open a new window to ensure it will be taken into account.
  await pushPref(COOKIE_BEHAVIOR_PREF, COOKIE_BEHAVIORS.LIMIT_FOREIGN);
  const { hud, win } = await openNewWindowAndConsole(TEST_URI);

  const message = await waitFor(() =>
    findMessage(
      hud,
      `Request to access cookie or storage on ${BLOCKED_URL} was blocked because we are ` +
        `blocking all third-party storage access requests and content blocking is enabled`
    )
  );
  await testLearnMoreClickOpenNewTab(
    message,
    getStorageErrorUrl("CookieBlockedForeign")
  );
  win.close();
});

add_task(async function testAllCookieBlockedMessage() {
  info("Test all cookies blocked message");
  // We change the pref and open a new window to ensure it will be taken into account.
  await pushPref(COOKIE_BEHAVIOR_PREF, COOKIE_BEHAVIORS.REJECT);
  const { hud, win } = await openNewWindowAndConsole(TEST_URI);

  const message = await waitFor(() =>
    findMessage(
      hud,
      `Request to access cookie or storage on ${BLOCKED_URL} was blocked because we are ` +
        `blocking all storage access requests`
    )
  );
  await testLearnMoreClickOpenNewTab(
    message,
    getStorageErrorUrl("CookieBlockedAll")
  );
  win.close();
});

add_task(async function testTrackerCookieBlockedMessage() {
  info("Test tracker cookie blocked message");
  // We change the pref and open a new window to ensure it will be taken into account.
  await pushPref(COOKIE_BEHAVIOR_PREF, COOKIE_BEHAVIORS.REJECT_TRACKER);
  const { hud, win } = await openNewWindowAndConsole(TEST_URI);

  const message = await waitFor(() =>
    findMessage(
      hud,
      `Request to access cookie or storage on ${BLOCKED_URL} was blocked because it came ` +
        `from a tracker and content blocking is enabled`
    )
  );
  await testLearnMoreClickOpenNewTab(
    message,
    getStorageErrorUrl("CookieBlockedTracker")
  );
  win.close();
});

add_task(async function testCookieBlockedByPermissionMessage() {
  info("Test cookie blocked by permission message");
  // Turn off tracking protection and add a block permission on the URL.
  await pushPref("privacy.trackingprotection.enabled", false);
  const p = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
    TRACKER_URL
  );
  Services.perms.addFromPrincipal(
    p,
    "cookie",
    Ci.nsIPermissionManager.DENY_ACTION
  );

  const { hud, win } = await openNewWindowAndConsole(TEST_URI);
  const message = await waitFor(() =>
    findMessage(
      hud,
      `Request to access cookies or ` +
        `storage on ${BLOCKED_URL} was blocked because of custom cookie permission`
    )
  );
  await testLearnMoreClickOpenNewTab(
    message,
    getStorageErrorUrl("CookieBlockedByPermission")
  );
  win.close();

  // Remove the custom permission.
  Services.perms.removeFromPrincipal(p, "cookie");
});

function getStorageErrorUrl(category) {
  const BASE_STORAGE_ERROR_URL =
    "https://developer.mozilla.org/docs/Mozilla/Firefox/" +
    "Privacy/Storage_access_policy/Errors/";
  const STORAGE_ERROR_URL_PARAMS = new URLSearchParams({
    utm_source: "devtools",
    utm_medium: "firefox-cookie-errors",
    utm_campaign: "default",
  }).toString();
  return `${BASE_STORAGE_ERROR_URL}${category}?${STORAGE_ERROR_URL_PARAMS}`;
}

async function testLearnMoreClickOpenNewTab(message, expectedUrl) {
  info("Clicking on the Learn More link");

  const learnMoreLink = message.querySelector(".learn-more-link");
  const linkSimulation = await simulateLinkClick(learnMoreLink);
  checkLink({
    ...linkSimulation,
    expectedLink: expectedUrl,
    expectedTab: "tab",
  });
}

function checkLink({ link, where, expectedLink, expectedTab }) {
  is(link, expectedLink, `Clicking the provided link opens ${link}`);
  is(where, expectedTab, `Clicking the provided link opens in expected tab`);
}
