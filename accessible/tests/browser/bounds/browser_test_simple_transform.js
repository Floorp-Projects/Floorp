/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// test basic translation
addAccessibleTask(
  `<p id="translate">hello world</p>`,
  async function(browser, iframeDocAcc, contentDocAcc) {
    ok(iframeDocAcc, "IFRAME document accessible is present");
    await testBoundsInContent(iframeDocAcc, "translate", browser);

    await invokeContentTask(browser, [], () => {
      let p = content.document.getElementById("translate");
      p.style = "transform: translate(100px, 100px);";
    });

    await waitForContentPaint(browser);
    await testBoundsInContent(iframeDocAcc, "translate", browser);
  },
  { topLevel: true, iframe: true, remoteIframe: true }
);

// test basic rotation
addAccessibleTask(
  `<p id="rotate">hello world</p>`,
  async function(browser, iframeDocAcc, contentDocAcc) {
    ok(iframeDocAcc, "IFRAME document accessible is present");
    await testBoundsInContent(iframeDocAcc, "rotate", browser);

    await invokeContentTask(browser, [], () => {
      let p = content.document.getElementById("rotate");
      p.style = "transform: rotate(-40deg);";
    });

    await waitForContentPaint(browser);
    await testBoundsInContent(iframeDocAcc, "rotate", browser);
  },
  { topLevel: true, iframe: true, remoteIframe: true }
);

// test basic scale
addAccessibleTask(
  `<p id="scale">hello world</p>`,
  async function(browser, iframeDocAcc, contentDocAcc) {
    ok(iframeDocAcc, "IFRAME document accessible is present");
    await testBoundsInContent(iframeDocAcc, "scale", browser);

    await invokeContentTask(browser, [], () => {
      let p = content.document.getElementById("scale");
      p.style = "transform: scale(2);";
    });

    await waitForContentPaint(browser);
    await testBoundsInContent(iframeDocAcc, "scale", browser);
  },
  { topLevel: true, iframe: true, remoteIframe: true }
);
