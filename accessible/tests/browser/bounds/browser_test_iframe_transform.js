/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TRANSLATION_OFFSET = 50;
const ELEM_ID = "test-elem-id";

// Modify the style of an iframe within the content process. This is different
// from, e.g., invokeSetStyle, because this function doesn't rely on
// invokeContentTask, which runs in the context of the iframe itself.
async function invokeSetStyleIframe(browser, id, style, value) {
  if (value) {
    Logger.log(`Setting ${style} style to ${value} for iframe with id: ${id}`);
  } else {
    Logger.log(`Removing ${style} style from iframe with id: ${id}`);
  }

  // Translate the iframe itself (not content within it).
  await SpecialPowers.spawn(
    browser,
    [id, style, value],
    (iframeId, iframeStyle, iframeValue) => {
      const elm = content.document.getElementById(iframeId);
      if (iframeValue) {
        elm.style[iframeStyle] = iframeValue;
      } else {
        delete elm.style[iframeStyle];
      }
    }
  );
}

// Test the accessible's bounds, comparing them to the content bounds from DOM.
// This function also accepts an offset, which is necessary in some cases where
// DOM doesn't know about cross-process offsets.
function testBoundsWithOffset(browser, iframeDocAcc, id, domElmBounds, offset) {
  // Get the bounds as reported by the accessible.
  const acc = findAccessibleChildByID(iframeDocAcc, id);
  const accX = {};
  const accY = {};
  const accWidth = {};
  const accHeight = {};
  acc.getBounds(accX, accY, accWidth, accHeight);

  // getContentBoundsForDOMElm's result doesn't include iframe translation
  // for in-process iframes, but does for out-of-process iframes. To account
  // for that here, manually add in the translation offset when examining an
  // in-process iframe.
  const addTranslationOffset = !gIsRemoteIframe;
  const expectedX = addTranslationOffset
    ? domElmBounds[0] + offset
    : domElmBounds[0];
  const expectedY = addTranslationOffset
    ? domElmBounds[1] + offset
    : domElmBounds[1];
  const expectedWidth = domElmBounds[2];
  const expectedHeight = domElmBounds[3];

  let boundsAreEquivalent = true;
  boundsAreEquivalent &&= accX.value == expectedX;
  boundsAreEquivalent &&= accY.value == expectedY;
  boundsAreEquivalent &&= accWidth.value == expectedWidth;
  boundsAreEquivalent &&= accHeight.value == expectedHeight;
  return boundsAreEquivalent;
}

addAccessibleTask(
  `<div id='${ELEM_ID}'>hello world</div>`,
  async function (browser, iframeDocAcc, contentDocAcc) {
    ok(iframeDocAcc, "IFRAME document accessible is present");

    await testBoundsWithContent(iframeDocAcc, ELEM_ID, browser);

    // Translate the iframe, which should modify cross-process offset.
    await invokeSetStyleIframe(
      browser,
      DEFAULT_IFRAME_ID,
      "transform",
      `translate(${TRANSLATION_OFFSET}px, ${TRANSLATION_OFFSET}px)`
    );

    // Allow content to advance to update DOM, then capture the DOM bounds.
    await waitForContentPaint(browser);
    const domElmBoundsAfterTranslate = await getContentBoundsForDOMElm(
      browser,
      ELEM_ID
    );

    // Ensure that there's enough time for the cache to update.
    await untilCacheOk(() => {
      return testBoundsWithOffset(
        browser,
        iframeDocAcc,
        ELEM_ID,
        domElmBoundsAfterTranslate,
        TRANSLATION_OFFSET
      );
    }, "Accessible bounds have changed in the cache and match DOM bounds.");

    // Adjust padding of the iframe, then verify bounds adjust properly.
    // iframes already have a border by default, so we check padding here.
    const PADDING_OFFSET = 100;
    await invokeSetStyleIframe(
      browser,
      DEFAULT_IFRAME_ID,
      "padding",
      `${PADDING_OFFSET}px`
    );

    // Allow content to advance to update DOM, then capture the DOM bounds.
    await waitForContentPaint(browser);
    const domElmBoundsAfterAddingPadding = await getContentBoundsForDOMElm(
      browser,
      ELEM_ID
    );

    await untilCacheOk(() => {
      return testBoundsWithOffset(
        browser,
        iframeDocAcc,
        ELEM_ID,
        domElmBoundsAfterAddingPadding,
        TRANSLATION_OFFSET
      );
    }, "Accessible bounds have changed in the cache and match DOM bounds.");
  },
  {
    topLevel: false,
    iframe: true,
    remoteIframe: true,
    iframeAttrs: {
      style: `height: 100px; width: 100px;`,
    },
  }
);

/**
 * Test document bounds change notifications.
 * Note: This uses iframes to change the doc container size in order
 * to have the doc accessible's bounds change.
 */
addAccessibleTask(
  `<div id="div" style="width: 30px; height: 30px"></div>`,
  async function (browser, accDoc, foo) {
    const docWidth = () => {
      let width = {};
      accDoc.getBounds({}, {}, width, {});
      return width.value;
    };

    await untilCacheIs(docWidth, 0, "Doc width is 0");
    await invokeSetStyleIframe(browser, DEFAULT_IFRAME_ID, "width", `300px`);
    await untilCacheIs(docWidth, 300, "Doc width is 300");
  },
  {
    chrome: false,
    topLevel: false,
    iframe: true,
    remoteIframe: true,
    iframeAttrs: { style: "width: 0;" },
  }
);

/**
 * Test document bounds after re-creating an iframe.
 */
addAccessibleTask(
  `
<ol id="ol">
  <iframe id="iframe" src="data:text/html,"></iframe>
</ol>
  `,
  async function (browser, docAcc) {
    let iframeDoc = findAccessibleChildByID(docAcc, "iframe").firstChild;
    ok(iframeDoc, "Got the iframe document");
    const origX = {};
    const origY = {};
    iframeDoc.getBounds(origX, origY, {}, {});
    let reordered = waitForEvent(EVENT_REORDER, docAcc);
    await invokeContentTask(browser, [], () => {
      // This will cause a bounds cache update to be queued for the iframe doc.
      content.document.getElementById("iframe").width = "600";
      // This will recreate the ol a11y subtree, including the iframe. The
      // iframe document will be unbound briefly while this happens. We want to
      // be sure processing the bounds cache update queued above doesn't assert
      // while the document is unbound. The setTimeout is necessary to get the
      // cache update to happen at the right time.
      content.setTimeout(
        () => (content.document.getElementById("ol").type = "i"),
        0
      );
    });
    await reordered;
    const iframe = findAccessibleChildByID(docAcc, "iframe");
    // We don't currently fire an event when a DocAccessible is re-bound to a new OuterDoc.
    await BrowserTestUtils.waitForCondition(() => iframe.firstChild);
    iframeDoc = iframe.firstChild;
    ok(iframeDoc, "Got the iframe document after re-creation");
    const newX = {};
    const newY = {};
    iframeDoc.getBounds(newX, newY, {}, {});
    ok(
      origX.value == newX.value && origY.value == newY.value,
      "Iframe document x and y are same after iframe re-creation"
    );
  }
);
