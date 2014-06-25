// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function test() {
  if (!isLandscapeMode()) {
    todo(false, "browser_snapped_tests need landscape mode to run.");
    return;
  }

  runTests();
}

let kTransformTimeout = 5000;

let gEdit = null;
let tabAdded = false;

function setUp() {
  if (!tabAdded) {
    yield addTab(chromeRoot + "res/textdivs01.html");
    tabAdded = true;
  }
  yield hideContextUI();
}

/*
gTests.push({
  desc: "soft keyboard reliability",
  setUp: setUp,
  run: function() {
    yield waitForMs(3000);

    let edit = Browser.selectedBrowser.contentDocument.getElementById("textinput");
    // show the soft keyboard
    let keyboardPromise = waitForObserver("metro_softkeyboard_shown", 20000);
    sendNativeTap(edit);
    yield waitForMs(5000);
    sendNativeTap(edit);
    yield keyboardPromise;
    yield waitForMs(5000);

    // hide the soft keyboard / navbar
    keyboardPromise = waitForObserver("metro_softkeyboard_hidden", 20000);
    sendNativeTap(Browser.selectedBrowser.contentDocument.getElementById("first"));
    yield keyboardPromise;
    yield waitForMs(5000);
  },
  tearDown: function () {
    clearNativeTouchSequence();
  }
});
*/

gTests.push({
  desc: "native long tap works",
  setUp: setUp,
  run: function() {
    let edit = Browser.selectedBrowser.contentDocument.getElementById("textinput");
    let promise = waitForEvent(document, "popupshown");
    sendNativeLongTap(edit);
    yield promise;
    ContextMenuUI.hide();
  },
  tearDown: function () {
    clearNativeTouchSequence();
  }
});

/* Double-tap is disabled (bug 950832).
gTests.push({
  desc: "double tap transforms",
  setUp: setUp,
  run: function() {
    let beginPromise = waitForObserver("apzc-transform-begin", kTransformTimeout);
    let endPromise = waitForObserver("apzc-transform-end", kTransformTimeout);

    sendNativeDoubleTap(Browser.selectedBrowser.contentDocument.getElementById("second"));

    yield beginPromise;
    yield endPromise;

    beginPromise = waitForObserver("apzc-transform-begin", kTransformTimeout);
    endPromise = waitForObserver("apzc-transform-end", kTransformTimeout);

    sendNativeDoubleTap(Browser.selectedBrowser.contentDocument.getElementById("second"));

    yield beginPromise;
    yield endPromise;
  },
  tearDown: function () {
    clearNativeTouchSequence();
  }
});
*/

gTests.push({
  desc: "scroll transforms",
  setUp: setUp,
  run: function() {
    let beginPromise = waitForObserver("apzc-transform-begin", kTransformTimeout);
    let endPromise = waitForObserver("apzc-transform-end", kTransformTimeout);

    var touchdrag = new TouchDragAndHold();
    touchdrag.useNativeEvents = true;
    touchdrag.nativePointerId = 1;
    yield touchdrag.start(Browser.selectedTab.browser.contentWindow,
                          10, 100, 10, 10);
    touchdrag.end();

    yield beginPromise;
    yield endPromise;
  },
  tearDown: function () {
    clearNativeTouchSequence();
  }
});
