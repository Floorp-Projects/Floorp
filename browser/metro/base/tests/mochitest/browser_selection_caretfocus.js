/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let gWindow = null;

///////////////////////////////////////////////////
// form input tests
///////////////////////////////////////////////////

function setUpAndTearDown() {
  emptyClipboard();
  if (gWindow)
    clearSelection(gWindow);
  yield waitForCondition(function () {
    return !SelectionHelperUI.isSelectionUIVisible;
  });
  InputSourceHelper.isPrecise = false;
  InputSourceHelper.fireUpdate();
}

gTests.push({
  desc: "normalize browser",
  setUp: setUpAndTearDown,
  tearDown: setUpAndTearDown,
  run: function test() {
    info(chromeRoot + "browser_selection_caretfocus.html");
    yield addTab(chromeRoot + "browser_selection_caretfocus.html");

    yield waitForCondition(function () {
      return !BrowserUI.isStartTabVisible;
    });

    yield hideContextUI();

    gWindow = Browser.selectedTab.browser.contentWindow;
  },
});

function tapText(aIndex) {
  gWindow = Browser.selectedTab.browser.contentWindow;
  let id = "Text" + aIndex;
  info("tapping " + id);
  let element = gWindow.document.getElementById(id);
  if (element.contentDocument) {
    element = element.contentDocument.getElementById("textarea");
    gWindow = element.ownerDocument.defaultView;
  }
  sendElementTap(gWindow, element, 100, 10);
  return element;
}

gTests.push({
  desc: "focus navigation",
  setUp: setUpAndTearDown,
  tearDown: setUpAndTearDown,
  run: function test() {
    for (let iteration = 0; iteration < 3; iteration++) {
      for (let input = 1; input <= 6; input++) {
        let element = tapText(input);
        if (input == 6) {
          // div
          yield waitForCondition(function () {
            return !SelectionHelperUI.isActive;
          });
        } else {
          // input
          yield SelectionHelperUI.pingSelectionHandler();
          yield waitForCondition(function () {
            return SelectionHelperUI.isCaretUIVisible;
          });
          ok(element == gWindow.document.activeElement, "element has focus");
        }
      }
    }
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
