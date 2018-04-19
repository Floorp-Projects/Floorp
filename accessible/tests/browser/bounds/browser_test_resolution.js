/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/layout.js */

async function testScaledBounds(browser, accDoc, scale, id, type = "object") {
  let acc = findAccessibleChildByID(accDoc, id);

  // Get document offset
  let [docX, docY] = getBounds(accDoc);

  // Get the unscaled bounds of the accessible
  let [x, y, width, height] = type == "text" ?
    getRangeExtents(acc, 0, -1, COORDTYPE_SCREEN_RELATIVE) : getBounds(acc);

  await ContentTask.spawn(browser, scale, _scale => {
    setResolution(document, _scale);
  });

  let [scaledX, scaledY, scaledWidth, scaledHeight] = type == "text" ?
    getRangeExtents(acc, 0, -1, COORDTYPE_SCREEN_RELATIVE) : getBounds(acc);

  let name = prettyName(acc);
  isWithin(scaledWidth, width * scale, 2, "Wrong scaled width of " + name);
  isWithin(scaledHeight, height * scale, 2, "Wrong scaled height of " + name);
  isWithin(scaledX - docX, (x - docX) * scale, 2, "Wrong scaled x of " + name);
  isWithin(scaledY - docY, (y - docY) * scale, 2, "Wrong scaled y of " + name);

  await ContentTask.spawn(browser, {}, () => {
    setResolution(document, 1.0);
  });
}

async function runTests(browser, accDoc) {
  loadFrameScripts(browser, { name: "layout.js", dir: MOCHITESTS_DIR });

  await testScaledBounds(browser, accDoc, 2.0, "p1");
  await testScaledBounds(browser, accDoc, 0.5, "p2");
  await testScaledBounds(browser, accDoc, 3.5, "b1");

  await testScaledBounds(browser, accDoc, 2.0, "p1", "text");
  await testScaledBounds(browser, accDoc, 0.75, "p2", "text");
}

/**
 * Test accessible boundaries when page is zoomed
 */
addAccessibleTask(`
<p id='p1' style='font-family: monospace;'>Tilimilitryamdiya</p>
<p id="p2">para 2</p>
<button id="b1">Hello</button>
`,
  runTests
);
