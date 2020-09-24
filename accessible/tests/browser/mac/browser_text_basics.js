/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function testRangeAtMarker(macDoc, marker, attribute, expected, msg) {
  let range = macDoc.getParameterizedAttributeValue(attribute, marker);
  is(stringForRange(macDoc, range), expected, msg);
}

function testUIElement(macDoc, marker, msg, expectedRole, expectedValue) {
  let elem = macDoc.getParameterizedAttributeValue(
    "AXUIElementForTextMarker",
    marker
  );
  is(
    elem.getAttributeValue("AXRole"),
    expectedRole,
    `${msg}: element role matches`
  );
  is(elem.getAttributeValue("AXValue"), expectedValue, `${msg}: element value`);
  let elemRange = macDoc.getParameterizedAttributeValue(
    "AXTextMarkerRangeForUIElement",
    elem
  );
  is(
    stringForRange(macDoc, elemRange),
    expectedValue,
    `${msg}: element range matches element value`
  );
}

function testWords(macDoc, marker, msg, expectedLeft, expectedRight) {
  testRangeAtMarker(
    macDoc,
    marker,
    "AXLeftWordTextMarkerRangeForTextMarker",
    expectedLeft,
    `${msg}: left word matches`
  );

  testRangeAtMarker(
    macDoc,
    marker,
    "AXRightWordTextMarkerRangeForTextMarker",
    expectedRight,
    `${msg}: right word matches`
  );
}

function testLines(
  macDoc,
  marker,
  msg,
  expectedLine,
  expectedLeft,
  expectedRight
) {
  testRangeAtMarker(
    macDoc,
    marker,
    "AXLineTextMarkerRangeForTextMarker",
    expectedLine,
    `${msg}: line matches`
  );

  testRangeAtMarker(
    macDoc,
    marker,
    "AXLeftLineTextMarkerRangeForTextMarker",
    expectedLeft,
    `${msg}: left line matches`
  );

  testRangeAtMarker(
    macDoc,
    marker,
    "AXRightLineTextMarkerRangeForTextMarker",
    expectedRight,
    `${msg}: right line matches`
  );
}

// Tests consistency in text markers between:
// 1. "Linked list" forward navagation
// 2. Getting markers by index
// 3. "Linked list" reverse navagation
// For each iteration method check that the returned index is consistent
function testMarkerIntegrity(accDoc, expectedMarkerValues) {
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

    testWords(
      macDoc,
      marker,
      `At index ${count}`,
      ...expectedMarkerValues[count].words
    );
    testLines(
      macDoc,
      marker,
      `At index ${count}`,
      ...expectedMarkerValues[count].lines
    );
    testUIElement(
      macDoc,
      marker,
      `At index ${count}`,
      ...expectedMarkerValues[count].element
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

  ok(
    !macDoc.getParameterizedAttributeValue(
      "AXNextTextMarkerForTextMarker",
      marker
    ),
    "Iterated through all markers"
  );

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

addAccessibleTask("mac/doc_textmarker_test.html", async (browser, accDoc) => {
  const expectedMarkerValues = await SpecialPowers.spawn(
    browser,
    [],
    async () => {
      return content.wrappedJSObject.EXPECTED;
    }
  );

  testMarkerIntegrity(accDoc, expectedMarkerValues);
});
