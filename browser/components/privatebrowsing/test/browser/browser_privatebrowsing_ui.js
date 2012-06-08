/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that the gPrivateBrowsingUI object, the Private Browsing
// menu item and its XUL <command> element work correctly.

function test() {
  // initialization
  waitForExplicitFinish();
  gPrefService.setBoolPref("browser.privatebrowsing.keep_current_session", true);
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);
  let observerData;
  function observer(aSubject, aTopic, aData) {
    if (aTopic == "private-browsing")
      observerData = aData;
  }
  Services.obs.addObserver(observer, "private-browsing", false);
  let pbMenuItem = document.getElementById("privateBrowsingItem");
  // add a new blank tab to ensure the title can be meaningfully compared later
  gBrowser.selectedTab = gBrowser.addTab();
  let originalTitle = document.title;

  function testNewWindow(aCallback, expected) {
    Services.obs.addObserver(function observer1(aSubject, aTopic, aData) {
      aSubject.addEventListener("load", function() {
        aSubject.removeEventListener("load", arguments.callee);
        executeSoon(function() {
          let ui = aSubject.gPrivateBrowsingUI;
          is(ui.privateBrowsingEnabled, expected, "The privateBrowsingEnabled property on the new window is set correctly");
          is(ui.privateWindow, expected, "The privateWindow property on the new window is set correctly");

          Services.obs.addObserver(function observer2(aSubject, aTopic, aData) {
            aCallback();
            Services.obs.removeObserver(observer2, "domwindowclosed");
          }, "domwindowclosed", false);
          aSubject.close();
        });
        Services.obs.removeObserver(observer1, "domwindowopened");
      }, false);
    }, "domwindowopened", false);
    OpenBrowserWindow();
  }

  // test the gPrivateBrowsingUI object
  ok(gPrivateBrowsingUI, "The gPrivateBrowsingUI object exists");
  is(pb.privateBrowsingEnabled, false, "The private browsing mode should not be started initially");
  is(gPrivateBrowsingUI.privateBrowsingEnabled, false, "gPrivateBrowsingUI should expose the correct private browsing status");
  is(gPrivateBrowsingUI.privateWindow, false, "gPrivateBrowsingUI should expose the correct per-window private browsing status");
  ok(pbMenuItem, "The Private Browsing menu item exists");
  is(pbMenuItem.getAttribute("label"), pbMenuItem.getAttribute("startlabel"), "The Private Browsing menu item should read \"Start Private Browsing\"");
  testNewWindow(function() {
    gPrivateBrowsingUI.toggleMode();
    is(pb.privateBrowsingEnabled, true, "The private browsing mode should be started");
    is(gPrivateBrowsingUI.privateBrowsingEnabled, true, "gPrivateBrowsingUI should expose the correct private browsing status");
    is(gPrivateBrowsingUI.privateWindow, true, "gPrivateBrowsingUI should expose the correct per-window private browsing status");
    // check to see if the Private Browsing mode was activated successfully
    is(observerData, "enter", "Private Browsing mode was activated using the gPrivateBrowsingUI object");
    is(pbMenuItem.getAttribute("label"), pbMenuItem.getAttribute("stoplabel"), "The Private Browsing menu item should read \"Stop Private Browsing\"");
    testNewWindow(function() {
      gPrivateBrowsingUI.toggleMode()
      is(pb.privateBrowsingEnabled, false, "The private browsing mode should not be started");
      is(gPrivateBrowsingUI.privateBrowsingEnabled, false, "gPrivateBrowsingUI should expose the correct private browsing status");
      is(gPrivateBrowsingUI.privateWindow, false, "gPrivateBrowsingUI should expose the correct per-window private browsing status");
      // check to see if the Private Browsing mode was deactivated successfully
      is(observerData, "exit", "Private Browsing mode was deactivated using the gPrivateBrowsingUI object");
      is(pbMenuItem.getAttribute("label"), pbMenuItem.getAttribute("startlabel"), "The Private Browsing menu item should read \"Start Private Browsing\"");

      testNewWindow(function() {
        // These are tests for the privateWindow setter.  Note that the setter should
        // not be used anywhere else for now!
        setPrivateWindow(window, true);
        is(gPrivateBrowsingUI.privateWindow, true, "gPrivateBrowsingUI should accept the correct per-window private browsing status");
        setPrivateWindow(window, false);
        is(gPrivateBrowsingUI.privateWindow, false, "gPrivateBrowsingUI should accept the correct per-window private browsing status");

        // now, test using the <command> object
        let cmd = document.getElementById("Tools:PrivateBrowsing");
        isnot(cmd, null, "XUL command object for the private browsing service exists");
        var func = new Function("", cmd.getAttribute("oncommand"));
        func.call(cmd);
        // check to see if the Private Browsing mode was activated successfully
        is(observerData, "enter", "Private Browsing mode was activated using the command object");
        // check to see that the window title has been changed correctly
        isnot(document.title, originalTitle, "Private browsing mode has correctly changed the title");
        func.call(cmd);
        // check to see if the Private Browsing mode was deactivated successfully
        is(observerData, "exit", "Private Browsing mode was deactivated using the command object");
        // check to see that the window title has been restored correctly
        is(document.title, originalTitle, "Private browsing mode has correctly restored the title");

        // cleanup
        gBrowser.removeCurrentTab();
        Services.obs.removeObserver(observer, "private-browsing");
        gPrefService.clearUserPref("browser.privatebrowsing.keep_current_session");

        finish();
      }, false);
    }, true);
  }, false);
}
