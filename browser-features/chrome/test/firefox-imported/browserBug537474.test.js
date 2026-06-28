// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
// @ts-check
/// <reference path="../../@types/mochitest-compat.d.ts" />

// Upstream path:
//   browser/base/content/test/general/browser_bug537474.js
// Upstream source:
//   https://hg.mozilla.org/mozilla-central/raw-file/tip/browser/base/content/test/general/browser_bug537474.js
// Retrieved:
//   2026-06-28
// Original test type:
//   Firefox browser-chrome
// Local changes:
//   Renamed to Floorp colocated format, added provenance and type reference,
//   opened a dedicated tab before exercising OPEN_CURRENTWINDOW, and made tab
//   cleanup explicit for the Floorp runner.

/**
 * @typedef {Window & {
 *   browserDOMWindow: {
 *     openURI: (
 *       uri: unknown,
 *       opener: unknown,
 *       where: unknown,
 *       flags: unknown,
 *       triggeringPrincipal: unknown,
 *     ) => void;
 *   };
 * }} BrowserChromeWindow
 */

/**
 * @param {XULElement} target
 */
function hasTab(target) {
  for (const tab of gBrowser.tabs) {
    if (tab === target) {
      return true;
    }
  }
  return false;
}

add_task(async function testOpenUriInCurrentWindow() {
  /** @type {XULElement | null} */
  let tab = /** @type {XULElement} */ (
    await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank")
  );

  registerCleanupFunction(async () => {
    if (tab && hasTab(tab)) {
      await BrowserTestUtils.removeTab(tab);
      tab = null;
    }
  });

  const browserLoadedPromise = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    "about:mozilla",
  );
  const browserWindow = /** @type {BrowserChromeWindow} */ (window);
  browserWindow.browserDOMWindow.openURI(
    makeURI("about:mozilla"),
    null,
    Ci.nsIBrowserDOMWindow.OPEN_CURRENTWINDOW,
    null,
    Services.scriptSecurityManager.getSystemPrincipal(),
  );
  await browserLoadedPromise;

  is(
    gBrowser.currentURI?.spec,
    "about:mozilla",
    "page loads in the current content window",
  );
});
