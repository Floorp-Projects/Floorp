/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../mochitest/role.js */
/* import-globals-from ../../mochitest/states.js */
loadScripts(
  { name: "role.js", dir: MOCHITESTS_DIR },
  { name: "states.js", dir: MOCHITESTS_DIR }
);

function stringForRange(macDoc, range) {
  return macDoc.getParameterizedAttributeValue(
    "AXStringForTextMarkerRange_",
    range
  );
}

function testWordAtMarker(
  macDoc,
  marker,
  expectedLeft,
  expectedRight,
  options = {}
) {
  let left = macDoc.getParameterizedAttributeValue(
    "AXLeftWordTextMarkerRangeForTextMarker",
    marker
  );
  let right = macDoc.getParameterizedAttributeValue(
    "AXRightWordTextMarkerRangeForTextMarker",
    marker
  );
  (options.leftIs || is)(
    stringForRange(macDoc, left),
    expectedLeft,
    "Left word matches"
  );
  (options.rightIs || is)(
    stringForRange(macDoc, right),
    expectedRight,
    "Right word matches"
  );

  return macDoc.getParameterizedAttributeValue(
    "AXNextTextMarkerForTextMarker",
    marker
  );
}

// Read-only tests
addAccessibleTask(`<p id="p">Hello World</p>`, async (browser, accDoc) => {
  let macDoc = accDoc.nativeInterface.QueryInterface(
    Ci.nsIAccessibleMacInterface
  );

  let startMarker = macDoc.getAttributeValue("AXStartTextMarker");
  let endMarker = macDoc.getAttributeValue("AXEndTextMarker");
  let range = macDoc.getParameterizedAttributeValue(
    "AXTextMarkerRangeForUnorderedTextMarkers",
    [startMarker, endMarker]
  );
  is(stringForRange(macDoc, range), "Hello World");

  let evt = waitForMacEvent("AXSelectedTextChanged");
  await SpecialPowers.spawn(browser, [], () => {
    let p = content.document.getElementById("p");
    let r = new content.Range();
    r.setStart(p.firstChild, 1);
    r.setEnd(p.firstChild, 8);

    let s = content.getSelection();
    s.addRange(r);
  });
  await evt;

  range = macDoc.getAttributeValue("AXSelectedTextMarkerRange");
  is(stringForRange(macDoc, range), "ello Wo");
});

addAccessibleTask(`<p>hello world goodbye</p>`, async (browser, accDoc) => {
  let macDoc = accDoc.nativeInterface.QueryInterface(
    Ci.nsIAccessibleMacInterface
  );

  let marker = macDoc.getAttributeValue("AXStartTextMarker");

  function testWordAndAdvance(left, right) {
    testWordAtMarker(macDoc, marker, left, right);
    marker = macDoc.getParameterizedAttributeValue(
      "AXNextTextMarkerForTextMarker",
      marker
    );
  }

  testWordAndAdvance("hello", "hello");
  testWordAndAdvance("hello", "hello");
  testWordAndAdvance("hello", "hello");
  testWordAndAdvance("hello", "hello");
  testWordAndAdvance("hello", "hello");
  testWordAndAdvance("hello", " ");
  testWordAndAdvance(" ", "world");
  testWordAndAdvance("world", "world");
  testWordAndAdvance("world", "world");
  testWordAndAdvance("world", "world");
  testWordAndAdvance("world", "world");
  testWordAndAdvance("world", " ");
  testWordAndAdvance(" ", "goodbye");
  testWordAndAdvance("goodbye", "goodbye");
  testWordAndAdvance("goodbye", "goodbye");
  testWordAndAdvance("goodbye", "goodbye");
  testWordAndAdvance("goodbye", "goodbye");
  testWordAndAdvance("goodbye", "goodbye");
  testWordAndAdvance("goodbye", "goodbye");
  testWordAndAdvance("goodbye", "");

  ok(!marker, "Iterated through all markers");
});

addAccessibleTask(
  `<p>hello world <a href="#">i love you</a> goodbye</p>`,
  async (browser, accDoc) => {
    let macDoc = accDoc.nativeInterface.QueryInterface(
      Ci.nsIAccessibleMacInterface
    );

    let marker = macDoc.getAttributeValue("AXStartTextMarker");

    function testWordAndAdvance(left, right, options = {}) {
      testWordAtMarker(macDoc, marker, left, right, options);
      marker = macDoc.getParameterizedAttributeValue(
        "AXNextTextMarkerForTextMarker",
        marker
      );
    }

    testWordAndAdvance("hello", "hello");
    testWordAndAdvance("hello", "hello");
    testWordAndAdvance("hello", "hello");
    testWordAndAdvance("hello", "hello");
    testWordAndAdvance("hello", "hello");
    testWordAndAdvance("hello", " ");
    testWordAndAdvance(" ", "world");
    testWordAndAdvance("world", "world");
    testWordAndAdvance("world", "world");
    testWordAndAdvance("world", "world");
    testWordAndAdvance("world", "world");
    testWordAndAdvance("world", " ");
    testWordAndAdvance(" ", "i");
    testWordAndAdvance("i", " ");
    testWordAndAdvance(" ", "love");
    testWordAndAdvance("love", "love");
    testWordAndAdvance("love", "love");
    testWordAndAdvance("love", "love");
    testWordAndAdvance("love", " ");
    testWordAndAdvance(" ", "you");
    testWordAndAdvance("you", "you");
    testWordAndAdvance("you", "you");
    testWordAndAdvance("you", " ");
    testWordAndAdvance(" ", "goodbye");
    testWordAndAdvance("goodbye", "goodbye");
    testWordAndAdvance("goodbye", "goodbye");
    testWordAndAdvance("goodbye", "goodbye");
    testWordAndAdvance("goodbye", "goodbye");
    testWordAndAdvance("goodbye", "goodbye");
    testWordAndAdvance("goodbye", "goodbye");
    testWordAndAdvance("goodbye", "");
  }
);

addAccessibleTask(
  `<p>hello <a href=#">wor</a>ld goodbye</p>`,
  async (browser, accDoc) => {
    let macDoc = accDoc.nativeInterface.QueryInterface(
      Ci.nsIAccessibleMacInterface
    );

    let marker = macDoc.getAttributeValue("AXStartTextMarker");

    for (let i = 0; i < 7; i++) {
      marker = macDoc.getParameterizedAttributeValue(
        "AXNextTextMarkerForTextMarker",
        marker
      );
    }

    let left = macDoc.getParameterizedAttributeValue(
      "AXLeftWordTextMarkerRangeForTextMarker",
      marker
    );
    let right = macDoc.getParameterizedAttributeValue(
      "AXRightWordTextMarkerRangeForTextMarker",
      marker
    );
    is(stringForRange(macDoc, left), "world", "Left word matches");
    is(stringForRange(macDoc, right), "world", "Right word matches");

    marker = macDoc.getParameterizedAttributeValue(
      "AXNextTextMarkerForTextMarker",
      marker
    );

    left = macDoc.getParameterizedAttributeValue(
      "AXLeftWordTextMarkerRangeForTextMarker",
      marker
    );
    right = macDoc.getParameterizedAttributeValue(
      "AXRightWordTextMarkerRangeForTextMarker",
      marker
    );
    is(stringForRange(macDoc, left), "world", "Left word matches");
    is(stringForRange(macDoc, right), "world", "Right word matches");
  }
);
