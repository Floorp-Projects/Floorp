/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let gWindow = null;
var gFrame = null;

function setUpAndTearDown() {
  emptyClipboard();

  if (gWindow) {
    clearSelection(gWindow);
  }

  if (gFrame) {
    clearSelection(gFrame);
  }

  yield waitForCondition(function () {
    return !SelectionHelperUI.isSelectionUIVisible;
  });

  InputSourceHelper.isPrecise = false;
  InputSourceHelper.fireUpdate();
}

gTests.push({
  desc: "Selection monocles for frame content that is located inside " +
        "scrollable container.",
  setUp: setUpAndTearDown,
  tearDown: setUpAndTearDown,
  run: function test() {
    let urlToLoad = chromeRoot +
        "browser_selection_frame_in_scrollable_container.html";
    info(urlToLoad);
    yield addTab(urlToLoad);

    ContextUI.dismiss();
    yield waitForCondition(() => !ContextUI.navbarVisible);

    gWindow = Browser.selectedTab.browser.contentWindow;
    gFrame = gWindow.document.getElementById("frame1");

    // Select some content inside frame.
    let promise = waitForEvent(document, "popupshown");
    sendContextMenuClickToWindow(gFrame.contentWindow, 10, 10);
    yield promise;

    let selectMenuItem = document.getElementById("context-select");
    promise = waitForEvent(document, "popuphidden");
    sendElementTap(gWindow, selectMenuItem);
    yield promise;
    yield waitForCondition(()=>SelectionHelperUI.isSelectionUIVisible);

    // Scroll frame inside scrollable container.
    let initialYPos = SelectionHelperUI.endMark.yPos;
    let touchDrag = new TouchDragAndHold();
    touchDrag.useNativeEvents = true;
    yield touchDrag.start(gWindow, 100, 90, 100, 50);
    touchDrag.end();

    yield waitForCondition(() => !SelectionHelperUI.hasActiveDrag);
    yield SelectionHelperUI.pingSelectionHandler();

    yield waitForCondition(()=>SelectionHelperUI.isSelectionUIVisible);

    ok(initialYPos - SelectionHelperUI.endMark.yPos > 10,
        "Selection monocles followed scrolled content.");
  }
});

function test() {
  // We need this until bug 859742 is fully resolved.
  setDevPixelEqualToPx();
  runTests();
}
