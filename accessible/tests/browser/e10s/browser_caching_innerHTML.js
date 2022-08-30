/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test caching of innerHTML on math elements for Windows clients.
 */
addAccessibleTask(
  `
<p id="p">test</p>
<math id="math"><mfrac><mi>x</mi><mi>y</mi></mfrac></math>
  `,
  async function(browser, docAcc) {
    if (!isCacheEnabled) {
      // Stop the harness from complaining that this file is empty when run with
      // the cache disabled.
      todo(false, "Cache disabled for a cache only test");
      return;
    }

    const p = findAccessibleChildByID(docAcc, "p");
    let hasHtml;
    try {
      p.cache.getStringProperty("html");
      hasHtml = true;
    } catch (e) {
      hasHtml = false;
    }
    ok(!hasHtml, "p doesn't have cached html");

    const math = findAccessibleChildByID(docAcc, "math");
    is(
      math.cache.getStringProperty("html"),
      "<mfrac><mi>x</mi><mi>y</mi></mfrac>",
      "math cached html is correct"
    );

    info("Mutating math");
    await invokeContentTask(browser, [], () => {
      content.document.querySelectorAll("mi")[1].textContent = "z";
    });
    await untilCacheIs(
      () => math.cache.getStringProperty("html"),
      "<mfrac><mi>x</mi><mi>z</mi></mfrac>",
      "math cached html is correct after mutation"
    );
  },
  {
    topLevel: true,
    iframe: isCacheEnabled,
    remoteIframe: isCacheEnabled,
  }
);
