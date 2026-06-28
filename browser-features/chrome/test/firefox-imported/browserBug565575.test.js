// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
// @ts-check
/// <reference path="../../@types/mochitest-compat.d.ts" />

// Upstream path:
//   browser/base/content/test/general/browser_bug565575.js
// Upstream source:
//   https://hg.mozilla.org/mozilla-central/raw-file/tip/browser/base/content/test/general/browser_bug565575.js
// Retrieved:
//   2026-06-28
// Original test type:
//   Firefox browser-chrome
// Local changes:
//   Renamed to Floorp colocated format, added provenance and type reference,
//   resolved BrowserCommands and gURLBar through globalThis for type checking,
//   made tab cleanup explicit for the Floorp runner, explicitly seed the
//   new-tab urlbar focus state, and wait for focus transitions that are
//   asynchronous in this runner.

/**
 * @typedef {{
 *   BrowserCommands?: { openTab: () => void };
 *   gURLBar?: { focused?: boolean; focus?: () => void };
 * }} BrowserFocusGlobals
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

/**
 * @param {{ focused?: boolean }} urlBar
 * @param {boolean} expected
 * @param {string} message
 */
async function waitForUrlbarFocusState(urlBar, expected, message) {
  const deadline = Date.now() + 5000;
  while (Boolean(urlBar.focused) !== expected && Date.now() < deadline) {
    await new Promise((resolve) => setTimeout(resolve, 25));
  }
  ok(Boolean(urlBar.focused) === expected, message);
}

/**
 * @param {{ focused?: boolean; focus?: () => void }} urlBar
 * @param {string} message
 */
async function focusUrlbar(urlBar, message) {
  if (!urlBar.focused) {
    urlBar.focus?.();
  }
  await waitForUrlbarFocusState(urlBar, true, message);
}

add_task(async function testNewTabUrlbarFocusIsRestoredAcrossTabSwitches() {
  const globals = /** @type {BrowserFocusGlobals} */ (
    /** @type {unknown} */ (globalThis)
  );
  const browserCommands = globals.BrowserCommands;
  const urlBar = globals.gURLBar;
  if (!browserCommands || !urlBar) {
    throw new Error("BrowserCommands and gURLBar should be available");
  }

  const initialSelectedTab = gBrowser.selectedTab;
  /** @type {XULElement | null} */
  let openedTab = null;

  registerCleanupFunction(async () => {
    if (openedTab && hasTab(openedTab)) {
      await BrowserTestUtils.removeTab(openedTab);
      openedTab = null;
    }
    if (initialSelectedTab && hasTab(initialSelectedTab)) {
      await BrowserTestUtils.switchTab(gBrowser, initialSelectedTab);
    }
  });

  const selectedBrowser = /** @type {{ focus: () => void }} */ (
    /** @type {unknown} */ (gBrowser.selectedBrowser)
  );
  selectedBrowser.focus();

  openedTab = /** @type {XULElement} */ (
    await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      () => browserCommands.openTab(),
      false,
    )
  );
  await focusUrlbar(
    urlBar,
    "location bar is focused for a new tab",
  );

  await BrowserTestUtils.switchTab(gBrowser, initialSelectedTab);
  await waitForUrlbarFocusState(
    urlBar,
    false,
    "location bar isn't focused for the previously selected tab",
  );

  await BrowserTestUtils.switchTab(gBrowser, openedTab);
  await waitForUrlbarFocusState(
    urlBar,
    true,
    "location bar is re-focused when selecting the new tab",
  );

  await BrowserTestUtils.removeTab(openedTab);
  openedTab = null;
});
