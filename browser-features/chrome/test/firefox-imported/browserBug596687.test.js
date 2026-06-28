// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
// @ts-check
/// <reference path="../../@types/mochitest-compat.d.ts" />

// Upstream path:
//   browser/base/content/test/general/browser_bug596687.js
// Upstream source:
//   https://hg.mozilla.org/mozilla-central/raw-file/tip/browser/base/content/test/general/browser_bug596687.js
// Retrieved:
//   2026-06-28
// Original test type:
//   Firefox browser-chrome
// Local changes:
//   Renamed to Floorp colocated format, added provenance and type reference,
//   awaited the local BrowserTestUtils.removeTab subset, and made cleanup
//   tolerant of partial failures.

/**
 * @typedef {XULElement & {
 *   addEventListener: (type: string, listener: EventListener) => void;
 *   removeEventListener: (type: string, listener: EventListener) => void;
 * }} TabLike
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

add_task(async function test() {
  /** @type {TabLike | null} */
  let tab = /** @type {TabLike} */ (
    await BrowserTestUtils.openNewForegroundTab(gBrowser)
  );

  registerCleanupFunction(async () => {
    if (tab && hasTab(tab)) {
      await BrowserTestUtils.removeTab(tab);
      tab = null;
    }
  });

  let gotTabAttrModified = false;
  let gotTabClose = false;

  function onTabAttrModified() {
    gotTabAttrModified = true;
  }

  function onTabClose() {
    gotTabClose = true;
    tab?.addEventListener("TabAttrModified", onTabAttrModified);
  }

  tab.addEventListener("TabClose", onTabClose);

  await BrowserTestUtils.removeTab(tab);

  ok(gotTabClose, "should have got the TabClose event");
  ok(
    !gotTabAttrModified,
    "shouldn't have got the TabAttrModified event after TabClose",
  );

  tab.removeEventListener("TabClose", onTabClose);
  tab.removeEventListener("TabAttrModified", onTabAttrModified);
  tab = null;
});
