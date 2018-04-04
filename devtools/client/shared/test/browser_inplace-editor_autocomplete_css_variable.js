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
//    expected post label corresponding with the input box value,
//  ]
const testData = [
  ["v", "v", -1, 0, null],
  ["a", "va", -1, 0, null],
  ["r", "var", -1, 0, null],
  ["(", "var(", -1, 0, null],
  ["-", "var(--abc", 0, 4, "blue"],
  ["VK_BACK_SPACE", "var(-", -1, 0, null],
  ["-", "var(--abc", 0, 4, "blue"],
  ["VK_DOWN", "var(--def", 1, 4, "red"],
  ["VK_DOWN", "var(--ghi", 2, 4, "green"],
  ["VK_DOWN", "var(--jkl", 3, 4, "yellow"],
  ["VK_DOWN", "var(--abc", 0, 4, "blue"],
  ["VK_DOWN", "var(--def", 1, 4, "red"],
  ["VK_LEFT", "var(--def", -1, 0, null],
];

const CSS_VARIABLES = [
  ["--abc", "blue"],
  ["--def", "red"],
  ["--ghi", "green"],
  ["--jkl", "yellow"]
];

const mockGetCSSValuesForPropertyName = function(propertyName) {
  return [];
};

add_task(async function() {
  await addTab("data:text/html;charset=utf-8,inplace editor CSS variable autocomplete");
  let [host, win, doc] = await createHost();

  let xulDocument = win.top.document;
  let popup = new AutocompletePopup(xulDocument, { autoSelect: true });

  await new Promise(resolve => {
    createInplaceEditorAndClick({
      start: runAutocompletionTest,
      contentType: InplaceEditor.CONTENT_TYPES.CSS_VALUE,
      property: {
        name: "color"
      },
      cssVariables: new Map(CSS_VARIABLES),
      done: resolve,
      popup: popup
    }, doc);
  });

  popup.destroy();
  host.destroy();
  gBrowser.removeCurrentTab();
});

let runAutocompletionTest = async function(editor) {
  info("Starting to test for css variable completion");
  editor._getCSSValuesForPropertyName = mockGetCSSValuesForPropertyName;

  for (let data of testData) {
    await testCompletion(data, editor);
  }

  EventUtils.synthesizeKey("VK_RETURN", {}, editor.input.defaultView);
};
