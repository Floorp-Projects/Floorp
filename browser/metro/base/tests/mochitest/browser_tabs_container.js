// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function test() {
  runTests();
}

function setup() {
  yield waitForCondition(function () {
    return Elements.tabList && Elements.tabList.strip;
  });

  if (!ContextUI.tabbarVisible) {
    ContextUI.displayTabs();
  }
}
function tearDown() {
  cleanUpOpenedTabs();
}

function isElementVisible(elm) {
  return elm.ownerDocument.defaultView.getComputedStyle(elm).getPropertyValue("visibility") == "visible";
}


gTests.push({
  desc: "No scrollbuttons when tab strip doesnt require overflow",
  setUp: setup,
  run: function () {
    let tabStrip = Elements.tabList.strip;
    let tabs = Elements.tabList.strip.querySelectorAll("documenttab");

    // sanity check tab count:
    is(tabs.length, 1, "1 tab present");

    // test imprecise mode
    let imprecisePromise = waitForObserver("metro_imprecise_input");
    notifyImprecise();
    yield imprecisePromise;

    ok(!isElementVisible(tabStrip._scrollButtonUp), "left scrollbutton is hidden in imprecise mode");
    ok(!isElementVisible(tabStrip._scrollButtonDown), "right scrollbutton is hidden in imprecise mode");

    // test precise mode
    let precisePromise = waitForObserver("metro_precise_input");
    notifyPrecise();
    yield precisePromise;

    ok(!isElementVisible(tabStrip._scrollButtonUp), "Bug 952297 - left scrollbutton is hidden in precise mode");
    ok(!isElementVisible(tabStrip._scrollButtonDown), "right scrollbutton is hidden in precise mode");
  },
  tearDown: tearDown
});

gTests.push({
  desc: "Scrollbuttons not visible when tabstrip has overflow in imprecise input mode",
  setUp: function(){
    yield setup();
    // ensure we're in imprecise mode
    let imprecisePromise = waitForObserver("metro_imprecise_input");
    notifyImprecise();
    yield imprecisePromise;
  },
  run: function () {
    // add enough tabs to get overflow in the tabstrip
    let strip = Elements.tabList.strip;
    ok(strip && strip.scrollClientSize && strip.scrollSize, "Sanity check tabstrip strip is present and expected properties available");

    while (strip.scrollSize <= strip.scrollClientSize) {
      yield addTab("about:mozilla");
    }

    ok(!isElementVisible(Elements.tabList.strip._scrollButtonUp), "left scrollbutton is hidden in imprecise mode");
    ok(!isElementVisible(Elements.tabList.strip._scrollButtonDown), "right scrollbutton is hidden in imprecise mode");

  }
});

gTests.push({
  desc: "Scrollbuttons become visible when tabstrip has overflow in precise input mode",
  setUp: function(){
    yield setup();
    // ensure we're in precise mode
    let precisePromise = waitForObserver("metro_precise_input");
    notifyPrecise();
    yield precisePromise;
  },
  run: function () {
    let strip = Elements.tabList.strip;
    ok(strip && strip.scrollClientSize && strip.scrollSize, "Sanity check tabstrip is present and expected properties available");

    // add enough tabs to get overflow in the tabstrip
    while (strip.scrollSize <= strip.scrollClientSize) {
      yield addTab("about:mozilla");
    }

    ok(isElementVisible(Elements.tabList.strip._scrollButtonUp), "left scrollbutton should be visible in precise mode");
    ok(isElementVisible(Elements.tabList.strip._scrollButtonDown), "right scrollbutton should be visible in precise mode");

    // select the first tab
    Browser.selectedTab = Browser.tabs[0];
    yield waitForMs(1000); // XXX maximum duration of arrowscrollbox _scrollAnim
    ok(Elements.tabList.strip._scrollButtonUp.disabled, "left scrollbutton should be disabled when 1st tab is selected and tablist has overflow");
    ok(!Elements.tabList.strip._scrollButtonDown.disabled, "right scrollbutton should be enabled when 1st tab is selected and tablist has overflow");

    // select the last tab
    Browser.selectedTab = Browser.tabs[Browser.tabs.length - 1];
    yield waitForMs(1000); // XXX maximum duration of arrowscrollbox _scrollAnim
    ok(!Elements.tabList.strip._scrollButtonUp.disabled, "left scrollbutton should be enabled when 1st tab is selected and tablist has overflow");
    ok(Elements.tabList.strip._scrollButtonDown.disabled, "right scrollbutton should be disabled when last tab is selected and tablist has overflow");

  }
});
