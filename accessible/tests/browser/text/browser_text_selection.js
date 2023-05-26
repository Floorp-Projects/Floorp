/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/text.js */

function waitForSelectionChange(selectionAcc, caretAcc) {
  if (!caretAcc) {
    caretAcc = selectionAcc;
  }
  return waitForEvents(
    [
      [EVENT_TEXT_SELECTION_CHANGED, selectionAcc],
      // We must swallow the caret events as well to avoid confusion with later,
      // unrelated caret events.
      [EVENT_TEXT_CARET_MOVED, caretAcc],
    ],
    true
  );
}

function changeDomSelection(
  browser,
  anchorId,
  anchorOffset,
  focusId,
  focusOffset
) {
  return invokeContentTask(
    browser,
    [anchorId, anchorOffset, focusId, focusOffset],
    (
      contentAnchorId,
      contentAnchorOffset,
      contentFocusId,
      contentFocusOffset
    ) => {
      // We want the text node, so we use firstChild.
      content.window
        .getSelection()
        .setBaseAndExtent(
          content.document.getElementById(contentAnchorId).firstChild,
          contentAnchorOffset,
          content.document.getElementById(contentFocusId).firstChild,
          contentFocusOffset
        );
    }
  );
}

function testSelectionRange(
  browser,
  root,
  startContainer,
  startOffset,
  endContainer,
  endOffset
) {
  let selRange = root.selectionRanges.queryElementAt(0, nsIAccessibleTextRange);
  testTextRange(
    selRange,
    getAccessibleDOMNodeID(root),
    startContainer,
    startOffset,
    endContainer,
    endOffset
  );
}

/**
 * Test text selection via keyboard.
 */
addAccessibleTask(
  `
<textarea id="textarea">ab</textarea>
<div id="editable" contenteditable>
  <p id="p1">a</p>
  <p id="p2">bc</p>
  <p id="pWithLink">d<a id="link" href="https://example.com/">e</a><span id="textAfterLink">f</span></p>
</div>
  `,
  async function (browser, docAcc) {
    queryInterfaces(docAcc, [nsIAccessibleText]);

    const textarea = findAccessibleChildByID(docAcc, "textarea", [
      nsIAccessibleText,
    ]);
    info("Focusing textarea");
    let caretMoved = waitForEvent(EVENT_TEXT_CARET_MOVED, textarea);
    textarea.takeFocus();
    await caretMoved;
    testSelectionRange(browser, textarea, textarea, 0, textarea, 0);
    is(textarea.selectionCount, 0, "textarea selectionCount is 0");
    is(docAcc.selectionCount, 0, "document selectionCount is 0");

    info("Selecting a in textarea");
    let selChanged = waitForSelectionChange(textarea);
    EventUtils.synthesizeKey("KEY_ArrowRight", { shiftKey: true });
    await selChanged;
    testSelectionRange(browser, textarea, textarea, 0, textarea, 1);
    testTextGetSelection(textarea, 0, 1, 0);

    info("Selecting b in textarea");
    selChanged = waitForSelectionChange(textarea);
    EventUtils.synthesizeKey("KEY_ArrowRight", { shiftKey: true });
    await selChanged;
    testSelectionRange(browser, textarea, textarea, 0, textarea, 2);
    testTextGetSelection(textarea, 0, 2, 0);

    info("Unselecting b in textarea");
    selChanged = waitForSelectionChange(textarea);
    EventUtils.synthesizeKey("KEY_ArrowLeft", { shiftKey: true });
    await selChanged;
    testSelectionRange(browser, textarea, textarea, 0, textarea, 1);
    testTextGetSelection(textarea, 0, 1, 0);

    info("Unselecting a in textarea");
    // We don't fire selection changed when the selection collapses.
    caretMoved = waitForEvent(EVENT_TEXT_CARET_MOVED, textarea);
    EventUtils.synthesizeKey("KEY_ArrowLeft", { shiftKey: true });
    await caretMoved;
    testSelectionRange(browser, textarea, textarea, 0, textarea, 0);
    is(textarea.selectionCount, 0, "textarea selectionCount is 0");

    const editable = findAccessibleChildByID(docAcc, "editable", [
      nsIAccessibleText,
    ]);
    const p1 = findAccessibleChildByID(docAcc, "p1", [nsIAccessibleText]);
    info("Focusing editable, caret to start");
    caretMoved = waitForEvent(EVENT_TEXT_CARET_MOVED, p1);
    await changeDomSelection(browser, "p1", 0, "p1", 0);
    await caretMoved;
    testSelectionRange(browser, editable, p1, 0, p1, 0);
    is(editable.selectionCount, 0, "editable selectionCount is 0");
    is(p1.selectionCount, 0, "p1 selectionCount is 0");
    is(docAcc.selectionCount, 0, "document selectionCount is 0");

    info("Selecting a in editable");
    selChanged = waitForSelectionChange(p1);
    await changeDomSelection(browser, "p1", 0, "p1", 1);
    await selChanged;
    testSelectionRange(browser, editable, p1, 0, p1, 1);
    testTextGetSelection(editable, 0, 1, 0);
    testTextGetSelection(p1, 0, 1, 0);
    const p2 = findAccessibleChildByID(docAcc, "p2", [nsIAccessibleText]);
    if (browser.isRemoteBrowser) {
      is(p2.selectionCount, 0, "p2 selectionCount is 0");
    } else {
      todo(
        false,
        "Siblings report wrong selection in non-cache implementation"
      );
    }

    // Selecting across two Accessibles with only a partial selection in the
    // second.
    info("Selecting ab in editable");
    selChanged = waitForSelectionChange(editable, p2);
    await changeDomSelection(browser, "p1", 0, "p2", 1);
    await selChanged;
    testSelectionRange(browser, editable, p1, 0, p2, 1);
    testTextGetSelection(editable, 0, 2, 0);
    testTextGetSelection(p1, 0, 1, 0);
    testTextGetSelection(p2, 0, 1, 0);

    const pWithLink = findAccessibleChildByID(docAcc, "pWithLink", [
      nsIAccessibleText,
    ]);
    const link = findAccessibleChildByID(docAcc, "link", [nsIAccessibleText]);
    // Selecting both text and a link.
    info("Selecting de in editable");
    selChanged = waitForSelectionChange(pWithLink, link);
    await changeDomSelection(browser, "pWithLink", 0, "link", 1);
    await selChanged;
    testSelectionRange(browser, editable, pWithLink, 0, link, 1);
    testTextGetSelection(editable, 2, 3, 0);
    testTextGetSelection(pWithLink, 0, 2, 0);
    testTextGetSelection(link, 0, 1, 0);

    // Selecting a link and text on either side.
    info("Selecting def in editable");
    selChanged = waitForSelectionChange(pWithLink, pWithLink);
    await changeDomSelection(browser, "pWithLink", 0, "textAfterLink", 1);
    await selChanged;
    testSelectionRange(browser, editable, pWithLink, 0, pWithLink, 3);
    testTextGetSelection(editable, 2, 3, 0);
    testTextGetSelection(pWithLink, 0, 3, 0);
    testTextGetSelection(link, 0, 1, 0);

    // Noncontiguous selection.
    info("Selecting a in editable");
    selChanged = waitForSelectionChange(p1);
    await changeDomSelection(browser, "p1", 0, "p1", 1);
    await selChanged;
    info("Adding c to selection in editable");
    selChanged = waitForSelectionChange(p2);
    await invokeContentTask(browser, [], () => {
      const r = content.document.createRange();
      const p2text = content.document.getElementById("p2").firstChild;
      r.setStart(p2text, 0);
      r.setEnd(p2text, 1);
      content.window.getSelection().addRange(r);
    });
    await selChanged;
    let selRanges = editable.selectionRanges;
    is(selRanges.length, 2, "2 selection ranges");
    testTextRange(
      selRanges.queryElementAt(0, nsIAccessibleTextRange),
      "range 0",
      p1,
      0,
      p1,
      1
    );
    testTextRange(
      selRanges.queryElementAt(1, nsIAccessibleTextRange),
      "range 1",
      p2,
      0,
      p2,
      1
    );
    is(editable.selectionCount, 2, "editable selectionCount is 2");
    testTextGetSelection(editable, 0, 1, 0);
    testTextGetSelection(editable, 1, 2, 1);
    if (browser.isRemoteBrowser) {
      is(p1.selectionCount, 1, "p1 selectionCount is 1");
      testTextGetSelection(p1, 0, 1, 0);
      is(p2.selectionCount, 1, "p2 selectionCount is 1");
      testTextGetSelection(p2, 0, 1, 0);
    } else {
      todo(
        false,
        "Siblings report wrong selection in non-cache implementation"
      );
    }
  },
  {
    chrome: true,
    topLevel: true,
    iframe: true,
    remoteIframe: true,
  }
);

/**
 * Tabbing to an input selects all its text. Test that the cached selection
 *reflects this. This has to be done separately from the other selection tests
 * because prior contentEditable selection changes the events that get fired.
 */
addAccessibleTask(
  `
<button id="before">Before</button>
<input id="input" value="test">
  `,
  async function (browser, docAcc) {
    // The tab order is different when there's an iframe, so focus a control
    // before the input to make tab consistent.
    info("Focusing before");
    const before = findAccessibleChildByID(docAcc, "before");
    // Focusing a button fires a selection event. We must swallow this to
    // avoid confusing the later test.
    let events = waitForOrderedEvents([
      [EVENT_FOCUS, before],
      [EVENT_TEXT_SELECTION_CHANGED, docAcc],
    ]);
    before.takeFocus();
    await events;

    const input = findAccessibleChildByID(docAcc, "input", [nsIAccessibleText]);
    info("Tabbing to input");
    events = waitForEvents(
      {
        expected: [
          [EVENT_FOCUS, input],
          [EVENT_TEXT_SELECTION_CHANGED, input],
        ],
        unexpected: [[EVENT_TEXT_SELECTION_CHANGED, docAcc]],
      },
      "input",
      false,
      (args, task) => invokeContentTask(browser, args, task)
    );
    EventUtils.synthesizeKey("KEY_Tab");
    await events;
    testSelectionRange(browser, input, input, 0, input, 4);
    testTextGetSelection(input, 0, 4, 0);
  },
  {
    chrome: true,
    topLevel: true,
    iframe: true,
    remoteIframe: true,
  }
);

/**
 * Test text selection via API.
 */
addAccessibleTask(
  `
  <p id="paragraph">hello world</p>
  <ol>
    <li id="li">Number one</li>
  </ol>
  `,
  async function (browser, docAcc) {
    const paragraph = findAccessibleChildByID(docAcc, "paragraph", [
      nsIAccessibleText,
    ]);

    let selChanged = waitForSelectionChange(paragraph);
    paragraph.setSelectionBounds(0, 2, 4);
    await selChanged;
    testTextGetSelection(paragraph, 2, 4, 0);

    selChanged = waitForSelectionChange(paragraph);
    paragraph.addSelection(6, 10);
    await selChanged;
    testTextGetSelection(paragraph, 6, 10, 1);
    is(paragraph.selectionCount, 2, "paragraph selectionCount is 2");

    selChanged = waitForSelectionChange(paragraph);
    paragraph.removeSelection(0);
    await selChanged;
    testTextGetSelection(paragraph, 6, 10, 0);
    is(paragraph.selectionCount, 1, "paragraph selectionCount is 1");

    const li = findAccessibleChildByID(docAcc, "li", [nsIAccessibleText]);

    selChanged = waitForSelectionChange(li);
    li.setSelectionBounds(0, 1, 8);
    await selChanged;
    testTextGetSelection(li, 3, 8, 0);
  },
  {
    chrome: true,
    topLevel: true,
    iframe: true,
    remoteIframe: true,
  }
);
