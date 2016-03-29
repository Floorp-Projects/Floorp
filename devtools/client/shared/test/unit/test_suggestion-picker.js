/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test the suggestion-picker helper methods.
 */
const {require} = Components.utils.import("resource://devtools/shared/Loader.jsm", {});
const {
  findMostRelevantIndex,
  findMostRelevantCssPropertyIndex
} = require("devtools/client/shared/suggestion-picker");

/**
 * Run all tests defined below.
 */
function run_test() {
  ensureMostRelevantIndexProvidedByHelperFunction();
  ensureMostRelevantIndexProvidedByClassMethod();
  ensureErrorThrownWithInvalidArguments();
}

/**
 * Generic test data.
 */
const TEST_DATA = [
  {
    // Match in sortedItems array.
    items: ["chrome", "edge", "firefox"],
    sortedItems: ["firefox", "chrome", "edge"],
    expectedIndex: 2
  }, {
    // No match in sortedItems array.
    items: ["apple", "oranges", "banana"],
    sortedItems: ["kiwi", "pear", "peach"],
    expectedIndex: 0
  }, {
    // Empty items array.
    items: [],
    sortedItems: ["empty", "arrays", "can't", "have", "relevant", "indexes"],
    expectedIndex: -1
  }
];

function ensureMostRelevantIndexProvidedByHelperFunction() {
  do_print("Running ensureMostRelevantIndexProvidedByHelperFunction()");

  for (let testData of TEST_DATA) {
    let { items, sortedItems, expectedIndex } = testData;
    let mostRelevantIndex = findMostRelevantIndex(items, sortedItems);
    strictEqual(mostRelevantIndex, expectedIndex);
  }
}

/**
 * CSS properties test data.
 */
const CSS_TEST_DATA = [
  {
    items: [
      "backface-visibility",
      "background",
      "background-attachment",
      "background-blend-mode",
      "background-clip",
      "background-color",
      "background-image",
      "background-origin",
      "background-position",
      "background-repeat"
    ],
    expectedIndex: 1
  },
  {
    items: [
      "caption-side",
      "clear",
      "clip",
      "clip-path",
      "clip-rule",
      "color",
      "color-interpolation",
      "color-interpolation-filters",
      "content",
      "counter-increment"
    ],
    expectedIndex: 5
  },
  {
    items: [
      "direction",
      "display",
      "dominant-baseline"
    ],
    expectedIndex: 1
  },
  {
    items: [
      "object-fit",
      "object-position",
      "offset-block-end",
      "offset-block-start",
      "offset-inline-end",
      "offset-inline-start",
      "opacity",
      "order",
      "orphans",
      "outline"
    ],
    expectedIndex: 6
  },
  {
    items: [
      "white-space",
      "widows",
      "width",
      "will-change",
      "word-break",
      "word-spacing",
      "word-wrap",
      "writing-mode"
    ],
    expectedIndex: 2
  }
];

function ensureMostRelevantIndexProvidedByClassMethod() {
  do_print("Running ensureMostRelevantIndexProvidedByClassMethod()");

  for (let testData of CSS_TEST_DATA) {
    let { items, expectedIndex } = testData;
    let mostRelevantIndex = findMostRelevantCssPropertyIndex(items);
    strictEqual(mostRelevantIndex, expectedIndex);
  }
}

function ensureErrorThrownWithInvalidArguments() {
  do_print("Running ensureErrorThrownWithInvalidTypeArgument()");

  let expectedError = "Please provide valid items and sortedItems arrays.";
  // No arguments passed.
  throws(() => findMostRelevantIndex(), expectedError);
  // Invalid arguments passed.
  throws(() => findMostRelevantIndex([]), expectedError);
  throws(() => findMostRelevantIndex(null, []), expectedError);
  throws(() => findMostRelevantIndex([], "string"), expectedError);
  throws(() => findMostRelevantIndex("string", []), expectedError);
}
