/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/text.js */
/* import-globals-from ../../mochitest/attributes.js */
loadScripts(
  { name: "text.js", dir: MOCHITESTS_DIR },
  { name: "attributes.js", dir: MOCHITESTS_DIR }
);

const boldAttrs = { "font-weight": "700" };

/*
 * Given a text accessible and a list of ranges
 * check if those ranges match the misspelled ranges in the accessible.
 */
function misspelledRangesMatch(acc, ranges) {
  let offset = 0;
  let expectedRanges = [...ranges];
  let charCount = acc.characterCount;
  while (offset < charCount) {
    let start = {};
    let end = {};
    let attributes = acc.getTextAttributes(false, offset, start, end);
    offset = end.value;
    try {
      if (attributes.getStringProperty("invalid") == "spelling") {
        let expected = expectedRanges.shift();
        if (
          !expected ||
          expected[0] != start.value ||
          expected[1] != end.value
        ) {
          return false;
        }
      }
    } catch (err) {}
  }

  return !expectedRanges.length;
}

/*
 * Returns a promise that resolves after a text attribute changed event
 * brings us to a state where the misspelled ranges match.
 */
async function waitForMisspelledRanges(acc, ranges) {
  await waitForEvent(EVENT_TEXT_ATTRIBUTE_CHANGED);
  await untilCacheOk(
    () => misspelledRangesMatch(acc, ranges),
    `Misspelled ranges match: ${JSON.stringify(ranges)}`
  );
}

/**
 * Test spelling errors.
 */
addAccessibleTask(
  `
<textarea id="textarea" spellcheck="true">test tset tset test</textarea>
<div contenteditable id="editable" spellcheck="true">plain<span> ts</span>et <b>bold</b></div>
  `,
  async function (browser, docAcc) {
    const textarea = findAccessibleChildByID(docAcc, "textarea", [
      nsIAccessibleText,
    ]);
    info("Focusing textarea");
    let spellingChanged = waitForMisspelledRanges(textarea, [
      [5, 9],
      [10, 14],
    ]);
    textarea.takeFocus();
    await spellingChanged;

    // Test removal of a spelling error.
    info('textarea: Changing first "tset" to "test"');
    // setTextRange fires multiple EVENT_TEXT_ATTRIBUTE_CHANGED, so replace by
    // selecting and typing instead.
    spellingChanged = waitForMisspelledRanges(textarea, [[10, 14]]);
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("textarea").setSelectionRange(5, 9);
    });
    EventUtils.sendString("test");
    // Move the cursor to trigger spell check.
    EventUtils.synthesizeKey("KEY_ArrowRight");
    await spellingChanged;

    // Test addition of a spelling error.
    info('textarea: Changing it back to "tset"');
    spellingChanged = waitForMisspelledRanges(textarea, [
      [5, 9],
      [10, 14],
    ]);
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("textarea").setSelectionRange(5, 9);
    });
    EventUtils.sendString("tset");
    EventUtils.synthesizeKey("KEY_ArrowRight");
    await spellingChanged;

    // Ensure that changing the text without changing any spelling errors
    // correctly updates offsets.
    info('textarea: Changing first "test" to "the"');
    // Spelling errors don't change, so we won't get
    // EVENT_TEXT_ATTRIBUTE_CHANGED. We change the text, wait for the insertion
    // and then select a character so we know when the change is done.
    let inserted = waitForEvent(EVENT_TEXT_INSERTED, textarea);
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("textarea").setSelectionRange(0, 4);
    });
    EventUtils.sendString("the");
    await inserted;
    let selected = waitForEvent(EVENT_TEXT_SELECTION_CHANGED, textarea);
    EventUtils.synthesizeKey("KEY_ArrowRight", { shiftKey: true });
    await selected;
    const expectedRanges = [
      [4, 8],
      [9, 13],
    ];
    await untilCacheOk(
      () => misspelledRangesMatch(textarea, expectedRanges),
      `Misspelled ranges match: ${JSON.stringify(expectedRanges)}`
    );

    const editable = findAccessibleChildByID(docAcc, "editable", [
      nsIAccessibleText,
    ]);
    info("Focusing editable");
    spellingChanged = waitForMisspelledRanges(editable, [[6, 10]]);
    editable.takeFocus();
    await spellingChanged;
    // Test normal text and spelling errors crossing text nodes.
    testTextAttrs(editable, 0, {}, {}, 0, 6, true); // "plain "
    // Ensure we detect the spelling error even though there is a style change
    // after it.
    testTextAttrs(editable, 6, { invalid: "spelling" }, {}, 6, 10, true); // "tset"
    testTextAttrs(editable, 10, {}, {}, 10, 11, true); // " "
    // Ensure a style change is still detected in the presence of a spelling
    // error.
    testTextAttrs(editable, 11, boldAttrs, {}, 11, 15, true); // "bold"
  },
  {
    chrome: true,
    topLevel: true,
    iframe: true,
    remoteIframe: true,
  }
);
