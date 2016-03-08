/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_style_attr_test_runner.js */

"use strict";

// Test CSS autocompletion of the style attributes stops after closing the
// attribute using a matching quote.

loadHelperScript("helper_style_attr_test_runner.js");

const TEST_URL = URL_ROOT + "doc_markup_edit.html";

// test data format :
//  [
//    what key to press,
//    expected input box value after keypress,
//    expected input.selectionStart,
//    expected input.selectionEnd,
//    is popup expected to be open ?
//  ]
const TEST_DATA_DOUBLE = [
  ["s", "s", 1, 1, false],
  ["t", "st", 2, 2, false],
  ["y", "sty", 3, 3, false],
  ["l", "styl", 4, 4, false],
  ["e", "style", 5, 5, false],
  ["=", "style=", 6, 6, false],
  ["\"", "style=\"", 7, 7, false],
  ["c", "style=\"color", 8, 12, true],
  ["VK_RIGHT", "style=\"color", 12, 12, false],
  [":", "style=\"color:aliceblue", 13, 22, true],
  ["b", "style=\"color:beige", 14, 18, true],
  ["VK_RIGHT", "style=\"color:beige", 18, 18, false],
  ["\"", "style=\"color:beige\"", 19, 19, false],
  [" ", "style=\"color:beige\" ", 20, 20, false],
  ["d", "style=\"color:beige\" d", 21, 21, false],
  ["a", "style=\"color:beige\" da", 22, 22, false],
  ["t", "style=\"color:beige\" dat", 23, 23, false],
  ["a", "style=\"color:beige\" data", 24, 24, false],
  ["VK_RETURN", "style=\"color:beige\"",
   -1, -1, false]
];

// Check that single quote attribute is also supported
const TEST_DATA_SINGLE = [
  ["s", "s", 1, 1, false],
  ["t", "st", 2, 2, false],
  ["y", "sty", 3, 3, false],
  ["l", "styl", 4, 4, false],
  ["e", "style", 5, 5, false],
  ["=", "style=", 6, 6, false],
  ["'", "style='", 7, 7, false],
  ["c", "style='color", 8, 12, true],
  ["VK_RIGHT", "style='color", 12, 12, false],
  [":", "style='color:aliceblue", 13, 22, true],
  ["b", "style='color:beige", 14, 18, true],
  ["VK_RIGHT", "style='color:beige", 18, 18, false],
  ["'", "style='color:beige'", 19, 19, false],
  [" ", "style='color:beige' ", 20, 20, false],
  ["d", "style='color:beige' d", 21, 21, false],
  ["a", "style='color:beige' da", 22, 22, false],
  ["t", "style='color:beige' dat", 23, 23, false],
  ["a", "style='color:beige' data", 24, 24, false],
  ["VK_RETURN", "style=\"color:beige\"",
   -1, -1, false]
];

// Check that autocompletion is still enabled after using url('1)
const TEST_DATA_INNER = [
  ["s", "s", 1, 1, false],
  ["t", "st", 2, 2, false],
  ["y", "sty", 3, 3, false],
  ["l", "styl", 4, 4, false],
  ["e", "style", 5, 5, false],
  ["=", "style=", 6, 6, false],
  ["\"", "style=\"", 7, 7, false],
  ["b", "style=\"border", 8, 13, true],
  ["a", "style=\"background", 9, 17, true],
  ["VK_RIGHT", "style=\"background", 17, 17, false],
  [":", "style=\"background:aliceblue", 18, 27, true],
  ["u", "style=\"background:unset", 19, 23, true],
  ["r", "style=\"background:url", 20, 21, false],
  ["l", "style=\"background:url", 21, 21, false],
  ["(", "style=\"background:url(", 22, 22, false],
  ["'", "style=\"background:url('", 23, 23, false],
  ["1", "style=\"background:url('1", 24, 24, false],
  ["'", "style=\"background:url('1'", 25, 25, false],
  [")", "style=\"background:url('1')", 26, 26, false],
  [";", "style=\"background:url('1');", 27, 27, false],
  [" ", "style=\"background:url('1'); ", 28, 28, false],
  ["c", "style=\"background:url('1'); color", 29, 33, true],
  ["VK_RIGHT", "style=\"background:url('1'); color", 33, 33, false],
  [":", "style=\"background:url('1'); color:aliceblue", 34, 43, true],
  ["b", "style=\"background:url('1'); color:beige", 35, 39, true],
  ["VK_RETURN", "style=\"background:url('1'); color:beige\"", -1, -1, false]
];

add_task(function* () {
  let {inspector} = yield openInspectorForURL(TEST_URL);

  yield runStyleAttributeAutocompleteTests(inspector, TEST_DATA_DOUBLE);
  yield runStyleAttributeAutocompleteTests(inspector, TEST_DATA_SINGLE);
  yield runStyleAttributeAutocompleteTests(inspector, TEST_DATA_INNER);
});
