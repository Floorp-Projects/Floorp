/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "http://mochi.test:8888/browser/browser/devtools/" +
                 "responsivedesign/test/touch.html";

add_task(function*() {
  yield addTab(TEST_URI);
  let mgr = ResponsiveUI.ResponsiveUIManager;
  let mgrOn = once(mgr, "on");
  mgr.toggle(window, gBrowser.selectedTab);
  yield mgrOn;
  yield testWithNoTouch();
  yield mgr.getResponsiveUIForTab(gBrowser.selectedTab).enableTouch();
  yield testWithTouch();
  yield mgr.getResponsiveUIForTab(gBrowser.selectedTab).disableTouch();
  yield testWithNoTouch();
  let mgrOff = once(mgr, "off");
  mgr.toggle(window, gBrowser.selectedTab);
  yield mgrOff;
});

function* testWithNoTouch() {
  let div = content.document.querySelector("div");
  let x = 2, y = 2;
  yield BrowserTestUtils.synthesizeMouse("div", x, y,
        { type: "mousedown", isSynthesized: false }, gBrowser.selectedBrowser);
  x += 20; y += 10;
  yield BrowserTestUtils.synthesizeMouse("div", x, y,
        { type: "mousemove", isSynthesized: false }, gBrowser.selectedBrowser);
  is(div.style.transform, "none", "touch didn't work");
  yield BrowserTestUtils.synthesizeMouse("div", x, y,
        { type: "mouseup", isSynthesized: false }, gBrowser.selectedBrowser);
}

function* testWithTouch() {
  let div = content.document.querySelector("div");
  let x = 2, y = 2;
  yield BrowserTestUtils.synthesizeMouse("div", x, y,
        { type: "mousedown", isSynthesized: false }, gBrowser.selectedBrowser);
  x += 20; y += 10;
  yield BrowserTestUtils.synthesizeMouse("div", x, y,
        { type: "mousemove", isSynthesized: false }, gBrowser.selectedBrowser);
  is(div.style.transform, "translate(20px, 10px)", "touch worked");
  yield BrowserTestUtils.synthesizeMouse("div", x, y,
        { type: "mouseup", isSynthesized: false }, gBrowser.selectedBrowser);
  is(div.style.transform, "none", "end event worked");
}
