/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* Test case that ensures Array and other list types are not alphabetically sorted in the
 * Object Inspector.
 *
 * The tested types are:
 *  - Array
 *  - NodeList
 *  - Object
 *  - Int8Array
 *  - Int16Array
 *  - Int32Array
 *  - Uint8Array
 *  - Uint16Array
 *  - Uint32Array
 *  - Uint8ClampedArray
 *  - Float32Array
 *  - Float64Array
 */

const TEST_URI = `data:text/html;charset=utf-8,
  <html>
    <head>
      <title>Test document for bug 977500</title>
    </head>
    <body>
    <div></div> <div></div> <div></div>
    <div></div> <div></div> <div></div>
    <div></div> <div></div> <div></div>
    <div></div> <div></div> <div></div>
    </body>
  </html>`;

const typedArrayTypes = [
  "Int8Array",
  "Int16Array",
  "Int32Array",
  "Uint8Array",
  "Uint16Array",
  "Uint32Array",
  "Uint8ClampedArray",
  "Float32Array",
  "Float64Array"
];

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  // Array
  await testKeyOrder(hud, "Array(0,1,2,3,4,5,6,7,8,9,10)",
                    ["0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10"]);
  // NodeList
  await testKeyOrder(hud, "document.querySelectorAll('div')",
                    ["0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10"]);
  // Object
  await testKeyOrder(hud, "Object({'hello':1, 1:5, 10:2, 4:2, 'abc':1})",
                    ["1", "4", "10", "abc", "hello"]);

  // Typed arrays.
  for (let type of typedArrayTypes) {
    // size of 80 is enough to get 11 items on all ArrayTypes except for Float64Array.
    let size = type === "Float64Array" ? 120 : 80;
    await testKeyOrder(hud, `new ${type}(new ArrayBuffer(${size}))`,
                      ["0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10"]);
  }
});

async function testKeyOrder(hud, command, expectedKeys) {
  info(`Testing command: [${command}]`);

  info("Wait for a new .result message with an object inspector to be displayed");
  let resultsCount = findMessages(hud, "", ".result").length;
  hud.jsterm.execute(command);
  let oi = await waitFor(() => {
    let results = findMessages(hud, "", ".result");
    if (results.length == resultsCount + 1) {
      return results.pop().querySelector(".tree");
    }
    return false;
  });

  info("Expand object inspector");
  let onOiExpanded = waitFor(() => {
    return oi.querySelectorAll(".node").length >= expectedKeys.length;
  });
  oi.querySelector(".arrow").click();
  await onOiExpanded;

  let labelNodes = oi.querySelectorAll(".object-label");
  for (let i = 0; i < expectedKeys.length; i++) {
    let key = expectedKeys[i];
    let labelNode = labelNodes[i];
    is(labelNode.textContent, key, `Object inspector key is sorted as expected (${key})`);
  }
}
