/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Test the fix for bug 441778 to ensure site-specific page zoom doesn't get
 * modified by sub-document loads of content from a different domain.
 */

function test() {
  waitForExplicitFinish();

  const TEST_PAGE_URL = 'data:text/html,<body><iframe src=""></iframe></body>';
  const TEST_IFRAME_URL = "http://test2.example.org/";

  // Prepare the test tab
  gBrowser.selectedTab = gBrowser.addTab();
  let testBrowser = gBrowser.selectedBrowser;

  testBrowser.addEventListener("load", function () {
    testBrowser.removeEventListener("load", arguments.callee, true);

    // Change the zoom level and then save it so we can compare it to the level
    // after loading the sub-document.
    FullZoom.enlarge();
    var zoomLevel = ZoomManager.zoom;

    // Start the sub-document load.
    executeSoon(function () {
      testBrowser.addEventListener("load", function (e) {
        testBrowser.removeEventListener("load", arguments.callee, true);

        is(e.target.defaultView.location, TEST_IFRAME_URL, "got the load event for the iframe");
        is(ZoomManager.zoom, zoomLevel, "zoom is retained after sub-document load");

        gBrowser.removeCurrentTab();
        finish();
      }, true);
      content.document.querySelector("iframe").src = TEST_IFRAME_URL;
    });
  }, true);

  content.location = TEST_PAGE_URL;
}
