/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const testPageURL =
  "https://example.com/browser/dom/quota/test/browser_permissionsPrompt.html";

add_task(async function testPermissionDenied() {
  removePermission(testPageURL, "persistent-storage");
  info("Creating tab");
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  info("Loading test page: " + testPageURL);
  gBrowser.selectedBrowser.loadURI(testPageURL);
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  registerPopupEventHandler("popupshowing", function () {
    ok(true, "prompt showing");
  });
  registerPopupEventHandler("popupshown", function () {
    ok(true, "prompt shown");
    triggerSecondaryCommand(this);
  });
  registerPopupEventHandler("popuphidden", function () {
    ok(true, "prompt hidden");
  });

  await promiseMessage(false, gBrowser);

  is(getPermission(testPageURL, "persistent-storage"),
     Components.interfaces.nsIPermissionManager.DENY_ACTION,
     "Correct permission set");
  unregisterAllPopupEventHandlers();
  gBrowser.removeCurrentTab();
  // Keep persistent-storage permission for the next test.
});

add_task(async function testNoPermissionPrompt() {
  info("Creating tab");
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  info("Loading test page: " + testPageURL);
  gBrowser.selectedBrowser.loadURI(testPageURL);
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  registerPopupEventHandler("popupshowing", function () {
    ok(false, "Shouldn't show a popup this time");
  });
  registerPopupEventHandler("popupshown", function () {
    ok(false, "Shouldn't show a popup this time");
  });
  registerPopupEventHandler("popuphidden", function () {
    ok(false, "Shouldn't show a popup this time");
  });

  await promiseMessage(false, gBrowser);

  is(getPermission(testPageURL, "persistent-storage"),
     Components.interfaces.nsIPermissionManager.DENY_ACTION,
     "Correct permission set");
  unregisterAllPopupEventHandlers();
  gBrowser.removeCurrentTab();
  removePermission(testPageURL, "persistent-storage");
});

add_task(async function testPermissionDeniedDismiss() {
  info("Creating tab");
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  info("Loading test page: " + testPageURL);
  gBrowser.selectedBrowser.loadURI(testPageURL);
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  registerPopupEventHandler("popupshowing", function () {
    ok(true, "prompt showing");
  });
  registerPopupEventHandler("popupshown", function () {
    ok(true, "prompt shown");
    // Dismiss permission prompt.
    dismissNotification(this);
  });
  registerPopupEventHandler("popuphidden", function () {
    ok(true, "prompt hidden");
  });

  await promiseMessage(false, gBrowser);

  is(getPermission(testPageURL, "persistent-storage"),
     Components.interfaces.nsIPermissionManager.DENY_ACTION,
     "Correct permission set");
  unregisterAllPopupEventHandlers();
  gBrowser.removeCurrentTab();
  removePermission(testPageURL, "persistent-storage");
});
