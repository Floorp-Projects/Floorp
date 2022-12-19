/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// test basic translation
addAccessibleTask(
  `<p id="translate">hello world</p>`,
  async function(browser, iframeDocAcc, contentDocAcc) {
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
  async function(browser, docAcc) {
    await testBoundsWithContent(docAcc, "p1", browser);
    await testBoundsWithContent(docAcc, "p2", browser);
  },
  { topLevel: true, iframe: true, remoteIframe: true }
);

// test basic rotation
addAccessibleTask(
  `<p id="rotate">hello world</p>`,
  async function(browser, iframeDocAcc, contentDocAcc) {
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
  async function(browser, iframeDocAcc, contentDocAcc) {
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
  async function(browser, docAcc) {
    if (isCacheEnabled) {
      for (const id of ["willChangeTop", "willChangeInner"]) {
        let hasTransform;
        try {
          const acc = findAccessibleChildByID(docAcc, id);
          acc.cache.getStringProperty("transform");
          hasTransform = true;
        } catch (e) {
          hasTransform = false;
        }
        ok(!hasTransform, `${id} has no cached transform`);
      }
    }
    await testBoundsWithContent(docAcc, "willChangeTopP2", browser);
    await testBoundsWithContent(docAcc, "willChangeInnerP2", browser);
  },
  { topLevel: true, iframe: true, remoteIframe: true }
);
