/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_inplace_editor.js */

"use strict";

const { InplaceEditor } = require("devtools/client/shared/inplace-editor");
const { AutocompletePopup } = require("devtools/client/shared/autocomplete-popup");
loadHelperScript("helper_inplace_editor.js");

// Test the inplace-editor autocomplete popup for CSS values suggestions.
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
  ["b", "block", -1, 0],
  ["VK_BACK_SPACE", "b", -1, 0],
  ["VK_BACK_SPACE", "", -1, 0],
  ["i", "inline", 0, 2],
  ["VK_DOWN", "inline-block", 1, 2],
  ["VK_DOWN", "inline", 0, 2],
  ["VK_LEFT", "inline", -1, 0],
];

const mockGetCSSValuesForPropertyName = function (propertyName) {
  let values = {
    "display": [
      "block",
      "flex",
      "inline",
      "inline-block",
      "none",
    ]
  };
  return values[propertyName] || [];
};

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," +
    "inplace editor CSS value autocomplete");
  let [host, win, doc] = yield createHost();

  let xulDocument = win.top.document;
  let popup = new AutocompletePopup({ doc: xulDocument }, { autoSelect: true });

  yield new Promise(resolve => {
    createInplaceEditorAndClick({
      start: runAutocompletionTest,
      contentType: InplaceEditor.CONTENT_TYPES.CSS_VALUE,
      property: {
        name: "display"
      },
      done: resolve,
      popup: popup
    }, doc);
  });

  popup.destroy();
  host.destroy();
  gBrowser.removeCurrentTab();
});

let runAutocompletionTest = Task.async(function* (editor) {
  info("Starting to test for css property completion");
  editor._getCSSValuesForPropertyName = mockGetCSSValuesForPropertyName;

  for (let data of testData) {
    yield testCompletion(data, editor);
  }

  EventUtils.synthesizeKey("VK_RETURN", {}, editor.input.defaultView);
});
