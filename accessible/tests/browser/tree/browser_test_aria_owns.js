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
  let one = findAccessibleChildByID(accDoc, "one");
  let two = findAccessibleChildByID(accDoc, "two");
  let three = findAccessibleChildByID(accDoc, "three");
  let four = findAccessibleChildByID(accDoc, "four");

  testChildrenIds(one, ["a"]);
  testChildrenIds(two, ["b", "c", "d"]);
  testChildrenIds(three, []);

  let onReorders = waitForEvents({
    expected: [
      [EVENT_REORDER, "two"]], // children will be reordered via aria-owns
    unexpected: [
      [EVENT_REORDER, "one"],  // child will remain in place
      [EVENT_REORDER, "three"], // none of its children will be reclaimed
      [EVENT_REORDER, "four"]] // child will remain in place
  });

  await ContentTask.spawn(browser, null, async function() {
    // aria-own ordinal child in place, should be a no-op.
    document.getElementById("one").setAttribute("aria-owns", "a");
    // remove aria-owned child that is already ordinal, should be no-op.
    document.getElementById("four").removeAttribute("aria-owns");
    // shuffle aria-owns with markup child.
    document.getElementById("two").setAttribute("aria-owns", "d c");
  });

  await onReorders;

  testChildrenIds(one, ["a"]);
  testChildrenIds(two, ["b", "d", "c"]);
  testChildrenIds(three, []);
  testChildrenIds(four, ["e"]);

  onReorders = waitForEvent(EVENT_REORDER, "one");

  await ContentTask.spawn(browser, null, async function() {
    let aa = document.createElement("li");
    aa.id = "aa";
    document.getElementById("one").appendChild(aa);
  });

  await onReorders;

  testChildrenIds(one, ["aa", "a"]);

  onReorders = waitForEvents([
      [EVENT_REORDER, "two"],    // "b" will go to "three"
      [EVENT_REORDER, "three"], // some children will be reclaimed and acquired
      [EVENT_REORDER, "one"]]); // removing aria-owns will reorder native children

  await ContentTask.spawn(browser, null, async function() {
    // removing aria-owns should reorder the children
    document.getElementById("one").removeAttribute("aria-owns");
    // child order will be overridden by aria-owns
    document.getElementById("three").setAttribute("aria-owns", "b d");
  });

  await onReorders;

  testChildrenIds(one, ["a", "aa"]);
  testChildrenIds(two, ["c"]);
  testChildrenIds(three, ["b", "d"]);
}

/**
 * Test caching of accessible object states
 */
addAccessibleTask(`
    <ul id="one">
      <li id="a">Test</li>
    </ul>
    <ul id="two" aria-owns="d">
      <li id="b">Test 2</li>
      <li id="c">Test 3</li>
    </ul>
    <ul id="three">
      <li id="d">Test 4</li>
    </ul>
    <ul id="four" aria-owns="e">
      <li id="e">Test 5</li>
    </ul>
    `, runTests);
