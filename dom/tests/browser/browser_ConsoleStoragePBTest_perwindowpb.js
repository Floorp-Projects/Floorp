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
  let testURI =
    "http://example.com/browser/dom/tests/browser/test-console-api.html";
  let ConsoleAPIStorage = Cc["@mozilla.org/consoleAPI-storage;1"].getService(
    Ci.nsIConsoleAPIStorage
  );

  function getInnerWindowId(aWindow) {
    return aWindow.windowGlobalChild.innerWindowId;
  }

  function whenNewWindowLoaded(aOptions, aCallback) {
    let win = OpenBrowserWindow(aOptions);
    win.addEventListener(
      "load",
      function () {
        aCallback(win);
      },
      { once: true }
    );
  }

  function doTest(aIsPrivateMode, aWindow, aCallback) {
    BrowserTestUtils.browserLoaded(aWindow.gBrowser.selectedBrowser).then(
      () => {
        function observe(aSubject) {
          afterEvents = ConsoleAPIStorage.getEvents(innerID);
          is(
            beforeEvents.length == afterEvents.length - 1,
            storageShouldOccur,
            "storage should" + (storageShouldOccur ? "" : " not") + " occur"
          );

          executeSoon(function () {
            ConsoleAPIStorage.removeLogEventListener(observe);
            aCallback();
          });
        }

        ConsoleAPIStorage.addLogEventListener(
          observe,
          aWindow.document.nodePrincipal
        );
        aWindow.nativeConsole.log(
          "foo bar baz (private: " + aIsPrivateMode + ")"
        );
      }
    );

    // We expect that console API messages are always stored.
    storageShouldOccur = true;
    innerID = getInnerWindowId(aWindow);
    beforeEvents = ConsoleAPIStorage.getEvents(innerID);
    BrowserTestUtils.startLoadingURIString(
      aWindow.gBrowser.selectedBrowser,
      testURI
    );
  }

  function testOnWindow(aOptions, aCallback) {
    whenNewWindowLoaded(aOptions, function (aWin) {
      windowsToClose.push(aWin);
      // execute should only be called when need, like when you are opening
      // web pages on the test. If calling executeSoon() is not necesary, then
      // call whenNewWindowLoaded() instead of testOnWindow() on your test.
      executeSoon(() => aCallback(aWin));
    });
  }

  // this function is called after calling finish() on the test.
  registerCleanupFunction(function () {
    windowsToClose.forEach(function (aWin) {
      aWin.close();
    });
  });

  // test first when not on private mode
  testOnWindow({}, function (aWin) {
    doTest(false, aWin, function () {
      // then test when on private mode
      testOnWindow({ private: true }, function (aWin) {
        doTest(true, aWin, finish);
      });
    });
  });
}
