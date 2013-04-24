/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let gWindow = null;
var gFrame = null;

const kMarkerOffsetY = 12;
const kCommonWaitMs = 5000;
const kCommonPollMs = 100;

///////////////////////////////////////////////////
// text area tests
///////////////////////////////////////////////////

function setUpAndTearDown() {
  emptyClipboard();
  if (gWindow)
    clearSelection(gWindow);
  if (gFrame)
    clearSelection(gFrame);
  yield waitForCondition(function () {
      return !SelectionHelperUI.isSelectionUIVisible;
    }, kCommonWaitMs, kCommonPollMs);
}

gTests.push({
  desc: "normalize browser",
  setUp: setUpAndTearDown,
  tearDown: setUpAndTearDown,
  run: function test() {
    info(chromeRoot + "browser_selection_textarea.html");
    yield addTab(chromeRoot + "browser_selection_textarea.html");

    yield waitForCondition(function () {
      return !StartUI.isStartPageVisible;
      }, 10000, 100);

    yield hideContextUI();

    gWindow = Browser.selectedTab.browser.contentWindow;
    InputSourceHelper.isPrecise = false;
  },
});

gTests.push({
  desc: "textarea basic selection",
  setUp: setUpAndTearDown,
  tearDown: setUpAndTearDown,
  run: function test() {
    let textarea = gWindow.document.getElementById("inputtext");
    textarea.focus();

    let promise = waitForEvent(document, "popupshown");
    sendContextMenuClick(355, 50);
    yield promise;

    checkContextUIMenuItemVisibility(["context-select",
                                      "context-select-all"]);

    let menuItem = document.getElementById("context-select");
    ok(menuItem, "menu item exists");
    ok(!menuItem.hidden, "menu item visible");
    let popupPromise = waitForEvent(document, "popuphidden");
    EventUtils.synthesizeMouse(menuItem, 10, 10, {}, gWindow);
    yield popupPromise;
    ok(popupPromise && !(popupPromise instanceof Error), "promise error");

    yield waitForCondition(function () {
        return SelectionHelperUI.isSelectionUIVisible;
      }, kCommonWaitMs, kCommonPollMs);

    // check text selection
    is(getTrimmedSelection(textarea).toString(), "pictures", "selection test");

    clearSelection(textarea);
  },
});

gTests.push({
  desc: "textarea complex drag selection",
  setUp: setUpAndTearDown,
  tearDown: setUpAndTearDown,
  run: function test() {
    // work around for buggy context menu display
    yield waitForMs(100);

    let textarea = gWindow.document.getElementById("inputtext");

    let promise = waitForEvent(document, "popupshown");
    sendContextMenuClick(355, 50);
    yield promise;

    checkContextUIMenuItemVisibility(["context-select",
                                      "context-select-all"]);

    let menuItem = document.getElementById("context-select");
    ok(menuItem, "menu item exists");
    ok(!menuItem.hidden, "menu item visible");
    let popupPromise = waitForEvent(document, "popuphidden");
    EventUtils.synthesizeMouse(menuItem, 10, 10, {}, gWindow);
    yield popupPromise;
    ok(popupPromise && !(popupPromise instanceof Error), "promise error");

    yield waitForCondition(function () {
        return SelectionHelperUI.isSelectionUIVisible;
      }, kCommonWaitMs, kCommonPollMs);

    is(SelectionHelperUI.isActive, true, "selection active");
    is(getTrimmedSelection(textarea).toString(), "pictures", "selection test");

    let xpos = SelectionHelperUI.endMark.xPos;
    let ypos = SelectionHelperUI.endMark.yPos + 10;

    var touchdrag = new TouchDragAndHold();

    // end marker and off the text area to the right
    yield touchdrag.start(gWindow, xpos, ypos, 1200, 400);
    let textLength = getTrimmedSelection(textarea).toString().length;
    yield waitForCondition(function () {
      let newTextLength = getTrimmedSelection(textarea).toString().length;
      if (textLength != newTextLength) {
        textLength = newTextLength;
        return false;
      }
      return true;
    }, 45000, 1000);

    touchdrag.end();
    touchdrag = null;

    yield waitForCondition(function () {
        return !SelectionHelperUI.hasActiveDrag;
      }, kCommonWaitMs, kCommonPollMs);
    yield SelectionHelperUI.pingSelectionHandler();

    let text = getTrimmedSelection(textarea).toString();
    let end = text.substring(text.length - "(end)".length);
    is(end, "(end)", "selection test");
  },
});

function test() {
  if (isDebugBuild()) {
    todo(false, "selection tests can't run in debug builds.");
    return;
  }

  if (!isLandscapeMode()) {
    todo(false, "browser_selection_tests need landscape mode to run.");
    return;
  }

  requestLongerTimeout(3);
  runTests();
}
