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
  aboutBrowser.addEventListener("load", function onAboutBrowserLoad() {
    aboutBrowser.removeEventListener("load", onAboutBrowserLoad, true);
    let tabMozilla = gBrowser.addTab();
    gBrowser.selectedTab = tabMozilla;

    let mozillaBrowser = gBrowser.getBrowserForTab(tabMozilla);
    mozillaBrowser.addEventListener("load", function onMozillaBrowserLoad() {
      mozillaBrowser.removeEventListener("load", onMozillaBrowserLoad, true);
      let mozillaZoom = ZoomManager.zoom;

      // change the zoom on the mozilla page
      FullZoom.enlarge();
      // make sure the zoom level has been changed
      isnot(ZoomManager.zoom, mozillaZoom, "Zoom level can be changed");
      mozillaZoom = ZoomManager.zoom;

      // switch to about: tab
      gBrowser.selectedTab = tabAbout;

      // switch back to mozilla tab
      gBrowser.selectedTab = tabMozilla;

      // make sure the zoom level has not changed
      is(ZoomManager.zoom, mozillaZoom,
        "Entering private browsing should not reset the zoom on a tab");

      // leave private browsing mode
      pb.privateBrowsingEnabled = false;

      // cleanup
      gPrefService.clearUserPref("browser.privatebrowsing.keep_current_session");
      FullZoom.reset();
      gBrowser.removeTab(tabMozilla);
      gBrowser.removeTab(tabAbout);
      finish();
    }, true);
    mozillaBrowser.contentWindow.location = "about:mozilla";
  }, true);
  aboutBrowser.contentWindow.location = "about:";
}
