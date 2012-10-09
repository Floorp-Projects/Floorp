/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that cancelling the unloading of a page with a beforeunload
// handler prevents the private browsing mode transition.

function test() {
  const TEST_PAGE_1 = "data:text/html,<body%20onbeforeunload='return%20false;'>first</body>";
  const TEST_PAGE_2 = "data:text/html,<body%20onbeforeunload='return%20false;'>second</body>";
  let pb = Cc["@mozilla.org/privatebrowsing;1"]
             .getService(Ci.nsIPrivateBrowsingService);

  let rejectDialog = 0;
  let acceptDialog = 0;
  let confirmCalls = 0;
  function promptObserver(aSubject, aTopic, aData) {
    let dialogWin = aSubject.QueryInterface(Ci.nsIDOMWindow);
    confirmCalls++;
    if (acceptDialog-- > 0)
      dialogWin.document.documentElement.getButton("accept").click();
    else if (rejectDialog-- > 0)
      dialogWin.document.documentElement.getButton("cancel").click();
  }

  Services.obs.addObserver(promptObserver, "common-dialog-loaded", false);

  waitForExplicitFinish();
  let browser1 = gBrowser.addTab().linkedBrowser;
  browser1.addEventListener("load", function() {
    browser1.removeEventListener("load", arguments.callee, true);

    let browser2 = gBrowser.addTab().linkedBrowser;
    browser2.addEventListener("load", function() {
      browser2.removeEventListener("load", arguments.callee, true);

      rejectDialog = 1;
      pb.privateBrowsingEnabled = true;

      ok(!pb.privateBrowsingEnabled, "Private browsing mode should not have been activated");
      is(confirmCalls, 1, "Only one confirm box should be shown");
      is(gBrowser.tabs.length, 3,
         "No tabs should be closed because private browsing mode transition was canceled");
      is(gBrowser.tabContainer.firstChild.linkedBrowser.currentURI.spec, "about:blank",
         "The first tab should be a blank tab");
      is(gBrowser.tabContainer.firstChild.nextSibling.linkedBrowser.currentURI.spec, TEST_PAGE_1,
         "The middle tab should be the same one we opened");
      is(gBrowser.tabContainer.lastChild.linkedBrowser.currentURI.spec, TEST_PAGE_2,
         "The last tab should be the same one we opened");
      is(rejectDialog, 0, "Only one confirm dialog should have been rejected");

      confirmCalls = 0;
      acceptDialog = 2;
      pb.privateBrowsingEnabled = true;

      ok(pb.privateBrowsingEnabled, "Private browsing mode should have been activated");
      is(confirmCalls, 2, "Only two confirm boxes should be shown");
      is(gBrowser.tabs.length, 1,
         "Incorrect number of tabs after transition into private browsing");
      gBrowser.selectedBrowser.addEventListener("load", function() {
        gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);

        is(gBrowser.currentURI.spec, "about:privatebrowsing",
           "Incorrect page displayed after private browsing transition");
        is(acceptDialog, 0, "Two confirm dialogs should have been accepted");

        gBrowser.selectedBrowser.addEventListener("load", function() {
          gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);

          gBrowser.selectedTab = gBrowser.addTab();
          gBrowser.selectedBrowser.addEventListener("load", function() {
            gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);

            confirmCalls = 0;
            rejectDialog = 1;
            pb.privateBrowsingEnabled = false;

            ok(pb.privateBrowsingEnabled, "Private browsing mode should not have been deactivated");
            is(confirmCalls, 1, "Only one confirm box should be shown");
            is(gBrowser.tabs.length, 2,
               "No tabs should be closed because private browsing mode transition was canceled");
            is(gBrowser.tabContainer.firstChild.linkedBrowser.currentURI.spec, TEST_PAGE_1,
               "The first tab should be the same one we opened");
            is(gBrowser.tabContainer.lastChild.linkedBrowser.currentURI.spec, TEST_PAGE_2,
               "The last tab should be the same one we opened");
            is(rejectDialog, 0, "Only one confirm dialog should have been rejected");

            // Ensure that all restored tabs are loaded without waiting for the
            // user to bring them to the foreground, by resetting the related
            // preference (see the "firefox.js" defaults file for details).
            Services.prefs.setBoolPref("browser.sessionstore.restore_on_demand", false);

            confirmCalls = 0;
            acceptDialog = 2;
            pb.privateBrowsingEnabled = false;

            ok(!pb.privateBrowsingEnabled, "Private browsing mode should have been deactivated");
            is(confirmCalls, 2, "Only two confirm boxes should be shown");
            is(gBrowser.tabs.length, 3,
               "Incorrect number of tabs after transition into private browsing");

            let loads = 0;
            function waitForLoad(event) {
              gBrowser.removeEventListener("load", arguments.callee, true);

              if (++loads != 3)
                return;

              is(gBrowser.tabContainer.firstChild.linkedBrowser.currentURI.spec, "about:blank",
                 "The first tab should be a blank tab");
              is(gBrowser.tabContainer.firstChild.nextSibling.linkedBrowser.currentURI.spec, TEST_PAGE_1,
                 "The middle tab should be the same one we opened");
              is(gBrowser.tabContainer.lastChild.linkedBrowser.currentURI.spec, TEST_PAGE_2,
                 "The last tab should be the same one we opened");
              is(acceptDialog, 0, "Two confirm dialogs should have been accepted");
              is(acceptDialog, 0, "Two prompts should have been raised");

              acceptDialog = 2;
              gBrowser.removeTab(gBrowser.tabContainer.lastChild);
              gBrowser.removeTab(gBrowser.tabContainer.lastChild);
              gBrowser.getBrowserAtIndex(gBrowser.tabContainer.selectedIndex).contentWindow.focus();

              Services.obs.removeObserver(promptObserver, "common-dialog-loaded", false);
              Services.prefs.clearUserPref("browser.sessionstore.restore_on_demand");
              finish();
            }
            for (let i = 0; i < gBrowser.browsers.length; ++i)
              gBrowser.browsers[i].addEventListener("load", waitForLoad, true);
          }, true);
          gBrowser.selectedBrowser.loadURI(TEST_PAGE_2);
        }, true);
        gBrowser.selectedBrowser.loadURI(TEST_PAGE_1);
      }, true);
    }, true);
    browser2.loadURI(TEST_PAGE_2);
  }, true);
  browser1.loadURI(TEST_PAGE_1);
}
