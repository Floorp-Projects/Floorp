/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const CLUSTER_AMOUNT = Ci.nsISelectionListener.CLUSTER_AMOUNT;
const WORD_AMOUNT = Ci.nsISelectionListener.WORD_AMOUNT;
const LINE_AMOUNT = Ci.nsISelectionListener.LINE_AMOUNT;
const BEGINLINE_AMOUNT = Ci.nsISelectionListener.BEGINLINE_AMOUNT;
const ENDLINE_AMOUNT = Ci.nsISelectionListener.ENDLINE_AMOUNT;

const isMac = AppConstants.platform == "macosx";

function matchCaretMoveEvent(id, caretOffset) {
  return evt => {
    evt.QueryInterface(nsIAccessibleCaretMoveEvent);
    return (
      getAccessibleDOMNodeID(evt.accessible) == id &&
      evt.caretOffset == caretOffset
    );
  };
}

addAccessibleTask(
  `<textarea id="textarea" style="scrollbar-width: none;" cols="15">` +
    `one two three four five six seven eight` +
    `</textarea>`,
  async function (browser, accDoc) {
    const textarea = findAccessibleChildByID(accDoc, "textarea");
    let caretMoved = waitForEvent(
      EVENT_TEXT_CARET_MOVED,
      matchCaretMoveEvent("textarea", 0)
    );
    textarea.takeFocus();
    let evt = await caretMoved;
    evt.QueryInterface(nsIAccessibleCaretMoveEvent);
    ok(!evt.isAtEndOfLine, "Caret is not at end of line");

    caretMoved = waitForEvent(
      EVENT_TEXT_CARET_MOVED,
      matchCaretMoveEvent("textarea", 1)
    );
    EventUtils.synthesizeKey("KEY_ArrowRight");
    evt = await caretMoved;
    evt.QueryInterface(nsIAccessibleCaretMoveEvent);
    ok(!evt.isAtEndOfLine, "Caret is not at end of line");
    is(evt.granularity, CLUSTER_AMOUNT, "Caret moved by cluster");

    caretMoved = waitForEvent(
      EVENT_TEXT_CARET_MOVED,
      matchCaretMoveEvent("textarea", 15)
    );
    EventUtils.synthesizeKey("KEY_ArrowDown");
    evt = await caretMoved;
    evt.QueryInterface(nsIAccessibleCaretMoveEvent);
    todo(!evt.isAtEndOfLine, "Caret is not at end of line");
    is(evt.granularity, LINE_AMOUNT, "Caret moved by line");

    caretMoved = waitForEvent(
      EVENT_TEXT_CARET_MOVED,
      matchCaretMoveEvent("textarea", 14)
    );
    if (isMac) {
      EventUtils.synthesizeKey("KEY_ArrowLeft", { metaKey: true });
    } else {
      EventUtils.synthesizeKey("KEY_Home");
    }
    evt = await caretMoved;
    evt.QueryInterface(nsIAccessibleCaretMoveEvent);
    ok(!evt.isAtEndOfLine, "Caret is not at end of line");
    is(evt.granularity, BEGINLINE_AMOUNT, "Caret moved to line start");

    caretMoved = waitForEvent(
      EVENT_TEXT_CARET_MOVED,
      matchCaretMoveEvent("textarea", 28)
    );
    if (isMac) {
      EventUtils.synthesizeKey("KEY_ArrowRight", { metaKey: true });
    } else {
      EventUtils.synthesizeKey("KEY_End");
    }
    evt = await caretMoved;
    evt.QueryInterface(nsIAccessibleCaretMoveEvent);
    ok(evt.isAtEndOfLine, "Caret is at end of line");
    is(evt.granularity, ENDLINE_AMOUNT, "Caret moved to line end");

    caretMoved = waitForEvent(
      EVENT_TEXT_CARET_MOVED,
      matchCaretMoveEvent("textarea", 24)
    );
    if (isMac) {
      EventUtils.synthesizeKey("KEY_ArrowLeft", { altKey: true });
    } else {
      EventUtils.synthesizeKey("KEY_ArrowLeft", { ctrlKey: true });
    }
    evt = await caretMoved;
    evt.QueryInterface(nsIAccessibleCaretMoveEvent);
    ok(!evt.isAtEndOfLine, "Caret is not at end of line");
    is(evt.granularity, WORD_AMOUNT, "Caret moved by word");
  }
);
