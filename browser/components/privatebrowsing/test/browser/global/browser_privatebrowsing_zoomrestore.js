/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that about:privatebrowsing does not appear zoomed in
// if there is already a zoom site pref for about:blank (bug 487656).

function test() {
  // initialization
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);
  waitForExplicitFinish();

  let tabBlank = gBrowser.selectedTab;
  let blankBrowser = gBrowser.getBrowserForTab(tabBlank);
  blankBrowser.addEventListener("load", function() {
    blankBrowser.removeEventListener("load", arguments.callee, true);

    // change the zoom on the blank page
    FullZoom.enlarge();
    isnot(ZoomManager.zoom, 1, "Zoom level for about:blank should be changed");

    // enter private browsing mode
    pb.privateBrowsingEnabled = true;
    let tabAboutPB = gBrowser.selectedTab;
    let browserAboutPB = gBrowser.getBrowserForTab(tabAboutPB);
    browserAboutPB.addEventListener("load", function() {
      browserAboutPB.removeEventListener("load", arguments.callee, true);
      setTimeout(function() {
        // make sure the zoom level is set to 1
        is(ZoomManager.zoom, 1, "Zoom level for about:privatebrowsing should be reset");
        finishTest();
      }, 0);
    }, true);
  }, true);
  blankBrowser.loadURI("about:blank");
}

function finishTest() {
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);
  // leave private browsing mode
  pb.privateBrowsingEnabled = false;
  let tabBlank = gBrowser.selectedTab;
  let blankBrowser = gBrowser.getBrowserForTab(tabBlank);
  blankBrowser.addEventListener("load", function() {
    blankBrowser.removeEventListener("load", arguments.callee, true);

    executeSoon(function() {
      // cleanup
      FullZoom.reset();
      finish();
    });
  }, true);
}
