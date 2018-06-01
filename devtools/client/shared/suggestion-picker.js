/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Allows to find the lowest ranking index in an index
 * of suggestions, by comparing it to another array of "most relevant" items
 * which has been sorted by relevance.
 *
 * Example usage:
 *  let sortedBrowsers = ["firefox", "safari", "edge", "chrome"];
 *  let myBrowsers = ["brave", "chrome", "firefox"];
 *  let bestBrowserIndex = findMostRelevantIndex(myBrowsers, sortedBrowsers);
 *  // returns "2", the index of firefox in myBrowsers array
 *
 * @param {Array} items
 *        Array of items to compare against sortedItems.
 * @param {Array} sortedItems
 *        Array of sorted items that suggestions are evaluated against. Array
 *        should be sorted by relevance, most relevant item first.
 * @return {Number}
 */
function findMostRelevantIndex(items, sortedItems) {
  if (!Array.isArray(items) || !Array.isArray(sortedItems)) {
    throw new Error("Please provide valid items and sortedItems arrays.");
  }

  // If the items array is empty, no valid index can be found.
  if (!items.length) {
    return -1;
  }

  // Return 0 if no match was found in the suggestion list.
  let bestIndex = 0;
  let lowestIndex = Infinity;
  items.forEach((item, i) => {
    const index = sortedItems.indexOf(item);
    if (index !== -1 && index <= lowestIndex) {
      lowestIndex = index;
      bestIndex = i;
    }
  });

  return bestIndex;
}

/**
 * Top 100 CSS property names sorted by relevance, most relevant first.
 *
 * List based on the one used by Chrome devtools :
 * https://code.google.com/p/chromium/codesearch#chromium/src/third_party/
 * WebKit/Source/devtools/front_end/sdk/CSSMetadata.js&q=CSSMetadata&
 * sq=package:chromium&type=cs&l=676
 *
 * The data is a mix of https://www.chromestatus.com/metrics/css and usage
 * metrics from popular sites collected via https://gist.github.com/NV/3751436
 *
 * @type {Array}
 */
const SORTED_CSS_PROPERTIES = [
  "width",
  "margin",
  "height",
  "padding",
  "font-size",
  "border",
  "display",
  "position",
  "text-align",
  "background",
  "background-color",
  "top",
  "font-weight",
  "color",
  "overflow",
  "font-family",
  "margin-top",
  "float",
  "opacity",
  "cursor",
  "left",
  "text-decoration",
  "background-image",
  "right",
  "line-height",
  "margin-left",
  "visibility",
  "margin-bottom",
  "padding-top",
  "z-index",
  "margin-right",
  "background-position",
  "vertical-align",
  "padding-left",
  "background-repeat",
  "border-bottom",
  "padding-right",
  "border-top",
  "padding-bottom",
  "clear",
  "white-space",
  "bottom",
  "border-color",
  "max-width",
  "border-radius",
  "border-right",
  "outline",
  "border-left",
  "font-style",
  "content",
  "min-width",
  "min-height",
  "box-sizing",
  "list-style",
  "border-width",
  "box-shadow",
  "font",
  "border-collapse",
  "text-shadow",
  "text-indent",
  "border-style",
  "max-height",
  "text-overflow",
  "background-size",
  "text-transform",
  "zoom",
  "list-style-type",
  "border-spacing",
  "word-wrap",
  "overflow-y",
  "transition",
  "border-top-color",
  "border-bottom-color",
  "border-top-right-radius",
  "letter-spacing",
  "border-top-left-radius",
  "border-bottom-left-radius",
  "border-bottom-right-radius",
  "overflow-x",
  "pointer-events",
  "border-right-color",
  "transform",
  "border-top-width",
  "border-bottom-width",
  "border-right-width",
  "direction",
  "animation",
  "border-left-color",
  "clip",
  "border-left-width",
  "table-layout",
  "src",
  "resize",
  "word-break",
  "background-clip",
  "transform-origin",
  "font-variant",
  "filter",
  "quotes",
  "word-spacing"
];

/**
 * Helper to find the most relevant CSS property name in a provided array.
 *
 * @param items {Array}
 *              Array of CSS property names.
 */
function findMostRelevantCssPropertyIndex(items) {
  return findMostRelevantIndex(items, SORTED_CSS_PROPERTIES);
}

exports.findMostRelevantIndex = findMostRelevantIndex;
exports.findMostRelevantCssPropertyIndex = findMostRelevantCssPropertyIndex;
