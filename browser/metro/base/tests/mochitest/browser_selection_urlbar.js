/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
    yield waitForCondition(function () {
      return !SelectionHelperUI.isSelectionUIVisible;
    });
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

    // wait for popup animation to complete, it interferes with edit selection testing
    let autocompletePopup = document.getElementById("urlbar-autocomplete-scroll");
    yield waitForEvent(autocompletePopup, "transitionend");

    SelectionHelperUI.attachEditSession(ChromeSelectionHandler, editCoords.x,
        editCoords.y, edit);
    ok(SelectionHelperUI.isSelectionUIVisible, "selection enabled");

    let selection = edit.QueryInterface(Components.interfaces.nsIDOMXULTextBoxElement)
                        .editor.selection;
    let rects = selection.getRangeAt(0).getClientRects();
    let midX = Math.ceil(((rects[0].right - rects[0].left) * .5) + rects[0].left);
    let midY = Math.ceil(((rects[0].bottom - rects[0].top) * .5) + rects[0].top);

    sendTap(window, midX, midY);

    ok(SelectionHelperUI.isCaretUIVisible, "caret browsing enabled");

    clearSelection(edit);
    yield waitForCondition(function () {
      return !SelectionHelperUI.isSelectionUIVisible;
    });
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
    SelectionHelperUI.attachEditSession(ChromeSelectionHandler, editCoords.x,
        editCoords.y, edit);
    edit.blur();
    ok(!SelectionHelperUI.isSelectionUIVisible, "selection no longer enabled");
    clearSelection(edit);
    yield waitForCondition(function () {
      return !SelectionHelperUI.isSelectionUIVisible;
    });
  }
});

function getClipboardCondition(aExpected) {
  return () => aExpected == SpecialPowers.getClipboardData("text/unicode");
}

gTests.push({
  desc: "bug 894715 - URLs selected by touch are copied with trimming",
  run: function () {
    gWindow = window;
    yield showNavBar();

    let edit = document.getElementById("urlbar-edit");
    edit.value = "http://www.wikipedia.org/";

    sendElementTap(window, edit);
    edit.select();

    let panel = ContextMenuUI._menuPopup._panel;
    let promise = waitForEvent(panel, "popupshown")
    sendContextMenuClickToElement(window, edit);
    ok((yield promise), "show context menu");

    let copy = document.getElementById("context-copy");
    ok(!copy.hidden, "copy menu item is visible")

    let condition = getClipboardCondition("http://www.wikipedia.org/");
    let promise = waitForCondition(condition);
    sendElementTap(window, copy);
    ok((yield promise), "copy text onto clipboard")

    clearSelection(edit);
    edit.blur();
  }
})

gTests.push({
  desc: "bug 965832 - selection monocles move with the nav bar",
  run: function() {
    yield showNavBar();

    let originalUtils = Services.metro;
    Services.metro = {
      keyboardHeight: 0,
      keyboardVisible: false
    };
    registerCleanupFunction(function() {
      Services.metro = originalUtils;
    });

    let edit = document.getElementById("urlbar-edit");
    edit.value = "http://www.wikipedia.org/";

    sendElementTap(window, edit);
    
    let promise = waitForEvent(window, "MozDeckOffsetChanged");
    Services.metro.keyboardHeight = 300;
    Services.metro.keyboardVisible = true;
    Services.obs.notifyObservers(null, "metro_softkeyboard_shown", null);
    yield promise;

    yield waitForCondition(function () {
      return SelectionHelperUI.isSelectionUIVisible;
    });

    promise = waitForEvent(window, "MozDeckOffsetChanged");
    Services.metro.keyboardHeight = 0;
    Services.metro.keyboardVisible = false;
    Services.obs.notifyObservers(null, "metro_softkeyboard_hidden", null);
    yield promise;

    yield waitForCondition(function () {
      return SelectionHelperUI.isSelectionUIVisible;
    });

    clearSelection(edit);
    edit.blur();

    yield waitForCondition(function () {
      return !SelectionHelperUI.isSelectionUIVisible;
    });
  }
});

gTests.push({
  desc: "Bug 957646 - Selection monocles sometimes don't display when tapping" +
        " text in the nav bar.",
  run: function() {
    yield showNavBar();

    let edit = document.getElementById("urlbar-edit");
    edit.value = "about:mozilla";

    let editRectangle = edit.getBoundingClientRect();

    // Tap outside the input but close enough for fluffing to take effect.
    sendTap(window, editRectangle.left + 50, editRectangle.top - 2);

    yield waitForCondition(function () {
      return SelectionHelperUI.isSelectionUIVisible;
    });
  }
});

gTests.push({
  desc: "Bug 972574 - Monocles not matching selection after double tap" +
        " in URL text field.",
  run: function() {
    yield showNavBar();

    let MARGIN_OF_ERROR = 15;
    let EST_URLTEXT_WIDTH = 125;

    let edit = document.getElementById("urlbar-edit");
    edit.value = "http://www.wikipedia.org/";

    // Determine a tap point centered on URL.
    let editRectangle = edit.getBoundingClientRect();
    let midX = editRectangle.left + Math.ceil(EST_URLTEXT_WIDTH / 2);
    let midY = editRectangle.top + Math.ceil(editRectangle.height / 2);

    // Tap inside the input for fluffing to take effect.
    sendTap(window, midX, midY);

    // Double-tap inside the input to selectALL.
    sendDoubleTap(window, midX, midY);

    // Check for start/end monocles positioned within accepted margins.
    checkMonoclePositionRange("start",
      Math.ceil(editRectangle.left - MARGIN_OF_ERROR),
      Math.ceil(editRectangle.left + MARGIN_OF_ERROR),
      Math.ceil(editRectangle.top + editRectangle.height - MARGIN_OF_ERROR),
      Math.ceil(editRectangle.top + editRectangle.height + MARGIN_OF_ERROR));
    checkMonoclePositionRange("end",
      Math.ceil(editRectangle.left + EST_URLTEXT_WIDTH - MARGIN_OF_ERROR),
      Math.ceil(editRectangle.left + EST_URLTEXT_WIDTH + MARGIN_OF_ERROR),
      Math.ceil(editRectangle.top + editRectangle.height - MARGIN_OF_ERROR),
      Math.ceil(editRectangle.top + editRectangle.height + MARGIN_OF_ERROR));
  }
});

gTests.push({
  desc: "Bug 972428 - grippers not appearing under the URL field when adding " +
        "text.",
  run: function() {
    let inputField = document.getElementById("urlbar-edit").inputField;
    let inputFieldRectangle = inputField.getBoundingClientRect();

    let chromeHandlerSpy = spyOnMethod(ChromeSelectionHandler, "msgHandler");

    // Reset URL to empty string
    inputField.value = "";
    inputField.blur();

    // Activate URL input
    sendTap(window, inputFieldRectangle.left + 50, inputFieldRectangle.top + 5);

    // Wait until ChromeSelectionHandler tries to attach selection
    yield waitForCondition(() => chromeHandlerSpy.argsForCall.some(
        (args) => args[0] == "Browser:SelectionAttach"));

    ok(!SelectHelperUI.isSelectionUIVisible && !SelectHelperUI.isCaretUIVisible,
        "Neither CaretUI nor SelectionUI is visible on empty input.");

    inputField.value = "Test text";

    sendTap(window, inputFieldRectangle.left + 10, inputFieldRectangle.top + 5);

    yield waitForCondition(() => SelectionHelperUI.isCaretUIVisible);
    chromeHandlerSpy.restore();
    inputField.blur();
  }
});

gTests.push({
  desc: "Bug 858206 - Drag selection monocles should not push other monocles " +
        "out of the way.",
  run: function test() {
    yield showNavBar();

    let edit = document.getElementById("urlbar-edit");
    edit.value = "about:mozilla";

    let editRectangle = edit.getBoundingClientRect();

    sendTap(window, editRectangle.left, editRectangle.top);

    yield waitForCondition(() => SelectionHelperUI.isSelectionUIVisible);

    let selection = edit.QueryInterface(
        Components.interfaces.nsIDOMXULTextBoxElement).editor.selection;
    let selectionRectangle = selection.getRangeAt(0).getClientRects()[0];

    // Place caret to the input start
    sendTap(window, selectionRectangle.left + 2, selectionRectangle.top + 2);
    yield waitForCondition(() => !SelectionHelperUI.isSelectionUIVisible &&
        SelectionHelperUI.isCaretUIVisible);

    let startXPos = SelectionHelperUI.caretMark.xPos;
    let startYPos = SelectionHelperUI.caretMark.yPos + 10;
    let touchDrag = new TouchDragAndHold();
    yield touchDrag.start(gWindow, startXPos, startYPos, startXPos + 200,
        startYPos);
    yield waitForCondition(() => getTrimmedSelection(edit).toString() ==
        "about:mozilla", kCommonWaitMs, kCommonPollMs);

    touchDrag.end();
    yield waitForCondition(() => !SelectionHelperUI.hasActiveDrag,
        kCommonWaitMs, kCommonPollMs);

    // Place caret to the input end
    sendTap(window, selectionRectangle.right - 2, selectionRectangle.top + 2);
    yield waitForCondition(() => !SelectionHelperUI.isSelectionUIVisible &&
        SelectionHelperUI.isCaretUIVisible);

    startXPos = SelectionHelperUI.caretMark.xPos;
    startYPos = SelectionHelperUI.caretMark.yPos + 10;
    yield touchDrag.start(gWindow, startXPos, startYPos, startXPos - 200,
        startYPos);
    yield waitForCondition(() => getTrimmedSelection(edit).toString() ==
        "about:mozilla", kCommonWaitMs, kCommonPollMs);

    touchDrag.end();
    yield waitForCondition(() => !SelectionHelperUI.hasActiveDrag,
        kCommonWaitMs, kCommonPollMs);

    // Place caret in the middle
    let midX = Math.ceil(((selectionRectangle.right - selectionRectangle.left) *
        .5) + selectionRectangle.left);
    let midY = Math.ceil(((selectionRectangle.bottom - selectionRectangle.top) *
        .5) + selectionRectangle.top);

    sendTap(window, midX, midY);
    yield waitForCondition(() => !SelectionHelperUI.isSelectionUIVisible &&
        SelectionHelperUI.isCaretUIVisible);

    startXPos = SelectionHelperUI.caretMark.xPos;
    startYPos = SelectionHelperUI.caretMark.yPos + 10;
    yield touchDrag.start(gWindow, startXPos, startYPos, startXPos - 200,
        startYPos);
    yield waitForCondition(() => getTrimmedSelection(edit).toString() ==
        "about:", kCommonWaitMs, kCommonPollMs);

    touchDrag.end();
    yield waitForCondition(() => !SelectionHelperUI.hasActiveDrag,
        kCommonWaitMs, kCommonPollMs);

    // Now try to swap monocles
    startXPos = SelectionHelperUI.startMark.xPos;
    startYPos = SelectionHelperUI.startMark.yPos + 10;
    yield touchDrag.start(gWindow, startXPos, startYPos, startXPos + 200,
        startYPos);
    yield waitForCondition(() => getTrimmedSelection(edit).toString() ==
        "mozilla", kCommonWaitMs, kCommonPollMs);
      touchDrag.end();
    yield waitForCondition(() => !SelectionHelperUI.hasActiveDrag &&
        SelectionHelperUI.isSelectionUIVisible, kCommonWaitMs, kCommonPollMs);
  }
});

function test() {
  if (!isLandscapeMode()) {
    todo(false, "browser_selection_tests need landscape mode to run.");
    return;
  }
  runTests();
}
