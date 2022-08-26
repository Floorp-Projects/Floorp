/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that CSS property names are autocompleted and cycled correctly when
// creating a new property in the rule view.

const D_PROPERTY_ENABLED = SpecialPowers.getBoolPref(
  "layout.css.d-property.enabled"
);

// format :
//  [
//    what key to press,
//    expected input box value after keypress,
//    is the popup open,
//    is a suggestion selected in the popup,
//  ]
const OPEN = true,
  SELECTED = true;
var testData = [
  ["d", "display", OPEN, SELECTED],
  ["VK_DOWN", "dominant-baseline", OPEN, SELECTED],
  D_PROPERTY_ENABLED ? ["VK_DOWN", "d", OPEN, SELECTED] : [],
  ["VK_DOWN", "direction", OPEN, SELECTED],
  ["VK_DOWN", "display", OPEN, SELECTED],
  ["VK_UP", "direction", OPEN, SELECTED],
  D_PROPERTY_ENABLED ? ["VK_UP", "d", OPEN, SELECTED] : [],
  ["VK_UP", "dominant-baseline", OPEN, SELECTED],
  ["VK_UP", "display", OPEN, SELECTED],
  ["VK_BACK_SPACE", "d", !OPEN, !SELECTED],
  ["i", "display", OPEN, SELECTED],
  ["s", "display", !OPEN, !SELECTED],
  ["VK_BACK_SPACE", "dis", !OPEN, !SELECTED],
  ["VK_BACK_SPACE", "di", !OPEN, !SELECTED],
  ["VK_BACK_SPACE", "d", !OPEN, !SELECTED],
  ["VK_BACK_SPACE", "", !OPEN, !SELECTED],
  ["f", "font-size", OPEN, SELECTED],
  ["i", "filter", OPEN, SELECTED],
  ["VK_ESCAPE", null, !OPEN, !SELECTED],
];

const TEST_URI = "<h1 style='border: 1px solid red'>Header</h1>";

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { toolbox, inspector, view } = await openRuleView();

  info("Test autocompletion after 1st page load");
  await runAutocompletionTest(toolbox, inspector, view);

  info("Test autocompletion after page navigation");
  await reloadBrowser();
  await runAutocompletionTest(toolbox, inspector, view);
});

async function runAutocompletionTest(toolbox, inspector, view) {
  info("Selecting the test node");
  await selectNode("h1", inspector);

  info("Focusing the css property editable field");
  const ruleEditor = getRuleViewRuleEditor(view, 0);
  const editor = await focusNewRuleViewProperty(ruleEditor);

  info("Starting to test for css property completion");
  for (let i = 0; i < testData.length; i++) {
    if (!testData[i].length) {
      continue;
    }
    await testCompletion(testData[i], editor, view);
  }
}

async function testCompletion(
  [key, completion, open, isSelected],
  editor,
  view
) {
  info("Pressing key " + key);
  info("Expecting " + completion);
  info("Is popup opened: " + open);
  info("Is item selected: " + isSelected);

  let onSuggest;

  if (/(right|back_space|escape)/gi.test(key)) {
    info("Adding event listener for right|back_space|escape keys");
    onSuggest = once(editor.input, "keypress");
  } else {
    info("Waiting for after-suggest event on the editor");
    onSuggest = editor.once("after-suggest");
  }

  // Also listening for popup opened/closed events if needed.
  const popupEvent = open ? "popup-opened" : "popup-closed";
  const onPopupEvent =
    editor.popup.isOpen !== open ? once(editor.popup, popupEvent) : null;

  info("Synthesizing key " + key);
  EventUtils.synthesizeKey(key, {}, view.styleWindow);

  await onSuggest;
  await onPopupEvent;

  info("Checking the state");
  if (completion !== null) {
    is(editor.input.value, completion, "Correct value is autocompleted");
  }
  if (!open) {
    ok(!(editor.popup && editor.popup.isOpen), "Popup is closed");
  } else {
    ok(editor.popup.isOpen, "Popup is open");
    is(editor.popup.selectedIndex !== -1, isSelected, "An item is selected");
  }
}
