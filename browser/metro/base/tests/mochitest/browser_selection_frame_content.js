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
// sub frame content tests
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
  InputSourceHelper.isPrecise = false;
  InputSourceHelper.fireUpdate();
}

gTests.push({
  desc: "normalize browser",
  setUp: setUpAndTearDown,
  tearDown: setUpAndTearDown,
  run: function test() {
    info(chromeRoot + "browser_selection_frame_content.html");
    yield addTab(chromeRoot + "browser_selection_frame_content.html");

    yield waitForCondition(function () {
      return !BrowserUI.isStartTabVisible;
      }, 10000, 100);

    yield hideContextUI();

    gWindow = Browser.selectedTab.browser.contentWindow;
    gFrame = gWindow.document.getElementById("frame1");
  },
});

gTests.push({
  desc: "iframe basic selection",
  setUp: setUpAndTearDown,
  tearDown: setUpAndTearDown,
  run: function test() {
    gFrame.focus();

    sendContextMenuClick(165, 35);

    yield waitForCondition(function () {
        return SelectionHelperUI.isSelectionUIVisible;
      }, kCommonWaitMs, kCommonPollMs);

    // active state
    is(SelectionHelperUI.isActive, true, "selection active");
    // check text selection
    is(getTrimmedSelection(gFrame).toString(), "Alice", "selection test");
  },
});

gTests.push({
  desc: "iframe expand / collapse selection",
  setUp: setUpAndTearDown,
  tearDown: setUpAndTearDown,
  run: function test() {
    gFrame.focus();
    
    sendContextMenuClick(162, 180);

    yield waitForCondition(function () {
        return SelectionHelperUI.isSelectionUIVisible;
      }, kCommonWaitMs, kCommonPollMs);

    // active state
    is(SelectionHelperUI.isActive, true, "selection active");
    is(getTrimmedSelection(gFrame).toString(), "Alice", "selection test");

    let ypos = SelectionHelperUI.endMark.yPos + kMarkerOffsetY;

    let touchdrag = new TouchDragAndHold();
    yield touchdrag.start(gWindow, SelectionHelperUI.endMark.xPos, ypos, 640, ypos);
    touchdrag.end();

    yield waitForCondition(function () {
        return !SelectionHelperUI.hasActiveDrag;
      }, kCommonWaitMs, kCommonPollMs);
    yield SelectionHelperUI.pingSelectionHandler();

    is(SelectionHelperUI.isActive, true, "selection active");
    is(getTrimmedSelection(gFrame).toString(), "Alice was beginning to get very tired of sitting by her sister on the bank",
       "selection test");

    touchdrag = new TouchDragAndHold();
    yield touchdrag.start(gWindow, SelectionHelperUI.endMark.xPos, ypos, 320, ypos);
    touchdrag.end();

    yield waitForCondition(function () {
        return !SelectionHelperUI.hasActiveDrag;
      }, kCommonWaitMs, kCommonPollMs);
    yield SelectionHelperUI.pingSelectionHandler();

    is(SelectionHelperUI.isActive, true, "selection active");
    is(getTrimmedSelection(gFrame).toString(), "Alice was beginning to get very", "selection test");
  },
});

gTests.push({
  desc: "scrolled iframe selection",
  setUp: setUpAndTearDown,
  run: function test() {
    gFrame.focus();

    let scrollPromise = waitForEvent(gFrame.contentDocument.defaultView, "scroll");
    gFrame.contentDocument.defaultView.scrollBy(0, 200);
    yield scrollPromise;

    sendContextMenuClick(30, 240);

    yield waitForCondition(function () {
        return SelectionHelperUI.isSelectionUIVisible;
      }, kCommonWaitMs, kCommonPollMs);

    is(SelectionHelperUI.isActive, true, "selection active");
    is(getTrimmedSelection(gFrame).toString(), "started", "selection test");

    let promise = waitForEvent(document, "popupshown");
    sendContextMenuClick(527, 188);

    yield promise;
    ok(promise && !(promise instanceof Error), "promise error");
    ok(ContextMenuUI._menuPopup.visible, "is visible");

    let menuItem = document.getElementById("context-copy");
    ok(menuItem, "menu item exists");
    ok(!menuItem.hidden, "menu item visible");
    let popupPromise = waitForEvent(document, "popuphidden");
    sendElementTap(gWindow, menuItem);
    yield popupPromise;
    ok(popupPromise && !(popupPromise instanceof Error), "promise error");

    // The wait is needed to give time to populate the clipboard.
    let string = "";
    yield waitForCondition(function () {
      string = SpecialPowers.getClipboardData("text/unicode");
      return string === "started";
    });
    is(string, "started", "copy text");

  },
  tearDown: function tearDown() {
    emptyClipboard();
    let scrollPromise = waitForEvent(gFrame.contentDocument.defaultView, "scroll");
    gFrame.contentDocument.defaultView.scrollBy(0, -200);
    yield scrollPromise;
    clearSelection(gWindow);
    clearSelection(gFrame);
    yield waitForCondition(function () {
        return !SelectionHelperUI.isSelectionUIVisible;
      }, kCommonWaitMs, kCommonPollMs);
    yield hideContextUI();
  },
});

gTests.push({
  desc: "iframe within scrolled page selection",
  setUp: setUpAndTearDown,
  run: function test() {
    gFrame.focus();

    let scrollPromise = waitForEvent(gWindow, "scroll");
    gWindow.scrollBy(0, 200);
    yield scrollPromise;

    scrollPromise = waitForEvent(gFrame.contentDocument.defaultView, "scroll");
    gFrame.contentDocument.defaultView.scrollBy(0, 200);
    yield scrollPromise;

    sendContextMenuClick(114, 130);

    yield waitForCondition(function () {
        return SelectionHelperUI.isSelectionUIVisible;
      }, kCommonWaitMs, kCommonPollMs);

    is(SelectionHelperUI.isActive, true, "selection active");
    is(getTrimmedSelection(gFrame).toString(), "moment", "selection test");
  },
  tearDown: function tearDown() {
    emptyClipboard();
    clearSelection(gWindow);
    clearSelection(gFrame);
    let scrollPromise = waitForEvent(gFrame.contentDocument.defaultView, "scroll");
    gFrame.contentDocument.defaultView.scrollBy(0, -200);
    yield scrollPromise;
    scrollPromise = waitForEvent(gWindow, "scroll");
    gWindow.scrollBy(0, -200);
    yield scrollPromise;
    yield waitForCondition(function () {
      return !SelectionHelperUI.isSelectionUIVisible;
    }, kCommonWaitMs, kCommonPollMs);
    yield hideContextUI();
  },
});

function test() {
  if (!isLandscapeMode()) {
    todo(false, "browser_selection_tests need landscape mode to run.");
    return;
  }
  runTests();
}
