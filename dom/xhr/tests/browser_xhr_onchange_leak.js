/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// Bug 1336811 - An XHR that has a .onreadystatechange waiting should
// not leak forever once the tab is closed. CC optimizations need to be
// turned off once it is closed.

add_task(async function test() {
  // We need to reuse the content process when we navigate so the entire process
  // with the possible-leaking window doesn't get torn down.
  await SpecialPowers.pushPrefEnv({
    set: [["dom.ipc.keepProcessesAlive.webIsolated.perOrigin", 1]],
  });

  const url =
    "http://mochi.test:8888/browser/dom/xhr/tests/browser_xhr_onchange_leak.html";
  let newTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  let browser = gBrowser.selectedBrowser;
  let pageShowPromise = BrowserTestUtils.waitForContentEvent(
    browser,
    "pageshow",
    true
  );
  BrowserTestUtils.startLoadingURIString(browser, "http://mochi.test:8888/");
  await pageShowPromise;

  ok(pageShowPromise, "need to check something");
  BrowserTestUtils.removeTab(newTab);
});
