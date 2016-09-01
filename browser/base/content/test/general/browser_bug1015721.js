/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const TEST_PAGE = "http://example.org/browser/browser/base/content/test/general/zoom_test.html";

var gTab1, gTab2, gLevel1;

function test() {
  waitForExplicitFinish();

  Task.spawn(function* () {
    gTab1 = gBrowser.addTab();
    gTab2 = gBrowser.addTab();

    yield FullZoomHelper.selectTabAndWaitForLocationChange(gTab1);
    yield FullZoomHelper.load(gTab1, TEST_PAGE);
    yield FullZoomHelper.load(gTab2, TEST_PAGE);
  }).then(zoomTab1, FullZoomHelper.failAndContinue(finish));
}

function zoomTab1() {
  Task.spawn(function* () {
    is(gBrowser.selectedTab, gTab1, "Tab 1 is selected");
    FullZoomHelper.zoomTest(gTab1, 1, "Initial zoom of tab 1 should be 1");
    FullZoomHelper.zoomTest(gTab2, 1, "Initial zoom of tab 2 should be 1");

    let browser1 = gBrowser.getBrowserForTab(gTab1);
    yield BrowserTestUtils.synthesizeMouse(null, 10, 10, {
      wheel: true, ctrlKey: true, deltaY: -1, deltaMode: WheelEvent.DOM_DELTA_LINE
    }, browser1);

    info("Waiting for tab 1 to be zoomed");
    yield promiseWaitForCondition(() => {
      gLevel1 = ZoomManager.getZoomForBrowser(browser1);
      return gLevel1 > 1;
    });

    yield FullZoomHelper.selectTabAndWaitForLocationChange(gTab2);
    FullZoomHelper.zoomTest(gTab2, gLevel1, "Tab 2 should have zoomed along with tab 1");
  }).then(finishTest, FullZoomHelper.failAndContinue(finish));
}

function finishTest() {
  Task.spawn(function* () {
    yield FullZoomHelper.selectTabAndWaitForLocationChange(gTab1);
    yield FullZoom.reset();
    yield FullZoomHelper.removeTabAndWaitForLocationChange(gTab1);
    yield FullZoomHelper.selectTabAndWaitForLocationChange(gTab2);
    yield FullZoom.reset();
    yield FullZoomHelper.removeTabAndWaitForLocationChange(gTab2);
  }).then(finish, FullZoomHelper.failAndContinue(finish));
}
