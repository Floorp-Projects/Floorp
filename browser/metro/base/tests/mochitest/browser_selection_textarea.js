/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let gWindow = null;
var gFrame = null;

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
  InputSourceHelper.isPrecise = false;
  InputSourceHelper.fireUpdate();
}

gTests.push({
  desc: "normalize browser",
  setUp: setUpAndTearDown,
  tearDown: setUpAndTearDown,
  run: function test() {
    info(chromeRoot + "browser_selection_textarea.html");
    yield addTab(chromeRoot + "browser_selection_textarea.html");

    yield waitForCondition(function () {
      return !BrowserUI.isStartTabVisible;
      }, 10000, 100);

    yield hideContextUI();

    gWindow = Browser.selectedTab.browser.contentWindow;
  },
});

gTests.push({
  desc: "textarea selection and drag",
  setUp: setUpAndTearDown,
  tearDown: setUpAndTearDown,
  run: function test() {
    // work around for buggy context menu display
    yield waitForMs(100);

    let textarea = gWindow.document.getElementById("inputtext");
    textarea.focus();

    let promise = waitForEvent(document, "popupshown");
    sendContextMenuClickToElement(gWindow, textarea, 20, 10);
    yield promise;

    checkContextUIMenuItemVisibility(["context-select",
                                      "context-select-all"]);

    let menuItem = document.getElementById("context-select");
    ok(menuItem, "menu item exists");
    ok(!menuItem.hidden, "menu item visible");
    let popupPromise = waitForEvent(document, "popuphidden");
    sendElementTap(gWindow, menuItem);
    yield popupPromise;
    ok(popupPromise && !(popupPromise instanceof Error), "promise error");

    yield waitForCondition(function () {
        return SelectionHelperUI.isSelectionUIVisible;
      }, kCommonWaitMs, kCommonPollMs);

    is(SelectionHelperUI.isActive, true, "selection active");
    is(getTrimmedSelection(textarea).toString(), "Alice", "selection test");

    let xpos = SelectionHelperUI.endMark.xPos;
    let ypos = SelectionHelperUI.endMark.yPos + 10;

    var touchdrag = new TouchDragAndHold();

    // end marker and off the text area to the right
    yield touchdrag.start(gWindow, xpos, ypos, 1200, 400);
    let token = "(end)";
    yield waitForCondition(function () {
      let selection = getTrimmedSelection(textarea).toString();
      if (selection.length < token.length ||
          selection.substring(selection.length - token.length) != token) {
        return false;
      }
      return true;
    }, 5000, 100);

    touchdrag.end();
    touchdrag = null;

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
