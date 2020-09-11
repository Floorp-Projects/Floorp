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
    "AXStringForTextMarkerRange",
    range
  );
}

function testUIElementAtMarker(macDoc, marker, expectedString) {
  let elem = macDoc.getParameterizedAttributeValue(
    "AXUIElementForTextMarker",
    marker
  );
  is(elem.getAttributeValue("AXRole"), "AXStaticText");
  is(elem.getAttributeValue("AXValue"), expectedString);
  let elemRange = macDoc.getParameterizedAttributeValue(
    "AXTextMarkerRangeForUIElement",
    elem
  );
  is(stringForRange(macDoc, elemRange), expectedString);
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

addAccessibleTask(`<p>hello world goodbye</p>`, (browser, accDoc) => {
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
  testMarkerIntegrity(accDoc);
});

addAccessibleTask(
  `<p>hello world <a href="#">i love you</a> goodbye</p>`,
  (browser, accDoc) => {
    let macDoc = accDoc.nativeInterface.QueryInterface(
      Ci.nsIAccessibleMacInterface
    );

    let marker = macDoc.getAttributeValue("AXStartTextMarker");

    function testWordAndAdvance(left, right, elemText) {
      testWordAtMarker(macDoc, marker, left, right);
      testUIElementAtMarker(macDoc, marker, elemText);
      marker = macDoc.getParameterizedAttributeValue(
        "AXNextTextMarkerForTextMarker",
        marker
      );
    }

    testWordAndAdvance("hello", "hello", "hello world ");
    testWordAndAdvance("hello", "hello", "hello world ");
    testWordAndAdvance("hello", "hello", "hello world ");
    testWordAndAdvance("hello", "hello", "hello world ");
    testWordAndAdvance("hello", "hello", "hello world ");
    testWordAndAdvance("hello", " ", "hello world ");
    testWordAndAdvance(" ", "world", "hello world ");
    testWordAndAdvance("world", "world", "hello world ");
    testWordAndAdvance("world", "world", "hello world ");
    testWordAndAdvance("world", "world", "hello world ");
    testWordAndAdvance("world", "world", "hello world ");
    testWordAndAdvance("world", " ", "hello world ");
    testWordAndAdvance(" ", "i", "i love you");
    testWordAndAdvance("i", " ", "i love you");
    testWordAndAdvance(" ", "love", "i love you");
    testWordAndAdvance("love", "love", "i love you");
    testWordAndAdvance("love", "love", "i love you");
    testWordAndAdvance("love", "love", "i love you");
    testWordAndAdvance("love", " ", "i love you");
    testWordAndAdvance(" ", "you", "i love you");
    testWordAndAdvance("you", "you", "i love you");
    testWordAndAdvance("you", "you", "i love you");
    testWordAndAdvance("you", " ", "i love you");
    testWordAndAdvance(" ", "goodbye", " goodbye");
    testWordAndAdvance("goodbye", "goodbye", " goodbye");
    testWordAndAdvance("goodbye", "goodbye", " goodbye");
    testWordAndAdvance("goodbye", "goodbye", " goodbye");
    testWordAndAdvance("goodbye", "goodbye", " goodbye");
    testWordAndAdvance("goodbye", "goodbye", " goodbye");
    testWordAndAdvance("goodbye", "goodbye", " goodbye");
    testWordAndAdvance("goodbye", "", " goodbye");

    ok(!marker, "Iterated through all markers");
    testMarkerIntegrity(accDoc);
  }
);

addAccessibleTask(
  `<p>hello <a href=#">wor</a>ld goodbye</p>`,
  (browser, accDoc) => {
    let macDoc = accDoc.nativeInterface.QueryInterface(
      Ci.nsIAccessibleMacInterface
    );

    let marker = macDoc.getAttributeValue("AXStartTextMarker");

    function getMarkerAtIndex(index) {
      return macDoc.getParameterizedAttributeValue(
        "AXTextMarkerForIndex",
        index
      );
    }

    testUIElementAtMarker(macDoc, marker, "hello ");

    marker = getMarkerAtIndex(7);
    testUIElementAtMarker(macDoc, marker, "wor");

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

    marker = getMarkerAtIndex(14);
    testUIElementAtMarker(macDoc, marker, "ld goodbye");

    testMarkerIntegrity(accDoc);
  }
);

addAccessibleTask(
  `<ul><li>hello</li><li>world</li></ul>
   <ul style="list-style: none;"><li>goodbye</li><li>universe</li></ul>`,
  (browser, accDoc) => {
    testMarkerIntegrity(accDoc);
  }
);

addAccessibleTask(
  `<div id="a">hello</div><div id="b">world</div>`,
  (browser, accDoc) => {
    testMarkerIntegrity(accDoc);
  }
);

addAccessibleTask(
  `<p style="width: 1rem">Lorem ipsum</p>`,
  (browser, accDoc) => {
    testMarkerIntegrity(accDoc);
  }
);

addAccessibleTask(`<p>hello <input> world</p>`, (browser, accDoc) => {
  testMarkerIntegrity(accDoc);
});

// Tests consistency in text markers between:
// 1. "Linked list" forward navagation
// 2. Getting markers by index
// 3. "Linked list" reverse navagation
// For each iteration method check that the returned index is consistent
function testMarkerIntegrity(accDoc) {
  let macDoc = accDoc.nativeInterface.QueryInterface(
    Ci.nsIAccessibleMacInterface
  );

  let count = 0;

  // Iterate forward with "AXNextTextMarkerForTextMarker"
  let marker = macDoc.getAttributeValue("AXStartTextMarker");
  while (marker) {
    let index = macDoc.getParameterizedAttributeValue(
      "AXIndexForTextMarker",
      marker
    );
    is(
      index,
      count,
      `Correct index in "AXNextTextMarkerForTextMarker": ${count}`
    );

    marker = macDoc.getParameterizedAttributeValue(
      "AXNextTextMarkerForTextMarker",
      marker
    );
    count++;
  }

  // Use "AXTextMarkerForIndex" to retrieve all text markers
  for (let i = 0; i < count; i++) {
    marker = macDoc.getParameterizedAttributeValue("AXTextMarkerForIndex", i);
    let index = macDoc.getParameterizedAttributeValue(
      "AXIndexForTextMarker",
      marker
    );
    is(index, i, `Correct index in "AXTextMarkerForIndex": ${i}`);
  }

  // Iterate backward with "AXPreviousTextMarkerForTextMarker"
  marker = macDoc.getAttributeValue("AXEndTextMarker");
  while (marker) {
    count--;
    let index = macDoc.getParameterizedAttributeValue(
      "AXIndexForTextMarker",
      marker
    );
    is(
      index,
      count,
      `Correct index in "AXPreviousTextMarkerForTextMarker": ${count}`
    );
    marker = macDoc.getParameterizedAttributeValue(
      "AXPreviousTextMarkerForTextMarker",
      marker
    );
  }

  is(count, 0, "Iterated backward through all text markers");
}
