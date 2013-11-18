/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let gWindow = null;
var gInput = null;

const kCommonWaitMs = 7000;
const kCommonPollMs = 200;

///////////////////////////////////////////////////
// form input tests
///////////////////////////////////////////////////

function setUpAndTearDown() {
  emptyClipboard();
  if (gWindow)
    clearSelection(gWindow);
  if (gInput)
    clearSelection(gInput);
  yield waitForCondition(function () {
      return !SelectionHelperUI.isSelectionUIVisible;
    }, kCommonWaitMs, kCommonPollMs);
  InputSourceHelper.isPrecise = false;
  InputSourceHelper.fireUpdate();
}

/*
  5px top margin
  25px tall text input
  300px wide
*/

gTests.push({
  desc: "normalize browser",
  setUp: setUpAndTearDown,
  tearDown: setUpAndTearDown,
  run: function test() {
    info(chromeRoot + "browser_selection_inputs.html");
    yield addTab(chromeRoot + "browser_selection_inputs.html");

    yield waitForCondition(function () {
      return !BrowserUI.isStartTabVisible;
      });

    yield hideContextUI();

    gWindow = Browser.selectedTab.browser.contentWindow;
    gInput = gWindow.document.getElementById("a");
  },
});

gTests.push({
  desc: "basic text input selection",
  setUp: setUpAndTearDown,
  tearDown: setUpAndTearDown,
  run: function test() {
    gInput.blur();
    gInput.selectionStart = gInput.selectionEnd = 0;

    let promise = waitForEvent(document, "popupshown");
    sendContextMenuClick(200, 17);
    yield promise;

    checkContextUIMenuItemVisibility(["context-select",
                                      "context-select-all"]);

    let menuItem = document.getElementById("context-select");
    ok(menuItem, "menu item exists");
    ok(!menuItem.hidden, "menu item visible");
    let popupPromise = waitForEvent(document, "popuphidden");
    sendElementTap(gWindow, menuItem);
    yield popupPromise;

    yield waitForCondition(function () {
        return SelectionHelperUI.isSelectionUIVisible;
      }, kCommonWaitMs, kCommonPollMs);

    is(getTrimmedSelection(gInput).toString(), "went", "selection test");
    is(gWindow.document.activeElement, gInput, "input focused");
  },
});

gTests.push({
  desc: "drag left to scroll",
  setUp: setUpAndTearDown,
  tearDown: setUpAndTearDown,
  run: function test() {
    gInput.selectionStart = gInput.selectionEnd = gInput.value.length;
    yield waitForEvent(window, "scroll");

    let promise = waitForEvent(document, "popupshown");
    sendContextMenuClick(190, 17);
    yield promise;

    checkContextUIMenuItemVisibility(["context-select",
                                      "context-select-all"]);

    let menuItem = document.getElementById("context-select");
    ok(menuItem, "menu item exists");
    ok(!menuItem.hidden, "menu item visible");
    let popupPromise = waitForEvent(document, "popuphidden");
    sendElementTap(gWindow, menuItem);
    yield popupPromise;

    yield waitForCondition(function () {
        return SelectionHelperUI.isSelectionUIVisible;
      }, kCommonWaitMs, kCommonPollMs);

    // check text selection
    is(getTrimmedSelection(gInput).toString(), "way", "selection test");

    // to the left
    let xpos = SelectionHelperUI.startMark.xPos;
    let ypos = SelectionHelperUI.startMark.yPos + 10;
    var touchdrag = new TouchDragAndHold();
    yield touchdrag.start(gWindow, xpos, ypos, 10, ypos);
    yield waitForCondition(function () {
      return getTrimmedSelection(gInput).toString() == 
        "The rabbit-hole went straight on like a tunnel for some way";
    }, kCommonWaitMs, kCommonPollMs);
    touchdrag.end();

    yield waitForCondition(function () {
        return !SelectionHelperUI.hasActiveDrag;
      }, kCommonWaitMs, kCommonPollMs);
    yield SelectionHelperUI.pingSelectionHandler();
  },
});

gTests.push({
  desc: "drag right to scroll and bug 862025",
  setUp: setUpAndTearDown,
  tearDown: setUpAndTearDown,
  run: function test() {
    gInput.selectionStart = gInput.selectionEnd = 0;
    yield waitForEvent(window, "scroll");

    let promise = waitForEvent(document, "popupshown");
    sendContextMenuClick(230, 17);
    yield promise;

    checkContextUIMenuItemVisibility(["context-select",
                                      "context-select-all"]);

    let menuItem = document.getElementById("context-select");
    ok(menuItem, "menu item exists");
    ok(!menuItem.hidden, "menu item visible");
    let popupPromise = waitForEvent(document, "popuphidden");
    sendElementTap(gWindow, menuItem);
    yield popupPromise;

    yield waitForCondition(function () {
        return SelectionHelperUI.isSelectionUIVisible;
      }, kCommonWaitMs, kCommonPollMs);

    // check text selection
    is(getTrimmedSelection(gInput).toString(), "straight", "selection test");

    // to the right with scroll
    let xpos = SelectionHelperUI.endMark.xPos;
    let ystartpos = SelectionHelperUI.endMark.yPos + 10;
    var touchdrag = new TouchDragAndHold();
    yield touchdrag.start(gWindow, xpos, ystartpos, 510, ystartpos);
    // hold the monocle in position outside the edit to trigger drag
    yield waitForCondition(function () {
      return getTrimmedSelection(gInput).toString() == 
        "straight on like a tunnel for some way and then dipped suddenly down";
    }, kCommonWaitMs, kCommonPollMs);
    touchdrag.end();

    yield waitForCondition(function () {
        return !SelectionHelperUI.hasActiveDrag;
      }, kCommonWaitMs, kCommonPollMs);
    yield SelectionHelperUI.pingSelectionHandler();

    // straight down - selection shouldn't change
    let xpos = SelectionHelperUI.endMark.xPos;
    let ypos = SelectionHelperUI.endMark.yPos + 10;
    yield touchdrag.start(gWindow, xpos, ypos, xpos, ypos + 150);
    // no touchend here, we want to continue the drag below

    yield SelectionHelperUI.pingSelectionHandler();
    is(getTrimmedSelection(gInput).toString(), "straight on like a tunnel for some way and then dipped suddenly down", "selection test");

    // left and up with no scrolling - selection should shrink
    yield touchdrag.move(130, ystartpos);
    touchdrag.end();

    yield SelectionHelperUI.pingSelectionHandler();
    is(getTrimmedSelection(gInput).toString(), "straight on like a tunnel for", "selection test");
  },
});

function test() {
  if (!isLandscapeMode()) {
    todo(false, "browser_selection_tests need landscape mode to run.");
    return;
  }
  // XXX need this until bugs 886624 and 859742 are fully resolved
  setDevPixelEqualToPx();
  runTests();
}
