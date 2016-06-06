/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_style_attr_test_runner.js */

"use strict";

// Test CSS autocompletion of the style attribute can be triggered when the
// caret is before a non-word character.

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
const TEST_DATA = [
  ["s", "s", 1, 1, false],
  ["t", "st", 2, 2, false],
  ["y", "sty", 3, 3, false],
  ["l", "styl", 4, 4, false],
  ["e", "style", 5, 5, false],
  ["=", "style=", 6, 6, false],
  ["\"", "style=\"", 7, 7, false],
  ["\"", "style=\"\"", 8, 8, false],
  ["VK_LEFT", "style=\"\"", 7, 7, false],
  ["c", "style=\"color\"", 8, 12, true],
  ["o", "style=\"color\"", 9, 12, true],
  ["VK_RIGHT", "style=\"color\"", 12, 12, false],
  [":", "style=\"color:aliceblue\"", 13, 22, true],
  ["b", "style=\"color:beige\"", 14, 18, true],
  ["VK_RIGHT", "style=\"color:beige\"", 18, 18, false],
  [";", "style=\"color:beige;\"", 19, 19, false],
  [";", "style=\"color:beige;;\"", 20, 20, false],
  ["VK_LEFT", "style=\"color:beige;;\"", 19, 19, false],
  ["p", "style=\"color:beige;padding;\"", 20, 26, true],
  ["VK_RIGHT", "style=\"color:beige;padding;\"", 26, 26, false],
  [":", "style=\"color:beige;padding:calc;\"", 27, 31, true],
  ["0", "style=\"color:beige;padding:0;\"", 28, 28, false],
  ["VK_RETURN", "style=\"color:beige;padding:0;\"",
   -1, -1, false]
];

add_task(function* () {
  let {inspector} = yield openInspectorForURL(TEST_URL);

  yield runStyleAttributeAutocompleteTests(inspector, TEST_DATA);
});
