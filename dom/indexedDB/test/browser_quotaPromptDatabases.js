/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Make sure this is a unique origin or the tests will randomly fail!
const testPageURL = "http://bug704464-3.example.com/browser/" +
  "dom/indexedDB/test/browser_quotaPromptDatabases.html";
const notificationID = "indexedDB-quota-prompt";

function test()
{
  waitForExplicitFinish();
  requestLongerTimeout(10);
  removePermission(testPageURL, "indexedDB-unlimited");
  Services.prefs.setIntPref("dom.indexedDB.warningQuota", 2);
  executeSoon(test1);
}

let addMoreTest1Count = 0;

function test1()
{
  gBrowser.selectedTab = gBrowser.addTab();

  gBrowser.selectedBrowser.addEventListener("load", function () {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);

    let seenPopupCount;

    setFinishedCallback(function(result) {
      is(result, "ready", "Got 'ready' result");

      setFinishedCallback(function(result) {
        is(result, "complete", "Got 'complete' result");

        if (addMoreTest1Count >= seenPopupCount + 5) {
          setFinishedCallback(function(result) {
            is(result, "finished", "Got 'finished' result");
            is(getPermission(testPageURL, "indexedDB-unlimited"),
               Components.interfaces.nsIPermissionManager.ALLOW_ACTION,
               "Correct permission set");
            gBrowser.removeCurrentTab();
            unregisterAllPopupEventHandlers();
            addMoreTest1Count = seenPopupCount;
            executeSoon(finish);
          });
          executeSoon(function() { dispatchEvent("indexedDB-done"); });
        }
        else {
          ++addMoreTest1Count;
          executeSoon(function() { dispatchEvent("indexedDB-addMore"); });
        }
      });
      ++addMoreTest1Count;
      executeSoon(function() { dispatchEvent("indexedDB-addMore"); });
    });

    registerPopupEventHandler("popupshowing", function () {
      ok(true, "prompt showing");
      seenPopupCount = addMoreTest1Count - 1;
    });
    registerPopupEventHandler("popupshown", function () {
      ok(true, "prompt shown");
      triggerMainCommand(this);
    });
    registerPopupEventHandler("popuphidden", function () {
      ok(true, "prompt hidden");
    });

  }, true);

  info("loading test page: " + testPageURL);
  content.location = testPageURL;
}
