/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the behaviour of the CSS autocomplete for CSS value displayed on
// multiple lines. Expected behavior is:
// - UP/DOWN should navigate in the input and not increment/decrement numbers
// - typing a new value should still trigger the autocomplete
// - UP/DOWN when the autocomplete popup is displayed should cycle through
//   suggestions

const LONG_CSS_VALUE =
  "transparent linear-gradient(0deg, blue 0%, white 5%, red 10%, blue 15%, " +
  "white 20%, red 25%, blue 30%, white 35%, red 40%, blue 45%, white 50%, " +
  "red 55%, blue 60%, white 65%, red 70%, blue 75%, white 80%, red 85%, " +
  "blue 90%, white 95% ) repeat scroll 0% 0%";

const EXPECTED_CSS_VALUE = LONG_CSS_VALUE.replace("95%", "95%, red");

const TEST_URI =
  `<style>
    .title {
      background: ${LONG_CSS_VALUE};
    }
  </style>
  <h1 class=title>Header</h1>`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view} = await openRuleView();

  info("Selecting the test node");
  await selectNode("h1", inspector);

  info("Focusing the property editable field");
  const rule = getRuleViewRuleEditor(view, 1).rule;
  const prop = rule.textProps[0];

  // Calculate offsets to click in the middle of the first box quad.
  const rect = prop.editor.valueSpan.getBoundingClientRect();
  const firstQuad = prop.editor.valueSpan.getBoxQuads()[0];
  // For a multiline value, the first quad left edge is not aligned with the
  // bounding rect left edge. The offsets expected by focusEditableField are
  // relative to the bouding rectangle, so we need to translate the x-offset.
  const x = firstQuad.bounds.left - rect.left + firstQuad.bounds.width / 2;
  // The first quad top edge is aligned with the bounding top edge, no
  // translation needed here.
  const y = firstQuad.bounds.height / 2;

  info("Focusing the css property editable value");
  const editor = await focusEditableField(view, prop.editor.valueSpan, x, y);

  info("Moving the caret next to a number");
  let pos = editor.input.value.indexOf("0deg") + 1;
  editor.input.setSelectionRange(pos, pos);
  is(editor.input.value[editor.input.selectionStart - 1], "0",
    "Input caret is after a 0");

  info("Check that UP/DOWN navigates in the input, even when next to a number");
  EventUtils.synthesizeKey("VK_DOWN", {}, view.styleWindow);
  ok(editor.input.selectionStart !== pos, "Input caret moved");
  is(editor.input.value, LONG_CSS_VALUE, "Input value was not decremented.");

  info("Move the caret to the end of the gradient definition.");
  pos = editor.input.value.indexOf("95%") + 3;
  editor.input.setSelectionRange(pos, pos);

  info("Sending \", re\" to the editable field.");
  for (const key of ", re") {
    await synthesizeKeyForAutocomplete(key, editor, view.styleWindow);
  }

  info("Check the autocomplete can still be displayed.");
  ok(editor.popup && editor.popup.isOpen, "Autocomplete popup is displayed.");
  is(editor.popup.selectedIndex, 0,
    "Autocomplete has an item selected by default");

  let item = editor.popup.getItemAtIndex(editor.popup.selectedIndex);
  is(item.label, "rebeccapurple",
    "Check autocomplete displays expected value.");

  info("Check autocomplete suggestions can be cycled using UP/DOWN arrows.");

  await synthesizeKeyForAutocomplete("VK_DOWN", editor, view.styleWindow);
  ok(editor.popup.selectedIndex, 1, "Using DOWN cycles autocomplete values.");
  await synthesizeKeyForAutocomplete("VK_DOWN", editor, view.styleWindow);
  ok(editor.popup.selectedIndex, 2, "Using DOWN cycles autocomplete values.");
  await synthesizeKeyForAutocomplete("VK_UP", editor, view.styleWindow);
  is(editor.popup.selectedIndex, 1, "Using UP cycles autocomplete values.");
  item = editor.popup.getItemAtIndex(editor.popup.selectedIndex);
  is(item.label, "red", "Check autocomplete displays expected value.");

  info("Select the background-color suggestion with a mouse click.");
  let onRuleviewChanged = view.once("ruleview-changed");
  const onSuggest = editor.once("after-suggest");

  const node = editor.popup._list.childNodes[editor.popup.selectedIndex];
  EventUtils.synthesizeMouseAtCenter(node, {}, editor.popup._window);

  view.debounce.flush();
  await onSuggest;
  await onRuleviewChanged;

  is(editor.input.value, EXPECTED_CSS_VALUE,
    "Input value correctly autocompleted");

  info("Press ESCAPE to leave the input.");
  onRuleviewChanged = view.once("ruleview-changed");
  EventUtils.synthesizeKey("VK_ESCAPE", {}, view.styleWindow);
  await onRuleviewChanged;
});

/**
 * Send the provided key to the currently focused input of the provided window.
 * Wait for the editor to emit "after-suggest" to make sure the autocompletion
 * process is finished.
 *
 * @param {String} key
 *        The key to send to the input.
 * @param {InplaceEditor} editor
 *        The inplace editor which owns the focused input.
 * @param {Window} win
 *        Window in which the key event will be dispatched.
 */
async function synthesizeKeyForAutocomplete(key, editor, win) {
  const onSuggest = editor.once("after-suggest");
  EventUtils.synthesizeKey(key, {}, win);
  await onSuggest;
}
