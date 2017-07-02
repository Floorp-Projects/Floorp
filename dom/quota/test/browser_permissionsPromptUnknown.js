/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const testPageURL =
  "https://example.com/browser/dom/quota/test/browser_permissionsPrompt.html";

add_task(async function testPermissionUnknownInPrivateWindow() {
  removePermission(testPageURL, "persistent-storage");
  info("Creating private window");
  let win = await BrowserTestUtils.openNewBrowserWindow({ private : true });

  registerPopupEventHandler("popupshowing", function () {
    ok(false, "Shouldn't show a popup this time");
  }, win);
  registerPopupEventHandler("popupshown", function () {
    ok(false, "Shouldn't show a popup this time");
  }, win);
  registerPopupEventHandler("popuphidden", function () {
    ok(false, "Shouldn't show a popup this time");
  }, win);

  info("Creating private tab");
  win.gBrowser.selectedTab = win.gBrowser.addTab();

  info("Loading test page: " + testPageURL);
  win.gBrowser.selectedBrowser.loadURI(testPageURL);
  await waitForMessage(false, win.gBrowser);

  is(getPermission(testPageURL, "persistent-storage"),
     Components.interfaces.nsIPermissionManager.UNKNOWN_ACTION,
     "Correct permission set");
  unregisterAllPopupEventHandlers(win);
  win.gBrowser.removeCurrentTab();
  win.close();
  removePermission(testPageURL, "persistent-storage");
});
