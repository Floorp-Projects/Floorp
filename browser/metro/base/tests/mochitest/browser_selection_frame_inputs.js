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
  InputSourceHelper.isPrecise = false;
  InputSourceHelper.fireUpdate();
}

gTests.push({
  desc: "normalize browser",
  setUp: setUpAndTearDown,
  tearDown: setUpAndTearDown,
  run: function test() {
    info(chromeRoot + "browser_selection_frame_inputs.html");
    yield addTab(chromeRoot + "browser_selection_frame_inputs.html");

    yield waitForCondition(function () {
      return !BrowserUI.isStartTabVisible;
      }, 10000, 100);

    yield hideContextUI();

    gWindow = Browser.selectedTab.browser.contentWindow;
    gFrame = gWindow.document.getElementById("frame1");
    gInput = gFrame.contentDocument.getElementById("textinput");

    ok(gWindow != null, "gWindow");
    ok(gFrame != null, "gFrame");
    ok(gInput != null, "gInput");
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
    sendContextMenuClickToElement(gWindow, gFrame, 135, 10);
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

    is(getTrimmedSelection(gInput).toString(), "straight", "selection test");

    let rect = gFrame.getBoundingClientRect();
    checkMonoclePositionRange("start", rect.left + 125, rect.left + 135, rect.top + 20, rect.top + 30);
    checkMonoclePositionRange("end", rect.left + 165, rect.left + 175, rect.top + 20, rect.top + 30);
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
    sendContextMenuClickToElement(gWindow, gFrame, 135, 10);
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
  runTests();
}
