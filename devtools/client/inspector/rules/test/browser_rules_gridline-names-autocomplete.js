/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that CSS property values are autocompleted and cycled
// correctly when editing an existing property in the rule view.

// format :
//  [
//    what key to press,
//    modifers,
//    expected input box value after keypress,
//    is the popup open,
//    is a suggestion selected in the popup,
//    expect ruleview-changed,
//  ]

const OPEN = true, SELECTED = true, CHANGE = true;
const changeTestData = [
  ["c", {}, "col1-start", OPEN, SELECTED, CHANGE],
  ["o", {}, "col1-start", OPEN, SELECTED, CHANGE],
  ["l", {}, "col1-start", OPEN, SELECTED, CHANGE],
  ["VK_DOWN", {}, "col2-start", OPEN, SELECTED, CHANGE],
  ["VK_RIGHT", {}, "col2-start", !OPEN, !SELECTED, !CHANGE],
];

// Creates a new CSS property value.
// Checks that grid-area autocompletes column and row names.
const newAreaTestData = [
  ["g", {}, "gap", OPEN, SELECTED, !CHANGE],
  ["VK_DOWN", {}, "grid", OPEN, SELECTED, !CHANGE],
  ["VK_DOWN", {}, "grid-area", OPEN, SELECTED, !CHANGE],
  ["VK_TAB", {}, "", !OPEN, !SELECTED, !CHANGE],
  "grid-line-names-updated",
  ["c", {}, "col1-start", OPEN, SELECTED, CHANGE],
  ["VK_BACK_SPACE", {}, "c", !OPEN, !SELECTED, CHANGE],
  ["VK_BACK_SPACE", {}, "", !OPEN, !SELECTED, CHANGE],
  ["r", {}, "row1-start", OPEN, SELECTED, CHANGE],
  ["r", {}, "rr", !OPEN, !SELECTED, CHANGE],
  ["VK_BACK_SPACE", {}, "r", !OPEN, !SELECTED, CHANGE],
  ["o", {}, "row1-start", OPEN, SELECTED, CHANGE],
  ["VK_RETURN", {}, "", !OPEN, !SELECTED, CHANGE],
];

// Creates a new CSS property value.
// Checks that grid-row only autocompletes row names.
const newRowTestData = [
  ["g", {}, "gap", OPEN, SELECTED, !CHANGE],
  ["r", {}, "grid", OPEN, SELECTED, !CHANGE],
  ["i", {}, "grid", OPEN, SELECTED, !CHANGE],
  ["d", {}, "grid", OPEN, SELECTED, !CHANGE],
  ["-", {}, "grid-area", OPEN, SELECTED, !CHANGE],
  ["r", {}, "grid-row", OPEN, SELECTED, !CHANGE],
  ["VK_RETURN", {}, "", !OPEN, !SELECTED, !CHANGE],
  "grid-line-names-updated",
  ["c", {}, "c", !OPEN, !SELECTED, CHANGE],
  ["VK_BACK_SPACE", {}, "", !OPEN, !SELECTED, CHANGE],
  ["r", {}, "row1-start", OPEN, SELECTED, CHANGE],
  ["VK_TAB", {}, "", !OPEN, !SELECTED, CHANGE],
];

const TEST_URL = URL_ROOT + "doc_grid_names.html";

add_task(async function() {
  await addTab(TEST_URL);
  const {toolbox, inspector, view} = await openRuleView();

  info("Test autocompletion changing a preexisting property");
  await runChangePropertyAutocompletionTest(toolbox, inspector, view, changeTestData);

  info("Test autocompletion creating a new property");
  await runNewPropertyAutocompletionTest(toolbox, inspector, view, newAreaTestData);

  info("Test autocompletion creating a new property");
  await runNewPropertyAutocompletionTest(toolbox, inspector, view, newRowTestData);
});

async function runNewPropertyAutocompletionTest(toolbox, inspector, view, testData) {
  info("Selecting the test node");
  await selectNode("#cell2", inspector);

  info("Focusing the css property editable field");
  const ruleEditor = getRuleViewRuleEditor(view, 0);
  const editor = await focusNewRuleViewProperty(ruleEditor);
  const gridLineNamesUpdated = inspector.once("grid-line-names-updated");

  info("Starting to test for css property completion");
  for (const data of testData) {
    if (data == "grid-line-names-updated") {
      await gridLineNamesUpdated;
      continue;
    }
    await testCompletion(data, editor, view);
  }
}

async function runChangePropertyAutocompletionTest(toolbox, inspector, view, testData) {
  info("Selecting the test node");
  await selectNode("#cell3", inspector);

  const ruleEditor = getRuleViewRuleEditor(view, 1).rule;
  const prop = ruleEditor.textProps[0];

  info("Focusing the css property editable value");
  const gridLineNamesUpdated = inspector.once("grid-line-names-updated");
  let editor = await focusEditableField(view, prop.editor.valueSpan);
  await gridLineNamesUpdated;

  info("Starting to test for css property completion");
  for (const data of testData) {
    // Re-define the editor at each iteration, because the focus may have moved
    // from property to value and back
    editor = inplaceEditor(view.styleDocument.activeElement);
    await testCompletion(data, editor, view);
  }
}

async function testCompletion([key, modifiers, completion, open, selected, change],
                         editor, view) {
  info("Pressing key " + key);
  info("Expecting " + completion);
  info("Is popup opened: " + open);
  info("Is item selected: " + selected);

  let onDone;
  if (change) {
    // If the key triggers a ruleview-changed, wait for that event, it will
    // always be the last to be triggered and tells us when the preview has
    // been done.
    onDone = view.once("ruleview-changed");
  } else {
    // Otherwise, expect an after-suggest event (except if the popup gets
    // closed).
    onDone = key !== "VK_RIGHT" && key !== "VK_BACK_SPACE"
             ? editor.once("after-suggest")
             : null;
  }

  // Also listening for popup opened/closed events if needed.
  const popupEvent = open ? "popup-opened" : "popup-closed";
  const onPopupEvent = editor.popup.isOpen !== open
    ? once(editor.popup, popupEvent)
    : null;

  info("Synthesizing key " + key + ", modifiers: " + Object.keys(modifiers));

  EventUtils.synthesizeKey(key, modifiers, view.styleWindow);

  // Flush the debounce for the preview text.
  view.debounce.flush();

  await onDone;
  await onPopupEvent;

  // The key might have been a TAB or shift-TAB, in which case the editor will
  // be a new one
  editor = inplaceEditor(view.styleDocument.activeElement);

  info("Checking the state");
  if (completion !== null) {
    is(editor.input.value, completion, "Correct value is autocompleted");
  }

  if (!open) {
    ok(!(editor.popup && editor.popup.isOpen), "Popup is closed");
  } else {
    ok(editor.popup.isOpen, "Popup is open");
    is(editor.popup.selectedIndex !== -1, selected, "An item is selected");
  }
}
