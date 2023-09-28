/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function testRangeAtMarker(macDoc, marker, attribute, expected, msg) {
  let range = macDoc.getParameterizedAttributeValue(attribute, marker);
  is(stringForRange(macDoc, range), expected, msg);
}

function testUIElement(
  macDoc,
  marker,
  msg,
  expectedRole,
  expectedValue,
  expectedRange
) {
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
    expectedRange,
    `${msg}: element range matches element value`
  );
}

function testStyleRun(macDoc, marker, msg, expectedStyleRun) {
  testRangeAtMarker(
    macDoc,
    marker,
    "AXStyleTextMarkerRangeForTextMarker",
    expectedStyleRun,
    `${msg}: style run matches`
  );
}

function testParagraph(macDoc, marker, msg, expectedParagraph) {
  testRangeAtMarker(
    macDoc,
    marker,
    "AXParagraphTextMarkerRangeForTextMarker",
    expectedParagraph,
    `${msg}: paragraph matches`
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

function* markerIterator(macDoc, reverse = false) {
  let m = macDoc.getAttributeValue(
    reverse ? "AXEndTextMarker" : "AXStartTextMarker"
  );
  let c = 0;
  while (m) {
    yield [m, c++];
    m = macDoc.getParameterizedAttributeValue(
      reverse
        ? "AXPreviousTextMarkerForTextMarker"
        : "AXNextTextMarkerForTextMarker",
      m
    );
  }
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

  // Iterate forward with "AXNextTextMarkerForTextMarker"
  let prevMarker;
  let count = 0;
  for (let [marker, index] of markerIterator(macDoc)) {
    count++;
    let markerIndex = macDoc.getParameterizedAttributeValue(
      "AXIndexForTextMarker",
      marker
    );
    is(
      markerIndex,
      index,
      `Correct index in "AXNextTextMarkerForTextMarker": ${index}`
    );
    if (prevMarker) {
      let range = macDoc.getParameterizedAttributeValue(
        "AXTextMarkerRangeForUnorderedTextMarkers",
        [prevMarker, marker]
      );
      is(
        macDoc.getParameterizedAttributeValue(
          "AXLengthForTextMarkerRange",
          range
        ),
        1,
        `[${index}] marker moved one character`
      );
    }
    prevMarker = marker;

    testWords(
      macDoc,
      marker,
      `At index ${index}`,
      ...expectedMarkerValues[index].words
    );
    testLines(
      macDoc,
      marker,
      `At index ${index}`,
      ...expectedMarkerValues[index].lines
    );
    testUIElement(
      macDoc,
      marker,
      `At index ${index}`,
      ...expectedMarkerValues[index].element
    );
    testParagraph(
      macDoc,
      marker,
      `At index ${index}`,
      expectedMarkerValues[index].paragraph
    );
    testStyleRun(
      macDoc,
      marker,
      `At index ${index}`,
      expectedMarkerValues[index].style
    );
  }

  is(expectedMarkerValues.length, count, `Correct marker count: ${count}`);

  // Use "AXTextMarkerForIndex" to retrieve all text markers
  for (let i = 0; i < count; i++) {
    let marker = macDoc.getParameterizedAttributeValue(
      "AXTextMarkerForIndex",
      i
    );
    let index = macDoc.getParameterizedAttributeValue(
      "AXIndexForTextMarker",
      marker
    );
    is(index, i, `Correct index in "AXTextMarkerForIndex": ${i}`);

    if (i == count - 1) {
      ok(
        !macDoc.getParameterizedAttributeValue(
          "AXNextTextMarkerForTextMarker",
          marker
        ),
        "Iterated through all markers"
      );
    }
  }

  count = expectedMarkerValues.length;

  // Iterate backward with "AXPreviousTextMarkerForTextMarker"
  for (let [marker] of markerIterator(macDoc, true)) {
    if (count <= 0) {
      ok(false, "Exceeding marker count");
      break;
    }
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
  }

  is(count, 0, "Iterated backward through all text markers");
}

// Run tests with old word segmenter
addAccessibleTask("mac/doc_textmarker_test.html", async (browser, accDoc) => {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["intl.icu4x.segmenter.enabled", false],
      ["layout.word_select.stop_at_punctuation", true], // This is default
    ],
  });

  const expectedValues = await SpecialPowers.spawn(browser, [], async () => {
    return content.wrappedJSObject.getExpected(false, true);
  });

  testMarkerIntegrity(accDoc, expectedValues);

  await SpecialPowers.popPrefEnv();
});

// new UAX#14 segmenter without stop_at_punctuation.
addAccessibleTask("mac/doc_textmarker_test.html", async (browser, accDoc) => {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["intl.icu4x.segmenter.enabled", true],
      ["layout.word_select.stop_at_punctuation", false],
    ],
  });

  const expectedValues = await SpecialPowers.spawn(browser, [], async () => {
    return content.wrappedJSObject.getExpected(true, false);
  });

  testMarkerIntegrity(accDoc, expectedValues);

  await SpecialPowers.popPrefEnv();
});

// new UAX#14 segmenter with stop_at_punctuation
addAccessibleTask("mac/doc_textmarker_test.html", async (browser, accDoc) => {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["intl.icu4x.segmenter.enabled", true],
      ["layout.word_select.stop_at_punctuation", true], // this is default
    ],
  });

  const expectedValues = await SpecialPowers.spawn(browser, [], async () => {
    return content.wrappedJSObject.getExpected(true, true);
  });

  testMarkerIntegrity(accDoc, expectedValues);

  await SpecialPowers.popPrefEnv();
});

// Test text marker lesser-than operator
addAccessibleTask(
  `<p id="p">hello <a id="a" href="#">goodbye</a> world</p>`,
  async (browser, accDoc) => {
    let macDoc = accDoc.nativeInterface.QueryInterface(
      Ci.nsIAccessibleMacInterface
    );

    let start = macDoc.getParameterizedAttributeValue(
      "AXTextMarkerForIndex",
      1
    );
    let end = macDoc.getParameterizedAttributeValue("AXTextMarkerForIndex", 10);

    let range = macDoc.getParameterizedAttributeValue(
      "AXTextMarkerRangeForUnorderedTextMarkers",
      [end, start]
    );
    is(stringForRange(macDoc, range), "ello good");
  }
);

addAccessibleTask(
  `<input id="input" value=""><a href="#">goodbye</a>`,
  async (browser, accDoc) => {
    let macDoc = accDoc.nativeInterface.QueryInterface(
      Ci.nsIAccessibleMacInterface
    );

    let input = getNativeInterface(accDoc, "input");

    let range = macDoc.getParameterizedAttributeValue(
      "AXTextMarkerRangeForUIElement",
      input
    );

    is(stringForRange(macDoc, range), "", "string value is correct");
  }
);

addAccessibleTask(
  `<div role="listbox" id="box">
    <input type="radio" name="test" role="option" title="First item"/>
    <input type="radio" name="test" role="option" title="Second item"/>
  </div>`,
  async (browser, accDoc) => {
    let box = getNativeInterface(accDoc, "box");
    const children = box.getAttributeValue("AXChildren");
    is(children.length, 2, "Listbox contains two items");
    is(children[0].getAttributeValue("AXValue"), "First item");
    is(children[1].getAttributeValue("AXValue"), "Second item");
  }
);

addAccessibleTask(
  `<div id="t">
    A link <b>should</b> explain <em>clearly</em> what information the <i>reader</i> will get by clicking on that link.
  </div>`,
  async (browser, accDoc) => {
    let t = getNativeInterface(accDoc, "t");
    const children = t.getAttributeValue("AXChildren");
    const expectedTitles = [
      "A link ",
      "should",
      " explain ",
      "clearly",
      " what information the ",
      "reader",
      " will get by clicking on that link. ",
    ];
    is(children.length, 7, "container has seven children");
    children.forEach((child, index) => {
      is(child.getAttributeValue("AXValue"), expectedTitles[index]);
    });
  }
);

addAccessibleTask(
  `<a href="#">link</a> <input id="input" value="hello">`,
  async (browser, accDoc) => {
    let macDoc = accDoc.nativeInterface.QueryInterface(
      Ci.nsIAccessibleMacInterface
    );

    let input = getNativeInterface(accDoc, "input");
    let range = macDoc.getParameterizedAttributeValue(
      "AXTextMarkerRangeForUIElement",
      input
    );

    let firstMarkerInInput = macDoc.getParameterizedAttributeValue(
      "AXStartTextMarkerForTextMarkerRange",
      range
    );

    let leftWordRange = macDoc.getParameterizedAttributeValue(
      "AXLeftWordTextMarkerRangeForTextMarker",
      firstMarkerInInput
    );
    let str = macDoc.getParameterizedAttributeValue(
      "AXStringForTextMarkerRange",
      leftWordRange
    );
    is(str, "hello", "Left word at start of input should be right word");
  }
);

addAccessibleTask(`<p id="p">hello world</p>`, async (browser, accDoc) => {
  let macDoc = accDoc.nativeInterface.QueryInterface(
    Ci.nsIAccessibleMacInterface
  );

  let p = getNativeInterface(accDoc, "p");
  let range = macDoc.getParameterizedAttributeValue(
    "AXTextMarkerRangeForUIElement",
    p
  );

  let bounds = macDoc.getParameterizedAttributeValue(
    "AXBoundsForTextMarkerRange",
    range
  );

  ok(bounds.origin && bounds.size, "Returned valid bounds");
});
