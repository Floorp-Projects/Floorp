/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
function test() {
  // initialization
  waitForExplicitFinish();
  let windowsToClose = [];
  let innerID;
  let beforeEvents;
  let afterEvents;
  let storageShouldOccur;
  let consoleObserver;
  let testURI =
    "http://example.com/browser/dom/tests/browser/test-console-api.html";
  let ConsoleAPIStorage = Cc["@mozilla.org/consoleAPI-storage;1"]
                            .getService(Ci.nsIConsoleAPIStorage);

  function getInnerWindowId(aWindow) {
    return aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                  .getInterface(Ci.nsIDOMWindowUtils)
                  .currentInnerWindowID;
  }

  function whenNewWindowLoaded(aOptions, aCallback) {
    let win = OpenBrowserWindow(aOptions);
    win.addEventListener("load", function onLoad() {
      win.removeEventListener("load", onLoad, false);
      aCallback(win);
    }, false);
  }

  function doTest(aIsPrivateMode, aWindow, aCallback) {
    aWindow.gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
      aWindow.gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);

      consoleObserver = {
        observe: function(aSubject, aTopic, aData) {
          if (aTopic == "console-api-log-event") {
            afterEvents = ConsoleAPIStorage.getEvents(innerID);
            is(beforeEvents.length == afterEvents.length - 1, storageShouldOccur,
              "storage should" + (storageShouldOccur ? "" : " not") + " occur");

            executeSoon(function() {
              Services.obs.removeObserver(consoleObserver, "console-api-log-event");
              aCallback();
            });
          }
        }
      };

      aWindow.Services.obs.addObserver(
        consoleObserver, "console-api-log-event", false);
      aWindow.nativeConsole.log("foo bar baz (private: " + aIsPrivateMode + ")");
    }, true);

    // We expect that console API messages are always stored.
    storageShouldOccur = true;
    innerID = getInnerWindowId(aWindow);
    beforeEvents = ConsoleAPIStorage.getEvents(innerID);
    aWindow.gBrowser.selectedBrowser.loadURI(testURI);
  }

  function testOnWindow(aOptions, aCallback) {
    whenNewWindowLoaded(aOptions, function(aWin) {
      windowsToClose.push(aWin);
      // execute should only be called when need, like when you are opening
      // web pages on the test. If calling executeSoon() is not necesary, then
      // call whenNewWindowLoaded() instead of testOnWindow() on your test.
      executeSoon(() => aCallback(aWin));
    });
  };

   // this function is called after calling finish() on the test.
  registerCleanupFunction(function() {
    windowsToClose.forEach(function(aWin) {
      aWin.close();
    });
  });

  // test first when not on private mode
  testOnWindow({}, function(aWin) {
    doTest(false, aWin, function() {
      // then test when on private mode
      testOnWindow({private: true}, function(aWin) {
        doTest(true, aWin, finish);
      });
    });
  });
}
