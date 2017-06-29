/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that CSS property names and values are autocompleted and cycled
// correctly when editing new properties in the rule view.

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
const testData = [
  ["d", {}, "display", OPEN, SELECTED, !CHANGE],
  ["VK_TAB", {}, "", OPEN, !SELECTED, CHANGE],
  ["VK_DOWN", {}, "block", OPEN, SELECTED, CHANGE],
  ["n", {}, "none", !OPEN, !SELECTED, CHANGE],
  ["VK_TAB", {shiftKey: true}, "display", !OPEN, !SELECTED, CHANGE],
  ["VK_BACK_SPACE", {}, "", !OPEN, !SELECTED, !CHANGE],
  ["o", {}, "overflow", OPEN, SELECTED, !CHANGE],
  ["u", {}, "outline", OPEN, SELECTED, !CHANGE],
  ["VK_DOWN", {}, "outline-color", OPEN, SELECTED, !CHANGE],
  ["VK_TAB", {}, "none", !OPEN, !SELECTED, CHANGE],
  ["r", {}, "rebeccapurple", OPEN, SELECTED, CHANGE],
  ["VK_DOWN", {}, "red", OPEN, SELECTED, CHANGE],
  ["VK_DOWN", {}, "rgb", OPEN, SELECTED, CHANGE],
  ["VK_DOWN", {}, "rgba", OPEN, SELECTED, CHANGE],
  ["VK_DOWN", {}, "rosybrown", OPEN, SELECTED, CHANGE],
  ["VK_DOWN", {}, "royalblue", OPEN, SELECTED, CHANGE],
  ["VK_RIGHT", {}, "royalblue", !OPEN, !SELECTED, !CHANGE],
  [" ", {}, "royalblue aliceblue", OPEN, SELECTED, CHANGE],
  ["!", {}, "royalblue !important", !OPEN, !SELECTED, CHANGE],
  ["VK_ESCAPE", {}, null, !OPEN, !SELECTED, CHANGE]
];

const TEST_URI = `
  <style type="text/css">
    h1 {
      border: 1px solid red;
    }
  </style>
  <h1>Test element</h1>
`;

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {toolbox, inspector, view, testActor} = yield openRuleView();

  info("Test autocompletion after 1st page load");
  yield runAutocompletionTest(toolbox, inspector, view);

  info("Test autocompletion after page navigation");
  yield reloadPage(inspector, testActor);
  yield runAutocompletionTest(toolbox, inspector, view);
});

function* runAutocompletionTest(toolbox, inspector, view) {
  info("Selecting the test node");
  yield selectNode("h1", inspector);

  info("Focusing a new css property editable property");
  let ruleEditor = getRuleViewRuleEditor(view, 1);
  let editor = yield focusNewRuleViewProperty(ruleEditor);

  info("Starting to test for css property completion");
  for (let i = 0; i < testData.length; i++) {
    // Re-define the editor at each iteration, because the focus may have moved
    // from property to value and back
    editor = inplaceEditor(view.styleDocument.activeElement);
    yield testCompletion(testData[i], editor, view);
  }
}

function* testCompletion([key, modifiers, completion, open, selected, change],
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
  let popupEvent = open ? "popup-opened" : "popup-closed";
  let onPopupEvent = editor.popup.isOpen !== open ? once(editor.popup, popupEvent) : null;

  info("Synthesizing key " + key + ", modifiers: " + Object.keys(modifiers));
  EventUtils.synthesizeKey(key, modifiers, view.styleWindow);

  // Flush the debounce for the preview text.
  view.debounce.flush();

  yield onDone;
  yield onPopupEvent;

  info("Checking the state");
  if (completion !== null) {
    // The key might have been a TAB or shift-TAB, in which case the editor will
    // be a new one
    editor = inplaceEditor(view.styleDocument.activeElement);
    is(editor.input.value, completion, "Correct value is autocompleted");
  }
  if (!open) {
    ok(!(editor.popup && editor.popup.isOpen), "Popup is closed");
  } else {
    ok(editor.popup.isOpen, "Popup is open");
    is(editor.popup.selectedIndex !== -1, selected, "An item is selected");
  }
}
