/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

async function finishContentPaint(browser) {
  await SpecialPowers.spawn(browser, [], () => {
    return new Promise(function(r) {
      content.requestAnimationFrame(() => content.setTimeout(r));
    });
  });
}

async function testContentBoundsInIframe(iframeDocAcc, id, browser) {
  const acc = findAccessibleChildByID(iframeDocAcc, id);
  const x = {};
  const y = {};
  const width = {};
  const height = {};
  acc.getBounds(x, y, width, height);

  await invokeContentTask(
    browser,
    [id, x.value, y.value, width.value, height.value],
    (_id, _x, _y, _width, _height) => {
      const { Layout: LayoutUtils } = ChromeUtils.import(
        "chrome://mochitests/content/browser/accessible/tests/browser/Layout.jsm"
      );

      let [
        expectedX,
        expectedY,
        expectedWidth,
        expectedHeight,
      ] = LayoutUtils.getBoundsForDOMElm(_id, content.document);

      ok(
        _x >= expectedX - 5 || _x <= expectedX + 5,
        "Got " + _x + ", accurate x for " + _id
      );
      ok(
        _y >= expectedY - 5 || _y <= expectedY + 5,
        "Got " + _y + ", accurate y for " + _id
      );
      ok(
        _width >= expectedWidth - 5 || _width <= expectedWidth + 5,
        "Got " + _width + ", accurate width for " + _id
      );
      ok(
        _height >= expectedHeight - 5 || _height <= expectedHeight + 5,
        "Got " + _height + ", accurate height for " + _id
      );
    }
  );
}

// test basic translation
addAccessibleTask(
  `<p id="translate">hello world</p>`,
  async function(browser, iframeDocAcc, contentDocAcc) {
    ok(iframeDocAcc, "IFRAME document accessible is present");
    await testContentBoundsInIframe(iframeDocAcc, "translate", browser);

    await invokeContentTask(browser, [], () => {
      let p = content.document.getElementById("translate");
      p.style = "transform: translate(100px, 100px);";
    });

    await finishContentPaint(browser);
    await testContentBoundsInIframe(iframeDocAcc, "translate", browser);
  },
  { topLevel: true, iframe: true, remoteIframe: true }
);

// test basic rotation
addAccessibleTask(
  `<p id="rotate">hello world</p>`,
  async function(browser, iframeDocAcc, contentDocAcc) {
    ok(iframeDocAcc, "IFRAME document accessible is present");
    await testContentBoundsInIframe(iframeDocAcc, "rotate", browser);

    await invokeContentTask(browser, [], () => {
      let p = content.document.getElementById("rotate");
      p.style = "transform: rotate(-40deg);";
    });

    await finishContentPaint(browser);
    await testContentBoundsInIframe(iframeDocAcc, "rotate", browser);
  },
  { topLevel: true, iframe: true, remoteIframe: true }
);

// test basic scale
addAccessibleTask(
  `<p id="scale">hello world</p>`,
  async function(browser, iframeDocAcc, contentDocAcc) {
    ok(iframeDocAcc, "IFRAME document accessible is present");
    await testContentBoundsInIframe(iframeDocAcc, "scale", browser);

    await invokeContentTask(browser, [], () => {
      let p = content.document.getElementById("scale");
      p.style = "transform: scale(2);";
    });

    await finishContentPaint(browser);
    await testContentBoundsInIframe(iframeDocAcc, "scale", browser);
  },
  { topLevel: true, iframe: true, remoteIframe: true }
);
