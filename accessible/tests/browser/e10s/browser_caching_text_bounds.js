/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/layout.js */
loadScripts({ name: "layout.js", dir: MOCHITESTS_DIR });

async function testTextNode(accDoc, browser, id) {
  await testTextRange(accDoc, browser, id, 0, -1);
}

async function testTextRange(accDoc, browser, id, start, end) {
  const r = await invokeContentTask(
    browser,
    [id, start, end],
    (_id, _start, _end) => {
      const htNode = content.document.getElementById(_id);
      let [eX, eY, eW, eH] = [
        Number.MAX_SAFE_INTEGER,
        Number.MAX_SAFE_INTEGER,
        0,
        0,
      ];
      let traversed = 0;
      let localStart = _start;
      let endTraversal = false;
      for (let element of htNode.childNodes) {
        // ignore whitespace, but not embedded elements
        let isEmbeddedElement = false;
        if (element.length == undefined) {
          if (!element.firstChild) {
            continue;
          } else {
            isEmbeddedElement = true;
          }
        }

        if (element.length + traversed < _start) {
          // If our start index is not within this
          // node, keep looking.
          traversed += element.length;
          localStart -= element.length;
          continue;
        }

        let rect;
        if (isEmbeddedElement) {
          rect = element.getBoundingClientRect();
        } else {
          const range = content.document.createRange();
          range.setStart(element, localStart);

          if (_end != -1 && _end - traversed <= element.length) {
            // If the current node contains
            // our end index, stop here.
            endTraversal = true;
            range.setEnd(element, _end - traversed);
          } else {
            range.setEnd(element, element.length);
          }

          rect = range.getBoundingClientRect();
        }

        const oldX = eX == Number.MAX_SAFE_INTEGER ? 0 : eX;
        const oldY = eY == Number.MAX_SAFE_INTEGER ? 0 : eY;
        eX = Math.min(eX, rect.x);
        eY = Math.min(eY, rect.y);
        eW = Math.abs(Math.max(oldX + eW, rect.x + rect.width) - eX);
        eH = Math.abs(Math.max(oldY + eH, rect.y + rect.height) - eY);

        if (endTraversal) {
          break;
        }
        localStart = 0;
        traversed += element.length;
      }
      return [Math.round(eX), Math.round(eY), Math.round(eW), Math.round(eH)];
    }
  );
  let hyperTextNode = findAccessibleChildByID(accDoc, id);

  // test against parent-relative coords, because getBoundingClientRect
  // is relative to the document, not the screen. this won't work on nested
  // elements (ie. any hypertext whose parent is not the doc).
  testTextBounds(hyperTextNode, start, end, r, COORDTYPE_PARENT_RELATIVE);
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
    if (isWinNoCache) {
      ok(true, "skipping tests, running on windows without cache");
      // We have to do this in at least one of these sub-tasks because
      // otherwise the test harness complains this file is empty when
      // it runs on windows without the cache enabled.
      return;
    }

    await testTextNode(accDoc, browser, "p1");
    await testTextNode(accDoc, browser, "p2");
    await testTextNode(accDoc, browser, "p3");
    await testTextNode(accDoc, browser, "p4");
  },
  {
    iframe: true,
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
    await testTextNode(accDoc, browser, "p4");
    await testTextNode(accDoc, browser, "p5");
    // await testTextNode(accDoc, browser, "p6"); // w/o cache, fails width (a 259, e 250), w/ cache wrong w, h in iframe (line wrapping)
    await testTextNode(accDoc, browser, "p7");
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
    await testTextNode(accDoc, browser, "p4");
    if (!isCacheEnabled) {
      await testTextNode(accDoc, browser, "p5"); // w/ cache fails x, w - off by one char
    }
    // await testTextNode(accDoc, browser, "p6"); // w/o cache, fails width (a 259, e 250), w/ cache fails w, h in iframe (line wrapping)
    await testTextNode(accDoc, browser, "p7");
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

/**
 * Test simple vertical text in rl and lr layouts
 */
addAccessibleTask(
  `
  <div style="writing-mode: vertical-rl;">
    <p id='p1'>你好世界</p>
    <p id='p2'>hello world</p>
    <br>
    <p id='p3'>こんにちは世界</p>
  </div>
  <div style="writing-mode: vertical-lr;">
    <p id='p4'>你好世界</p>
    <p id='p5'>hello world</p>
    <br>
    <p id='p6'>こんにちは世界</p>
  </div>
  `,
  async function(browser, accDoc) {
    info("Testing vertical-rl");
    await testTextNode(accDoc, browser, "p1");
    await testTextNode(accDoc, browser, "p2");
    await testTextNode(accDoc, browser, "p3");
    info("Testing vertical-lr");
    await testTextNode(accDoc, browser, "p4");
    await testTextNode(accDoc, browser, "p5");
    await testTextNode(accDoc, browser, "p6");
  },
  {
    topLevel: isCacheEnabled,
    iframe: isCacheEnabled,
  }
);

/**
 * Test multiline vertical-rl text
 */
addAccessibleTask(
  `
  <p id='p1' style='writing-mode: vertical-rl;'>你好世界<br>你好世界</p>
  <p id='p2' style='writing-mode: vertical-rl;'>hello world<br>hello world</p>
  <br>
  <p id='p3' style='writing-mode: vertical-rl;'>你好世界<br> 你好世界 你好世界</p>
  <p id='p4' style='writing-mode: vertical-rl;'>hello world<br> hello world hello world</p>
  `,
  async function(browser, accDoc) {
    info("Testing vertical-rl multiline");
    await testTextNode(accDoc, browser, "p1");
    await testTextNode(accDoc, browser, "p2");
    await testTextNode(accDoc, browser, "p3");
    // await testTextNode(accDoc, browser, "p4"); // off by 4 with caching, iframe
  },
  {
    topLevel: isCacheEnabled,
    iframe: isCacheEnabled,
  }
);

/**
 * Test text with embedded chars
 */
addAccessibleTask(
  `<p id='p1' style='font-family: monospace;'>hello <a href="google.com">world</a></p>
   <p id='p2' style='font-family: monospace;'>hello<br><a href="google.com">world</a></p>
   <div id='d3'><p></p>hello world</div>
   <div id='d4'>hello world<p></p></div>
   <div id='d5'>oh<p></p>hello world</div>`,
  async function(browser, accDoc) {
    info("Testing embedded chars");
    await testTextNode(accDoc, browser, "p1");
    await testTextNode(accDoc, browser, "p2");
    await testTextNode(accDoc, browser, "d3");
    await testTextNode(accDoc, browser, "d4");
    await testTextNode(accDoc, browser, "d5");
  },
  {
    topLevel: !isWinNoCache,
    iframe: !isWinNoCache,
  }
);

/**
 * Test bounds after text mutations.
 */
addAccessibleTask(
  `<p id="p">a</p>`,
  async function(browser, docAcc) {
    await testTextNode(docAcc, browser, "p");
    const p = findAccessibleChildByID(docAcc, "p");
    info("Appending a character to text leaf");
    let textInserted = waitForEvent(EVENT_TEXT_INSERTED, p);
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("p").firstChild.data = "ab";
    });
    await textInserted;
    await testTextNode(docAcc, browser, "p");
  },
  {
    chrome: true,
    topLevel: !isWinNoCache,
    iframe: !isWinNoCache,
  }
);

/**
 * Test character bounds on the insertion point at the end of a text box.
 */
addAccessibleTask(
  `<input id="input" value="a">`,
  async function(browser, docAcc) {
    const input = findAccessibleChildByID(docAcc, "input");
    testTextPos(input, 1, [0, 0], COORDTYPE_SCREEN_RELATIVE);
  },
  {
    chrome: true,
    topLevel: !isWinNoCache,
    iframe: !isWinNoCache,
  }
);
