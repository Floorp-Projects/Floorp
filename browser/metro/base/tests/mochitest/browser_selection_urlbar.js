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

gTests.push({
  desc: "normalize browser",
  run: function test() {
    info(chromeRoot + "res/textblock01.html");
    yield addTab(chromeRoot + "res/textblock01.html");

    yield waitForCondition(function () {
      return !BrowserUI.isStartTabVisible;
      });

    yield hideContextUI();

    InputSourceHelper.isPrecise = false;
    InputSourceHelper.fireUpdate();
  },
});

gTests.push({
  desc: "nav bar display",
  run: function test() {
    gWindow = window;

    yield showNavBar();

    let edit = document.getElementById("urlbar-edit");

    sendElementTap(window, edit, 100, 10);

    ok(SelectionHelperUI.isSelectionUIVisible, "selection ui active");

    sendElementTap(window, edit, 70, 10);

    ok(SelectionHelperUI.isCaretUIVisible, "caret ui active");

    // to the right
    let xpos = SelectionHelperUI.caretMark.xPos;
    let ypos = SelectionHelperUI.caretMark.yPos + 10;
    var touchdrag = new TouchDragAndHold();
    yield touchdrag.start(gWindow, xpos, ypos, 900, ypos);
    yield waitForCondition(function () {
      return getTrimmedSelection(edit).toString() ==
        "mochitests/content/metro/browser/metro/base/tests/mochitest/res/textblock01.html";
    }, kCommonWaitMs, kCommonPollMs);
    touchdrag.end();
    yield waitForMs(100);

    ok(SelectionHelperUI.isSelectionUIVisible, "selection ui active");
  },
});

gTests.push({
  desc: "bug 887120 - tap & hold to paste into urlbar",
  run: function() {
    gWindow = window;

    yield showNavBar();
    let edit = document.getElementById("urlbar-edit");

    SpecialPowers.clipboardCopyString("mozilla");
    sendContextMenuClickToElement(window, edit);
    yield waitForEvent(document, "popupshown");

    ok(ContextMenuUI._menuPopup.visible, "is visible");
    let paste = document.getElementById("context-paste");
    ok(!paste.hidden, "paste item is visible");

    sendElementTap(window, paste);
    ok(edit.popup.popupOpen, "bug: popup should be showing");

    clearSelection(edit);
  }
});

gTests.push({
  desc: "bug 895284 - tap selection",
  run: function() {
    gWindow = window;

    yield showNavBar();
    let edit = document.getElementById("urlbar-edit");
    edit.value = "wikipedia.org";
    edit.select();

    let editCoords = logicalCoordsForElement(edit);
    SelectionHelperUI.attachEditSession(ChromeSelectionHandler, editCoords.x, editCoords.y);

    ok(SelectionHelperUI.isSelectionUIVisible, "selection enabled");

    let selection = edit.QueryInterface(Components.interfaces.nsIDOMXULTextBoxElement)
                        .editor.selection;
    let rects = selection.getRangeAt(0).getClientRects();
    let midX = Math.ceil(((rects[0].right - rects[0].left) * .5) + rects[0].left);
    let midY = Math.ceil(((rects[0].bottom - rects[0].top) * .5) + rects[0].top);

    sendTap(window, midX, midY);

    ok(SelectionHelperUI.isCaretUIVisible, "caret browsing enabled");

    clearSelection(edit);
  }
});

gTests.push({
  desc: "bug 894713 - blur shuts down selection handling",
  run: function() {
    gWindow = window;
    yield showNavBar();
    let edit = document.getElementById("urlbar-edit");
    edit.value = "wikipedia.org";
    edit.select();
    let editCoords = logicalCoordsForElement(edit);
    SelectionHelperUI.attachEditSession(ChromeSelectionHandler, editCoords.x, editCoords.y);
    edit.blur();
    ok(!SelectionHelperUI.isSelectionUIVisible, "selection no longer enabled");
    clearSelection(edit);
  }
});

function test() {
  if (!isLandscapeMode()) {
    todo(false, "browser_selection_tests need landscape mode to run.");
    return;
  }
  runTests();
}
