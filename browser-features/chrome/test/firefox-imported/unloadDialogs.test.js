// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
// @ts-check
/// <reference path="../../@types/mochitest-compat.d.ts" />

// Upstream path:
//   browser/base/content/test/general/browser_unloaddialogs.js
// Upstream source:
//   https://hg.mozilla.org/mozilla-central/raw-file/tip/browser/base/content/test/general/browser_unloaddialogs.js
// Retrieved:
//   2026-06-28
// Original test type:
//   Firefox browser-chrome
// Local changes:
//   Renamed to Floorp colocated format, added provenance and type reference,
//   and made tab cleanup explicit for the Floorp runner.

const TEST_URLS = [
  "data:text/html,<script>" +
  "function handle(evt) {" +
  "evt.target.removeEventListener(evt.type, handle, true);" +
  "try { alert('This should NOT appear'); } catch(e) { }" +
  "}" +
  "window.addEventListener('pagehide', handle, true);" +
  "window.addEventListener('beforeunload', handle, true);" +
  "window.addEventListener('unload', handle, true);" +
  "</script><body>Testing alert during pagehide/beforeunload/unload</body>",
  "data:text/html,<script>" +
  "function handle(evt) {" +
  "evt.target.removeEventListener(evt.type, handle, true);" +
  "try { prompt('This should NOT appear'); } catch(e) { }" +
  "}" +
  "window.addEventListener('pagehide', handle, true);" +
  "window.addEventListener('beforeunload', handle, true);" +
  "window.addEventListener('unload', handle, true);" +
  "</script><body>Testing prompt during pagehide/beforeunload/unload</body>",
  "data:text/html,<script>" +
  "function handle(evt) {" +
  "evt.target.removeEventListener(evt.type, handle, true);" +
  "try { confirm('This should NOT appear'); } catch(e) { }" +
  "}" +
  "window.addEventListener('pagehide', handle, true);" +
  "window.addEventListener('beforeunload', handle, true);" +
  "window.addEventListener('unload', handle, true);" +
  "</script><body>Testing confirm during pagehide/beforeunload/unload</body>",
];

/** @type {XULElement[]} */
const openedTabs = [];

registerCleanupFunction(async () => {
  while (openedTabs.length > 0) {
    const tab = openedTabs.pop();
    if (tab) {
      await BrowserTestUtils.removeTab(tab);
    }
  }
});

add_task(async function testUnloadDialogsDoNotAppear() {
  for (const url of TEST_URLS) {
    const tab = /** @type {XULElement} */ (
      await BrowserTestUtils.openNewForegroundTab(gBrowser, url, false)
    );
    openedTabs.push(tab);
    ok(true, `Loaded page ${url}`);

    // Wait one turn of the event loop before closing, so everything settles.
    await new Promise((resolve) => setTimeout(resolve, 0));

    await BrowserTestUtils.removeTab(tab);
    openedTabs.pop();
    ok(true, `Closed page ${url} without timeout`);
  }
});
