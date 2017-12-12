/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_inplace_editor.js */

"use strict";

const AutocompletePopup = require("devtools/client/shared/autocomplete-popup");
const { InplaceEditor } = require("devtools/client/shared/inplace-editor");
loadHelperScript("helper_inplace_editor.js");

// Test the inplace-editor autocomplete popup for variable suggestions.
// Using a mocked list of CSS variables to avoid test failures linked to
// engine changes (new property, removed property, ...).
// Also using a mocked list of CSS properties to avoid autocompletion when
// typing in "var"

// format :
//  [
//    what key to press,
//    expected input box value after keypress,
//    selected suggestion index (-1 if popup is hidden),
//    number of suggestions in the popup (0 if popup is hidden),
//  ]
const testData = [
  ["v", "v", -1, 0],
  ["a", "va", -1, 0],
  ["r", "var", -1, 0],
  ["(", "var(", -1, 0],
  ["-", "var(--abc", 0, 2],
  ["VK_BACK_SPACE", "var(-", -1, 0],
  ["-", "var(--abc", 0, 2],
  ["VK_DOWN", "var(--def", 1, 2],
  ["VK_DOWN", "var(--abc", 0, 2],
  ["VK_LEFT", "var(--abc", -1, 0],
];

const mockGetCSSValuesForPropertyName = function (propertyName) {
  return [];
};

const mockGetCSSVariableNames = function () {
  return [
    "--abc",
    "--def",
  ]
};

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," +
    "inplace editor CSS variable autocomplete");
  let [host, win, doc] = yield createHost();

  let xulDocument = win.top.document;
  let popup = new AutocompletePopup(xulDocument, { autoSelect: true });

  yield new Promise(resolve => {
    createInplaceEditorAndClick({
      start: runAutocompletionTest,
      contentType: InplaceEditor.CONTENT_TYPES.CSS_VALUE,
      property: {
        name: "color"
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
  info("Starting to test for css variable completion");
  editor._getCSSValuesForPropertyName = mockGetCSSValuesForPropertyName;
  editor._getCSSVariableNames = mockGetCSSVariableNames;

  for (let data of testData) {
    yield testCompletion(data, editor);
  }

  EventUtils.synthesizeKey("VK_RETURN", {}, editor.input.defaultView);
});
