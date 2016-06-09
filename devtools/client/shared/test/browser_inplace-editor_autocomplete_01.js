/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_inplace_editor.js */

"use strict";

const { InplaceEditor } = require("devtools/client/shared/inplace-editor");
const { AutocompletePopup } = require("devtools/client/shared/autocomplete-popup");
loadHelperScript("helper_inplace_editor.js");

// Test the inplace-editor autocomplete popup for CSS properties suggestions.
// Using a mocked list of CSS properties to avoid test failures linked to
// engine changes (new property, removed property, ...).

// format :
//  [
//    what key to press,
//    expected input box value after keypress,
//    selected suggestion index (-1 if popup is hidden),
//    number of suggestions in the popup (0 if popup is hidden),
//  ]
const testData = [
  ["b", "border", 1, 3],
  ["VK_DOWN", "box-sizing", 2, 3],
  ["VK_DOWN", "background", 0, 3],
  ["VK_DOWN", "border", 1, 3],
  ["VK_BACK_SPACE", "b", -1, 0],
  ["VK_BACK_SPACE", "", -1, 0],
  ["VK_DOWN", "background", 0, 6],
  ["VK_LEFT", "background", -1, 0],
];

const mockGetCSSPropertyList = function () {
  return [
    "background",
    "border",
    "box-sizing",
    "color",
    "display",
    "visibility",
  ];
};

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," +
    "inplace editor CSS property autocomplete");
  let [host, win, doc] = yield createHost();

  let xulDocument = win.top.document;
  let popup = new AutocompletePopup(xulDocument, { autoSelect: true });
  yield new Promise(resolve => {
    createInplaceEditorAndClick({
      start: runPropertyAutocompletionTest,
      contentType: InplaceEditor.CONTENT_TYPES.CSS_PROPERTY,
      done: resolve,
      popup: popup
    }, doc);
  });

  host.destroy();
  gBrowser.removeCurrentTab();
});

let runPropertyAutocompletionTest = Task.async(function* (editor) {
  info("Starting to test for css property completion");
  editor._getCSSPropertyList = mockGetCSSPropertyList;

  for (let data of testData) {
    yield testCompletion(data, editor);
  }

  EventUtils.synthesizeKey("VK_RETURN", {}, editor.input.defaultView);
});
