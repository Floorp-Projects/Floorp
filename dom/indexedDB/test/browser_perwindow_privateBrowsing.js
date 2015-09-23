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
  // Avoids the actual prompt
  setPermission(testPageURL, "indexedDB");
  executeSoon(test1);
}

function test1()
{
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function () {
    if (content.location != testPageURL) {
      content.location = testPageURL;
      return;
    }
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);

    setFinishedCallback(function(result, exception) {
      ok(result instanceof IDBDatabase,
         "First database creation was successful");
      ok(!exception, "No exception");
      gBrowser.removeCurrentTab();

      executeSoon(test2);
    });
  }, true);
  content.location = testPageURL;
}

function test2()
{
  var win = OpenBrowserWindow({private: true});
  win.addEventListener("load", function onLoad() {
    win.removeEventListener("load", onLoad, false);
    executeSoon(() => test3(win));
  }, false);
  registerCleanupFunction(() => win.close());
}

function test3(win)
{
  win.gBrowser.selectedTab = win.gBrowser.addTab();
  win.gBrowser.selectedBrowser.addEventListener("load", function () {
    if (win.content.location != testPageURL) {
      win.content.location = testPageURL;
      return;
    }
    win.gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);

    setFinishedCallback(function(result, exception) {
      ok(!result, "No database");
      is(exception, "InvalidStateError", "Correct exception");
      win.gBrowser.removeCurrentTab();

      executeSoon(finish);
    }, win);
  }, true);
  win.content.location = testPageURL;
}
