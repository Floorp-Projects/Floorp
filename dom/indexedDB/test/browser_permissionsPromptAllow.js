/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const testPageURL = "http://mochi.test:8888/browser/" +
  "dom/indexedDB/test/browser_permissionsPrompt.html";
const notificationID = "indexedDB-permissions-prompt";

add_task(async function test1() {
  // We want a prompt.
  removePermission(testPageURL, "indexedDB");

  info("creating tab");
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  registerPopupEventHandler("popupshowing", function() {
    ok(true, "prompt showing");
  });
  registerPopupEventHandler("popupshown", function() {
    ok(true, "prompt shown");
    triggerMainCommand(this);
  });
  registerPopupEventHandler("popuphidden", function() {
    ok(true, "prompt hidden");
  });

  info("loading test page: " + testPageURL);
  gBrowser.selectedBrowser.loadURI(testPageURL);

  await waitForMessage(true, gBrowser);
  is(getPermission(testPageURL, "indexedDB"),
     Components.interfaces.nsIPermissionManager.ALLOW_ACTION,
     "Correct permission set");
  gBrowser.removeCurrentTab();
});

add_task(async function test2() {
  info("creating tab");
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  registerPopupEventHandler("popupshowing", function() {
    ok(false, "Shouldn't show a popup this time");
  });
  registerPopupEventHandler("popupshown", function() {
    ok(false, "Shouldn't show a popup this time");
  });
  registerPopupEventHandler("popuphidden", function() {
    ok(false, "Shouldn't show a popup this time");
  });

  info("loading test page: " + testPageURL);
  gBrowser.selectedBrowser.loadURI(testPageURL);

  await waitForMessage(true, gBrowser);
  is(getPermission(testPageURL, "indexedDB"),
     Components.interfaces.nsIPermissionManager.ALLOW_ACTION,
     "Correct permission set");
  gBrowser.removeCurrentTab();
  unregisterAllPopupEventHandlers();
  removePermission(testPageURL, "indexedDB");
});
