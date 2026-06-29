// SPDX-License-Identifier: CC0-1.0
// @colocated-env browser
// @ts-check
/// <reference path="../../@types/mochitest-compat.d.ts" />

// Upstream path:
//   browser/base/content/test/notificationbox/browser_notification_stacking.js
// Upstream source:
//   https://hg.mozilla.org/mozilla-central/raw-file/tip/browser/base/content/test/notificationbox/browser_notification_stacking.js
// Retrieved:
//   2026-06-28
// Original test type:
//   Firefox browser-chrome
// Local changes:
//   Renamed to Floorp colocated format, added provenance and type reference,
//   resolved the browser notification box from the browser window, replaced
//   BrowserTestUtils.waitForNotificationInNotificationBox and Assert.deepEqual
//   with local test helpers, and moved cleanup to registerCleanupFunction.

/**
 * @typedef {{
 *   PRIORITY_INFO_MEDIUM: number;
 *   PRIORITY_WARNING_MEDIUM: number;
 *   PRIORITY_CRITICAL_MEDIUM: number;
 *   stack: Element;
 *   appendNotification: (
 *     value: string,
 *     options: { label: string; priority: number },
 *   ) => Element;
 *   removeAllNotifications: (immediate?: boolean) => void;
 * }} NotificationBoxLike
 */

/**
 * @typedef {{
 *   closed?: boolean;
 *   close?: () => void;
 *   focus?: () => void;
 *   gBrowser?: GBrowserWithNotificationBox;
 *   gNotificationBox?: NotificationBoxLike;
 * }} BrowserWindowLike
 */

/**
 * @typedef {{
 *   getNotificationBox: (browser?: unknown) => NotificationBoxLike;
 *   getBrowserForTab?: (tab: XULElement) => unknown;
 *   selectedTab?: XULElement;
 * }} GBrowserWithNotificationBox
 */

/**
 * @param {BrowserWindowLike} browserWindow
 * @returns {NotificationBoxLike}
 */
function browserNotificationBox(browserWindow) {
  if (!browserWindow.gNotificationBox) {
    throw new Error("browser window should expose gNotificationBox");
  }
  return browserWindow.gNotificationBox;
}

/**
 * @param {() => boolean} predicate
 * @param {string} message
 * @param {number} [timeoutMs]
 */
async function waitForCondition(predicate, message, timeoutMs = 5000) {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    if (predicate()) {
      ok(true, message);
      return;
    }
    await new Promise((resolve) => setTimeout(resolve, 25));
  }
  ok(predicate(), message);
}

/**
 * @param {GBrowserWithNotificationBox} browser
 * @param {XULElement} tab
 */
function browserForTab(browser, tab) {
  const tabWithBrowser = /** @type {{ linkedBrowser?: unknown }} */ (
    /** @type {unknown} */ (tab)
  );
  if (tabWithBrowser.linkedBrowser) {
    return tabWithBrowser.linkedBrowser;
  }

  return browser.getBrowserForTab?.(tab);
}

/**
 * @param {NotificationBoxLike} box
 * @param {string} value
 */
async function waitForNotificationInNotificationBox(box, value) {
  await waitForCondition(
    () =>
      [...box.stack.children].some((child) =>
        child.getAttribute("value") === value
      ),
    `notification ${value} should be in the notification box`,
  );
}

/**
 * @param {Element[]} actual
 * @param {Element[]} expected
 * @param {string} message
 */
function isElementOrder(actual, expected, message) {
  is(actual.length, expected.length, `${message}: length`);
  for (let index = 0; index < expected.length; index++) {
    is(actual[index], expected[index], `${message}: item ${index}`);
  }
}

/**
 * @param {NotificationBoxLike} box
 * @param {string} label
 * @param {string} value
 * @param {"INFO" | "WARNING" | "CRITICAL"} priorityName
 */
async function addNotification(box, label, value, priorityName) {
  const priority = box[`PRIORITY_${priorityName}_MEDIUM`];
  const notification = box.appendNotification(value, { label, priority });
  await waitForNotificationInNotificationBox(box, value);
  return notification;
}

add_task(async function testStackingOrder() {
  const browserWindow = /** @type {BrowserWindowLike} */ (
    /** @type {unknown} */ (window)
  );
  /** @type {NotificationBoxLike | null} */
  let browserBox = null;
  /** @type {NotificationBoxLike | null} */
  let tabNotificationBox = null;

  registerCleanupFunction(() => {
    browserBox?.removeAllNotifications(true);
    tabNotificationBox?.removeAllNotifications(true);
  });

  const gBrowserWithNotificationBox = browserWindow.gBrowser;
  if (!gBrowserWithNotificationBox) {
    throw new Error("browser window should expose gBrowser");
  }

  browserBox = browserNotificationBox(browserWindow);
  await waitForCondition(
    () => Boolean(gBrowserWithNotificationBox.selectedTab),
    "browser window should select a tab",
  );

  const testTab = gBrowserWithNotificationBox.selectedTab;
  if (!testTab) {
    throw new Error("browser window should expose a selected tab");
  }
  const testBrowser = browserForTab(gBrowserWithNotificationBox, testTab);
  if (!testBrowser) {
    throw new Error("browser window tab should expose a browser");
  }
  tabNotificationBox = gBrowserWithNotificationBox.getNotificationBox(
    testBrowser,
  );

  ok(
    browserBox.stack.hasAttribute("prepend-notifications"),
    "Browser stack will prepend",
  );
  ok(
    !tabNotificationBox.stack.hasAttribute("prepend-notifications"),
    "Tab stack will append",
  );

  const browserOne = await addNotification(
    browserBox,
    "My first browser notification",
    "browser-one",
    "INFO",
  );

  const tabOne = await addNotification(
    tabNotificationBox,
    "My first tab notification",
    "tab-one",
    "CRITICAL",
  );

  const browserTwo = await addNotification(
    browserBox,
    "My second browser notification",
    "browser-two",
    "CRITICAL",
  );
  const browserThree = await addNotification(
    browserBox,
    "My third browser notification",
    "browser-three",
    "WARNING",
  );

  const tabTwo = await addNotification(
    tabNotificationBox,
    "My second tab notification",
    "tab-two",
    "INFO",
  );
  const tabThree = await addNotification(
    tabNotificationBox,
    "My third tab notification",
    "tab-three",
    "WARNING",
  );

  isElementOrder(
    [browserThree, browserTwo, browserOne],
    [...browserBox.stack.children],
    "Browser notifications prepended",
  );
  isElementOrder(
    [tabOne, tabTwo, tabThree],
    [...tabNotificationBox.stack.children],
    "Tab notifications appended",
  );
});
