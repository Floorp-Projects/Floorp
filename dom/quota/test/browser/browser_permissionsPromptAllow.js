/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const testPageURL =
  "https://example.com/browser/dom/quota/test/browser/permissionsPrompt.html";

addTest(async function testPermissionAllow() {
  removePermission(testPageURL, "persistent-storage");

  registerPopupEventHandler("popupshowing", function () {
    ok(true, "prompt showing");
  });
  registerPopupEventHandler("popupshown", function () {
    ok(true, "prompt shown");
    triggerMainCommand(this);
  });
  registerPopupEventHandler("popuphidden", function () {
    ok(true, "prompt hidden");
  });

  info("Creating tab");
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  info("Loading test page: " + testPageURL);
  BrowserTestUtils.startLoadingURIString(gBrowser.selectedBrowser, testPageURL);
  await waitForMessage(true, gBrowser);

  is(
    getPermission(testPageURL, "persistent-storage"),
    Ci.nsIPermissionManager.ALLOW_ACTION,
    "Correct permission set"
  );
  gBrowser.removeCurrentTab();
  unregisterAllPopupEventHandlers();
  // Keep persistent-storage permission for the next test.
});

addTest(async function testNoPermissionPrompt() {
  registerPopupEventHandler("popupshowing", function () {
    ok(false, "Shouldn't show a popup this time");
  });
  registerPopupEventHandler("popupshown", function () {
    ok(false, "Shouldn't show a popup this time");
  });
  registerPopupEventHandler("popuphidden", function () {
    ok(false, "Shouldn't show a popup this time");
  });

  info("Creating tab");
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  info("Loading test page: " + testPageURL);
  BrowserTestUtils.startLoadingURIString(gBrowser.selectedBrowser, testPageURL);
  await waitForMessage(true, gBrowser);

  is(
    getPermission(testPageURL, "persistent-storage"),
    Ci.nsIPermissionManager.ALLOW_ACTION,
    "Correct permission set"
  );
  gBrowser.removeCurrentTab();
  unregisterAllPopupEventHandlers();
  removePermission(testPageURL, "persistent-storage");
});
