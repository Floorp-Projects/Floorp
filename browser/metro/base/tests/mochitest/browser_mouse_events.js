/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// from MouseEvents.h
const leftButtonFlag = 1;
const rightButtonFlag = 2;

gTests.push({
  desc: "Test native mouse events",
  run: function () {
    let tab = yield addTab("about:mozilla");

    // Mousemove.
    let waitForMove = waitForEvent(document, "mousemove");
    synthesizeNativeMouseMove(tab.browser, 1, 1);
    synthesizeNativeMouseMove(tab.browser, 100, 100);
    let mousemove = yield waitForMove;
    is(mousemove.cancelable, false, "mousemove is not cancelable");
    is(mousemove.buttons, 0, "no buttons are down");

    // Left button down.
    let waitForDown1 = waitForEvent(document, "mousedown");
    synthesizeNativeMouseLDown(tab.browser, 100, 100);
    let mousedown1 = yield waitForDown1;
    is(mousedown1.cancelable, true, "mousedown is cancelable");
    is(mousedown1.buttons, leftButtonFlag, "left button is down");

    // Right button down.
    let waitForDown2 = waitForEvent(document, "mousedown");
    synthesizeNativeMouseRDown(tab.browser, 100, 100);
    let mousedown2 = yield waitForDown2;
    is(mousedown2.buttons, leftButtonFlag | rightButtonFlag, "both buttons are down");

    // Left button up.
    let waitForUp1 = waitForEvent(document, "mouseup");
    synthesizeNativeMouseLUp(tab.browser, 100, 100);
    let mouseup1 = yield waitForUp1;
    is(mouseup1.buttons, rightButtonFlag, "right button is down");

    // Right button up.
    let waitForUp2 = waitForEvent(document, "mouseup");
    synthesizeNativeMouseRUp(tab.browser, 100, 100);
    let mouseup2 = yield waitForUp2;
    is(mouseup2.buttons, 0, "no buttons are down");

    Browser.closeTab(tab, { forceClose: true });
  }
});

let test = runTests;
