/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that private browsing turns off doesn't cause zoom
// settings to be reset on tab switch (bug 464962)

function test() {
  waitForExplicitFinish();

  function testZoom(aWindow, aCallback) {
    executeSoon(function() {
      let tabAbout = aWindow.gBrowser.addTab();
      aWindow.gBrowser.selectedTab = tabAbout;

      let aboutBrowser = aWindow.gBrowser.getBrowserForTab(tabAbout);
      aboutBrowser.addEventListener("load", function onAboutBrowserLoad() {
        aboutBrowser.removeEventListener("load", onAboutBrowserLoad, true);
        let tabMozilla = aWindow.gBrowser.addTab();
        aWindow.gBrowser.selectedTab = tabMozilla;

        let mozillaBrowser = aWindow.gBrowser.getBrowserForTab(tabMozilla);
        mozillaBrowser.addEventListener("load", function onMozillaBrowserLoad() {
          mozillaBrowser.removeEventListener("load", onMozillaBrowserLoad, true);
          let mozillaZoom = aWindow.ZoomManager.zoom;

          // change the zoom on the mozilla page
          aWindow.FullZoom.enlarge();
          // make sure the zoom level has been changed
          isnot(aWindow.ZoomManager.zoom, mozillaZoom, "Zoom level can be changed");
          mozillaZoom = aWindow.ZoomManager.zoom;

          // switch to about: tab
          aWindow.gBrowser.selectedTab = tabAbout;

          // switch back to mozilla tab
          aWindow.gBrowser.selectedTab = tabMozilla;

          // make sure the zoom level has not changed
          is(aWindow.ZoomManager.zoom, mozillaZoom,
            "Entering private browsing should not reset the zoom on a tab");

          // cleanup
          aWindow.FullZoom.reset();
          aWindow.gBrowser.removeTab(tabMozilla);
          aWindow.gBrowser.removeTab(tabAbout);
          aWindow.close();
          aCallback();
        }, true);
        mozillaBrowser.contentWindow.location = "about:mozilla";
      }, true);
      aboutBrowser.contentWindow.location = "about:";
    });
  }

  whenNewWindowLoaded({private: true}, function(privateWindow) {
    testZoom(privateWindow, finish);
  });
}
