/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const testWorkerURL = "http://mochi.test:8888/browser/" +
  "dom/indexedDB/test/browser_permissionsWorker.html";
const testSharedWorkerURL = "http://mochi.test:8888/browser/" +
  "dom/indexedDB/test/browser_permissionsSharedWorker.html";
const notificationID = "indexedDB-permissions-prompt";

add_task(async function test1() {
  // We want a prompt.
  removePermission(testWorkerURL, "indexedDB");
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

  info("creating tab");
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  info("loading test page: " + testWorkerURL);
  gBrowser.selectedBrowser.loadURI(testWorkerURL);

  await waitForMessage("ok", gBrowser);
  is(getPermission(testWorkerURL, "indexedDB"),
     Components.interfaces.nsIPermissionManager.ALLOW_ACTION,
     "Correct permission set");
  gBrowser.removeCurrentTab();
});

add_task(async function test2() {
  // We want a prompt.
  removePermission(testSharedWorkerURL, "indexedDB");

  registerPopupEventHandler("popupshowing", function() {
    ok(false, "prompt showing");
  });
  registerPopupEventHandler("popupshown", function() {
    ok(false, "prompt shown");
  });
  registerPopupEventHandler("popuphidden", function() {
    ok(false, "prompt hidden");
  });

  info("creating tab");
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);


  info("loading test page: " + testSharedWorkerURL);
  gBrowser.selectedBrowser.loadURI(testSharedWorkerURL);
  await waitForMessage("InvalidStateError", gBrowser);
  is(getPermission(testSharedWorkerURL, "indexedDB"),
     Components.interfaces.nsIPermissionManager.UNKNOWN_ACTION,
     "Correct permission set");
  gBrowser.removeCurrentTab();
});
