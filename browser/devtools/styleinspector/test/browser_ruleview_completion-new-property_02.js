/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that CSS property names and values are autocompleted and cycled correctly
// when editing new properties in the rule view

// format :
//  [
//    what key to press,
//    modifers,
//    expected input box value after keypress,
//    selectedIndex of the popup,
//    total items in the popup
//  ]
let testData = [
  ["a", {accelKey: true, ctrlKey: true}, "", -1, 0],
  ["d", {}, "direction", 0, 3],
  ["VK_DOWN", {}, "display", 1, 3],
  ["VK_TAB", {}, "", -1, 10],
  ["VK_DOWN", {}, "-moz-box", 0, 10],
  ["n", {}, "none", -1, 0],
  ["VK_TAB", {shiftKey: true}, "display", -1, 0],
  ["VK_BACK_SPACE", {}, "", -1, 0],
  ["c", {}, "caption-side", 0, 10],
  ["o", {}, "color", 0, 7],
  ["VK_TAB", {}, "none", -1, 0],
  ["r", {}, "rebeccapurple", 0, 6],
  ["VK_DOWN", {}, "red", 1, 6],
  ["VK_DOWN", {}, "rgb", 2, 6],
  ["VK_DOWN", {}, "rgba", 3, 6],
  ["VK_DOWN", {}, "rosybrown", 4, 6],
  ["VK_DOWN", {}, "royalblue", 5, 6],
  ["VK_RIGHT", {}, "royalblue", -1, 0],
  [" ", {}, "royalblue !important", 0, 10],
  ["!", {}, "royalblue !important", 0, 0],
  ["VK_ESCAPE", {}, null, -1, 0]
];

let TEST_URL = "data:text/html;charset=utf-8,<style>h1{border: 1px solid red}</style>" +
  "<h1>Test element</h1>";

add_task(function*() {
  yield addTab(TEST_URL);
  let {toolbox, inspector, view} = yield openRuleView();

  info("Selecting the test node");
  yield selectNode("h1", inspector);

  info("Focusing a new css property editable property");
  let brace = view.styleDocument.querySelectorAll(".ruleview-ruleclose")[1];
  let editor = yield focusEditableField(view, brace);

  info("Starting to test for css property completion");
  for (let i = 0; i < testData.length; i ++) {
    // Re-define the editor at each iteration, because the focus may have moved
    // from property to value and back
    editor = inplaceEditor(view.styleDocument.activeElement);
    yield testCompletion(testData[i], editor, view);
  }
});

function* testCompletion([key, modifiers, completion, index, total], editor, view) {
  info("Pressing key " + key);
  info("Expecting " + completion + ", " + index + ", " + total);

  let onKeyPress;

  if (/tab/ig.test(key)) {
    info("Waiting for the new property or value editor to get focused");
    let brace = view.styleDocument.querySelectorAll(".ruleview-ruleclose")[1];
    onKeyPress = once(brace.parentNode, "focus", true);
  } else if (/(right|back_space|escape|return)/ig.test(key) ||
             (modifiers.accelKey || modifiers.ctrlKey)) {
    info("Adding event listener for right|escape|back_space|return keys");
    onKeyPress = once(editor.input, "keypress");
  } else {
    info("Waiting for after-suggest event on the editor");
    onKeyPress = editor.once("after-suggest");
  }

  info("Synthesizing key " + key + ", modifiers: " + Object.keys(modifiers));
  EventUtils.synthesizeKey(key, modifiers, view.styleWindow);

  yield onKeyPress;
  yield wait(1); // Equivalent of executeSoon

  info("Checking the state");
  if (completion != null) {
    // The key might have been a TAB or shift-TAB, in which case the editor will
    // be a new one
    editor = inplaceEditor(view.styleDocument.activeElement);
    is(editor.input.value, completion, "Correct value is autocompleted");
  }
  if (total == 0) {
    ok(!(editor.popup && editor.popup.isOpen), "Popup is closed");
  } else {
    ok(editor.popup._panel.state == "open" || editor.popup._panel.state == "showing", "Popup is open");
    is(editor.popup.getItems().length, total, "Number of suggestions match");
    is(editor.popup.selectedIndex, index, "Correct item is selected");
  }
}
