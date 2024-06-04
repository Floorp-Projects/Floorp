/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/attributes.js */
/* import-globals-from ../../mochitest/text.js */
loadScripts({ name: "attributes.js", dir: MOCHITESTS_DIR });

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
<p id="p">a<br role="presentation">b</p>
<p id="leafThenWrap" style="font-family: monospace; width: 2ch; word-break: break-word;"><span>a</span>bc</p>
<p id="bidi" style="font-family: monospace; width: 3ch; word-break: break-word">אb גד eו זח טj</p>
  `,
  async function (browser, docAcc) {
    for (const id of ["br", "pre"]) {
      const acc = findAccessibleChildByID(docAcc, id);
      testCharacterCount([acc], 11);
      testTextAtOffset(acc, BOUNDARY_LINE_START, [
        [0, 5, "ab cd\n", 0, 6],
        [6, 11, "ef gh", 6, 11],
      ]);
      testTextBeforeOffset(acc, BOUNDARY_LINE_START, [
        [0, 5, "", 0, 0],
        [6, 11, "ab cd\n", 0, 6],
      ]);
      testTextAfterOffset(acc, BOUNDARY_LINE_START, [
        [0, 5, "ef gh", 6, 11],
        [6, 11, "", 11, 11],
      ]);
      testTextAtOffset(acc, BOUNDARY_LINE_END, [
        [0, 5, "ab cd", 0, 5],
        [6, 11, "\nef gh", 5, 11],
      ]);
      testTextBeforeOffset(acc, BOUNDARY_LINE_END, [
        [0, 5, "", 0, 0],
        [6, 11, "ab cd", 0, 5],
      ]);
      testTextAfterOffset(acc, BOUNDARY_LINE_END, [
        [0, 5, "\nef gh", 5, 11],
        [6, 11, "", 11, 11],
      ]);
      testTextAtOffset(acc, BOUNDARY_WORD_START, [
        [0, 2, "ab ", 0, 3],
        [3, 5, "cd\n", 3, 6],
        [6, 8, "ef ", 6, 9],
        [9, 11, "gh", 9, 11],
      ]);
      testTextBeforeOffset(acc, BOUNDARY_WORD_START, [
        [0, 2, "", 0, 0],
        [3, 5, "ab ", 0, 3],
        [6, 8, "cd\n", 3, 6],
        [9, 11, "ef ", 6, 9],
      ]);
      testTextAfterOffset(acc, BOUNDARY_WORD_START, [
        [0, 2, "cd\n", 3, 6],
        [3, 5, "ef ", 6, 9],
        [6, 8, "gh", 9, 11],
        [9, 11, "", 11, 11],
      ]);
      testTextAtOffset(acc, BOUNDARY_WORD_END, [
        [0, 1, "ab", 0, 2],
        [2, 4, " cd", 2, 5],
        [5, 7, "\nef", 5, 8],
        [8, 11, " gh", 8, 11],
      ]);
      testTextBeforeOffset(acc, BOUNDARY_WORD_END, [
        [0, 2, "", 0, 0],
        [3, 5, "ab", 0, 2],
        // See below for offset 6.
        [7, 8, " cd", 2, 5],
        [9, 11, "\nef", 5, 8],
      ]);
      testTextBeforeOffset(acc, BOUNDARY_WORD_END, [[6, 6, " cd", 2, 5]]);
      testTextAfterOffset(acc, BOUNDARY_WORD_END, [
        [0, 2, " cd", 2, 5],
        [3, 5, "\nef", 5, 8],
        [6, 8, " gh", 8, 11],
        [9, 11, "", 11, 11],
      ]);
      testTextAtOffset(acc, BOUNDARY_PARAGRAPH, [
        [0, 5, "ab cd\n", 0, 6],
        [6, 11, "ef gh", 6, 11],
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
    const p = findAccessibleChildByID(docAcc, "p");
    testTextAtOffset(p, BOUNDARY_LINE_START, [
      [0, 0, "a", 0, 1],
      [1, 2, "b", 1, 2],
    ]);
    testTextAtOffset(p, BOUNDARY_PARAGRAPH, [[0, 2, "ab", 0, 2]]);
    const leafThenWrap = findAccessibleChildByID(docAcc, "leafThenWrap");
    testTextAtOffset(leafThenWrap, BOUNDARY_LINE_START, [
      [0, 1, "ab", 0, 2],
      [2, 3, "c", 2, 3],
    ]);
    const bidi = findAccessibleChildByID(docAcc, "bidi");
    testTextAtOffset(bidi, BOUNDARY_LINE_START, [
      [0, 2, "אb ", 0, 3],
      [3, 5, "גד ", 3, 6],
      [6, 8, "eו ", 6, 9],
      [9, 11, "זח ", 9, 12],
      [12, 14, "טj", 12, 14],
    ]);
  },
  { chrome: true, topLevel: true, iframe: true, remoteIframe: true }
);

/**
 * Test line offsets after text mutation.
 */
addAccessibleTask(
  `
<p id="initBr"><br></p>
<p id="rewrap" style="font-family: monospace; width: 2ch; word-break: break-word;"><span id="rewrap1">ac</span>def</p>
  `,
  async function (browser, docAcc) {
    const initBr = findAccessibleChildByID(docAcc, "initBr");
    testTextAtOffset(initBr, BOUNDARY_LINE_START, [
      [0, 0, "\n", 0, 1],
      [1, 1, "", 1, 1],
    ]);
    info("initBr: Inserting text before br");
    let reordered = waitForEvent(EVENT_REORDER, initBr);
    await invokeContentTask(browser, [], () => {
      const initBrNode = content.document.getElementById("initBr");
      initBrNode.insertBefore(
        content.document.createTextNode("a"),
        initBrNode.firstElementChild
      );
    });
    await reordered;
    testTextAtOffset(initBr, BOUNDARY_LINE_START, [
      [0, 1, "a\n", 0, 2],
      [2, 2, "", 2, 2],
    ]);

    const rewrap = findAccessibleChildByID(docAcc, "rewrap");
    testTextAtOffset(rewrap, BOUNDARY_LINE_START, [
      [0, 1, "ac", 0, 2],
      [2, 3, "de", 2, 4],
      [4, 5, "f", 4, 5],
    ]);
    info("rewrap: Changing ac to abc");
    reordered = waitForEvent(EVENT_REORDER, rewrap);
    await invokeContentTask(browser, [], () => {
      const rewrap1 = content.document.getElementById("rewrap1");
      rewrap1.textContent = "abc";
    });
    await reordered;
    testTextAtOffset(rewrap, BOUNDARY_LINE_START, [
      [0, 1, "ab", 0, 2],
      [2, 3, "cd", 2, 4],
      [4, 6, "ef", 4, 6],
    ]);
  },
  { chrome: true, topLevel: true, iframe: true, remoteIframe: true }
);

/**
 * Test retrieval of text offsets when an invalid offset is given.
 */
addAccessibleTask(
  `<p id="p">test</p>`,
  async function (browser, docAcc) {
    const p = findAccessibleChildByID(docAcc, "p");
    testTextAtOffset(p, BOUNDARY_LINE_START, [[5, 5, "", 0, 0]]);
    testTextBeforeOffset(p, BOUNDARY_LINE_START, [[5, 5, "", 0, 0]]);
    testTextAfterOffset(p, BOUNDARY_LINE_START, [[5, 5, "", 0, 0]]);
  },
  {
    // The old HyperTextAccessible implementation doesn't crash, but it returns
    // different offsets. This doesn't matter because they're invalid either
    // way. Since the new HyperTextAccessibleBase implementation is all we will
    // have soon, just test that.
    chrome: true,
    topLevel: true,
    iframe: true,
    remoteIframe: true,
  }
);

/**
 * Test HyperText embedded object methods.
 */
addAccessibleTask(
  `<div id="container">a<a id="link" href="https://example.com/">b</a>c</div>`,
  async function (browser, docAcc) {
    const container = findAccessibleChildByID(docAcc, "container", [
      nsIAccessibleHyperText,
    ]);
    is(container.linkCount, 1, "container linkCount is 1");
    let link = container.getLinkAt(0);
    queryInterfaces(link, [nsIAccessible, nsIAccessibleHyperText]);
    is(getAccessibleDOMNodeID(link), "link", "LinkAt 0 is the link");
    is(container.getLinkIndex(link), 0, "getLinkIndex for link is 0");
    is(link.startIndex, 1, "link's startIndex is 1");
    is(link.endIndex, 2, "link's endIndex is 2");
    is(container.getLinkIndexAtOffset(1), 0, "getLinkIndexAtOffset(1) is 0");
    is(container.getLinkIndexAtOffset(0), -1, "getLinkIndexAtOffset(0) is -1");
    is(link.linkCount, 0, "link linkCount is 0");
  },
  {
    chrome: true,
    topLevel: true,
    iframe: true,
    remoteIframe: true,
  }
);

/**
 * Test HyperText embedded object methods near a list bullet.
 */
addAccessibleTask(
  `<ul><li id="li"><a id="link" href="https://example.com/">a</a></li></ul>`,
  async function (browser, docAcc) {
    const li = findAccessibleChildByID(docAcc, "li", [nsIAccessibleHyperText]);
    let link = li.getLinkAt(0);
    queryInterfaces(link, [nsIAccessible]);
    is(getAccessibleDOMNodeID(link), "link", "LinkAt 0 is the link");
    is(li.getLinkIndex(link), 0, "getLinkIndex for link is 0");
    is(link.startIndex, 2, "link's startIndex is 2");
    is(li.getLinkIndexAtOffset(2), 0, "getLinkIndexAtOffset(2) is 0");
    is(li.getLinkIndexAtOffset(0), -1, "getLinkIndexAtOffset(0) is -1");
  },
  {
    chrome: true,
    topLevel: true,
    iframe: true,
    remoteIframe: true,
  }
);

const boldAttrs = { "font-weight": "700" };

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
  async function (browser, docAcc) {
    let defAttrs = {
      "text-position": "baseline",
      "font-style": "normal",
      "font-weight": "400",
    };

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
    topLevel: true,
    iframe: true,
    remoteIframe: true,
  }
);

/**
 * Test cluster offsets.
 */
addAccessibleTask(
  `<p id="clusters">À2🤦‍♂️🤦🏼‍♂️5x͇͕̦̍͂͒7È</p>`,
  async function testCluster(browser, docAcc) {
    const clusters = findAccessibleChildByID(docAcc, "clusters");
    testCharacterCount(clusters, 26);
    testTextAtOffset(clusters, BOUNDARY_CLUSTER, [
      [0, 1, "À", 0, 2],
      [2, 2, "2", 2, 3],
      [3, 7, "🤦‍♂️", 3, 8],
      [8, 14, "🤦🏼‍♂️", 8, 15],
      [15, 15, "5", 15, 16],
      [16, 22, "x͇͕̦̍͂͒", 16, 23],
      [23, 23, "7", 23, 24],
      [24, 25, "È", 24, 26],
      [26, 26, "", 26, 26],
    ]);
    // Ensure that BOUNDARY_CHAR returns single Unicode characters.
    testTextAtOffset(clusters, BOUNDARY_CHAR, [
      [0, 0, "A", 0, 1],
      [1, 1, "̀", 1, 2],
    ]);
  },
  { chrome: true, topLevel: true }
);
