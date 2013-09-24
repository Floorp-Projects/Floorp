/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let fm = Cc["@mozilla.org/focus-manager;1"].getService(Ci.nsIFocusManager);

function test() {
  waitForExplicitFinish();
  // Open the javascript console. It has the mac menu overlay, so browser.js is
  // loaded in it.
  let consoleWin = window.open("chrome://global/content/console.xul", "_blank",
                               "chrome,extrachrome,menubar,resizable,scrollbars,status,toolbar");
  testWithOpenWindow(consoleWin);
}

function testWithOpenWindow(consoleWin) {
  // Add a tab so we don't open the url into the current tab
  let newTab = gBrowser.addTab("http://example.com");
  gBrowser.selectedTab = newTab;

  let numTabs = gBrowser.tabs.length;

  waitForFocus(function() {
    // Sanity check
    is(fm.activeWindow, consoleWin,
       "the console window is focused");

    gBrowser.tabContainer.addEventListener("TabOpen", function(aEvent) {
      gBrowser.tabContainer.removeEventListener("TabOpen", arguments.callee, true);
      let browser = aEvent.originalTarget.linkedBrowser;
      browser.addEventListener("pageshow", function(event) {
        if (event.target.location.href != "about:addons")
          return;
        browser.removeEventListener("pageshow", arguments.callee, true);

        is(fm.activeWindow, window,
           "the browser window was focused");
        is(browser.currentURI.spec, "about:addons",
           "about:addons was loaded in the window");
        is(gBrowser.tabs.length, numTabs + 1,
           "a new tab was added");

        // Cleanup.
        executeSoon(function() {
          consoleWin.close();
          gBrowser.removeTab(gBrowser.selectedTab);
          gBrowser.removeTab(newTab);
          finish();
        });
      }, true);
    }, true);

    // Open the addons manager, uses switchToTabHavingURI.
    consoleWin.BrowserOpenAddonsMgr();
  }, consoleWin);
}

// Ideally we'd also check that the case for no open windows works, but we can't
// due to limitations with the testing framework.
