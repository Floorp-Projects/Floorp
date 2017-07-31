/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function testChildrenIds(acc, expectedIds) {
  let ids = arrayFromChildren(acc).map(child => getAccessibleDOMNodeID(child));
  Assert.deepEqual(ids, expectedIds,
    `Children for ${getAccessibleDOMNodeID(acc)} are wrong.`);
}

async function runTests(browser, accDoc) {
  let div = findAccessibleChildByID(accDoc, "div");
  let select = findAccessibleChildByID(accDoc, "select");

  testChildrenIds(div, ["b"]);
  testChildrenIds(select.firstChild, ["a"]);

  let onReorders = waitForEvents([
    [EVENT_REORDER, "div"],
    [EVENT_REORDER,
      evt => getAccessibleDOMNodeID(evt.accessible.parent) == "select"]
  ]);

  await ContentTask.spawn(browser, null, async function() {
    document.getElementById("div").removeAttribute("aria-owns");
  });

  await onReorders;

  testChildrenIds(div, []);
  testChildrenIds(select.firstChild, ["a", "b"]);
}

/**
 * Test caching of accessible object states
 */
addAccessibleTask(`
  <div id="div" role="group" aria-owns="b"></div>
  <select id="select">
    <option id="a"></option>
    <option id="b"></option>
  </select>
    `, runTests);
