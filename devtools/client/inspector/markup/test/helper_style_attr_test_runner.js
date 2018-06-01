/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* import-globals-from head.js */
"use strict";

/**
 * Perform an style attribute edition and autocompletion test in the test
 * url, for #node14. Test data should be an
 * array of arrays structured as follows :
 *  [
 *    what key to press,
 *    expected input box value after keypress,
 *    expected input.selectionStart,
 *    expected input.selectionEnd,
 *    is popup expected to be open ?
 *  ]
 *
 * The test will start by adding a new attribute on the node, and then send each
 * key specified in the testData. The last item of this array should leave the
 * new attribute editor, either by committing or cancelling the edit.
 *
 * @param {InspectorPanel} inspector
 * @param {Array} testData
 *        Array of arrays representing the characters to type for the new
 *        attribute as well as the expected state at each step
 */
async function runStyleAttributeAutocompleteTests(inspector, testData) {
  info("Expand all markup nodes");
  await inspector.markup.expandAll();

  info("Select #node14");
  const container = await focusNode("#node14", inspector);

  info("Focus and open the new attribute inplace-editor");
  const attr = container.editor.newAttr;
  attr.focus();
  EventUtils.sendKey("return", inspector.panelWin);
  const editor = inplaceEditor(attr);

  for (let i = 0; i < testData.length; i++) {
    const data = testData[i];

    // Expect a markupmutation event at the last iteration since that's when the
    // attribute is actually created.
    const onMutation = i === testData.length - 1
                     ? inspector.once("markupmutation") : null;

    info(`Entering test data ${i}: ${data[0]}, expecting: [${data[1]}]`);
    await enterData(data, editor, inspector);

    info(`Test data ${i} entered. Checking state.`);
    await checkData(data, editor, inspector);

    await onMutation;
  }

  // Undoing the action will remove the new attribute, so make sure to wait for
  // the markupmutation event here again.
  const onMutation = inspector.once("markupmutation");
  while (inspector.markup.undo.canUndo()) {
    await undoChange(inspector);
  }
  await onMutation;
}

/**
 * Process a test data entry.
 * @param {Array} data
 *        test data - click or key - to enter
 * @param {InplaceEditor} editor
 * @param {InspectorPanel} inspector
 * @return {Promise} promise that will resolve when the test data has been
 *         applied
 */
function enterData(data, editor, inspector) {
  const key = data[0];

  if (/^click_[0-9]+$/.test(key)) {
    const suggestionIndex = parseInt(key.split("_")[1], 10);
    return clickOnSuggestion(suggestionIndex, editor);
  }

  return sendKey(key, editor, inspector);
}

function clickOnSuggestion(index, editor) {
  return new Promise(resolve => {
    info("Clicking on item " + index + " in the list");
    editor.once("after-suggest", () => executeSoon(resolve));
    editor.popup._list.childNodes[index].click();
  });
}

function sendKey(key, editor, inspector) {
  return new Promise(resolve => {
    if (/(down|left|right|back_space|return)/ig.test(key)) {
      info("Adding event listener for down|left|right|back_space|return keys");
      editor.input.addEventListener("keypress", function onKeypress() {
        if (editor.input) {
          editor.input.removeEventListener("keypress", onKeypress);
        }
        executeSoon(resolve);
      });
    } else {
      editor.once("after-suggest", () => executeSoon(resolve));
    }

    EventUtils.synthesizeKey(key, {}, inspector.panelWin);
  });
}

/**
 * Verify that the inplace editor is in the expected state for the provided
 * test data.
 */
async function checkData(data, editor, inspector) {
  const [, completion, selStart, selEnd, popupOpen] = data;

  if (selEnd != -1) {
    is(editor.input.value, completion, "Completed value is correct");
    is(editor.input.selectionStart, selStart, "Selection start position is correct");
    is(editor.input.selectionEnd, selEnd, "Selection end position is correct");
    is(editor.popup.isOpen, popupOpen, "Popup is " + (popupOpen ? "open" : "closed"));
  } else {
    const nodeFront = await getNodeFront("#node14", inspector);
    const container = getContainerForNodeFront(nodeFront, inspector);
    const attr = container.editor.attrElements.get("style").querySelector(".editable");
    is(attr.textContent, completion, "Correct value is persisted after pressing Enter");
  }
}
