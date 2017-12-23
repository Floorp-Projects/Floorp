/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let NO_MOVE = { unexpected: [[EVENT_REORDER, "container"]] };
let MOVE = { expected: [[EVENT_REORDER, "container"]] };

// Set last ordinal child as aria-owned, should produce no reorder.
addAccessibleTask(`<ul id="container"><li id="a">Test</li></ul>`,
  async function(browser, accDoc) {
    let containerAcc = findAccessibleChildByID(accDoc, "container");

    testChildrenIds(containerAcc, ["a"]);

    await contentSpawnMutation(browser, NO_MOVE, function() {
      // aria-own ordinal child in place, should be a no-op.
      document.getElementById("container").setAttribute("aria-owns", "a");
    });

    testChildrenIds(containerAcc, ["a"]);
  }
);

// Add a new ordinal child to a container with an aria-owned child.
// Order should respect aria-owns.
addAccessibleTask(`<ul id="container"><li id="a">Test</li></ul>`,
  async function(browser, accDoc) {
    let containerAcc = findAccessibleChildByID(accDoc, "container");

    testChildrenIds(containerAcc, ["a"]);

    await contentSpawnMutation(browser, MOVE, function() {
      let container = document.getElementById("container");
      container.setAttribute("aria-owns", "a");

      let aa = document.createElement("li");
      aa.id = "aa";
      container.appendChild(aa);
    });

    testChildrenIds(containerAcc, ["aa", "a"]);

    await contentSpawnMutation(browser, MOVE, function() {
      document.getElementById("container").removeAttribute("aria-owns");
    });

    testChildrenIds(containerAcc, ["a", "aa"]);
  }
);

// Remove a no-move aria-owns attribute, should result in a no-move.
addAccessibleTask(`<ul id="container" aria-owns="a"><li id="a">Test</li></ul>`,
  async function(browser, accDoc) {
    let containerAcc = findAccessibleChildByID(accDoc, "container");

    testChildrenIds(containerAcc, ["a"]);

    await contentSpawnMutation(browser, NO_MOVE, function() {
      // remove aria-owned child that is already ordinal, should be no-op.
      document.getElementById("container").removeAttribute("aria-owns");
    });

    testChildrenIds(containerAcc, ["a"]);
  }
);

// Attempt to steal an aria-owned child. The attempt should fail.
addAccessibleTask(`
  <ul>
    <li id="a">Test</li>
  </ul>
  <ul aria-owns="a"></ul>
  <ul id="container"></ul>`,
  async function(browser, accDoc) {
    let containerAcc = findAccessibleChildByID(accDoc, "container");

    testChildrenIds(containerAcc, []);

    await contentSpawnMutation(browser, NO_MOVE, function() {
      document.getElementById("container").setAttribute("aria-owns", "a");
    });

    testChildrenIds(containerAcc, []);
  }
);

// Don't aria-own children of <select>
addAccessibleTask(`
  <div id="container" role="group" aria-owns="b"></div>
  <select id="select">
    <option id="a"></option>
    <option id="b"></option>
  </select>`,
  async function(browser, accDoc) {
    let containerAcc = findAccessibleChildByID(accDoc, "container");
    let selectAcc = findAccessibleChildByID(accDoc, "select");

    testChildrenIds(containerAcc, []);
    testChildrenIds(selectAcc.firstChild, ["a", "b"]);
  }
);

// Don't allow <select> to aria-own
addAccessibleTask(`
  <div id="container" role="group">
    <div id="a"></div>
    <div id="b"></div>
  </div>
  <select id="select" aria-owns="a">
    <option id="c"></option>
  </select>`,
  async function(browser, accDoc) {
    let containerAcc = findAccessibleChildByID(accDoc, "container");
    let selectAcc = findAccessibleChildByID(accDoc, "select");

    testChildrenIds(containerAcc, ["a", "b"]);
    testChildrenIds(selectAcc.firstChild, ["c"]);
  }
);

// Don't allow one <select> to aria-own an <option> from another <select>.
addAccessibleTask(`
  <select id="select1" aria-owns="c">
    <option id="a"></option>
    <option id="b"></option>
  </select>
  <select id="select2">
    <option id="c"></option>
  </select>`,
  async function(browser, accDoc) {
    let selectAcc1 = findAccessibleChildByID(accDoc, "select1");
    let selectAcc2 = findAccessibleChildByID(accDoc, "select2");

    testChildrenIds(selectAcc1.firstChild, ["a", "b"]);
    testChildrenIds(selectAcc2.firstChild, ["c"]);
  }
);

// Don't allow a <select> to reorder its children with aria-owns.
addAccessibleTask(`
  <select id="container" aria-owns="c b a">
    <option id="a"></option>
    <option id="b"></option>
    <option id="c"></option>
  </select>`,
  async function(browser, accDoc) {
    let containerAcc = findAccessibleChildByID(accDoc, "container");

    testChildrenIds(containerAcc.firstChild, ["a", "b", "c"]);

    await contentSpawnMutation(browser, NO_MOVE, function() {
      document.getElementById("container").setAttribute("aria-owns", "a c b");
    });

    testChildrenIds(containerAcc.firstChild, ["a", "b", "c"]);
  }
);

// Don't crash if ID in aria-owns does not exist
addAccessibleTask(`
  <select id="container" aria-owns="boom" multiple></select>`,
  async function(browser, accDoc) {
    ok(true, "Did not crash");
  }
);

addAccessibleTask(`
  <ul id="one">
    <li id="a">Test</li>
    <li id="b">Test 2</li>
    <li id="c">Test 3</li>
  </ul>
  <ul id="two"></ul>`,
  async function(browser, accDoc) {
    let one = findAccessibleChildByID(accDoc, "one");
    let two = findAccessibleChildByID(accDoc, "two");

    let waitfor = { expected: [
      [EVENT_REORDER, "one"],
      [EVENT_REORDER, "two"]] };

    await contentSpawnMutation(browser, waitfor, function() {
      // Put same id twice in aria-owns
      document.getElementById("two").setAttribute("aria-owns", "a a");
    });

    testChildrenIds(one, ["b", "c"]);
    testChildrenIds(two, ["a"]);

    await contentSpawnMutation(browser, waitfor, function() {
      // If the previous double-id aria-owns worked correctly, we should
      // be in a good state and all is fine..
      document.getElementById("two").setAttribute("aria-owns", "a b");
    });

    testChildrenIds(one, ["c"]);
    testChildrenIds(two, ["a", "b"]);
  }
);

addAccessibleTask(`<div id="a"></div><div id="b"></div>`,
  async function(browser, accDoc) {
    testChildrenIds(accDoc, ["a", "b"]);

    let waitFor = {
      expected: [[ EVENT_REORDER, e => e.accessible == accDoc ]]
    };

    await contentSpawnMutation(browser, waitFor, function() {
      document.documentElement.style.display = "none";
      document.documentElement.getBoundingClientRect();
      document.body.setAttribute("aria-owns", "b a");
      document.documentElement.remove();
    });

    testChildrenIds(accDoc, []);
  }
);

// Don't allow ordinal child to be placed after aria-owned child (bug 1405796)
addAccessibleTask(`<div id="container"><div id="a">Hello</div></div>
                   <div><div id="c">There</div><div id="d">There</div></div>`,
  async function(browser, accDoc) {
    let containerAcc = findAccessibleChildByID(accDoc, "container");

    testChildrenIds(containerAcc, ["a"]);

    await contentSpawnMutation(browser, MOVE, function() {
      document.getElementById("container").setAttribute("aria-owns", "c");
    });

    testChildrenIds(containerAcc, ["a", "c"]);

    await contentSpawnMutation(browser, MOVE, function() {
      let span = document.createElement("span");
      document.getElementById("container").appendChild(span);

      let b = document.createElement("div");
      b.id = "b";
      document.getElementById("container").appendChild(b);
    });

    testChildrenIds(containerAcc, ["a", "b", "c"]);

    await contentSpawnMutation(browser, MOVE, function() {
      document.getElementById("container").setAttribute("aria-owns", "c d");
    });

    testChildrenIds(containerAcc, ["a", "b", "c", "d"]);
  }
);
