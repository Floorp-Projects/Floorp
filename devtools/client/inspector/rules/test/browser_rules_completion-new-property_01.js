/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that CSS property names are autocompleted and cycled correctly when
// creating a new property in the rule view.

// format :
//  [
//    what key to press,
//    expected input box value after keypress,
//    is the popup open,
//    is a suggestion selected in the popup,
//  ]
const OPEN = true, SELECTED = true;
var testData = [
  ["d", "display", OPEN, SELECTED],
  ["VK_DOWN", "dominant-baseline", OPEN, SELECTED],
  ["VK_DOWN", "direction", OPEN, SELECTED],
  ["VK_DOWN", "display", OPEN, SELECTED],
  ["VK_UP", "direction", OPEN, SELECTED],
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

  info("Focusing the css property editable field");
  let ruleEditor = getRuleViewRuleEditor(view, 0);
  let editor = yield focusNewRuleViewProperty(ruleEditor);

  info("Starting to test for css property completion");
  for (let i = 0; i < testData.length; i++) {
    yield testCompletion(testData[i], editor, view);
  }
}

function* testCompletion([key, completion, isOpen, isSelected], editor, view) {
  info("Pressing key " + key);
  info("Expecting " + completion);
  info("Is popup opened: " + isOpen);
  info("Is item selected: " + isSelected);

  let onSuggest;

  if (/(right|back_space|escape)/ig.test(key)) {
    info("Adding event listener for right|back_space|escape keys");
    onSuggest = once(editor.input, "keypress");
  } else {
    info("Waiting for after-suggest event on the editor");
    onSuggest = editor.once("after-suggest");
  }

  info("Synthesizing key " + key);
  EventUtils.synthesizeKey(key, {}, view.styleWindow);

  yield onSuggest;
  yield waitForTick();

  info("Checking the state");
  if (completion != null) {
    is(editor.input.value, completion, "Correct value is autocompleted");
  }
  if (!isOpen) {
    ok(!(editor.popup && editor.popup.isOpen), "Popup is closed");
  } else {
    ok(editor.popup._panel.state == "open" ||
       editor.popup._panel.state == "showing", "Popup is open");
    is(editor.popup.selectedIndex != -1, isSelected, "An item is selected");
  }
}
