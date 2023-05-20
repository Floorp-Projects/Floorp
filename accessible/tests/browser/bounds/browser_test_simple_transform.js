/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loadScripts({ name: "role.js", dir: MOCHITESTS_DIR });

// test basic translation
addAccessibleTask(
  `<p id="translate">hello world</p>`,
  async function (browser, iframeDocAcc, contentDocAcc) {
    ok(iframeDocAcc, "IFRAME document accessible is present");
    await testBoundsWithContent(iframeDocAcc, "translate", browser);

    await invokeContentTask(browser, [], () => {
      let p = content.document.getElementById("translate");
      p.style = "transform: translate(100px, 100px);";
    });

    await waitForContentPaint(browser);
    await testBoundsWithContent(iframeDocAcc, "translate", browser);
  },
  { topLevel: true, iframe: true, remoteIframe: true }
);

// Test translation with two children.
addAccessibleTask(
  `
<div role="main" style="translate: 0 300px;">
  <p id="p1">hello</p>
  <p id="p2">world</p>
</div>
  `,
  async function (browser, docAcc) {
    await testBoundsWithContent(docAcc, "p1", browser);
    await testBoundsWithContent(docAcc, "p2", browser);
  },
  { topLevel: true, iframe: true, remoteIframe: true }
);

// test basic rotation
addAccessibleTask(
  `<p id="rotate">hello world</p>`,
  async function (browser, iframeDocAcc, contentDocAcc) {
    ok(iframeDocAcc, "IFRAME document accessible is present");
    await testBoundsWithContent(iframeDocAcc, "rotate", browser);

    await invokeContentTask(browser, [], () => {
      let p = content.document.getElementById("rotate");
      p.style = "transform: rotate(-40deg);";
    });

    await waitForContentPaint(browser);
    await testBoundsWithContent(iframeDocAcc, "rotate", browser);
  },
  { topLevel: true, iframe: true, remoteIframe: true }
);

// test basic scale
addAccessibleTask(
  `<p id="scale">hello world</p>`,
  async function (browser, iframeDocAcc, contentDocAcc) {
    ok(iframeDocAcc, "IFRAME document accessible is present");
    await testBoundsWithContent(iframeDocAcc, "scale", browser);

    await invokeContentTask(browser, [], () => {
      let p = content.document.getElementById("scale");
      p.style = "transform: scale(2);";
    });

    await waitForContentPaint(browser);
    await testBoundsWithContent(iframeDocAcc, "scale", browser);
  },
  { topLevel: true, iframe: true, remoteIframe: true }
);

// Test will-change: transform with no transform.
addAccessibleTask(
  `
<div id="willChangeTop" style="will-change: transform;">
  <p>hello</p>
  <p id="willChangeTopP2">world</p>
</div>
<div role="group">
  <div id="willChangeInner" style="will-change: transform;">
    <p>hello</p>
    <p id="willChangeInnerP2">world</p>
  </div>
</div>
  `,
  async function (browser, docAcc) {
    // Even though willChangeTop has no transform, it has
    // will-change: transform, which means nsIFrame::IsTransformed returns
    // true. We don't cache identity matrices, but because there is an offset
    // to the root frame, layout includes this in the returned transform
    // matrix. That means we get a non-identity matrix and thus we cache it.
    // This is why we only test the identity matrix cache optimization for
    // willChangeInner.
    let hasTransform;
    try {
      const willChangeInner = findAccessibleChildByID(
        docAcc,
        "willChangeInner"
      );
      willChangeInner.cache.getStringProperty("transform");
      hasTransform = true;
    } catch (e) {
      hasTransform = false;
    }
    ok(!hasTransform, "willChangeInner has no cached transform");
    await testBoundsWithContent(docAcc, "willChangeTopP2", browser);
    await testBoundsWithContent(docAcc, "willChangeInnerP2", browser);
  },
  { topLevel: true, iframe: true, remoteIframe: true }
);

// Verify that a transform forces creation of an accessible.
addAccessibleTask(
  `
<div id="container">
  <div style="transform:translate(100px,100px);">
    <p>test</p>
  </div>
</div>

<div id="div-presentational" role="presentation" style="transform:translate(100px,100px);">
  <p>test</p>
</div>
  `,
  async function (browser, docAcc) {
    const tree = { TEXT_CONTAINER: [{ PARAGRAPH: [{ TEXT_LEAF: [] }] }] };

    const divWithTransform = findAccessibleChildByID(
      docAcc,
      "container"
    ).firstChild;
    testAccessibleTree(divWithTransform, tree);
    // testBoundsWithContent takes an id, but divWithTransform doesn't have one,
    // so we can't test the bounds for it.

    // An accessible should still be created, even if the role is "presentation."
    const divPresentational = findAccessibleChildByID(
      docAcc,
      "div-presentational"
    );
    testAccessibleTree(divPresentational, tree);
    await testBoundsWithContent(docAcc, "div-presentational", browser);
  },
  { topLevel: true, iframe: true, remoteIframe: true }
);

// Verify that adding a transform on the fly forces creation of an accessible.
addAccessibleTask(
  `
<div id="div-to-transform" role="none" style="position: absolute; width: 300px; height: 300px;">
  <p>test</p>
</div>
  `,
  async function (browser, docAcc) {
    let divToTransform = findAccessibleChildByID(docAcc, "div-to-transform");
    ok(!divToTransform, "There should not be a div accessible.");

    // Translate the div.
    await invokeContentTask(browser, [], () => {
      let div = content.document.getElementById("div-to-transform");
      div.style.transform = "translate(100%, 100%)";
    });
    await waitForContentPaint(browser);

    // Verify that the SECTION accessible appeared after we gave it a transform.
    divToTransform = findAccessibleChildByID(docAcc, "div-to-transform");
    const tree = {
      TEXT_CONTAINER: [{ PARAGRAPH: [{ TEXT_LEAF: [] }] }],
    };
    testAccessibleTree(divToTransform, tree);

    // Verify that the bounds of the div are correctly modified.
    await testBoundsWithContent(docAcc, "div-to-transform", browser);
  },
  { topLevel: true, iframe: true, remoteIframe: true }
);

// Test translated, position: absolute Accessible in a container.
addAccessibleTask(
  `
<div id="container">
  <div id="transform" style="position: absolute; transform: translate(100px, 100px);">
    <p id="p">test</p>
  </div>
</div>
  `,
  async function (browser, docAcc) {
    await testBoundsWithContent(docAcc, "transform", browser);
    await testBoundsWithContent(docAcc, "p", browser);
  },
  { topLevel: true, iframe: true, remoteIframe: true }
);

// Test bounds of a rotated element after scroll.
addAccessibleTask(
  `
<div id="scrollable" style="transform: rotate(180deg); overflow: scroll; height: 500px;">
  <p id="test">hello world</p><hr style="height: 100vh;">
</div>
  `,
  async function (browser, docAcc) {
    info(
      "Testing that the unscrolled bounds of a transformed element are correct."
    );
    await testBoundsWithContent(docAcc, "test", browser);

    await invokeContentTask(browser, [], () => {
      // Scroll the scrollable region down (scrolls up due to the transform).
      let elem = content.document.getElementById("scrollable");
      elem.scrollTo(0, elem.scrollHeight);
    });

    info(
      "Testing that the scrolled bounds of a transformed element are correct."
    );
    await testBoundsWithContent(docAcc, "test", browser);
  },
  { topLevel: true, iframe: true, remoteIframe: true }
);
