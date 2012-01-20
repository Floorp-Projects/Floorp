/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const TEST_PAGE = "http://example.org/browser/browser/base/content/test/zoom_test.html";
const TEST_VIDEO = "http://example.org/browser/browser/base/content/test/video.ogg";

var gTab1, gTab2, gLevel1, gLevel2;

function test() {
  waitForExplicitFinish();

  gTab1 = gBrowser.addTab();
  gTab2 = gBrowser.addTab();
  gBrowser.selectedTab = gTab1;

  load(gTab1, TEST_PAGE, function() {
    load(gTab2, TEST_VIDEO, zoomTab1);
  });
}

function zoomTab1() {
  is(gBrowser.selectedTab, gTab1, "Tab 1 is selected");
  zoomTest(gTab1, 1, "Initial zoom of tab 1 should be 1");
  zoomTest(gTab2, 1, "Initial zoom of tab 2 should be 1");

  FullZoom.enlarge();
  gLevel1 = ZoomManager.getZoomForBrowser(gBrowser.getBrowserForTab(gTab1));

  ok(gLevel1 > 1, "New zoom for tab 1 should be greater than 1");
  zoomTest(gTab2, 1, "Zooming tab 1 should not affect tab 2");

  gBrowser.selectedTab = gTab2;
  zoomTest(gTab2, 1, "Tab 2 is still unzoomed after it is selected");
  zoomTest(gTab1, gLevel1, "Tab 1 is still zoomed");

  zoomTab2();
}

function zoomTab2() {
  is(gBrowser.selectedTab, gTab2, "Tab 2 is selected");

  FullZoom.reduce();
  let gLevel2 = ZoomManager.getZoomForBrowser(gBrowser.getBrowserForTab(gTab2));

  ok(gLevel2 < 1, "New zoom for tab 2 should be less than 1");
  zoomTest(gTab1, gLevel1, "Zooming tab 2 should not affect tab 1");

  afterZoom(function() {
    zoomTest(gTab1, gLevel1, "Tab 1 should have the same zoom after it's selected");

    testNavigation();
  });
  gBrowser.selectedTab = gTab1;
}

function testNavigation() {
  load(gTab1, TEST_VIDEO, function() {
    zoomTest(gTab1, 1, "Zoom should be 1 when a video was loaded");
    navigate(BACK, function() {
      zoomTest(gTab1, gLevel1, "Zoom should be restored when a page is loaded");
      navigate(FORWARD, function() {
        zoomTest(gTab1, 1, "Zoom should be 1 again when navigating back to a video");
        finishTest();
      });
    });
  });
}

var finishTestStarted  = false;
function finishTest() {
  ok(!finishTestStarted, "finishTest called more than once");
  finishTestStarted = true;

  gBrowser.selectedTab = gTab1;
  FullZoom.reset();
  gBrowser.removeTab(gTab1);

  gBrowser.selectedTab = gTab2;
  FullZoom.reset();
  gBrowser.removeTab(gTab2);

  finish();
}

function zoomTest(tab, val, msg) {
  is(ZoomManager.getZoomForBrowser(tab.linkedBrowser), val, msg);
}

function load(tab, url, cb) {
  let didLoad = false;
  let didZoom = false;
  tab.linkedBrowser.addEventListener("load", function onload(event) {
    event.currentTarget.removeEventListener("load", onload, true);
    didLoad = true;
    if (didZoom)
      executeSoon(cb);
  }, true);

  afterZoom(function() {
    didZoom = true;
    if (didLoad)
      executeSoon(cb);
  });

  tab.linkedBrowser.loadURI(url);
}

const BACK = 0;
const FORWARD = 1;
function navigate(direction, cb) {
  let didPs = false;
  let didZoom = false;
  gBrowser.addEventListener("pageshow", function onpageshow(event) {
    gBrowser.removeEventListener("pageshow", onpageshow, true);
    didPs = true;
    if (didZoom)
      executeSoon(cb);
  }, true);

  afterZoom(function() {
    didZoom = true;
    if (didPs)
      executeSoon(cb);
  });

  if (direction == BACK)
    gBrowser.goBack();
  else if (direction == FORWARD)
    gBrowser.goForward();
}

function afterZoom(cb) {
  let oldSZFB = ZoomManager.setZoomForBrowser;
  ZoomManager.setZoomForBrowser = function(browser, value) {
    oldSZFB.call(ZoomManager, browser, value);
    ZoomManager.setZoomForBrowser = oldSZFB;
    executeSoon(cb);
  };
}
