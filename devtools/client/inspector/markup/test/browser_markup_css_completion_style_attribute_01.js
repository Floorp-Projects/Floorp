/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_style_attr_test_runner.js */

"use strict";

// Test CSS state is correctly determined and the corresponding suggestions are
// displayed. i.e. CSS property suggestions are shown when cursor is like:
// ```style="di|"``` where | is the cursor; And CSS value suggestion is
// displayed when the cursor is like: ```style="display:n|"``` properly. No
// suggestions should ever appear when the attribute is not a style attribute.
// The correctness and cycling of the suggestions is covered in the ruleview
// tests.

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
  ["d", "style=\"direction", 8, 16, true],
  ["VK_DOWN", "style=\"display", 8, 14, true],
  ["VK_TAB", "style=\"display", 14, 14, true],
  ["VK_TAB", "style=\"dominant-baseline", 24, 24, true],
  ["VK_TAB", "style=\"direction", 16, 16, true],
  ["click_1", "style=\"display", 14, 14, false],
  [":", "style=\"display:-moz-box", 15, 23, true],
  ["n", "style=\"display:none", 16, 19, false],
  ["VK_BACK_SPACE", "style=\"display:n", 16, 16, false],
  ["VK_BACK_SPACE", "style=\"display:", 15, 15, false],
  [" ", "style=\"display: -moz-box", 16, 24, true],
  [" ", "style=\"display:  -moz-box", 17, 25, true],
  ["i", "style=\"display:  inherit", 18, 24, true],
  ["VK_RIGHT", "style=\"display:  inherit", 24, 24, false],
  [";", "style=\"display:  inherit;", 25, 25, false],
  [" ", "style=\"display:  inherit; ", 26, 26, false],
  [" ", "style=\"display:  inherit;  ", 27, 27, false],
  ["VK_LEFT", "style=\"display:  inherit;  ", 26, 26, false],
  ["c", "style=\"display:  inherit; caption-side ", 27, 38, true],
  ["o", "style=\"display:  inherit; color ", 28, 31, true],
  ["VK_RIGHT", "style=\"display:  inherit; color ", 31, 31, false],
  [" ", "style=\"display:  inherit; color  ", 32, 32, false],
  ["c", "style=\"display:  inherit; color c ", 33, 33, false],
  ["VK_BACK_SPACE", "style=\"display:  inherit; color  ", 32, 32, false],
  [":", "style=\"display:  inherit; color :aliceblue ", 33, 42, true],
  ["c", "style=\"display:  inherit; color :cadetblue ", 34, 42, true],
  ["VK_DOWN", "style=\"display:  inherit; color :chartreuse ", 34, 43, true],
  ["VK_RIGHT", "style=\"display:  inherit; color :chartreuse ", 43, 43, false],
  [" ", "style=\"display:  inherit; color :chartreuse !important; ",
   44, 55, true],
  ["!", "style=\"display:  inherit; color :chartreuse !important; ",
   45, 55, false],
  ["VK_RIGHT", "style=\"display:  inherit; color :chartreuse !important; ",
   55, 55, false],
  ["VK_RETURN", "style=\"display:  inherit; color :chartreuse !important;\"",
   -1, -1, false]
];

add_task(function*() {
  info("Opening the inspector on the test URL");
  let {inspector} = yield openInspectorForURL(TEST_URL);

  yield runStyleAttributeAutocompleteTests(inspector, TEST_DATA);
});
