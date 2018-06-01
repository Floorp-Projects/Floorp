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

// Used for representing the expectation of a visible color swatch
const COLORSWATCH = true;
// format :
//  [
//    what key to press,
//    expected input box value after keypress,
//    selected suggestion index (-1 if popup is hidden),
//    number of suggestions in the popup (0 if popup is hidden),
//    expected post label corresponding with the input box value,
//    boolean representing if there should be a colour swatch visible,
//  ]
const testData = [
  ["v", "v", -1, 0, null, !COLORSWATCH],
  ["a", "va", -1, 0, null, !COLORSWATCH],
  ["r", "var", -1, 0, null, !COLORSWATCH],
  ["(", "var()", -1, 0, null, !COLORSWATCH],
  ["-", "var(--abc)", 0, 9, "inherit", !COLORSWATCH],
  ["VK_BACK_SPACE", "var(-)", -1, 0, null, !COLORSWATCH],
  ["-", "var(--abc)", 0, 9, "inherit", !COLORSWATCH],
  ["VK_DOWN", "var(--def)", 1, 9, "transparent", !COLORSWATCH],
  ["VK_DOWN", "var(--ghi)", 2, 9, "#00FF00", COLORSWATCH],
  ["VK_DOWN", "var(--jkl)", 3, 9, "rgb(255, 0, 0)", COLORSWATCH],
  ["VK_DOWN", "var(--mno)", 4, 9, "hsl(120, 60%, 70%)", COLORSWATCH],
  ["VK_DOWN", "var(--pqr)", 5, 9, "BlueViolet", COLORSWATCH],
  ["VK_DOWN", "var(--stu)", 6, 9, "15px", !COLORSWATCH],
  ["VK_DOWN", "var(--vwx)", 7, 9, "rgba(255, 0, 0, 0.4)", COLORSWATCH],
  ["VK_DOWN", "var(--yz)", 8, 9, "hsla(120, 60%, 70%, 0.3)", COLORSWATCH],
  ["VK_DOWN", "var(--abc)", 0, 9, "inherit", !COLORSWATCH],
  ["VK_DOWN", "var(--def)", 1, 9, "transparent", !COLORSWATCH],
  ["VK_DOWN", "var(--ghi)", 2, 9, "#00FF00", COLORSWATCH],
  ["VK_LEFT", "var(--ghi)", -1, 0, null, !COLORSWATCH],
];

const CSS_VARIABLES = [
  ["--abc", "inherit"],
  ["--def", "transparent"],
  ["--ghi", "#00FF00"],
  ["--jkl", "rgb(255, 0, 0)"],
  ["--mno", "hsl(120, 60%, 70%)"],
  ["--pqr", "BlueViolet"],
  ["--stu", "15px"],
  ["--vwx", "rgba(255, 0, 0, 0.4)"],
  ["--yz", "hsla(120, 60%, 70%, 0.3)"],
];

const mockGetCSSValuesForPropertyName = function(propertyName) {
  return [];
};

add_task(async function() {
  await addTab("data:text/html;charset=utf-8,inplace editor CSS variable autocomplete");
  const [host, win, doc] = await createHost();

  const xulDocument = win.top.document;
  const popup = new AutocompletePopup(xulDocument, { autoSelect: true });

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

const runAutocompletionTest = async function(editor) {
  info("Starting to test for css variable completion");
  editor._getCSSValuesForPropertyName = mockGetCSSValuesForPropertyName;

  for (const data of testData) {
    await testCompletion(data, editor);
  }

  EventUtils.synthesizeKey("VK_RETURN", {}, editor.input.defaultView);
};
