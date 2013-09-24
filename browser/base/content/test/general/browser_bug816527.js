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
      executeSoon(function() aCallback(aWin));
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

  function runTest(aSourceWindow, aDestWindow, aExpectSuccess, aCallback) {
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

        // Set the urlbar to include the moz-action
        aDestWindow.gURLBar.value = "moz-action:switchtab," + testURL;
        // Focus the urlbar so we can press enter
        aDestWindow.gURLBar.focus();

        // We want to see if the switchtab action works.  If it does, the
        // current tab will get closed, and that's what we detect with the
        // TabClose handler.  If pressing enter triggers a load in that tab,
        // then the load handler will get called.  Neither of these are
        // the desired effect here.  So if the test goes successfully, it is
        // the timeout handler which gets called.
        //
        // The reason that we can't avoid the timeout here is because we are
        // trying to test something which should not happen, so we just need
        // to wait for a while and then check whether any bad things have
        // happened.

        function onTabClose(aEvent) {
          aDestWindow.gBrowser.tabContainer.removeEventListener("TabClose", onTabClose, false);
          aDestWindow.gBrowser.removeEventListener("load", onLoad, false);
          clearTimeout(timeout);
          // Should only happen when we expect success
          ok(aExpectSuccess, "Tab closed as expected");
          aCallback();
        }
        function onLoad(aEvent) {
          aDestWindow.gBrowser.tabContainer.removeEventListener("TabClose", onTabClose, false);
          aDestWindow.gBrowser.removeEventListener("load", onLoad, false);
          clearTimeout(timeout);
          // Should only happen when we expect success
          ok(aExpectSuccess, "Tab loaded as expected");
          aCallback();
        }

        aDestWindow.gBrowser.tabContainer.addEventListener("TabClose", onTabClose, false);
        aDestWindow.gBrowser.addEventListener("load", onLoad, false);
        let timeout = setTimeout(function() {
          aDestWindow.gBrowser.tabContainer.removeEventListener("TabClose", onTabClose, false);
          aDestWindow.gBrowser.removeEventListener("load", onLoad, false);
          aCallback();
        }, 500);

        // Press enter!
        EventUtils.synthesizeKey("VK_RETURN", {});
      }, aDestWindow);
    }, true);
  }
}

