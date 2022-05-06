/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/layout.js */
loadScripts({ name: "layout.js", dir: MOCHITESTS_DIR });

async function testTextNode(accDoc, browser, id) {
  let hyperTextNode = findAccessibleChildByID(accDoc, id);
  let textNode = hyperTextNode.firstChild;

  let contentDPR = await getContentDPR(browser);
  let [x, y, width, height] = getBounds(textNode, contentDPR);
  testTextBounds(
    hyperTextNode,
    0,
    -1,
    [x, y, width, height],
    COORDTYPE_SCREEN_RELATIVE
  );
}

async function testTextMultiline(accDoc, browser, id) {
  const r = await invokeContentTask(browser, [id], _id => {
    const htNode = content.document.getElementById(_id);
    let [eX, eY, eW, eH] = [
      Number.MAX_SAFE_INTEGER,
      Number.MAX_SAFE_INTEGER,
      0,
      0,
    ];
    for (let element of htNode.childNodes) {
      // ignore whitespace
      if (element.length > 0) {
        const range = content.document.createRange();
        range.setStart(element, 0);
        range.setEnd(element, element.length);
        const rect = range.getBoundingClientRect();
        const oldX = eX == Number.MAX_SAFE_INTEGER ? 0 : eX;
        const oldY = eY == Number.MAX_SAFE_INTEGER ? 0 : eY;
        eX = Math.min(eX, rect.x);
        eY = Math.min(eY, rect.y);
        eW = Math.abs(Math.max(oldX + eW, rect.x + rect.width) - eX);
        eH = Math.abs(Math.max(oldY + eH, rect.y + rect.height) - eY);
      }
    }
    return [Math.round(eX), Math.round(eY), Math.round(eW), Math.round(eH)];
  });
  let hyperTextNode = findAccessibleChildByID(accDoc, id);

  // test against parent-relative coords, because getBoundingClientRect
  // is relative to the document, not the screen. this won't work on nested
  // elements (ie. any hypertext whose parent is not the doc).
  testTextBounds(hyperTextNode, 0, -1, r, COORDTYPE_PARENT_RELATIVE);
}

async function testTextRange(accDoc, browser, id, start, end) {
  const rect = await invokeContentTask(
    browser,
    [id, start, end],
    (_id, _start, _end) => {
      const range = content.document.createRange();
      const textNode = content.document.getElementById(_id).firstChild;
      range.setStart(textNode, _start);
      range.setEnd(textNode, _end);

      return range.getBoundingClientRect();
    }
  );

  let hyperTextNode = findAccessibleChildByID(accDoc, id);

  // test against parent-relative coords, because getBoundingClientRect
  // is relative to the document, not the screen. this won't work on nested
  // elements (ie. any hypertext whose parent is not the doc).
  testTextBounds(
    hyperTextNode,
    start,
    end,
    [rect.x, rect.y, rect.width, rect.height],
    COORDTYPE_PARENT_RELATIVE
  );
}
/**
 * Test the text range boundary for simple LtR text
 */
addAccessibleTask(
  `
  <p id='p1' style='font-family: monospace;'>Tilimilitryamdiya</p>
  <p id='p2' style='font-family: monospace;'>ل</p>
  <p id='p3' dir='ltr' style='font-family: monospace;'>Привіт Світ</p>
  <pre id='p4' style='font-family: monospace;'>a%0abcdef</pre>
  `,
  async function(browser, accDoc) {
    info("Testing simple LtR text");
    await testTextNode(accDoc, browser, "p1");
    await testTextNode(accDoc, browser, "p2");
    await testTextNode(accDoc, browser, "p3");
    await testTextNode(accDoc, browser, "p4");
  },
  {
    topLevel: !isWinNoCache,
    iframe: !isWinNoCache,
  }
);

/**
 * Test the partial text range boundary for LtR text
 */
addAccessibleTask(
  `
  <p id='p1' style='font-family: monospace;'>Tilimilitryamdiya</p>
  <p id='p2' dir='ltr' style='font-family: monospace;'>Привіт Світ</p>
  `,
  async function(browser, accDoc) {
    info("Testing partial ranges in LtR text");
    await testTextRange(accDoc, browser, "p1", 0, 4);
    await testTextRange(accDoc, browser, "p1", 2, 8);
    await testTextRange(accDoc, browser, "p1", 12, 17);
    await testTextRange(accDoc, browser, "p2", 0, 4);
    await testTextRange(accDoc, browser, "p2", 2, 8);
    await testTextRange(accDoc, browser, "p2", 6, 11);
  },
  {
    topLevel: !isWinNoCache,
    iframe: !isWinNoCache,
  }
);

/**
 * Test the text boundary for multiline LtR text
 */
addAccessibleTask(
  `
  <p id='p4' dir='ltr' style='font-family: monospace;'>Привіт Світ<br>Привіт Світ</p>
  <p id='p5' dir='ltr' style='font-family: monospace;'>Привіт Світ<br> Я ще трохи тексту в другому рядку</p>
  <p id='p6' style='font-family: monospace;'>hello world I'm on line one<br> and I'm a separate line two with slightly more text</p>
  <p id='p7' style='font-family: monospace;'>hello world<br>hello world</p>
  `,
  async function(browser, accDoc) {
    info("Testing multiline LtR text");
    await testTextMultiline(accDoc, browser, "p4");
    await testTextMultiline(accDoc, browser, "p5");
    // await testTextMultiline(accDoc, browser, "p6"); // w/o cache, fails width (a 259, e 250), w/ cache wrong w, h in iframe (line wrapping)
    await testTextMultiline(accDoc, browser, "p7");
  },
  {
    topLevel: !isWinNoCache,
    iframe: !isWinNoCache,
  }
);

/**
 * Test the text boundary for simple RtL text
 */
addAccessibleTask(
  `
  <p id='p1' dir='rtl' style='font-family: monospace;'>Tilimilitryamdiya</p>
  <p id='p2' dir='rtl' style='font-family: monospace;'>ل</p>
  <p id='p3' dir='rtl' style='font-family: monospace;'>لل لللل لل</p>
  <pre id='p4' dir='rtl' style='font-family: monospace;'>a%0abcdef</pre>
  `,
  async function(browser, accDoc) {
    info("Testing simple RtL text");
    await testTextNode(accDoc, browser, "p1");
    await testTextNode(accDoc, browser, "p2");
    await testTextNode(accDoc, browser, "p3");
    await testTextNode(accDoc, browser, "p4");
  },
  {
    topLevel: !isWinNoCache,
    iframe: !isWinNoCache,
  }
);

/**
 * Test the text boundary for multiline RtL text
 */
addAccessibleTask(
  `
  <p id='p4' dir='rtl' style='font-family: monospace;'>لل لللل لل<br>لل لللل لل</p>
  <p id='p5' dir='rtl' style='font-family: monospace;'>لل لللل لل<br> لل لل  لل لل ل لل لل لل</p>
  <p id='p6' dir='rtl' style='font-family: monospace;'>hello world I'm on line one<br> and I'm a separate line two with slightly more text</p>
  <p id='p7' dir='rtl' style='font-family: monospace;'>hello world<br>hello world</p>
  `,
  async function(browser, accDoc) {
    info("Testing multiline RtL text");
    await testTextMultiline(accDoc, browser, "p4");
    // await testTextMultiline(accDoc, browser, "p5");
    // await testTextMultiline(accDoc, browser, "p6"); // w/o cache, fails width (a 259, e 250), w/ cache fails w, h in iframe (line wrapping)
    await testTextMultiline(accDoc, browser, "p7");
  },
  {
    topLevel: !isWinNoCache,
    iframe: !isWinNoCache,
  }
);

/**
 * Test the partial text range boundary for RtL text
 */
addAccessibleTask(
  `
  <p id='p1' dir='rtl' style='font-family: monospace;'>Tilimilitryamdiya</p>
  <p id='p2' dir='rtl' style='font-family: monospace;'>لل لللل لل</p>
  `,
  async function(browser, accDoc) {
    info("Testing partial ranges in RtL text");
    await testTextRange(accDoc, browser, "p1", 0, 4);
    await testTextRange(accDoc, browser, "p1", 2, 8);
    await testTextRange(accDoc, browser, "p1", 12, 17);
    await testTextRange(accDoc, browser, "p2", 0, 4);
    await testTextRange(accDoc, browser, "p2", 2, 8);
    await testTextRange(accDoc, browser, "p2", 6, 10);
  },
  {
    topLevel: !isWinNoCache,
    iframe: !isWinNoCache,
  }
);
