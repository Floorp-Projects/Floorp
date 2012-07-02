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
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);

    setFinishedCallback(function(result, exception) {
      ok(result instanceof Components.interfaces.nsIIDBDatabase,
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
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);
  pb.privateBrowsingEnabled = true;

  executeSoon(test3);
}

function test3()
{
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function () {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);

    setFinishedCallback(function(result, exception) {
      ok(!result, "No database");
      is(exception, "InvalidStateError", "Correct exception");
      gBrowser.removeCurrentTab();

      executeSoon(test4);
    });
  }, true);
  content.location = testPageURL;
}

function test4()
{
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);
  pb.privateBrowsingEnabled = false;

  executeSoon(finish);
}
