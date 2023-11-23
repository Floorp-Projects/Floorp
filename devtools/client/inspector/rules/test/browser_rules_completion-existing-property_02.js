/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that CSS property names and values are autocompleted and cycled
// correctly when editing existing properties in the rule view.

// format :
//  [
//    what key to press,
//    modifers,
//    expected input box value after keypress,
//    is the popup open,
//    is a suggestion selected in the popup,
//    expect ruleview-changed,
//  ]

const OPEN = true,
  SELECTED = true,
  CHANGE = true;
var testData = [
  ["b", {}, "beige", OPEN, SELECTED, CHANGE],
  ["l", {}, "black", OPEN, SELECTED, CHANGE],
  ["VK_DOWN", {}, "blanchedalmond", OPEN, SELECTED, CHANGE],
  ["VK_DOWN", {}, "blue", OPEN, SELECTED, CHANGE],
  ["VK_RIGHT", {}, "blue", !OPEN, !SELECTED, !CHANGE],
  [" ", {}, "blue aliceblue", OPEN, SELECTED, CHANGE],
  ["!", {}, "blue !important", !OPEN, !SELECTED, CHANGE],
  ["VK_BACK_SPACE", {}, "blue !", !OPEN, !SELECTED, CHANGE],
  ["VK_BACK_SPACE", {}, "blue ", !OPEN, !SELECTED, CHANGE],
  ["VK_BACK_SPACE", {}, "blue", !OPEN, !SELECTED, CHANGE],
  ["VK_TAB", { shiftKey: true }, "color", !OPEN, !SELECTED, CHANGE],
  ["VK_BACK_SPACE", {}, "", !OPEN, !SELECTED, !CHANGE],
  ["d", {}, "display", OPEN, SELECTED, !CHANGE],
  ["VK_TAB", {}, "blue", !OPEN, !SELECTED, CHANGE],
  ["n", {}, "none", !OPEN, !SELECTED, CHANGE],
  ["VK_RETURN", {}, null, !OPEN, !SELECTED, CHANGE],
];

const TEST_URI = "<h1 style='color: red'>Header</h1>";

add_task(async function () {
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

  const prop = getTextProperty(view, 0, { color: "red" });

  info("Focusing the css property editable value");
  let editor = await focusEditableField(view, prop.editor.valueSpan);

  info("Starting to test for css property completion");
  for (let i = 0; i < testData.length; i++) {
    // Re-define the editor at each iteration, because the focus may have moved
    // from property to value and back
    editor = inplaceEditor(view.styleDocument.activeElement);
    await testCompletion(testData[i], editor, view);
  }
}

async function testCompletion(
  [key, modifiers, completion, open, selected, change],
  editor,
  view
) {
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
    onDone =
      key !== "VK_RIGHT" && key !== "VK_BACK_SPACE"
        ? editor.once("after-suggest")
        : null;
  }

  info("Synthesizing key " + key + ", modifiers: " + Object.keys(modifiers));

  // Also listening for popup opened/closed events if needed.
  const popupEvent = open ? "popup-opened" : "popup-closed";
  const onPopupEvent =
    editor.popup.isOpen !== open ? once(editor.popup, popupEvent) : null;

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

  if (key === "VK_RETURN") {
    ok(!editor, "Enter does not move focus to next element");
    return;
  }

  if (!open) {
    ok(!(editor.popup && editor.popup.isOpen), "Popup is closed");
  } else {
    ok(editor.popup.isOpen, "Popup is open");
    is(editor.popup.selectedIndex !== -1, selected, "An item is selected");
  }
}
