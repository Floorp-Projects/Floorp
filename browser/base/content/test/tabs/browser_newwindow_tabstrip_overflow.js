/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {LightweightThemeManager} = ChromeUtils.import("resource://gre/modules/LightweightThemeManager.jsm", {});
registerCleanupFunction(() => {
  LightweightThemeManager.currentTheme = null;
  Services.prefs.clearUserPref("lightweightThemes.usedThemes");
});

add_task(async function withoutLWT() {
  let win = await BrowserTestUtils.openNewBrowserWindow();
  ok(!win.gBrowser.tabContainer.hasAttribute("overflow"), "tab container not overflowing");
  ok(win.gBrowser.tabContainer.arrowScrollbox.hasAttribute("notoverflowing"), "arrow scrollbox not overflowing");
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function withLWT() {
  LightweightThemeManager.currentTheme = LightweightThemeManager.getUsedTheme("firefox-compact-light@mozilla.org");
  let win = await BrowserTestUtils.openNewBrowserWindow();
  ok(!win.gBrowser.tabContainer.hasAttribute("overflow"), "tab container not overflowing");
  ok(win.gBrowser.tabContainer.arrowScrollbox.hasAttribute("notoverflowing"), "arrow scrollbox not overflowing");
  await BrowserTestUtils.closeWindow(win);
});

