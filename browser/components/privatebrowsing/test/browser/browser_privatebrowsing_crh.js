/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that the Clear Recent History menu item and command
// is disabled inside the private browsing mode.

add_task(async function test() {
  function checkDisableOption(aPrivateMode, aWindow) {
    let crhCommand = aWindow.document.getElementById("Tools:Sanitize");
    ok(crhCommand, "The clear recent history command should exist");

    is(
      PrivateBrowsingUtils.isWindowPrivate(aWindow),
      aPrivateMode,
      "PrivateBrowsingUtils should report the correct per-window private browsing status"
    );
    is(
      crhCommand.hasAttribute("disabled"),
      aPrivateMode,
      "Clear Recent History command should be disabled according to the private browsing mode"
    );
  }

  let testURI = "http://mochi.test:8888/";

  let privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  let privateBrowser = privateWin.gBrowser.selectedBrowser;
  BrowserTestUtils.loadURI(privateBrowser, testURI);
  await BrowserTestUtils.browserLoaded(privateBrowser);

  info("Test on private window");
  checkDisableOption(true, privateWin);

  let win = await BrowserTestUtils.openNewBrowserWindow();
  let browser = win.gBrowser.selectedBrowser;
  BrowserTestUtils.loadURI(browser, testURI);
  await BrowserTestUtils.browserLoaded(browser);

  info("Test on public window");
  checkDisableOption(false, win);

  // Cleanup
  await BrowserTestUtils.closeWindow(privateWin);
  await BrowserTestUtils.closeWindow(win);
});
