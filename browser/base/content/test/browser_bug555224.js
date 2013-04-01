/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
const TEST_PAGE = "/browser/browser/base/content/test/dummy_page.html";
var gTestTab, gBgTab, gTestZoom;

function testBackgroundLoad() {
  Task.spawn(function () {
    is(ZoomManager.zoom, gTestZoom, "opening a background tab should not change foreground zoom");

    gBrowser.removeTab(gBgTab);

    yield FullZoomHelper.reset();
    gBrowser.removeTab(gTestTab);
  }).then(finish, FullZoomHelper.failAndContinue(finish));
}

function testInitialZoom() {
  Task.spawn(function () {
    is(ZoomManager.zoom, 1, "initial zoom level should be 1");
    yield FullZoomHelper.enlarge();

    gTestZoom = ZoomManager.zoom;
    isnot(gTestZoom, 1, "zoom level should have changed");

    gBgTab = gBrowser.addTab();
    yield FullZoomHelper.load(gBgTab, "http://mochi.test:8888" + TEST_PAGE);
  }).then(testBackgroundLoad, FullZoomHelper.failAndContinue(finish));
}

function test() {
  waitForExplicitFinish();

  Task.spawn(function () {
    gTestTab = gBrowser.addTab();
    yield FullZoomHelper.selectTabAndWaitForLocationChange(gTestTab);
    yield FullZoomHelper.load(gTestTab, "http://example.org" + TEST_PAGE);
  }).then(testInitialZoom, FullZoomHelper.failAndContinue(finish));
}
