/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const DEFAULT_THEME = "default-theme@mozilla.org";

const {AddonManager} = ChromeUtils.import("resource://gre/modules/AddonManager.jsm");

async function selectTheme(id) {
  let theme = await AddonManager.getAddonByID(id || DEFAULT_THEME);
  await theme.enable();
}

registerCleanupFunction(() => {
  return selectTheme(null);
});

add_task(async function withoutLWT() {
  let win = await BrowserTestUtils.openNewBrowserWindow();
  ok(!win.gBrowser.tabContainer.hasAttribute("overflow"), "tab container not overflowing");
  ok(win.gBrowser.tabContainer.arrowScrollbox.hasAttribute("notoverflowing"), "arrow scrollbox not overflowing");
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function withLWT() {
  await selectTheme("firefox-compact-light@mozilla.org");
  let win = await BrowserTestUtils.openNewBrowserWindow();
  ok(!win.gBrowser.tabContainer.hasAttribute("overflow"), "tab container not overflowing");
  ok(win.gBrowser.tabContainer.arrowScrollbox.hasAttribute("notoverflowing"), "arrow scrollbox not overflowing");
  await BrowserTestUtils.closeWindow(win);
});

