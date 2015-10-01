/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  let testURL = "http://example.org/browser/browser/base/content/test/general/dummy_page.html";

  function testOnWindow(aOptions, aCallback) {
    whenNewWindowLoaded(aOptions, function(aWin) {
      // execute should only be called when need, like when you are opening
      // web pages on the test. If calling executeSoon() is not necesary, then
      // call whenNewWindowLoaded() instead of testOnWindow() on your test.
      executeSoon(() => aCallback(aWin));
    });
  };

  testOnWindow({}, function(aNormalWindow) {
    testOnWindow({private: true}, function(aPrivateWindow) {
      runTest(aNormalWindow, aPrivateWindow, false, function() {
        aNormalWindow.close();
        aPrivateWindow.close();
        testOnWindow({}, function(aNormalWindow) {
          testOnWindow({private: true}, function(aPrivateWindow) {
            runTest(aPrivateWindow, aNormalWindow, false, function() {
              aNormalWindow.close();
              aPrivateWindow.close();
              testOnWindow({private: true}, function(aPrivateWindow) {
                runTest(aPrivateWindow, aPrivateWindow, false, function() {
                  aPrivateWindow.close();
                  testOnWindow({}, function(aNormalWindow) {
                    runTest(aNormalWindow, aNormalWindow, true, function() {
                      aNormalWindow.close();
                      finish();
                    });
                  });
                });
              });
            });
          });
        });
      });
    });
  });

  function runTest(aSourceWindow, aDestWindow, aExpectSwitch, aCallback) {
    // Open the base tab
    let baseTab = aSourceWindow.gBrowser.addTab(testURL);
    baseTab.linkedBrowser.addEventListener("load", function() {
      // Wait for the tab to be fully loaded so matching happens correctly
      if (baseTab.linkedBrowser.currentURI.spec == "about:blank")
        return;
      baseTab.linkedBrowser.removeEventListener("load", arguments.callee, true);

      let testTab = aDestWindow.gBrowser.addTab();

      waitForFocus(function() {
        // Select the testTab
        aDestWindow.gBrowser.selectedTab = testTab;

        // Ensure that this tab has no history entries
        ok(testTab.linkedBrowser.sessionHistory.count < 2,
           "The test tab has 1 or less history entries");
        // Ensure that this tab is on about:blank
        is(testTab.linkedBrowser.currentURI.spec, "about:blank",
           "The test tab is on about:blank");
        // Ensure that this tab's document has no child nodes
        ok(!testTab.linkedBrowser.contentDocument.body.hasChildNodes(),
           "The test tab has no child nodes");
        ok(!testTab.hasAttribute("busy"),
           "The test tab doesn't have the busy attribute");

        // Wait for the Awesomebar popup to appear.
        promiseAutocompleteResultPopup(testURL, aDestWindow).then(() => {
          if (aExpectSwitch) {
            // If we expect a tab switch then the current tab
            // will be closed and we switch to the other tab.
            let tabContainer = aDestWindow.gBrowser.tabContainer;
            tabContainer.addEventListener("TabClose", function onClose(event) {
              if (event.target == testTab) {
                tabContainer.removeEventListener("TabClose", onClose);
                executeSoon(aCallback);
              }
            });
          } else {
            // If we don't expect a tab switch then wait for the tab to load.
            testTab.addEventListener("load", function onLoad() {
              testTab.removeEventListener("load", onLoad, true);
              executeSoon(aCallback);
            }, true);
          }

          // Make sure the last match is selected.
          let {controller, popup} = aDestWindow.gURLBar;
          while (popup.selectedIndex < controller.matchCount - 1) {
            controller.handleKeyNavigation(KeyEvent.DOM_VK_DOWN);
          }

          // Execute the selected action.
          controller.handleEnter(true);
        });
      }, aDestWindow);
    }, true);
  }
}
