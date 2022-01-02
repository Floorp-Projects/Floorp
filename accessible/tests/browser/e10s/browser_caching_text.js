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

const isCacheEnabled = Services.prefs.getBoolPref(
  "accessibility.cache.enabled",
  false
);
// Some RemoteAccessible methods aren't supported on Windows when the cache is
// disabled.
const isWinNoCache = !isCacheEnabled && AppConstants.platform == "win";

/**
 * Test line and word offsets for various cases for both local and remote
 * Accessibles. There is more extensive coverage in ../../mochitest/text. These
 * tests don't need to duplicate all of that, since much of the underlying code
 * is unified. They should ensure that the cache works as expected and that
 * there is consistency between local and remote.
 */
addAccessibleTask(
  `
<p id="br">ab cd<br>ef gh</p>
<pre id="pre">ab cd
ef gh</pre>
<p id="linksStartEnd"><a href="https://example.com/">a</a>b<a href="https://example.com/">c</a></p>
<p id="linksBreaking">a<a href="https://example.com/">b<br>c</a>d</p>
  `,
  async function(browser, docAcc) {
    for (const id of ["br", "pre"]) {
      const acc = findAccessibleChildByID(docAcc, id);
      if (isWinNoCache) {
        todo(
          false,
          "Cache disabled, so RemoteAccessible doesn't support CharacterCount on Windows"
        );
      } else {
        testCharacterCount([acc], 11);
      }
      testTextAtOffset(acc, BOUNDARY_LINE_START, [
        [0, 5, "ab cd\n", 0, 6],
        [6, 11, "ef gh", 6, 11],
      ]);
      testTextAtOffset(acc, BOUNDARY_WORD_START, [
        [0, 2, "ab ", 0, 3],
        [3, 5, "cd\n", 3, 6],
        [6, 8, "ef ", 6, 9],
        [9, 11, "gh", 9, 11],
      ]);
    }
    const linksStartEnd = findAccessibleChildByID(docAcc, "linksStartEnd");
    testTextAtOffset(linksStartEnd, BOUNDARY_LINE_START, [
      [0, 3, `${kEmbedChar}b${kEmbedChar}`, 0, 3],
    ]);
    testTextAtOffset(linksStartEnd, BOUNDARY_WORD_START, [
      [0, 3, `${kEmbedChar}b${kEmbedChar}`, 0, 3],
    ]);
    const linksBreaking = findAccessibleChildByID(docAcc, "linksBreaking");
    testTextAtOffset(linksBreaking, BOUNDARY_LINE_START, [
      [0, 0, `a${kEmbedChar}`, 0, 2],
      [1, 1, `a${kEmbedChar}d`, 0, 3],
      [2, 3, `${kEmbedChar}d`, 1, 3],
    ]);
    if (isCacheEnabled) {
      testTextAtOffset(linksBreaking, BOUNDARY_WORD_START, [
        [0, 0, `a${kEmbedChar}`, 0, 2],
        [1, 1, `a${kEmbedChar}d`, 0, 3],
        [2, 3, `${kEmbedChar}d`, 1, 3],
      ]);
    } else {
      todo(
        false,
        "TextLeafPoint disabled, so word boundaries are incorrect for linksBreaking"
      );
    }
  },
  { chrome: true, topLevel: true, iframe: true, remoteIframe: true }
);

/**
 * Test HyperText embedded object methods.
 */
addAccessibleTask(
  `<div id="container">a<a id="link" href="https://example.com/">b</a>c</div>`,
  async function(browser, docAcc) {
    const container = findAccessibleChildByID(docAcc, "container", [
      nsIAccessibleHyperText,
    ]);
    let link = container.getLinkAt(0);
    queryInterfaces(link, [nsIAccessible]);
    is(getAccessibleDOMNodeID(link), "link", "LinkAt 0 is the link");
    is(container.getLinkIndex(link), 0, "getLinkIndex for link is 0");
    is(link.startIndex, 1, "link's startIndex is 1");
    is(container.getLinkIndexAtOffset(1), 0, "getLinkIndexAtOffset(1) is 0");
    is(container.getLinkIndexAtOffset(0), -1, "getLinkIndexAtOffset(0) is -1");
  },
  {
    chrome: true,
    topLevel: !isWinNoCache,
    iframe: !isWinNoCache,
    remoteIframe: !isWinNoCache,
  }
);

/**
 * Test text attribute methods.
 */
addAccessibleTask(
  `
<p id="plain">ab</p>
<p id="bold" style="font-weight: bold;">ab</p>
<p id="partialBold">ab<b>cd</b>ef</p>
<p id="consecutiveBold">ab<b>cd</b><b>ef</b>gh</p>
<p id="embeddedObjs">ab<a href="https://example.com/">cd</a><a href="https://example.com/">ef</a><a href="https://example.com/">gh</a>ij</p>
<p id="empty"></p>
<p id="fontFamilies" style="font-family: sans-serif;">ab<span style="font-family: monospace;">cd</span><span style="font-family: monospace;">ef</span>gh</p>
  `,
  async function(browser, docAcc) {
    let defAttrs = {
      "text-position": "baseline",
      "font-style": "normal",
      "font-weight": "400",
    };
    const boldAttrs = { "font-weight": "700" };

    const plain = findAccessibleChildByID(docAcc, "plain");
    testDefaultTextAttrs(plain, defAttrs, true);
    for (let offset = 0; offset <= 2; ++offset) {
      testTextAttrs(plain, offset, {}, defAttrs, 0, 2, true);
    }

    const bold = findAccessibleChildByID(docAcc, "bold");
    defAttrs["font-weight"] = "700";
    testDefaultTextAttrs(bold, defAttrs, true);
    testTextAttrs(bold, 0, {}, defAttrs, 0, 2, true);

    const partialBold = findAccessibleChildByID(docAcc, "partialBold");
    defAttrs["font-weight"] = "400";
    testDefaultTextAttrs(partialBold, defAttrs, true);
    testTextAttrs(partialBold, 0, {}, defAttrs, 0, 2, true);
    testTextAttrs(partialBold, 2, boldAttrs, defAttrs, 2, 4, true);
    testTextAttrs(partialBold, 4, {}, defAttrs, 4, 6, true);

    const consecutiveBold = findAccessibleChildByID(docAcc, "consecutiveBold");
    testDefaultTextAttrs(consecutiveBold, defAttrs, true);
    testTextAttrs(consecutiveBold, 0, {}, defAttrs, 0, 2, true);
    testTextAttrs(consecutiveBold, 2, boldAttrs, defAttrs, 2, 6, true);
    testTextAttrs(consecutiveBold, 6, {}, defAttrs, 6, 8, true);

    const embeddedObjs = findAccessibleChildByID(docAcc, "embeddedObjs");
    testDefaultTextAttrs(embeddedObjs, defAttrs, true);
    testTextAttrs(embeddedObjs, 0, {}, defAttrs, 0, 2, true);
    for (let offset = 2; offset <= 4; ++offset) {
      // attrs and defAttrs should be completely empty, so we pass
      // false for aSkipUnexpectedAttrs.
      testTextAttrs(embeddedObjs, offset, {}, {}, 2, 5, false);
    }
    testTextAttrs(embeddedObjs, 5, {}, defAttrs, 5, 7, true);

    const empty = findAccessibleChildByID(docAcc, "empty");
    testDefaultTextAttrs(empty, defAttrs, true);
    testTextAttrs(empty, 0, {}, defAttrs, 0, 0, true);

    const fontFamilies = findAccessibleChildByID(docAcc, "fontFamilies", [
      nsIAccessibleHyperText,
    ]);
    testDefaultTextAttrs(fontFamilies, defAttrs, true);
    testTextAttrs(fontFamilies, 0, {}, defAttrs, 0, 2, true);
    testTextAttrs(fontFamilies, 2, {}, defAttrs, 2, 6, true);
    testTextAttrs(fontFamilies, 6, {}, defAttrs, 6, 8, true);
  },
  {
    chrome: true,
    topLevel: isCacheEnabled,
    iframe: isCacheEnabled,
    remoteIframe: isCacheEnabled,
  }
);

/**
 * Test caret retrieval.
 */
addAccessibleTask(
  `
<textarea id="textarea" style="scrollbar-width: none;" cols="6">ab cd e</textarea>
<textarea id="empty"></textarea>
  `,
  async function(browser, docAcc) {
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
  },
  { chrome: true, topLevel: true, iframe: true, remoteIframe: true }
);
