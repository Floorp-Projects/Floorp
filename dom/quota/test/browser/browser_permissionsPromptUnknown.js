/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const testPageURL =
  "https://example.com/browser/dom/quota/test/browser/permissionsPrompt.html";

addTest(async function testPermissionUnknownInPrivateWindow() {
  removePermission(testPageURL, "persistent-storage");
  info("Creating private window");
  let win = await BrowserTestUtils.openNewBrowserWindow({ private: true });

  registerPopupEventHandler(
    "popupshowing",
    function() {
      ok(false, "Shouldn't show a popup this time");
    },
    win
  );
  registerPopupEventHandler(
    "popupshown",
    function() {
      ok(false, "Shouldn't show a popup this time");
    },
    win
  );
  registerPopupEventHandler(
    "popuphidden",
    function() {
      ok(false, "Shouldn't show a popup this time");
    },
    win
  );

  info("Creating private tab");
  win.gBrowser.selectedTab = BrowserTestUtils.addTab(win.gBrowser);

  info("Loading test page: " + testPageURL);
  BrowserTestUtils.loadURI(win.gBrowser.selectedBrowser, testPageURL);
  await waitForMessage(false, win.gBrowser);

  is(
    getPermission(testPageURL, "persistent-storage"),
    Ci.nsIPermissionManager.UNKNOWN_ACTION,
    "Correct permission set"
  );
  unregisterAllPopupEventHandlers(win);
  win.gBrowser.removeCurrentTab();
  await BrowserTestUtils.closeWindow(win);
  win = null;
  removePermission(testPageURL, "persistent-storage");
});
