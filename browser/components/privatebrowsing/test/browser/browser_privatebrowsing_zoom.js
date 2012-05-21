/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that private browsing turns off doesn't cause zoom
// settings to be reset on tab switch (bug 464962)

function test() {
  // initialization
  gPrefService.setBoolPref("browser.privatebrowsing.keep_current_session", true);
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);

  // enter private browsing mode
  pb.privateBrowsingEnabled = true;

  let tabAbout = gBrowser.addTab();
  gBrowser.selectedTab = tabAbout;

  waitForExplicitFinish();

  let aboutBrowser = gBrowser.getBrowserForTab(tabAbout);
  aboutBrowser.addEventListener("load", function () {
    aboutBrowser.removeEventListener("load", arguments.callee, true);
    let tabRobots = gBrowser.addTab();
    gBrowser.selectedTab = tabRobots;

    let robotsBrowser = gBrowser.getBrowserForTab(tabRobots);
    robotsBrowser.addEventListener("load", function () {
      robotsBrowser.removeEventListener("load", arguments.callee, true);
      let robotsZoom = ZoomManager.zoom;

      // change the zoom on the robots page
      FullZoom.enlarge();
      // make sure the zoom level has been changed
      isnot(ZoomManager.zoom, robotsZoom, "Zoom level can be changed");
      robotsZoom = ZoomManager.zoom;

      // switch to about: tab
      gBrowser.selectedTab = tabAbout;

      // switch back to robots tab
      gBrowser.selectedTab = tabRobots;

      // make sure the zoom level has not changed
      is(ZoomManager.zoom, robotsZoom,
        "Entering private browsing should not reset the zoom on a tab");

      // leave private browsing mode
      pb.privateBrowsingEnabled = false;

      // cleanup
      gPrefService.clearUserPref("browser.privatebrowsing.keep_current_session");
      FullZoom.reset();
      gBrowser.removeTab(tabRobots);
      gBrowser.removeTab(tabAbout);
      finish();
    }, true);
    robotsBrowser.contentWindow.location = "about:robots";
  }, true);
  aboutBrowser.contentWindow.location = "about:";
}
