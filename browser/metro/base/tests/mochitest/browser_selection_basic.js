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
// content (non-editable) tests
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
  yield hideContextUI();
}

gTests.push({
  desc: "normalize browser",
  setUp: setUpAndTearDown,
  tearDown: setUpAndTearDown,
  run: function test() {
    info(chromeRoot + "browser_selection_basic.html");
    yield addTab(chromeRoot + "browser_selection_basic.html");

    yield waitForCondition(function () {
        return !StartUI.isStartPageVisible;
      }, 10000, 100);

    gWindow = Browser.selectedTab.browser.contentWindow;
    InputSourceHelper.isPrecise = false;
  },
});

gTests.push({
  desc: "tap-hold to select",
  setUp: setUpAndTearDown,
  tearDown: setUpAndTearDown,
  run: function test() {
    sendContextMenuClick(30, 20);

    yield waitForCondition(function () {
        return SelectionHelperUI.isSelectionUIVisible;
      }, kCommonWaitMs, kCommonPollMs);

    is(getTrimmedSelection(gWindow).toString(), "There", "selection test");
  },
});

gTests.push({
  desc: "double-tap to select",
  setUp: setUpAndTearDown,
  tearDown: setUpAndTearDown,
  run: function test() {
    sendDoubleTap(gWindow, 30, 20);

    yield waitForCondition(function () {
        return SelectionHelperUI.isSelectionUIVisible;
      }, kCommonWaitMs, kCommonPollMs);

    is(getTrimmedSelection(gWindow).toString(), "There", "selection test");
  },
});

gTests.push({
  desc: "appbar interactions",
  setUp: setUpAndTearDown,
  tearDown: setUpAndTearDown,
  run: function test() {
    sendContextMenuClick(100, 20);

    yield waitForCondition(function () {
        return SelectionHelperUI.isSelectionUIVisible;
      }, kCommonWaitMs, kCommonPollMs);

    is(SelectionHelperUI.isActive, true, "selection active");
    is(getTrimmedSelection(gWindow).toString(), "nothing", "selection test");

    yield fireAppBarDisplayEvent();

    ok(ContextUI.isVisible, true, "appbar visible");

    yield hideContextUI();

    ok(!ContextUI.isVisible, true, "appbar hidden");
  },
});

gTests.push({
  desc: "simple drag selection",
  setUp: setUpAndTearDown,
  tearDown: setUpAndTearDown,
  run: function test() {
    yield waitForMs(100);
    sendContextMenuClick(100, 20);

    yield waitForCondition(function () {
        return SelectionHelperUI.isSelectionUIVisible;
      }, kCommonWaitMs, kCommonPollMs);

    is(SelectionHelperUI.isActive, true, "selection active");
    is(getTrimmedSelection(gWindow).toString(), "nothing", "selection test");

    let ypos = SelectionHelperUI.endMark.yPos + kMarkerOffsetY;

    let touchdrag = new TouchDragAndHold();
    yield touchdrag.start(gWindow, SelectionHelperUI.endMark.xPos, ypos, 190, ypos);
    touchdrag.end();

    yield waitForCondition(function () {
        return !SelectionHelperUI.hasActiveDrag;
      }, kCommonWaitMs, kCommonPollMs);
    yield SelectionHelperUI.pingSelectionHandler();

    is(SelectionHelperUI.isActive, true, "selection active");
    is(getTrimmedSelection(gWindow).toString(), "nothing so VERY", "selection test");
  },
});

gTests.push({
  desc: "expand / collapse selection",
  setUp: setUpAndTearDown,
  tearDown: setUpAndTearDown,
  run: function test() {
    sendContextMenuClick(30, 20);

    yield waitForCondition(function () {
        return SelectionHelperUI.isSelectionUIVisible;
      }, kCommonWaitMs, kCommonPollMs);

    is(SelectionHelperUI.isActive, true, "initial active");
    is(getTrimmedSelection(gWindow).toString(), "There", "initial selection test");

    for (let count = 0; count < 5; count++) {
      let ypos = SelectionHelperUI.endMark.yPos + kMarkerOffsetY;

      let touchdrag = new TouchDragAndHold();
      yield touchdrag.start(gWindow, SelectionHelperUI.endMark.xPos, ypos, 550, ypos);
      touchdrag.end();

      yield waitForCondition(function () {
          return !SelectionHelperUI.hasActiveDrag;
        }, kCommonWaitMs, kCommonPollMs);
      yield SelectionHelperUI.pingSelectionHandler();

      is(getTrimmedSelection(gWindow).toString(),
        "There was nothing so VERY remarkable in that; nor did Alice think it so",
        "long selection test");

      is(SelectionHelperUI.isActive, true, "selection active");

      touchdrag = new TouchDragAndHold();
      yield touchdrag.start(gWindow, SelectionHelperUI.endMark.xPos, ypos, 40, ypos);
      touchdrag.end();

      yield waitForCondition(function () {
          return !SelectionHelperUI.hasActiveDrag;
        }, kCommonWaitMs, kCommonPollMs);
      yield SelectionHelperUI.pingSelectionHandler();

      is(SelectionHelperUI.isActive, true, "selection active");
      is(getTrimmedSelection(gWindow).toString(), "There was", "short selection test");
    }
  },
});

gTests.push({
  desc: "expand / collapse selection scolled content",
  setUp: setUpAndTearDown,
  run: function test() {
    let scrollPromise = waitForEvent(gWindow, "scroll");
    gWindow.scrollBy(0, 200);
    yield scrollPromise;
    ok(scrollPromise && !(scrollPromise instanceof Error), "scrollPromise error");

    sendContextMenuClick(106, 20);

    yield waitForCondition(function () {
        return SelectionHelperUI.isSelectionUIVisible;
      }, kCommonWaitMs, kCommonPollMs);

    is(SelectionHelperUI.isActive, true, "selection active");
    is(getTrimmedSelection(gWindow).toString(), "moment", "selection test");

    let ypos = SelectionHelperUI.endMark.yPos + kMarkerOffsetY;

    let touchdrag = new TouchDragAndHold();
    yield touchdrag.start(gWindow, SelectionHelperUI.endMark.xPos, ypos, 550, ypos);
    touchdrag.end();

    yield waitForCondition(function () {
        return !SelectionHelperUI.hasActiveDrag;
      }, kCommonWaitMs, kCommonPollMs);
    yield SelectionHelperUI.pingSelectionHandler();

    is(getTrimmedSelection(gWindow).toString(),
      "moment down went Alice after it, never once considering how in",
      "selection test");

    is(SelectionHelperUI.isActive, true, "selection active");

    touchdrag = new TouchDragAndHold();
    yield touchdrag.start(gWindow, SelectionHelperUI.endMark.xPos, ypos, 150, ypos);
    touchdrag.end();
    
    yield waitForCondition(function () {
        return !SelectionHelperUI.hasActiveDrag;
      }, kCommonWaitMs, kCommonPollMs);
    yield SelectionHelperUI.pingSelectionHandler();

    is(getTrimmedSelection(gWindow).toString(), "moment down went", "selection test");

    touchdrag = new TouchDragAndHold();
    yield touchdrag.start(gWindow, SelectionHelperUI.endMark.xPos, ypos, 550, ypos);
    touchdrag.end();

    yield waitForCondition(function () {
        return !SelectionHelperUI.hasActiveDrag;
      }, kCommonWaitMs, kCommonPollMs);
    yield SelectionHelperUI.pingSelectionHandler();

    is(getTrimmedSelection(gWindow).toString(),
      "moment down went Alice after it, never once considering how in",
      "selection test");

    touchdrag = new TouchDragAndHold();
    yield touchdrag.start(gWindow, SelectionHelperUI.endMark.xPos, ypos, 160, ypos);
    touchdrag.end();
    
    yield waitForCondition(function () {
        return !SelectionHelperUI.hasActiveDrag;
      }, kCommonWaitMs, kCommonPollMs);
    yield SelectionHelperUI.pingSelectionHandler();

    is(getTrimmedSelection(gWindow).toString(),
      "moment down went",
      "selection test");
  },
  tearDown: function tearDown() {
    let scrollPromise = waitForEvent(gWindow, "scroll");
    gWindow.scrollBy(0, -200);
    yield scrollPromise;
    emptyClipboard();
    if (gWindow)
      clearSelection(gWindow);
    if (gFrame)
      clearSelection(gFrame);
    yield waitForCondition(function () {
        return !SelectionHelperUI.isSelectionUIVisible;
      }, kCommonWaitMs, kCommonPollMs);
    yield hideContextUI();
  },
});

gTests.push({
  desc: "scroll disables",
  setUp: setUpAndTearDown,
  run: function test() {
    sendContextMenuClick(100, 20);

    yield waitForCondition(function () {
        return SelectionHelperUI.isSelectionUIVisible;
      }, kCommonWaitMs, kCommonPollMs);

    is(SelectionHelperUI.isActive, true, "selection active");

    // scroll page
    sendTouchDrag(gWindow,
                  400,
                  400,
                  400,
                  200);

    yield waitForCondition(function () {
        return !SelectionHelperUI.isSelectionUIVisible;
      }, kCommonWaitMs, kCommonPollMs);

    // active state - should be disabled after a page scroll
    is(SelectionHelperUI.isActive, false, "selection inactive");
  },
  tearDown: function tearDown() {
    EventUtils.synthesizeKey("VK_HOME", {}, gWindow);
    emptyClipboard();
    if (gWindow)
      clearSelection(gWindow);
    if (gFrame)
      clearSelection(gFrame);
    yield waitForCondition(function () {
        return !SelectionHelperUI.isSelectionUIVisible;
      }, kCommonWaitMs, kCommonPollMs);
    yield hideContextUI();
  },
});

/*
disable until bug 860248 is addressed.
gTests.push({
  desc: "double-tap copy text in content",
  setUp: setUpHelper,
  run: function test() {

    sendContextMenuClick(30, 20);

    yield waitForCondition(function () {
        return SelectionHelperUI.isSelectionUIVisible;
      }, kCommonWaitMs, kCommonPollMs);

    sendDoubleTap(gWindow, 30, 20);

    yield waitForCondition(function () {
        return !SelectionHelperUI.isSelectionUIVisible;
      }, kCommonWaitMs, kCommonPollMs);
    
    // check copy text results
    let text = SpecialPowers.getClipboardData("text/unicode").trim();
    is(text, "There", "copy text test");

    // check for active selection
    is(getTrimmedSelection(gWindow).toString(), "", "selection test");
  },
  tearDown: tearDownHelper,
});

gTests.push({
  desc: "double-tap copy text in scrolled content",
  setUp: setUpHelper,
  run: function test() {
    let scrollPromise = waitForEvent(gWindow, "scroll");
    gWindow.scrollBy(0, 200);
    yield scrollPromise;
    ok(scrollPromise && !(scrollPromise instanceof Error), "scrollPromise error");

    sendContextMenuClick(30, 100);

    yield waitForCondition(function () {
        return SelectionHelperUI.isSelectionUIVisible;
      }, kCommonWaitMs, kCommonPollMs);

    sendDoubleTap(gWindow, 42, 100);

    yield waitForCondition(function () {
        return !SelectionHelperUI.isSelectionUIVisible;
      }, kCommonWaitMs, kCommonPollMs);

    // check copy text results
    let text = SpecialPowers.getClipboardData("text/unicode");
    is(text, "suddenly", "copy text test");

    // check for active selection
    is(getTrimmedSelection(gWindow).toString(), "", "selection test");
  },
  tearDown: function tearDown() {
    emptyClipboard();
    clearSelection(gWindow);
    let scrollPromise = waitForEvent(gWindow, "scroll");
    gWindow.scrollBy(0, -200);
    yield scrollPromise;
    yield waitForCondition(function () {
        return !SelectionHelperUI.isSelectionUIVisible;
      }, kCommonWaitMs, kCommonPollMs);
  },
});

gTests.push({
  desc: "single clicks on selection in non-editable content",
  setUp: setUpHelper,
  run: function test() {
    sendContextMenuClick(100, 20);

    yield waitForCondition(function () {
        return SelectionHelperUI.isSelectionUIVisible;
      }, kCommonWaitMs, kCommonPollMs);

    // active state
    is(SelectionHelperUI.isActive, true, "selection active");

    let ypos = SelectionHelperUI.endMark.yPos + kMarkerOffsetY;
    let touchdrag = new TouchDragAndHold();
    yield touchdrag.start(gWindow, SelectionHelperUI.endMark.xPos, ypos, 190, ypos);
    touchdrag.end();

    yield waitForCondition(function () {
        return !SelectionHelperUI.hasActiveDrag;
      }, kCommonWaitMs, kCommonPollMs);
    yield SelectionHelperUI.pingSelectionHandler();

    // active state
    is(SelectionHelperUI.isActive, true, "selection active");

    // click on selected text - nothing should change
    sendTap(gWindow, 240, 20);

    is(SelectionHelperUI.isActive, true, "selection active");

    // click outside the text - nothing should change
    sendTap(gWindow, 197, 119);

    is(SelectionHelperUI.isActive, true, "selection active");
  },
  tearDown: tearDownHelper,
});
*/

function test() {
  if (!isLandscapeMode()) {
    todo(false, "browser_selection_tests need landscape mode to run.");
    return;
  }

  requestLongerTimeout(3);
  runTests();
}
