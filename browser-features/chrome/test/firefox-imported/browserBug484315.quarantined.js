// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
// @ts-check
/// <reference path="../../@types/mochitest-compat.d.ts" />

// Upstream path:
//   browser/base/content/test/general/browser_bug484315.js
// Upstream source:
//   https://hg.mozilla.org/mozilla-central/raw-file/tip/browser/base/content/test/general/browser_bug484315.js
// Retrieved:
//   2026-06-28
// Mirror content id:
//   gecko-dev blob 21b4e69a33e66da44595e991142b4ca6a7b326a3
// Original test type:
//   Firefox browser-chrome
// Local changes:
//   Renamed to Floorp colocated format, added provenance and type reference,
//   waited for the newly opened browser window, and made pref/window cleanup
//   explicit for the Floorp runner.
// Quarantine:
//   Not discovered by the colocated runner because popup-window creation blocks
//   in some Floorp test profiles, while OpenBrowserWindow changes the original
//   Firefox popup semantics.

const CLOSE_WITH_LAST_TAB_PREF = "browser.tabs.closeWindowWithLastTab";

/**
 * @typedef {{
 *   closed: boolean;
 *   close: () => void;
 *   focus?: () => void;
 *   gBrowser?: {
 *     removeCurrentTab: () => void;
 *   };
 * }} BrowserWindowLike
 */

/**
 * @typedef {Window & {
 *   OpenBrowserWindow?: (options?: unknown) => BrowserWindowLike;
 * }} BrowserWindowOpener
 */

/**
 * @returns {BrowserWindowLike[]}
 */
function currentBrowserWindows() {
  const windows = [];
  const enumerator = Services.wm.getEnumerator("navigator:browser");
  while (enumerator.hasMoreElements()) {
    windows.push(/** @type {BrowserWindowLike} */ (enumerator.getNext()));
  }
  return windows;
}

/**
 * @param {BrowserWindowLike[]} previousWindows
 * @param {number} [timeoutMs]
 * @returns {Promise<BrowserWindowLike>}
 */
async function waitForNewBrowserWindow(previousWindows, timeoutMs = 5000) {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    for (const win of currentBrowserWindows()) {
      if (!previousWindows.includes(win)) {
        return win;
      }
    }
    await new Promise((resolve) => setTimeout(resolve, 25));
  }
  throw new Error("new browser window should open");
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
 * @param {BrowserWindowLike | null} excludedWindow
 */
function focusRemainingBrowserWindow(excludedWindow) {
  for (const browserWindow of currentBrowserWindows()) {
    if (browserWindow !== excludedWindow && !browserWindow.closed) {
      browserWindow.focus?.();
      return;
    }
  }
}

/**
 * @returns {BrowserWindowLike | null}
 */
function openTestBrowserWindow() {
  const opener = /** @type {BrowserWindowOpener} */ (window).OpenBrowserWindow;
  if (typeof opener === "function") {
    return opener.call(window, {});
  }

  return /** @type {BrowserWindowLike | null} */ (
    globalThis.open("about:blank", "", "width=100,height=100")
  );
}

add_task(async function test() {
  const previousWindows = currentBrowserWindows();
  const hadPref = Services.prefs.prefHasUserValue(CLOSE_WITH_LAST_TAB_PREF);
  const previousPrefValue = hadPref
    ? Services.prefs.getBoolPref(CLOSE_WITH_LAST_TAB_PREF)
    : undefined;

  /** @type {BrowserWindowLike | null} */
  let win = null;

  registerCleanupFunction(() => {
    if (win && !win.closed) {
      win.close();
    }

    if (hadPref && previousPrefValue !== undefined) {
      Services.prefs.setBoolPref(CLOSE_WITH_LAST_TAB_PREF, previousPrefValue);
    } else {
      Services.prefs.clearUserPref(CLOSE_WITH_LAST_TAB_PREF);
    }
  });

  const openedWindow = openTestBrowserWindow();
  win = openedWindow && !previousWindows.includes(openedWindow)
    ? openedWindow
    : await waitForNewBrowserWindow(previousWindows);

  Services.prefs.setBoolPref(CLOSE_WITH_LAST_TAB_PREF, false);
  win.gBrowser?.removeCurrentTab();

  await waitForCondition(
    () => Boolean(win?.closed),
    "popup is closed",
  );
  focusRemainingBrowserWindow(win);
});
