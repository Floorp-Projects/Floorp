/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_inplace_editor.js */

"use strict";

const AutocompletePopup = require("devtools/client/shared/autocomplete-popup");
const { InplaceEditor } = require("devtools/client/shared/inplace-editor");
loadHelperScript("helper_inplace_editor.js");

// Test the inplace-editor closes parentheses automatically.

// format :
//  [
//    what key to press,
//    expected input box value after keypress,
//    selected suggestion index (-1 if popup is hidden),
//    number of suggestions in the popup (0 if popup is hidden),
//  ]
const testData = [
  ["u", "u", -1, 0],
  ["r", "ur", -1, 0],
  ["l", "url", -1, 0],
  ["(", "url()", -1, 0],
  ["v", "url(v)", -1, 0],
  ["a", "url(va)", -1, 0],
  ["r", "url(var)", -1, 0],
  ["(", "url(var())", -1, 0],
  ["-", "url(var(-))", -1, 0],
  ["-", "url(var(--))", -1, 0],
  ["a", "url(var(--a))", -1, 0],
  [")", "url(var(--a))", -1, 0],
  [")", "url(var(--a))", -1, 0],
];

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," +
    "inplace editor parentheses autoclose");
  const [host, win, doc] = await createHost();

  const xulDocument = win.top.document;
  const popup = new AutocompletePopup(xulDocument, { autoSelect: true });
  await new Promise(resolve => {
    createInplaceEditorAndClick({
      start: runPropertyAutocompletionTest,
      contentType: InplaceEditor.CONTENT_TYPES.CSS_VALUE,
      property: {
        name: "background-image"
      },
      cssVariables: new Map(),
      done: resolve,
      popup: popup
    }, doc);
  });

  popup.destroy();
  host.destroy();
  gBrowser.removeCurrentTab();
});

const runPropertyAutocompletionTest = async function(editor) {
  info("Starting to test for css property completion");

  // No need to test autocompletion here, return an empty array.
  editor._getCSSValuesForPropertyName = () => [];

  for (const data of testData) {
    await testCompletion(data, editor);
  }

  EventUtils.synthesizeKey("VK_RETURN", {}, editor.input.defaultView);
};
