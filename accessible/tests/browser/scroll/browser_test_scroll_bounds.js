/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/layout.js */
loadScripts({ name: "layout.js", dir: MOCHITESTS_DIR });

async function runTests(browser, docAcc) {
  ok(docAcc, "iframe document acc is present");
  await testBoundsInContent(docAcc, "square", browser);
  await testBoundsInContent(docAcc, "rect", browser);

  await invokeContentTask(browser, [], () => {
    content.document.getElementById("square").scrollIntoView();
  });

  await waitForContentPaint(browser);

  await testBoundsInContent(docAcc, "square", browser);
  await testBoundsInContent(docAcc, "rect", browser);

  await invokeContentTask(browser, [], () => {
    content.document.getElementById("rect").scrollIntoView();
  });

  await waitForContentPaint(browser);

  await testBoundsInContent(docAcc, "square", browser);
  await testBoundsInContent(docAcc, "rect", browser);
}

/**
 * Test bounds of accessibles after scrolling
 */
addAccessibleTask(
  `
  <div id='square' style='height:100px; width:100px; background:green; margin-top:3000px; margin-bottom:4000px;'>
  </div>

  <div id='rect' style='height:40px; width:200px; background:blue; margin-bottom:3400px'>
  </div>
  `,
  runTests,
  { iframe: true, remoteIframe: true, chrome: true }
);
