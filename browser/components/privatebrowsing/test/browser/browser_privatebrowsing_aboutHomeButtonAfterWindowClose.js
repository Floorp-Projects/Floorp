/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test checks that the Session Restore about:home button
// is disabled in private mode

function test() {
  waitForExplicitFinish();

  function testNoSessionRestoreButton() {
    let win = OpenBrowserWindow({private: true});
    win.addEventListener("load", function onLoad() {
      win.removeEventListener("load", onLoad, false);
      executeSoon(function() {
        info("The second private window got loaded");
        let newTab = win.gBrowser.addTab("about:home");
        win.gBrowser.selectedTab = newTab;
        let tabBrowser = win.gBrowser.getBrowserForTab(newTab);
        tabBrowser.addEventListener("load", function tabLoadListener() {
          tabBrowser.removeEventListener("load", tabLoadListener, true);
          executeSoon(function() {
            info("about:home got loaded");
            let sessionRestoreButton = win.gBrowser
                                          .contentDocument
                                          .getElementById("restorePreviousSession");
            is(win.getComputedStyle(sessionRestoreButton).display, 
               "none", "The Session Restore about:home button should be disabled");
            win.close();
            finish();
          });
        }, true);
      });
    }, false);
  }

  let win = OpenBrowserWindow({private: true});
  win.addEventListener("load", function onload() {
    win.removeEventListener("load", onload, false);
    executeSoon(function() {
      info("The first private window got loaded");
      win.close();
      testNoSessionRestoreButton();
    });
  }, false);
}
