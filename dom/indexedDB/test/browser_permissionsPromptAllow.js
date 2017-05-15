/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const testPageURL = "http://mochi.test:8888/browser/" +
  "dom/indexedDB/test/browser_permissionsPrompt.html";
const notificationID = "indexedDB-permissions-prompt";

function test()
{
  waitForExplicitFinish();

  // We want a prompt.
  removePermission(testPageURL, "indexedDB");
  executeSoon(test1);
}

function test1()
{
  info("creating tab");
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  gBrowser.selectedBrowser.addEventListener("load", function () {
    setFinishedCallback(function(isIDBDatabase, exception) {
      ok(isIDBDatabase,
         "First database creation was successful");
      ok(!exception, "No exception");
      is(getPermission(testPageURL, "indexedDB"),
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

  info("loading test page: " + testPageURL);
  content.location = testPageURL;
}

function test2()
{
  info("creating tab");
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  gBrowser.selectedBrowser.addEventListener("load", function () {
    setFinishedCallback(function(isIDBDatabase, exception) {
      ok(isIDBDatabase,
         "First database creation was successful");
      ok(!exception, "No exception");
      is(getPermission(testPageURL, "indexedDB"),
         Components.interfaces.nsIPermissionManager.ALLOW_ACTION,
         "Correct permission set");
      gBrowser.removeCurrentTab();
      unregisterAllPopupEventHandlers();
      removePermission(testPageURL, "indexedDB");
      executeSoon(finish);
    });

    registerPopupEventHandler("popupshowing", function () {
      ok(false, "Shouldn't show a popup this time");
    });
    registerPopupEventHandler("popupshown", function () {
      ok(false, "Shouldn't show a popup this time");
    });
    registerPopupEventHandler("popuphidden", function () {
      ok(false, "Shouldn't show a popup this time");
    });

  }, {capture: true, once: true});

  info("loading test page: " + testPageURL);
  content.location = testPageURL;
}
