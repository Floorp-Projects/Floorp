/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let gWindow = null;
var gFrame = null;
var gInput = null;

const kMarkerOffsetY = 12;
const kCommonWaitMs = 5000;
const kCommonPollMs = 100;

///////////////////////////////////////////////////
// form input tests
///////////////////////////////////////////////////

function setUpAndTearDown() {
  emptyClipboard();
  if (gWindow)
    clearSelection(gWindow);
  if (gFrame)
    clearSelection(gFrame);
  if (gInput)
    clearSelection(gInput);
  yield waitForCondition(function () {
      return !SelectionHelperUI.isSelectionUIVisible;
    }, kCommonWaitMs, kCommonPollMs);
}

gTests.push({
  desc: "normalize browser",
  setUp: setUpAndTearDown,
  tearDown: setUpAndTearDown,
  run: function test() {
    info(chromeRoot + "browser_selection_frame_inputs.html");
    yield addTab(chromeRoot + "browser_selection_frame_inputs.html");

    yield waitForCondition(function () {
      return !StartUI.isStartPageVisible;
      }, 10000, 100);

    yield hideContextUI();

    gWindow = Browser.selectedTab.browser.contentWindow;
    gFrame = gWindow.document.getElementById("frame1");
    gInput = gFrame.contentDocument.getElementById("textinput");

    ok(gWindow != null, "gWindow");
    ok(gFrame != null, "gFrame");
    ok(gInput != null, "gInput");

    InputSourceHelper.isPrecise = false;
  },
});

gTests.push({
  desc: "basic selection",
  setUp: setUpAndTearDown,
  tearDown: setUpAndTearDown,
  run: function test() {
    gInput.focus();
    gInput.selectionStart = gInput.selectionEnd = 0;

    let promise = waitForEvent(document, "popupshown");
    sendContextMenuClick(232, 583);
    yield promise;

    checkContextUIMenuItemVisibility(["context-select",
                                      "context-select-all"]);

    let menuItem = document.getElementById("context-select");
    ok(menuItem, "menu item exists");
    ok(!menuItem.hidden, "menu item visible");
    let popupPromise = waitForEvent(document, "popuphidden");
    EventUtils.synthesizeMouse(menuItem, 10, 10, {}, gWindow);
    yield popupPromise;

    yield waitForCondition(function () {
        return SelectionHelperUI.isSelectionUIVisible;
      }, kCommonWaitMs, kCommonPollMs);

    is(getTrimmedSelection(gInput).toString(), "straight", "selection test");

    checkMonoclePositionRange("start", 210, 220, 600, 605);
    checkMonoclePositionRange("end", 250, 260, 600, 605);
  },
});

gTests.push({
  desc: "drag selection",
  setUp: setUpAndTearDown,
  tearDown: setUpAndTearDown,
  run: function test() {
    gInput.focus();
    gInput.selectionStart = gInput.selectionEnd = 0;

    let promise = waitForEvent(document, "popupshown");
    sendContextMenuClick(232, 583);
    yield promise;

    checkContextUIMenuItemVisibility(["context-select",
                                      "context-select-all"]);

    let menuItem = document.getElementById("context-select");
    ok(menuItem, "menu item exists");
    ok(!menuItem.hidden, "menu item visible");
    let popupPromise = waitForEvent(document, "popuphidden");
    EventUtils.synthesizeMouse(menuItem, 10, 10, {}, gWindow);
    yield popupPromise;

    yield waitForCondition(function () {
        return SelectionHelperUI.isSelectionUIVisible;
      }, kCommonWaitMs, kCommonPollMs);

    is(getTrimmedSelection(gInput).toString(), "straight", "selection test");

    // end marker to the right
    let xpos = SelectionHelperUI.endMark.xPos;
    let ypos = SelectionHelperUI.endMark.yPos + 10;
    var touchdrag = new TouchDragAndHold();
    yield touchdrag.start(gWindow, xpos, ypos, xpos + 350, ypos);
    yield waitForCondition(function () {
      return getTrimmedSelection(gInput).toString() == 
        "straight on like a tunnel for some way and then dipped suddenly down";
    }, 6000, 2000);
    touchdrag.end();

    yield waitForCondition(function () {
        return !SelectionHelperUI.hasActiveDrag;
      }, kCommonWaitMs, kCommonPollMs);
    yield SelectionHelperUI.pingSelectionHandler();

    // start marker to the left
    let xpos = SelectionHelperUI.startMark.xPos;
    let ypos = SelectionHelperUI.startMark.yPos + 10;
    var touchdrag = new TouchDragAndHold();
    yield touchdrag.start(gWindow, xpos, ypos, 10, ypos);
    yield waitForCondition(function () {
      return getTrimmedSelection(gInput).toString() == 
        "The rabbit-hole went straight on like a tunnel for some way and then dipped suddenly down";
    }, 6000, 2000);
    touchdrag.end();

    yield waitForCondition(function () {
        return !SelectionHelperUI.hasActiveDrag;
      }, kCommonWaitMs, kCommonPollMs);
    yield SelectionHelperUI.pingSelectionHandler();
  },
});

function test() {
  if (!isLandscapeMode()) {
    todo(false, "browser_selection_tests need landscape mode to run.");
    return;
  }

  requestLongerTimeout(3);
  runTests();
}
