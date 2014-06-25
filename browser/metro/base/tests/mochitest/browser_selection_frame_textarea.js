/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let gWindow = null;
var gFrame = null;
var gTextArea = null;

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
  if (gTextArea)
    clearSelection(gTextArea);
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
    info(chromeRoot + "browser_selection_frame_textarea.html");
    yield addTab(chromeRoot + "browser_selection_frame_textarea.html");

    yield waitForCondition(function () {
      return !BrowserUI.isStartTabVisible;
      }, 10000, 100);

    yield hideContextUI();

    gWindow = Browser.selectedTab.browser.contentWindow;
    gFrame = gWindow.document.getElementById("frame1");
    gTextArea = gFrame.contentDocument.getElementById("textarea");
    ok(gWindow != null, "gWindow");
    ok(gFrame != null, "gFrame");
    ok(gTextArea != null, "gTextArea");
  },
});

gTests.push({
  desc: "basic selection",
  setUp: setUpAndTearDown,
  tearDown: setUpAndTearDown,
  run: function test() {
    gTextArea.focus();
    gTextArea.selectionStart = gTextArea.selectionEnd = 0;

    let promise = waitForEvent(document, "popupshown");
    sendContextMenuClickToElement(gWindow, gFrame, 220, 80);
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

    is(getTrimmedSelection(gTextArea).toString(), "wondered", "selection test");

    checkMonoclePositionRange("start", 260, 280, 675, 690);
    checkMonoclePositionRange("end", 320, 340, 675, 690);
  },
});

gTests.push({
  desc: "drag selection 1",
  setUp: setUpAndTearDown,
  tearDown: setUpAndTearDown,
  run: function test() {
    gTextArea.focus();
    gTextArea.selectionStart = gTextArea.selectionEnd = 0;

    let promise = waitForEvent(document, "popupshown");
    sendContextMenuClickToElement(gWindow, gFrame, 220, 80);
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

    is(getTrimmedSelection(gTextArea).toString(), "wondered", "selection test");

    // end marker to the right
    let xpos = SelectionHelperUI.endMark.xPos;
    let ypos = SelectionHelperUI.endMark.yPos + 10;
    var touchdrag = new TouchDragAndHold();
    yield touchdrag.start(gWindow, xpos, ypos, xpos + 150, ypos);
    yield waitForCondition(function () {
      return getTrimmedSelection(gTextArea).toString() == 
        "wondered at this,";
    }, 6000, 2000);
    touchdrag.end();

    yield waitForCondition(function () {
        return !SelectionHelperUI.hasActiveDrag;
      }, kCommonWaitMs, kCommonPollMs);
    yield SelectionHelperUI.pingSelectionHandler();

    // start marker up and to the left
    let xpos = SelectionHelperUI.startMark.xPos;
    let ypos = SelectionHelperUI.startMark.yPos + 10;
    var touchdrag = new TouchDragAndHold();
    yield touchdrag.start(gWindow, xpos, ypos, 40, 500);
    yield waitForCondition(function () {
      return getTrimmedSelection(gTextArea).toString().substring(0, 17) == 
        "There was nothing";
    }, 6000, 2000);
    touchdrag.end();

    let promise = waitForEvent(document, "popupshown");
    sendContextMenuClick(250, 640);
    yield promise;

    checkContextUIMenuItemVisibility(["context-cut",
                                      "context-copy"]);

    let menuItem = document.getElementById("context-copy");
    ok(menuItem, "menu item exists");
    ok(!menuItem.hidden, "menu item visible");
    let popupPromise = waitForEvent(document, "popuphidden");
    sendElementTap(gWindow, menuItem);
    yield popupPromise;

    let string = "";
    yield waitForCondition(function () {
      string = SpecialPowers.getClipboardData("text/unicode");
      return string.substring(0, 17) === "There was nothing";
    });
  },
});

gTests.push({
  desc: "drag selection 2",
  setUp: setUpAndTearDown,
  tearDown: setUpAndTearDown,
  run: function test() {
    gTextArea.focus();
    gTextArea.selectionStart = gTextArea.selectionEnd = 0;

    let scrollPromise = waitForEvent(gWindow, "scroll");
    gWindow.scrollBy(0, 200);
    yield scrollPromise;

    let promise = waitForEvent(document, "popupshown");
    sendContextMenuClickToElement(gWindow, gFrame, 220, 80);
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

    is(getTrimmedSelection(gTextArea).toString(), "wondered", "selection test");

    // end marker to the right
    let xpos = SelectionHelperUI.endMark.xPos;
    let ypos = SelectionHelperUI.endMark.yPos + 10;
    var touchdrag = new TouchDragAndHold();
    yield touchdrag.start(gWindow, xpos, ypos, xpos + 150, ypos);
    yield waitForCondition(function () {
      return getTrimmedSelection(gTextArea).toString() == 
        "wondered at this,";
    }, 6000, 2000);
    touchdrag.end();

    yield waitForCondition(function () {
        return !SelectionHelperUI.hasActiveDrag;
      }, kCommonWaitMs, kCommonPollMs);
    yield SelectionHelperUI.pingSelectionHandler();

    // start marker up and to the left
    let xpos = SelectionHelperUI.startMark.xPos;
    let ypos = SelectionHelperUI.startMark.yPos + 10;
    var touchdrag = new TouchDragAndHold();
    yield touchdrag.start(gWindow, xpos, ypos, 40, 300);
    yield waitForCondition(function () {
      return getTrimmedSelection(gTextArea).toString().substring(0, 17) == 
        "There was nothing";
    }, 6000, 2000);
    touchdrag.end();

    let promise = waitForEvent(document, "popupshown");
    sendContextMenuClick(250, 440);
    yield promise;

    checkContextUIMenuItemVisibility(["context-cut",
                                      "context-copy"]);

    let menuItem = document.getElementById("context-copy");
    ok(menuItem, "menu item exists");
    ok(!menuItem.hidden, "menu item visible");
    let popupPromise = waitForEvent(document, "popuphidden");
    sendElementTap(gWindow, menuItem);
    yield popupPromise;

    let string = "";
    yield waitForCondition(function () {
      string = SpecialPowers.getClipboardData("text/unicode");
      return string.substring(0, 17) === "There was nothing";
    });
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
