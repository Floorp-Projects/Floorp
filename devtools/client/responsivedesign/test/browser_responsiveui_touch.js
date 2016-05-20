/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "http://mochi.test:8888/browser/devtools/client/" +
                 "responsivedesign/test/touch.html";
const layoutReflowSynthMouseMove = "layout.reflow.synthMouseMove";
const domViewportEnabled = "dom.meta-viewport.enabled";

add_task(function* () {
  let tab = yield addTab(TEST_URI);
  let {rdm} = yield openRDM(tab);
  yield pushPrefs([layoutReflowSynthMouseMove, false]);
  yield testWithNoTouch();
  yield rdm.enableTouch();
  yield testWithTouch();
  yield rdm.disableTouch();
  yield testWithNoTouch();
  yield closeRDM(rdm);
});

function* testWithNoTouch() {
  let div = content.document.querySelector("div");
  let x = 0, y = 0;

  info("testWithNoTouch: Initial test parameter and mouse mouse outside div element");
  x = -1, y = -1;
  yield BrowserTestUtils.synthesizeMouse("div", x, y,
        { type: "mousemove", isSynthesized: false }, gBrowser.selectedBrowser);
  div.style.transform = "none";
  div.style.backgroundColor = "";

  info("testWithNoTouch: Move mouse into the div element");
  yield BrowserTestUtils.synthesizeMouseAtCenter("div", { type: "mousemove", isSynthesized: false },
        gBrowser.selectedBrowser);
  is(div.style.backgroundColor, "red", "mouseenter or mouseover should work");

  info("testWithNoTouch: Drag the div element");
  yield BrowserTestUtils.synthesizeMouseAtCenter("div", { type: "mousedown", isSynthesized: false },
        gBrowser.selectedBrowser);
  x = 100; y = 100;
  yield BrowserTestUtils.synthesizeMouse("div", x, y,
        { type: "mousemove", isSynthesized: false }, gBrowser.selectedBrowser);
  is(div.style.transform, "none", "touchmove shouldn't work");
  yield BrowserTestUtils.synthesizeMouse("div", x, y,
        { type: "mouseup", isSynthesized: false }, gBrowser.selectedBrowser);

  info("testWithNoTouch: Move mouse out of the div element");
  x = -1; y = -1;
  yield BrowserTestUtils.synthesizeMouse("div", x, y,
        { type: "mousemove", isSynthesized: false }, gBrowser.selectedBrowser);
  is(div.style.backgroundColor, "blue", "mouseout or mouseleave should work");

  info("testWithNoTouch: Click the div element");
  yield synthesizeClick(div);
  is(div.dataset.isDelay, "false", "300ms delay between touch events and mouse events should not work");
}

function* testWithTouch() {
  let div = content.document.querySelector("div");
  let x = 0, y = 0;

  info("testWithTouch: Initial test parameter and mouse mouse outside div element");
  x = -1, y = -1;
  yield BrowserTestUtils.synthesizeMouse("div", x, y,
        { type: "mousemove", isSynthesized: false }, gBrowser.selectedBrowser);
  div.style.transform = "none";
  div.style.backgroundColor = "";

  info("testWithTouch: Move mouse into the div element");
  yield BrowserTestUtils.synthesizeMouseAtCenter("div", { type: "mousemove", isSynthesized: false },
        gBrowser.selectedBrowser);
  isnot(div.style.backgroundColor, "red", "mouseenter or mouseover should not work");

  info("testWithTouch: Drag the div element");
  yield BrowserTestUtils.synthesizeMouseAtCenter("div", { type: "mousedown", isSynthesized: false },
        gBrowser.selectedBrowser);
  x = 100; y = 100;
  yield BrowserTestUtils.synthesizeMouse("div", x, y,
        { type: "mousemove", isSynthesized: false }, gBrowser.selectedBrowser);
  isnot(div.style.transform, "none", "touchmove should work");
  yield BrowserTestUtils.synthesizeMouse("div", x, y,
        { type: "mouseup", isSynthesized: false }, gBrowser.selectedBrowser);

  info("testWithTouch: Move mouse out of the div element");
  x = -1; y = -1;
  yield BrowserTestUtils.synthesizeMouse("div", x, y,
        { type: "mousemove", isSynthesized: false }, gBrowser.selectedBrowser);
  isnot(div.style.backgroundColor, "blue", "mouseout or mouseleave should not work");

  yield testWithMetaViewportEnabled();
  yield testWithMetaViewportDisabled();
}

function* testWithMetaViewportEnabled() {
  yield pushPrefs([domViewportEnabled, true]);
  let meta = content.document.querySelector("meta[name=viewport]");
  let div = content.document.querySelector("div");
  div.dataset.isDelay = "false";

  info("testWithMetaViewportEnabled: click the div element with <meta name='viewport'>");
  meta.content = "";
  yield synthesizeClick(div);
  is(div.dataset.isDelay, "true", "300ms delay between touch events and mouse events should work");

  info("testWithMetaViewportEnabled: click the div element with <meta name='viewport' content='user-scalable=no'>");
  meta.content = "user-scalable=no";
  yield synthesizeClick(div);
  is(div.dataset.isDelay, "false", "300ms delay between touch events and mouse events should not work");

  info("testWithMetaViewportEnabled: click the div element with <meta name='viewport' content='minimum-scale=maximum-scale'>");
  meta.content = "minimum-scale=maximum-scale";
  yield synthesizeClick(div);
  is(div.dataset.isDelay, "false", "300ms delay between touch events and mouse events should not work");

  info("testWithMetaViewportEnabled: click the div element with <meta name='viewport' content='width=device-width'>");
  meta.content = "width=device-width";
  yield synthesizeClick(div);
  is(div.dataset.isDelay, "false", "300ms delay between touch events and mouse events should not work");
}

function* testWithMetaViewportDisabled() {
  yield pushPrefs([domViewportEnabled, false]);
  let meta = content.document.querySelector("meta[name=viewport]");
  let div = content.document.querySelector("div");
  div.dataset.isDelay = "false";

  info("testWithMetaViewportDisabled: click the div element with <meta name='viewport'>");
  meta.content = "";
  yield synthesizeClick(div);
  is(div.dataset.isDelay, "true", "300ms delay between touch events and mouse events should work");
}

function synthesizeClick(element) {
  let waitForClickEvent = BrowserTestUtils.waitForEvent(element, "click");
  BrowserTestUtils.synthesizeMouseAtCenter(element, { type: "mousedown", isSynthesized: false },
        gBrowser.selectedBrowser);
  BrowserTestUtils.synthesizeMouseAtCenter(element, { type: "mouseup", isSynthesized: false },
        gBrowser.selectedBrowser);
  return waitForClickEvent;
}

function pushPrefs(...aPrefs) {
  let deferred = promise.defer();
  SpecialPowers.pushPrefEnv({"set": aPrefs}, deferred.resolve);
  return deferred.promise;
}
