/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that the URL bar is focused when entering the private browsing mode.

function test() {
  // initialization
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);

  const TEST_URL = "data:text/plain,test";
  gBrowser.selectedTab = gBrowser.addTab();
  let browser = gBrowser.selectedBrowser;
  browser.addEventListener("load", function() {
    browser.removeEventListener("load", arguments.callee, true);

    // ensure that the URL bar is not focused initially
    browser.focus();
    isnot(document.commandDispatcher.focusedElement, gURLBar.inputField,
      "URL Bar should not be focused before entering the private browsing mode");
    // ensure that the URL bar is not empty initially
    isnot(gURLBar.value, "", "URL Bar should no longer be empty after leaving the private browsing mode");

    // enter private browsing mode
    pb.privateBrowsingEnabled = true;
    browser = gBrowser.selectedBrowser;
    browser.addEventListener("load", function() {
      browser.removeEventListener("load", arguments.callee, true);

      // setTimeout is needed here because the onload handler of about:privatebrowsing sets the focus
      setTimeout(function() {
        // ensure that the URL bar is focused inside the private browsing mode
        is(document.commandDispatcher.focusedElement, gURLBar.inputField,
          "URL Bar should be focused inside the private browsing mode");

        // ensure that the URL bar is emptied inside the private browsing mode
        is(gURLBar.value, "", "URL Bar should be empty inside the private browsing mode");

        // leave private browsing mode
        pb.privateBrowsingEnabled = false;
        browser = gBrowser.selectedBrowser;
        browser.addEventListener("load", function() {
          browser.removeEventListener("load", arguments.callee, true);

          // ensure that the URL bar is no longer focused after leaving the private browsing mode
          isnot(document.commandDispatcher.focusedElement, gURLBar.inputField,
            "URL Bar should no longer be focused after leaving the private browsing mode");

          // ensure that the URL bar is no longer empty after leaving the private browsing mode
          isnot(gURLBar.value, "", "URL Bar should no longer be empty after leaving the private browsing mode");

          gBrowser.removeCurrentTab();
          finish();
        }, true);
      }, 0);
    }, true);
  }, true);
  content.location = TEST_URL;

  waitForExplicitFinish();
}
