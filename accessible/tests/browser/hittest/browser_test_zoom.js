/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

async function runTests(browser, accDoc) {
  if (Services.appinfo.OS !== "Darwin") {
    const p1 = findAccessibleChildByID(accDoc, "p1");
    const p2 = findAccessibleChildByID(accDoc, "p2");
    await hitTest(browser, accDoc, p1, p1.firstChild);
    await hitTest(browser, accDoc, p2, p2.firstChild);

    await invokeContentTask(browser, [], () => {
      const { Layout } = ChromeUtils.import(
        "chrome://mochitests/content/browser/accessible/tests/browser/Layout.jsm"
      );

      Layout.zoomDocument(content.document, 2.0);
      content.document.body.offsetTop; // getBounds doesn't flush layout on its own.
    });

    await hitTest(browser, accDoc, p1, p1.firstChild);
    await hitTest(browser, accDoc, p2, p2.firstChild);
  } else {
    todo(
      false,
      "Bug 746974 - deepest child must be correct on all platforms, disabling on Mac!"
    );
  }
}

addAccessibleTask(`<p id="p1">para 1</p><p id="p2">para 2</p>`, runTests, {
  iframe: true,
  remoteIframe: true,
  // Ensure that all hittest elements are in view.
  iframeAttrs: { style: "left: 100px; top: 100px;" },
});
