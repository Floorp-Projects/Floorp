/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const testWorkerURL = "http://mochi.test:8888/browser/" +
  "dom/indexedDB/test/browser_permissionsWorker.html";
const testSharedWorkerURL = "http://mochi.test:8888/browser/" +
  "dom/indexedDB/test/browser_permissionsSharedWorker.html";
const notificationID = "indexedDB-permissions-prompt";

function test()
{
  waitForExplicitFinish();
  executeSoon(test1);
}

function test1()
{
  // We want a prompt.
  removePermission(testWorkerURL, "indexedDB");

  info("creating tab");
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  gBrowser.selectedBrowser.addEventListener("load", function () {
    setFinishedCallback(function(isIDBDatabase, exception) {
      ok(isIDBDatabase, "First database creation was successful");
      ok(!exception, "No exception");
      is(getPermission(testWorkerURL, "indexedDB"),
         Components.interfaces.nsIPermissionManager.ALLOW_ACTION,
         "Correct permission set");
      gBrowser.removeCurrentTab();
      executeSoon(test2);
    });

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

  }, {capture: true, once: true});

  info("loading test page: " + testWorkerURL);
  content.location = testWorkerURL;
}

function test2()
{
  // We want a prompt.
  removePermission(testSharedWorkerURL, "indexedDB");

  info("creating tab");
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  gBrowser.selectedBrowser.addEventListener("load", function () {
    setFinishedCallback(function(isIDBDatabase, exception) {
      ok(!isIDBDatabase, "First database creation was successful");
      ok(exception, "No exception");
      is(getPermission(testSharedWorkerURL, "indexedDB"),
         Components.interfaces.nsIPermissionManager.UNKNOWN_ACTION,
         "Correct permission set");
      gBrowser.removeCurrentTab();
      executeSoon(finish);
    });

    registerPopupEventHandler("popupshowing", function () {
      ok(false, "prompt showing");
    });
    registerPopupEventHandler("popupshown", function () {
      ok(false, "prompt shown");
    });
    registerPopupEventHandler("popuphidden", function () {
      ok(false, "prompt hidden");
    });

  }, {capture: true, once: true});

  info("loading test page: " + testSharedWorkerURL);
  content.location = testSharedWorkerURL;
}
