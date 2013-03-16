/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const TEST_PAGE = "http://example.org/browser/browser/base/content/test/zoom_test.html";
const TEST_VIDEO = "http://example.org/browser/browser/base/content/test/video.ogg";

var gTab1, gTab2, gLevel1, gLevel2;

function test() {
  Task.spawn(function () {
    waitForExplicitFinish();

    gTab1 = gBrowser.addTab();
    gTab2 = gBrowser.addTab();

    yield FullZoomHelper.selectTabAndWaitForLocationChange(gTab1);
    yield FullZoomHelper.load(gTab1, TEST_PAGE);
    yield FullZoomHelper.load(gTab2, TEST_VIDEO);
  }).then(zoomTab1, FullZoomHelper.failAndContinue(finish));
}

function zoomTab1() {
  Task.spawn(function () {
    is(gBrowser.selectedTab, gTab1, "Tab 1 is selected");
    FullZoomHelper.zoomTest(gTab1, 1, "Initial zoom of tab 1 should be 1");
    FullZoomHelper.zoomTest(gTab2, 1, "Initial zoom of tab 2 should be 1");

    yield FullZoomHelper.enlarge();
    gLevel1 = ZoomManager.getZoomForBrowser(gBrowser.getBrowserForTab(gTab1));

    ok(gLevel1 > 1, "New zoom for tab 1 should be greater than 1");
    FullZoomHelper.zoomTest(gTab2, 1, "Zooming tab 1 should not affect tab 2");

    yield FullZoomHelper.selectTabAndWaitForLocationChange(gTab2);
    FullZoomHelper.zoomTest(gTab2, 1, "Tab 2 is still unzoomed after it is selected");
    FullZoomHelper.zoomTest(gTab1, gLevel1, "Tab 1 is still zoomed");
  }).then(zoomTab2, FullZoomHelper.failAndContinue(finish));
}

function zoomTab2() {
  Task.spawn(function () {
    is(gBrowser.selectedTab, gTab2, "Tab 2 is selected");

    yield FullZoomHelper.reduce();
    let gLevel2 = ZoomManager.getZoomForBrowser(gBrowser.getBrowserForTab(gTab2));

    ok(gLevel2 < 1, "New zoom for tab 2 should be less than 1");
    FullZoomHelper.zoomTest(gTab1, gLevel1, "Zooming tab 2 should not affect tab 1");

    yield FullZoomHelper.selectTabAndWaitForLocationChange(gTab1);
    FullZoomHelper.zoomTest(gTab1, gLevel1, "Tab 1 should have the same zoom after it's selected");
  }).then(testNavigation, FullZoomHelper.failAndContinue(finish));
}

function testNavigation() {
  Task.spawn(function () {
    yield FullZoomHelper.load(gTab1, TEST_VIDEO);
    FullZoomHelper.zoomTest(gTab1, 1, "Zoom should be 1 when a video was loaded");
    yield FullZoomHelper.navigate(FullZoomHelper.BACK);
    FullZoomHelper.zoomTest(gTab1, gLevel1, "Zoom should be restored when a page is loaded");
    yield FullZoomHelper.navigate(FullZoomHelper.FORWARD);
    FullZoomHelper.zoomTest(gTab1, 1, "Zoom should be 1 again when navigating back to a video");
  }).then(finishTest, FullZoomHelper.failAndContinue(finish));
}

var finishTestStarted  = false;
function finishTest() {
  Task.spawn(function () {
    ok(!finishTestStarted, "finishTest called more than once");
    finishTestStarted = true;

    yield FullZoomHelper.selectTabAndWaitForLocationChange(gTab1);
    yield FullZoomHelper.reset();
    gBrowser.removeTab(gTab1);
    yield FullZoomHelper.selectTabAndWaitForLocationChange(gTab2);
    yield FullZoomHelper.reset();
    gBrowser.removeTab(gTab2);
  }).then(finish, FullZoomHelper.failAndContinue(finish));
}
