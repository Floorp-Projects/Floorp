/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that using UP/DOWN next to a number when editing a text node does not
// increment or decrement but simply navigates inside the editable field.

const TEST_URL = URL_ROOT + "doc_markup_edit.html";
const SELECTOR = ".node6";

add_task(async function() {
  const { inspector } = await openInspectorForURL(TEST_URL);

  info("Expanding all nodes");
  await inspector.markup.expandAll();
  await waitForMultipleChildrenUpdates(inspector);

  let nodeValue = await getFirstChildNodeValue(SELECTOR);
  let expectedValue = "line6";
  is(nodeValue, expectedValue, "The test node's text content is correct");

  info("Open editable field for .node6");
  const container = await focusNode(SELECTOR, inspector);
  const field = container.elt.querySelector("pre");
  field.focus();
  EventUtils.sendKey("return", inspector.panelWin);
  const editor = inplaceEditor(field);

  info("Initially, all the input content should be selected");
  checkSelectionPositions(editor, 0, expectedValue.length);

  info("Navigate using 'RIGHT': move the caret to the end");
  await sendKey("VK_RIGHT", {}, editor, inspector.panelWin);
  is(editor.input.value, expectedValue, "Value should not have changed");
  checkSelectionPositions(editor, expectedValue.length, expectedValue.length);

  info("Navigate using 'DOWN': no effect, already at the end");
  await sendKey("VK_DOWN", {}, editor, inspector.panelWin);
  is(editor.input.value, expectedValue, "Value should not have changed");
  checkSelectionPositions(editor, expectedValue.length, expectedValue.length);

  info("Navigate using 'UP': move to the start");
  await sendKey("VK_UP", {}, editor, inspector.panelWin);
  is(editor.input.value, expectedValue, "Value should not have changed");
  checkSelectionPositions(editor, 0, 0);

  info("Navigate using 'DOWN': move to the end");
  await sendKey("VK_DOWN", {}, editor, inspector.panelWin);
  is(editor.input.value, expectedValue, "Value should not have changed");
  checkSelectionPositions(editor, expectedValue.length, expectedValue.length);

  info("Type 'b' in the editable field");
  await sendKey("b", {}, editor, inspector.panelWin);
  expectedValue += "b";
  is(editor.input.value, expectedValue, "Value should be updated");

  info("Type 'a' in the editable field");
  await sendKey("a", {}, editor, inspector.panelWin);
  expectedValue += "a";
  is(editor.input.value, expectedValue, "Value should be updated");

  info("Create a new line using shift+RETURN");
  await sendKey("VK_RETURN", { shiftKey: true }, editor, inspector.panelWin);
  expectedValue += "\n";
  is(editor.input.value, expectedValue, "Value should have a new line");
  checkSelectionPositions(editor, expectedValue.length, expectedValue.length);

  info("Type '1' in the editable field");
  await sendKey("1", {}, editor, inspector.panelWin);
  expectedValue += "1";
  is(editor.input.value, expectedValue, "Value should be updated");
  checkSelectionPositions(editor, expectedValue.length, expectedValue.length);

  info("Navigate using 'UP': move back to the first line");
  await sendKey("VK_UP", {}, editor, inspector.panelWin);
  is(editor.input.value, expectedValue, "Value should not have changed");
  info("Caret should be back on the first line");
  checkSelectionPositions(editor, 1, 1);

  info("Commit the new value with RETURN, wait for the markupmutation event");
  const onMutated = inspector.once("markupmutation");
  await sendKey("VK_RETURN", {}, editor, inspector.panelWin);
  await onMutated;

  nodeValue = await getFirstChildNodeValue(SELECTOR);
  is(nodeValue, expectedValue, "The test node's text content is correct");
});

/**
 * Check that the editor selection is at the expected positions.
 */
function checkSelectionPositions(editor, expectedStart, expectedEnd) {
  is(
    editor.input.selectionStart,
    expectedStart,
    "Selection should start at " + expectedStart
  );
  is(
    editor.input.selectionEnd,
    expectedEnd,
    "Selection should end at " + expectedEnd
  );
}

/**
 * Send a key and expect to receive a keypress event on the editor's input.
 */
function sendKey(key, options, editor, win) {
  return new Promise(resolve => {
    info("Adding event listener for down|left|right|back_space|return keys");
    editor.input.addEventListener("keypress", function onKeypress() {
      if (editor.input) {
        editor.input.removeEventListener("keypress", onKeypress);
      }
      executeSoon(resolve);
    });

    EventUtils.synthesizeKey(key, options, win);
  });
}
