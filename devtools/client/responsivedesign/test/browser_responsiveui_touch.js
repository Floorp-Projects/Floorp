/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "http://mochi.test:8888/browser/devtools/client/" +
                 "responsivedesign/test/touch.html";

add_task(function* () {
  let tab = yield addTab(TEST_URI);
  let {rdm} = yield openRDM(tab);
  yield testWithNoTouch();
  yield rdm.enableTouch();
  yield testWithTouch();
  yield rdm.disableTouch();
  yield testWithNoTouch();
  yield closeRDM(rdm);
});

function* testWithNoTouch() {
  let div = content.document.querySelector("div");
  let x = 2, y = 2;
  yield BrowserTestUtils.synthesizeMouse("div", x, y,
        { type: "mousedown", isSynthesized: false }, gBrowser.selectedBrowser);
  x += 20; y += 10;
  yield BrowserTestUtils.synthesizeMouse("div", x, y,
        { type: "mousemove", isSynthesized: false }, gBrowser.selectedBrowser);
  is(div.style.transform, "none", "touch shouldn't work");
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
  is(div.style.transform, "translate(20px, 10px)", "touch should work");
  yield BrowserTestUtils.synthesizeMouse("div", x, y,
        { type: "mouseup", isSynthesized: false }, gBrowser.selectedBrowser);
  is(div.style.transform, "none", "end event should work");
}
