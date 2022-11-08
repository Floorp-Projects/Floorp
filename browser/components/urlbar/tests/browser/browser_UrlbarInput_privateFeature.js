/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that _loadURL correctly sets and passes on the `private` window
// attribute (or not) with various arguments.

add_task(async function privateFeatureSetOnNewWindowImplicitly() {
  let privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  let newWinOpened = BrowserTestUtils.waitForNewWindow();

  privateWin.gURLBar._loadURL("about:blank", null, "window", {});

  let newWin = await newWinOpened;
  Assert.equal(
    newWin.docShell.treeOwner
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIAppWindow).chromeFlags &
      Ci.nsIWebBrowserChrome.CHROME_PRIVATE_WINDOW,
    Ci.nsIWebBrowserChrome.CHROME_PRIVATE_WINDOW,
    "New window opened from existing private window should be marked as private"
  );
  await BrowserTestUtils.closeWindow(newWin);
  await BrowserTestUtils.closeWindow(privateWin);
});

add_task(async function privateFeatureSetOnNewWindowExplicitly() {
  let privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  let newWinOpened = BrowserTestUtils.waitForNewWindow();

  privateWin.gURLBar._loadURL("about:blank", null, "window", { private: true });

  let newWin = await newWinOpened;
  Assert.equal(
    newWin.docShell.treeOwner
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIAppWindow).chromeFlags &
      Ci.nsIWebBrowserChrome.CHROME_PRIVATE_WINDOW,
    Ci.nsIWebBrowserChrome.CHROME_PRIVATE_WINDOW,
    "New window opened from existing private window should be marked as private"
  );
  await BrowserTestUtils.closeWindow(newWin);
  await BrowserTestUtils.closeWindow(privateWin);
});

add_task(async function privateFeatureNotSetOnNewWindowExplicitly() {
  let privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  let newWinOpened = BrowserTestUtils.waitForNewWindow();

  privateWin.gURLBar._loadURL("about:blank", null, "window", {
    private: false,
  });

  let newWin = await newWinOpened;
  Assert.notEqual(
    newWin.docShell.treeOwner
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIAppWindow).chromeFlags &
      Ci.nsIWebBrowserChrome.CHROME_PRIVATE_WINDOW,
    Ci.nsIWebBrowserChrome.CHROME_PRIVATE_WINDOW,
    "New window opened from existing private window should be marked as private"
  );
  await BrowserTestUtils.closeWindow(newWin);
  await BrowserTestUtils.closeWindow(privateWin);
});
