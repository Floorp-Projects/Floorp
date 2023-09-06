/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
requestLongerTimeout(3);

/* import-globals-from ../../mochitest/layout.js */
loadScripts({ name: "layout.js", dir: MOCHITESTS_DIR });

// Note that testTextNode, testChar and testTextRange currently don't handle
// white space in the code that doesn't get rendered on screen. To work around
// this, ensure that containers you want to test are all on a single line in the
// test snippet.

async function testTextNode(accDoc, browser, id) {
  await testTextRange(accDoc, browser, id, 0, -1);
}

async function testChar(accDoc, browser, id, idx) {
  await testTextRange(accDoc, browser, id, idx, idx + 1);
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
          let potentialTextContainer = element;
          while (
            potentialTextContainer &&
            potentialTextContainer.length == undefined
          ) {
            potentialTextContainer = element.firstChild;
          }
          if (potentialTextContainer && potentialTextContainer.length) {
            // If we can reach some text from this container, use that as part
            // of our range. This is important when testing with intervening inline
            // elements. ie. <pre><code>ab%0acd
            element = potentialTextContainer;
          } else if (element.firstChild) {
            isEmbeddedElement = true;
          } else {
            continue;
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

  // Add in the doc's screen coords because getBoundingClientRect
  // is relative to the document, not the screen. This assumes the doc's
  // screen coords are correct. We use getBoundsInCSSPixels to avoid factoring
  // in the DPR ourselves.
  let x = {};
  let y = {};
  let w = {};
  let h = {};
  accDoc.getBoundsInCSSPixels(x, y, w, h);
  r[0] += x.value;
  r[1] += y.value;
  if (end != -1 && end - start == 1) {
    // If we're only testing a character, use this function because it calls
    // CharBounds() directly instead of TextBounds().
    testTextPos(hyperTextNode, start, [r[0], r[1]], COORDTYPE_SCREEN_RELATIVE);
  } else {
    testTextBounds(hyperTextNode, start, end, r, COORDTYPE_SCREEN_RELATIVE);
  }
}

/**
 * Since testTextChar can't handle non-rendered white space, this function first
 * uses testTextChar to verify the first character and then ensures all
 * characters thereafter have an incrementing x and a non-0 width.
 */
async function testLineWithNonRenderedSpace(docAcc, browser, id, length) {
  await testChar(docAcc, browser, id, 0);
  const acc = findAccessibleChildByID(docAcc, id, [nsIAccessibleText]);
  let prevX = -1;
  for (let offset = 0; offset < length; ++offset) {
    const x = {};
    const y = {};
    const w = {};
    const h = {};
    acc.getCharacterExtents(offset, x, y, w, h, COORDTYPE_SCREEN_RELATIVE);
    ok(x.value > prevX, `${id}: offset ${offset} x is larger (${x.value})`);
    prevX = x.value;
    ok(w.value > 0, `${id}: offset ${offset} width > 0`);
  }
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
  async function (browser, accDoc) {
    info("Testing simple LtR text");
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
  async function (browser, accDoc) {
    info("Testing partial ranges in LtR text");
    await testTextRange(accDoc, browser, "p1", 0, 4);
    await testTextRange(accDoc, browser, "p1", 2, 8);
    await testTextRange(accDoc, browser, "p1", 12, 17);
    await testTextRange(accDoc, browser, "p2", 0, 4);
    await testTextRange(accDoc, browser, "p2", 2, 8);
    await testTextRange(accDoc, browser, "p2", 6, 11);
  },
  {
    topLevel: true,
    iframe: true,
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
  async function (browser, accDoc) {
    info("Testing multiline LtR text");
    await testTextNode(accDoc, browser, "p4");
    await testTextNode(accDoc, browser, "p5");
    // await testTextNode(accDoc, browser, "p6"); // w/o cache, fails width (a 259, e 250), w/ cache wrong w, h in iframe (line wrapping)
    await testTextNode(accDoc, browser, "p7");
  },
  {
    topLevel: true,
    iframe: true,
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
  async function (browser, accDoc) {
    info("Testing simple RtL text");
    await testTextNode(accDoc, browser, "p1");
    await testTextNode(accDoc, browser, "p2");
    await testTextNode(accDoc, browser, "p3");
    await testTextNode(accDoc, browser, "p4");
  },
  {
    topLevel: true,
    iframe: true,
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
  async function (browser, accDoc) {
    info("Testing multiline RtL text");
    await testTextNode(accDoc, browser, "p4");
    //await testTextNode(accDoc, browser, "p5"); // w/ cache fails x, w - off by one char
    // await testTextNode(accDoc, browser, "p6"); // w/o cache, fails width (a 259, e 250), w/ cache fails w, h in iframe (line wrapping)
    await testTextNode(accDoc, browser, "p7");
  },
  {
    topLevel: true,
    iframe: true,
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
  async function (browser, accDoc) {
    info("Testing partial ranges in RtL text");
    await testTextRange(accDoc, browser, "p1", 0, 4);
    await testTextRange(accDoc, browser, "p1", 2, 8);
    await testTextRange(accDoc, browser, "p1", 12, 17);
    await testTextRange(accDoc, browser, "p2", 0, 4);
    await testTextRange(accDoc, browser, "p2", 2, 8);
    await testTextRange(accDoc, browser, "p2", 6, 10);
  },
  {
    topLevel: true,
    iframe: true,
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
  async function (browser, accDoc) {
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
    topLevel: true,
    iframe: true,
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
  async function (browser, accDoc) {
    info("Testing vertical-rl multiline");
    await testTextNode(accDoc, browser, "p1");
    await testTextNode(accDoc, browser, "p2");
    await testTextNode(accDoc, browser, "p3");
    // await testTextNode(accDoc, browser, "p4"); // off by 4 with caching, iframe
  },
  {
    topLevel: true,
    iframe: true,
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
  async function (browser, accDoc) {
    info("Testing embedded chars");
    await testTextNode(accDoc, browser, "p1");
    await testTextNode(accDoc, browser, "p2");
    await testTextNode(accDoc, browser, "d3");
    await testTextNode(accDoc, browser, "d4");
    await testTextNode(accDoc, browser, "d5");
  },
  {
    topLevel: true,
    iframe: true,
  }
);

/**
 * Test bounds after text mutations.
 */
addAccessibleTask(
  `<p id="p">a</p>`,
  async function (browser, docAcc) {
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
    topLevel: true,
    iframe: true,
  }
);

/**
 * Test character bounds on the insertion point at the end of a text box.
 */
addAccessibleTask(
  `<input id="input" value="a">`,
  async function (browser, docAcc) {
    const input = findAccessibleChildByID(docAcc, "input");
    testTextPos(input, 1, [0, 0], COORDTYPE_SCREEN_RELATIVE);
  },
  {
    chrome: true,
    topLevel: true,
    iframe: true,
  }
);

/**
 * Test character bounds after non-br line break.
 */
addAccessibleTask(
  `
  <style>
    @font-face {
      font-family: Ahem;
      src: url(${CURRENT_CONTENT_DIR}e10s/fonts/Ahem.sjs);
    }
    pre {
      font: 20px/20px Ahem;
    }
  </style>
  <pre id="t">XX
XXX</pre>`,
  async function (browser, docAcc) {
    await testChar(docAcc, browser, "t", 3);
  },
  {
    chrome: true,
    topLevel: true,
    iframe: true,
  }
);

/**
 * Test character bounds in a pre with padding.
 */
addAccessibleTask(
  `
  <style>
    @font-face {
      font-family: Ahem;
      src: url(${CURRENT_CONTENT_DIR}e10s/fonts/Ahem.sjs);
    }
    pre {
      font: 20px/20px Ahem;
      padding: 20px;
    }
  </style>
  <pre id="t">XX
XXX</pre>`,
  async function (browser, docAcc) {
    await testTextNode(docAcc, browser, "t");
    await testChar(docAcc, browser, "t", 3);
  },
  {
    chrome: true,
    topLevel: true,
    iframe: true,
  }
);

/**
 * Test text bounds with an invalid end offset.
 */
addAccessibleTask(
  `<p id="p">a</p>`,
  async function (browser, docAcc) {
    const p = findAccessibleChildByID(docAcc, "p");
    testTextBounds(p, 0, 2, [0, 0, 0, 0], COORDTYPE_SCREEN_RELATIVE);
  },
  { chrome: true, topLevel: !true }
);

/**
 * Test character bounds in an intervening inline element with non-br line breaks
 */
addAccessibleTask(
  `
  <style>
    @font-face {
      font-family: Ahem;
      src: url(${CURRENT_CONTENT_DIR}e10s/fonts/Ahem.sjs);
    }
    pre {
      font: 20px/20px Ahem;
    }
  </style>
  <pre id="t"><code role="none">XX
XXX
XX
X</pre>`,
  async function (browser, docAcc) {
    await testChar(docAcc, browser, "t", 0);
    await testChar(docAcc, browser, "t", 3);
    await testChar(docAcc, browser, "t", 7);
    await testChar(docAcc, browser, "t", 10);
  },
  {
    chrome: true,
    topLevel: true,
    iframe: true,
  }
);

/**
 * Test character bounds in an intervening inline element with margins
 * and with non-br line breaks
 */
addAccessibleTask(
  `
  <style>
    @font-face {
      font-family: Ahem;
      src: url(${CURRENT_CONTENT_DIR}e10s/fonts/Ahem.sjs);
    }
  </style>
  <div>hello<pre id="t" style="margin-left:100px;margin-top:30px;background-color:blue;">XX
XXX
XX
X</pre></div>`,
  async function (browser, docAcc) {
    await testChar(docAcc, browser, "t", 0);
    await testChar(docAcc, browser, "t", 3);
    await testChar(docAcc, browser, "t", 7);
    await testChar(docAcc, browser, "t", 10);
  },
  {
    chrome: true,
    topLevel: true,
    iframe: true,
  }
);

/**
 * Test text bounds in a textarea after scrolling.
 */
addAccessibleTask(
  `
<textarea id="textarea" rows="1">a
b
c</textarea>
  `,
  async function (browser, docAcc) {
    // We can't use testChar because Range.getBoundingClientRect isn't supported
    // inside textareas.
    const textarea = findAccessibleChildByID(docAcc, "textarea");
    textarea.QueryInterface(nsIAccessibleText);
    const oldY = {};
    textarea.getCharacterExtents(
      4,
      {},
      oldY,
      {},
      {},
      COORDTYPE_SCREEN_RELATIVE
    );
    info("Moving textarea caret to c");
    await invokeContentTask(browser, [], () => {
      const textareaDom = content.document.getElementById("textarea");
      textareaDom.focus();
      textareaDom.selectionStart = 4;
    });
    await waitForContentPaint(browser);
    const newY = {};
    textarea.getCharacterExtents(
      4,
      {},
      newY,
      {},
      {},
      COORDTYPE_SCREEN_RELATIVE
    );
    ok(newY.value < oldY.value, "y coordinate smaller after scrolling down");
  },
  { chrome: true, topLevel: true, iframe: !true }
);

/**
 * Test magic offsets with GetCharacter/RangeExtents.
 */
addAccessibleTask(
  `<input id="input" value="abc">`,
  async function (browser, docAcc) {
    const input = findAccessibleChildByID(docAcc, "input", [nsIAccessibleText]);
    info("Setting caret and focusing input");
    let caretMoved = waitForEvent(EVENT_TEXT_CARET_MOVED, input);
    await invokeContentTask(browser, [], () => {
      const inputDom = content.document.getElementById("input");
      inputDom.selectionStart = inputDom.selectionEnd = 1;
      inputDom.focus();
    });
    await caretMoved;
    is(input.caretOffset, 1, "input caretOffset is 1");
    let expectedX = {};
    let expectedY = {};
    let expectedW = {};
    let expectedH = {};
    let magicX = {};
    let magicY = {};
    let magicW = {};
    let magicH = {};
    input.getCharacterExtents(
      1,
      expectedX,
      expectedY,
      expectedW,
      expectedH,
      COORDTYPE_SCREEN_RELATIVE
    );
    input.getCharacterExtents(
      nsIAccessibleText.TEXT_OFFSET_CARET,
      magicX,
      magicY,
      magicW,
      magicH,
      COORDTYPE_SCREEN_RELATIVE
    );
    Assert.deepEqual(
      [magicX.value, magicY.value, magicW.value, magicH.value],
      [expectedX.value, expectedY.value, expectedW.value, expectedH.value],
      "GetCharacterExtents correct with TEXT_OFFSET_CARET"
    );
    input.getRangeExtents(
      1,
      3,
      expectedX,
      expectedY,
      expectedW,
      expectedH,
      COORDTYPE_SCREEN_RELATIVE
    );
    input.getRangeExtents(
      nsIAccessibleText.TEXT_OFFSET_CARET,
      nsIAccessibleText.TEXT_OFFSET_END_OF_TEXT,
      magicX,
      magicY,
      magicW,
      magicH,
      COORDTYPE_SCREEN_RELATIVE
    );
    Assert.deepEqual(
      [magicX.value, magicY.value, magicW.value, magicH.value],
      [expectedX.value, expectedY.value, expectedW.value, expectedH.value],
      "GetRangeExtents correct with TEXT_OFFSET_CARET/END_OF_TEXT"
    );
  },
  { chrome: true, topLevel: true, remoteIframe: !true }
);

/**
 * Test wrapped text and pre-formatted text beginning with an empty line.
 */
addAccessibleTask(
  `
<style>
  #wrappedText {
    width: 3ch;
    font-family: monospace;
  }
</style>
<p id="wrappedText"><a href="https://example.com/">a</a>b cd</p>
<p id="emptyFirstLine" style="white-space: pre-line;">
foo</p>
  `,
  async function (browser, docAcc) {
    await testChar(docAcc, browser, "wrappedText", 0);
    await testChar(docAcc, browser, "wrappedText", 1);
    await testChar(docAcc, browser, "wrappedText", 2);
    await testChar(docAcc, browser, "wrappedText", 3);
    await testChar(docAcc, browser, "wrappedText", 4);

    // We can't use testChar for emptyFirstLine because it doesn't handle white
    // space properly. Instead, verify that the first character is at the top
    // left of the text leaf.
    const emptyFirstLine = findAccessibleChildByID(docAcc, "emptyFirstLine", [
      nsIAccessibleText,
    ]);
    const emptyFirstLineLeaf = emptyFirstLine.firstChild;
    const leafX = {};
    const leafY = {};
    emptyFirstLineLeaf.getBounds(leafX, leafY, {}, {});
    testTextPos(
      emptyFirstLine,
      0,
      [leafX.value, leafY.value],
      COORDTYPE_SCREEN_RELATIVE
    );
  },
  { chrome: true, topLevel: true, remoteIframe: !true }
);

/**
 * Test character bounds in an intervening inline element with non-br line breaks
 */
addAccessibleTask(
  `
  <style>
    @font-face {
      font-family: Ahem;
      src: url(${CURRENT_CONTENT_DIR}e10s/fonts/Ahem.sjs);
    }
    pre {
      font: 20px/20px Ahem;
    }
  </style>
  <pre><code id="t" role="group">XX
XXX
XX
X</pre>`,
  async function (browser, docAcc) {
    await testChar(docAcc, browser, "t", 0);
    await testChar(docAcc, browser, "t", 3);
    await testChar(docAcc, browser, "t", 7);
    await testChar(docAcc, browser, "t", 10);
  },
  {
    chrome: true,
    topLevel: true,
    iframe: true,
  }
);

/**
 * Test character bounds where content white space isn't rendered.
 */
addAccessibleTask(
  `
<p id="single">a  b</p>
<p id="multi"><ins>a </ins>
b</p>
<pre id="pre">a  b</pre>
  `,
  async function (browser, docAcc) {
    await testLineWithNonRenderedSpace(docAcc, browser, "single", 3);
    await testLineWithNonRenderedSpace(docAcc, browser, "multi", 2);
    for (let offset = 0; offset < 4; ++offset) {
      await testChar(docAcc, browser, "pre", offset);
    }
  },
  { chrome: true, topLevel: true }
);
