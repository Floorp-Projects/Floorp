/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/text.js */

/**
 * Test caret retrieval.
 */
addAccessibleTask(
  `
<textarea id="textarea"
          spellcheck="false"
          style="scrollbar-width: none; font-family: 'Liberation Mono', monospace;"
          cols="6">ab cd e</textarea>
<textarea id="empty"></textarea>
<div id="contentEditable" contenteditable>a<span>b</span></div>
  `,
  async function (browser, docAcc) {
    const textarea = findAccessibleChildByID(docAcc, "textarea", [
      nsIAccessibleText,
    ]);
    let caretMoved = waitForEvent(EVENT_TEXT_CARET_MOVED, textarea);
    textarea.takeFocus();
    let evt = await caretMoved;
    is(textarea.caretOffset, 0, "Initial caret offset is 0");
    evt.QueryInterface(nsIAccessibleCaretMoveEvent);
    ok(!evt.isAtEndOfLine, "Caret is not at end of line");
    testTextAtOffset(
      kCaretOffset,
      BOUNDARY_CHAR,
      "a",
      0,
      1,
      textarea,
      kOk,
      kOk,
      kOk
    );
    testTextAtOffset(
      kCaretOffset,
      BOUNDARY_WORD_START,
      "ab ",
      0,
      3,
      textarea,
      kOk,
      kOk,
      kOk
    );
    testTextAtOffset(
      kCaretOffset,
      BOUNDARY_LINE_START,
      "ab cd ",
      0,
      6,
      textarea,
      kOk,
      kOk,
      kOk
    );

    caretMoved = waitForEvent(EVENT_TEXT_CARET_MOVED, textarea);
    EventUtils.synthesizeKey("KEY_ArrowRight");
    evt = await caretMoved;
    is(textarea.caretOffset, 1, "Caret offset is 1 after ArrowRight");
    evt.QueryInterface(nsIAccessibleCaretMoveEvent);
    ok(!evt.isAtEndOfLine, "Caret is not at end of line");
    testTextAtOffset(
      kCaretOffset,
      BOUNDARY_CHAR,
      "b",
      1,
      2,
      textarea,
      kOk,
      kOk,
      kOk
    );
    testTextAtOffset(
      kCaretOffset,
      BOUNDARY_WORD_START,
      "ab ",
      0,
      3,
      textarea,
      kOk,
      kOk,
      kOk
    );
    testTextAtOffset(
      kCaretOffset,
      BOUNDARY_LINE_START,
      "ab cd ",
      0,
      6,
      textarea,
      kOk,
      kOk,
      kOk
    );

    caretMoved = waitForEvent(EVENT_TEXT_CARET_MOVED, textarea);
    EventUtils.synthesizeKey("KEY_ArrowRight");
    evt = await caretMoved;
    is(textarea.caretOffset, 2, "Caret offset is 2 after ArrowRight");
    evt.QueryInterface(nsIAccessibleCaretMoveEvent);
    ok(!evt.isAtEndOfLine, "Caret is not at end of line");
    testTextAtOffset(
      kCaretOffset,
      BOUNDARY_CHAR,
      " ",
      2,
      3,
      textarea,
      kOk,
      kOk,
      kOk
    );
    testTextAtOffset(
      kCaretOffset,
      BOUNDARY_WORD_START,
      "ab ",
      0,
      3,
      textarea,
      kOk,
      kOk,
      kOk
    );
    testTextAtOffset(
      kCaretOffset,
      BOUNDARY_LINE_START,
      "ab cd ",
      0,
      6,
      textarea,
      kOk,
      kOk,
      kOk
    );

    caretMoved = waitForEvent(EVENT_TEXT_CARET_MOVED, textarea);
    EventUtils.synthesizeKey("KEY_ArrowRight");
    evt = await caretMoved;
    is(textarea.caretOffset, 3, "Caret offset is 3 after ArrowRight");
    evt.QueryInterface(nsIAccessibleCaretMoveEvent);
    ok(!evt.isAtEndOfLine, "Caret is not at end of line");
    testTextAtOffset(
      kCaretOffset,
      BOUNDARY_CHAR,
      "c",
      3,
      4,
      textarea,
      kOk,
      kOk,
      kOk
    );
    testTextAtOffset(
      kCaretOffset,
      BOUNDARY_WORD_START,
      "cd ",
      3,
      6,
      textarea,
      kOk,
      kOk,
      kOk
    );
    testTextAtOffset(
      kCaretOffset,
      BOUNDARY_LINE_START,
      "ab cd ",
      0,
      6,
      textarea,
      kOk,
      kOk,
      kOk
    );

    caretMoved = waitForEvent(EVENT_TEXT_CARET_MOVED, textarea);
    EventUtils.synthesizeKey("KEY_ArrowRight");
    evt = await caretMoved;
    is(textarea.caretOffset, 4, "Caret offset is 4 after ArrowRight");
    evt.QueryInterface(nsIAccessibleCaretMoveEvent);
    ok(!evt.isAtEndOfLine, "Caret is not at end of line");
    testTextAtOffset(
      kCaretOffset,
      BOUNDARY_CHAR,
      "d",
      4,
      5,
      textarea,
      kOk,
      kOk,
      kOk
    );
    testTextAtOffset(
      kCaretOffset,
      BOUNDARY_WORD_START,
      "cd ",
      3,
      6,
      textarea,
      kOk,
      kOk,
      kOk
    );
    testTextAtOffset(
      kCaretOffset,
      BOUNDARY_LINE_START,
      "ab cd ",
      0,
      6,
      textarea,
      kOk,
      kOk,
      kOk
    );

    caretMoved = waitForEvent(EVENT_TEXT_CARET_MOVED, textarea);
    EventUtils.synthesizeKey("KEY_ArrowRight");
    evt = await caretMoved;
    is(textarea.caretOffset, 5, "Caret offset is 5 after ArrowRight");
    evt.QueryInterface(nsIAccessibleCaretMoveEvent);
    ok(!evt.isAtEndOfLine, "Caret is not at end of line");
    testTextAtOffset(
      kCaretOffset,
      BOUNDARY_CHAR,
      " ",
      5,
      6,
      textarea,
      kOk,
      kOk,
      kOk
    );
    testTextAtOffset(
      kCaretOffset,
      BOUNDARY_WORD_START,
      "cd ",
      3,
      6,
      textarea,
      kOk,
      kOk,
      kOk
    );
    testTextAtOffset(
      kCaretOffset,
      BOUNDARY_LINE_START,
      "ab cd ",
      0,
      6,
      textarea,
      kOk,
      kOk,
      kOk
    );

    caretMoved = waitForEvent(EVENT_TEXT_CARET_MOVED, textarea);
    EventUtils.synthesizeKey("KEY_ArrowRight");
    evt = await caretMoved;
    is(textarea.caretOffset, 6, "Caret offset is 6 after ArrowRight");
    evt.QueryInterface(nsIAccessibleCaretMoveEvent);
    ok(evt.isAtEndOfLine, "Caret is at end of line");
    testTextAtOffset(
      kCaretOffset,
      BOUNDARY_CHAR,
      "",
      6,
      6,
      textarea,
      kOk,
      kOk,
      kOk
    );
    testTextAtOffset(
      kCaretOffset,
      BOUNDARY_CLUSTER,
      "",
      6,
      6,
      textarea,
      kOk,
      kOk,
      kOk
    );
    testTextAtOffset(
      kCaretOffset,
      BOUNDARY_WORD_START,
      "cd ",
      3,
      6,
      textarea,
      kOk,
      kOk,
      kOk
    );
    testTextAtOffset(
      kCaretOffset,
      BOUNDARY_LINE_START,
      "ab cd ",
      0,
      6,
      textarea,
      kOk,
      kOk,
      kOk
    );

    caretMoved = waitForEvent(EVENT_TEXT_CARET_MOVED, textarea);
    EventUtils.synthesizeKey("KEY_ArrowRight");
    evt = await caretMoved;
    is(textarea.caretOffset, 6, "Caret offset remains 6 after ArrowRight");
    evt.QueryInterface(nsIAccessibleCaretMoveEvent);
    ok(!evt.isAtEndOfLine, "Caret is not at end of line");
    // Caret is at start of second line.
    testTextAtOffset(
      kCaretOffset,
      BOUNDARY_CHAR,
      "e",
      6,
      7,
      textarea,
      kOk,
      kOk,
      kOk
    );
    testTextAtOffset(
      kCaretOffset,
      BOUNDARY_WORD_START,
      "e",
      6,
      7,
      textarea,
      kOk,
      kOk,
      kOk
    );
    testTextAtOffset(
      kCaretOffset,
      BOUNDARY_LINE_START,
      "e",
      6,
      7,
      textarea,
      kOk,
      kOk,
      kOk
    );

    caretMoved = waitForEvent(EVENT_TEXT_CARET_MOVED, textarea);
    EventUtils.synthesizeKey("KEY_ArrowRight");
    evt = await caretMoved;
    is(textarea.caretOffset, 7, "Caret offset is 7 after ArrowRight");
    evt.QueryInterface(nsIAccessibleCaretMoveEvent);
    ok(evt.isAtEndOfLine, "Caret is at end of line");
    // Caret is at end of textarea.
    testTextAtOffset(
      kCaretOffset,
      BOUNDARY_CHAR,
      "",
      7,
      7,
      textarea,
      kOk,
      kOk,
      kOk
    );
    testTextAtOffset(
      kCaretOffset,
      BOUNDARY_WORD_START,
      "e",
      6,
      7,
      textarea,
      kOk,
      kOk,
      kOk
    );
    testTextAtOffset(
      kCaretOffset,
      BOUNDARY_LINE_START,
      "e",
      6,
      7,
      textarea,
      kOk,
      kOk,
      kOk
    );

    const empty = findAccessibleChildByID(docAcc, "empty", [nsIAccessibleText]);
    caretMoved = waitForEvent(EVENT_TEXT_CARET_MOVED, empty);
    empty.takeFocus();
    evt = await caretMoved;
    is(empty.caretOffset, 0, "Caret offset in empty textarea is 0");
    evt.QueryInterface(nsIAccessibleCaretMoveEvent);
    ok(!evt.isAtEndOfLine, "Caret is not at end of line");

    const contentEditable = findAccessibleChildByID(docAcc, "contentEditable", [
      nsIAccessibleText,
    ]);
    caretMoved = waitForEvent(EVENT_TEXT_CARET_MOVED, contentEditable);
    contentEditable.takeFocus();
    evt = await caretMoved;
    is(
      contentEditable.caretOffset,
      0,
      "Initial caret offset in contentEditable is 0"
    );
    evt.QueryInterface(nsIAccessibleCaretMoveEvent);
    ok(!evt.isAtEndOfLine, "Caret is not at end of line");
    testTextAtOffset(
      kCaretOffset,
      BOUNDARY_CHAR,
      "a",
      0,
      1,
      contentEditable,
      kOk,
      kOk,
      kOk
    );
    caretMoved = waitForEvent(EVENT_TEXT_CARET_MOVED, contentEditable);
    EventUtils.synthesizeKey("KEY_ArrowRight");
    evt = await caretMoved;
    is(contentEditable.caretOffset, 1, "Caret offset is 1 after ArrowRight");
    evt.QueryInterface(nsIAccessibleCaretMoveEvent);
    ok(!evt.isAtEndOfLine, "Caret is not at end of line");
    testTextAtOffset(
      kCaretOffset,
      BOUNDARY_CHAR,
      "b",
      1,
      2,
      contentEditable,
      kOk,
      kOk,
      kOk
    );
  },
  { chrome: true, topLevel: true, iframe: true, remoteIframe: true }
);

/**
 * Test setting the caret.
 */
addAccessibleTask(
  `
<textarea id="textarea">ab\nc</textarea>
<div id="editable" contenteditable>
  <p id="p">a<a id="link" href="https://example.com/">b</a></p>
</div>
  `,
  async function (browser, docAcc) {
    const textarea = findAccessibleChildByID(docAcc, "textarea", [
      nsIAccessibleText,
    ]);
    info("textarea: Set caret offset to 0");
    let focused = waitForEvent(EVENT_FOCUS, textarea);
    let caretMoved = waitForEvent(EVENT_TEXT_CARET_MOVED, textarea);
    textarea.caretOffset = 0;
    await focused;
    await caretMoved;
    is(textarea.caretOffset, 0, "textarea caret correct");
    // Test setting caret to another line.
    info("textarea: Set caret offset to 3");
    caretMoved = waitForEvent(EVENT_TEXT_CARET_MOVED, textarea);
    textarea.caretOffset = 3;
    await caretMoved;
    is(textarea.caretOffset, 3, "textarea caret correct");
    // Test setting caret to the end.
    info("textarea: Set caret offset to 4 (end)");
    caretMoved = waitForEvent(EVENT_TEXT_CARET_MOVED, textarea);
    textarea.caretOffset = 4;
    await caretMoved;
    is(textarea.caretOffset, 4, "textarea caret correct");

    const editable = findAccessibleChildByID(docAcc, "editable", [
      nsIAccessibleText,
    ]);
    focused = waitForEvent(EVENT_FOCUS, editable);
    editable.takeFocus();
    await focused;
    const p = findAccessibleChildByID(docAcc, "p", [nsIAccessibleText]);
    info("p: Set caret offset to 0");
    caretMoved = waitForEvent(EVENT_TEXT_CARET_MOVED, p);
    p.caretOffset = 0;
    await focused;
    await caretMoved;
    is(p.caretOffset, 0, "p caret correct");
    const link = findAccessibleChildByID(docAcc, "link", [nsIAccessibleText]);
    info("link: Set caret offset to 0");
    caretMoved = waitForEvent(EVENT_TEXT_CARET_MOVED, link);
    link.caretOffset = 0;
    await caretMoved;
    is(link.caretOffset, 0, "link caret correct");
  },
  { chrome: true, topLevel: true, iframe: true, remoteIframe: true }
);
