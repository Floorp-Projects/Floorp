/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Make sure this is a unique origin or the tests will randomly fail!
const testPageURL = "http://bug704464-1.example.com/browser/" +
  "dom/indexedDB/test/browser_quotaPrompt.html";
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

        if (addMoreTest1Count > seenPopupCount + 5) {
          setFinishedCallback(function(result) {
            is(result, "finished", "Got 'finished' result");
            is(getPermission(testPageURL, "indexedDB-unlimited"),
               Components.interfaces.nsIPermissionManager.ALLOW_ACTION,
               "Correct permission set");
            gBrowser.removeCurrentTab();
            unregisterAllPopupEventHandlers();
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
      seenPopupCount = addMoreTest1Count;
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
  content.location = testPageURL + "?v=1";
}

function test2()
{
  gBrowser.selectedTab = gBrowser.addTab();

  gBrowser.selectedBrowser.addEventListener("load", function () {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);

    let seenPopup;
    let addMoreCount = 0;

    setFinishedCallback(function(result) {
      is(result, "ready", "Got 'ready' result");
      is(getPermission(testPageURL, "indexedDB-unlimited"),
         Components.interfaces.nsIPermissionManager.ALLOW_ACTION,
         "Correct permission set");

      setFinishedCallback(function(result) {
        is(result, "complete", "Got 'complete' result");
        ok(!seenPopup, "No popup");
        is(getPermission(testPageURL, "indexedDB-unlimited"),
           Components.interfaces.nsIPermissionManager.ALLOW_ACTION,
           "Correct permission set");

        if (addMoreCount > addMoreTest1Count + 5) {
          setFinishedCallback(function(result) {
            is(result, "finished", "Got 'finished' result");
            ok(!seenPopup, "No popup");
            is(getPermission(testPageURL, "indexedDB-unlimited"),
               Components.interfaces.nsIPermissionManager.ALLOW_ACTION,
               "Correct permission set");

            gBrowser.removeCurrentTab();
            unregisterAllPopupEventHandlers();
            removePermission(testPageURL, "indexedDB");
            Services.prefs.clearUserPref("dom.indexedDB.warningQuota");
            executeSoon(finish);
          });
          executeSoon(function() { dispatchEvent("indexedDB-done"); });
        }
        else {
          ++addMoreCount;
          executeSoon(function() { dispatchEvent("indexedDB-addMore"); });
        }
      });
      ++addMoreCount;
      executeSoon(function() { dispatchEvent("indexedDB-addMore"); });
    });

    registerPopupEventHandler("popupshowing", function () {
      ok(false, "Shouldn't show a popup this time");
      seenPopup = true;
    });
    registerPopupEventHandler("popupshown", function () {
      ok(false, "Shouldn't show a popup this time");
    });
    registerPopupEventHandler("popuphidden", function () {
      ok(false, "Shouldn't show a popup this time");
    });

  }, true);

  info("loading test page: " + testPageURL);
  content.location = testPageURL + "?v=3";
}
