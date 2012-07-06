/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Make sure this is a unique origin or the tests will randomly fail!
const testPageURL = "http://bug702292.example.com/browser/" +
  "dom/indexedDB/test/browser_quotaPromptDelete.html";
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
let haveReset = false;
let secondTimeCount = 0;

function test1()
{
  gBrowser.selectedTab = gBrowser.addTab();

  gBrowser.selectedBrowser.addEventListener("load", function () {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);

    let seenPopupCount;

    setFinishedCallback(function(result) {
      is(result, "ready", "Got 'ready' result");

      setFinishedCallback(function(result) {
        if (result == "abort") {
          setFinishedCallback(function(result) {
            is(result, "resetDone", "Got 'resetDone' result");

            function secondTimeThroughCallback(result) {
              is(result, "complete", "Got 'complete' result");

              // If we hit the quota on the Nth iteration last time, we should
              // be able to go N-1 iterations without hitting it after
              // obliterating the db.
              if (++secondTimeCount < addMoreTest1Count - 1) {
                secondTimeThroughAddMore();
              } else {
                setFinishedCallback(function(result) {
                  is(result, "finished", "Got 'finished' result");
                  is(getPermission(testPageURL, "indexedDB-unlimited"),
                     Components.interfaces.nsIPermissionManager.DENY_ACTION,
                     "Correct permission set");
                  gBrowser.removeCurrentTab();
                  unregisterAllPopupEventHandlers();
                  executeSoon(finish);
                });
                executeSoon(function() { dispatchEvent("indexedDB-done"); });
              }
            }

            function secondTimeThroughAddMore() {
              setFinishedCallback(secondTimeThroughCallback);
              executeSoon(function() { dispatchEvent("indexedDB-addMore"); });
            }

            haveReset = true;
            secondTimeThroughAddMore();
          });
          executeSoon(function() { dispatchEvent("indexedDB-reset"); });
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
      ok(!haveReset, "Shouldn't get here twice!");
      triggerSecondaryCommand(this, 0);
    });
    registerPopupEventHandler("popuphidden", function () {
      ok(true, "prompt hidden");
    });

  }, true);

  info("loading test page: " + testPageURL);
  content.location = testPageURL;
}
