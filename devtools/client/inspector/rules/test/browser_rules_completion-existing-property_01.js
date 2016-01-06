/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that CSS property names are autocompleted and cycled correctly when
// editing an existing property in the rule view.

const MAX_ENTRIES = 10;

// format :
//  [
//    what key to press,
//    expected input box value after keypress,
//    selectedIndex of the popup,
//    total items in the popup
//  ]
var testData = [
  ["VK_RIGHT", "font", -1, 0],
  ["-", "font-family", 0, MAX_ENTRIES],
  ["f", "font-family", 0, 2],
  ["VK_BACK_SPACE", "font-f", -1, 0],
  ["VK_BACK_SPACE", "font-", -1, 0],
  ["VK_BACK_SPACE", "font", -1, 0],
  ["VK_BACK_SPACE", "fon", -1, 0],
  ["VK_BACK_SPACE", "fo", -1, 0],
  ["VK_BACK_SPACE", "f", -1, 0],
  ["VK_BACK_SPACE", "", -1, 0],
  ["d", "direction", 0, 3],
  ["VK_DOWN", "display", 1, 3],
  ["VK_DOWN", "dominant-baseline", 2, 3],
  ["VK_DOWN", "direction", 0, 3],
  ["VK_DOWN", "display", 1, 3],
  ["VK_UP", "direction", 0, 3],
  ["VK_UP", "dominant-baseline", 2, 3],
  ["VK_UP", "display", 1, 3],
  ["VK_BACK_SPACE", "d", -1, 0],
  ["i", "direction", 0, 2],
  ["s", "display", -1, 0],
  ["VK_BACK_SPACE", "dis", -1, 0],
  ["VK_BACK_SPACE", "di", -1, 0],
  ["VK_BACK_SPACE", "d", -1, 0],
  ["VK_BACK_SPACE", "", -1, 0],
  ["VK_HOME", "", -1, 0],
  ["VK_END", "", -1, 0],
  ["VK_PAGE_UP", "", -1, 0],
  ["VK_PAGE_DOWN", "", -1, 0],
  ["f", "fill", 0, MAX_ENTRIES],
  ["i", "fill", 0, 4],
  ["VK_LEFT", "fill", -1, 0],
  ["VK_LEFT", "fill", -1, 0],
  ["i", "fiill", -1, 0],
  ["VK_ESCAPE", null, -1, 0],
];

const TEST_URI = "<h1 style='font: 24px serif'>Header</h1>";

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {toolbox, inspector, view} = yield openRuleView();

  info("Test autocompletion after 1st page load");
  yield runAutocompletionTest(toolbox, inspector, view);

  info("Test autocompletion after page navigation");
  yield reloadPage(inspector);
  yield runAutocompletionTest(toolbox, inspector, view);
});

function* runAutocompletionTest(toolbox, inspector, view) {
  info("Selecting the test node");
  yield selectNode("h1", inspector);

  info("Focusing the css property editable field");
  let propertyName = view.styleDocument
    .querySelectorAll(".ruleview-propertyname")[0];
  let editor = yield focusEditableField(view, propertyName);

  info("Starting to test for css property completion");
  for (let i = 0; i < testData.length; i++) {
    yield testCompletion(testData[i], editor, view);
  }
}

function* testCompletion([key, completion, index, total], editor, view) {
  info("Pressing key " + key);
  info("Expecting " + completion + ", " + index + ", " + total);

  let onSuggest;

  if (/(left|right|back_space|escape|home|end|page_up|page_down)/ig.test(key)) {
    info("Adding event listener for " +
      "left|right|back_space|escape|home|end|page_up|page_down keys");
    onSuggest = once(editor.input, "keypress");
  } else {
    info("Waiting for after-suggest event on the editor");
    onSuggest = editor.once("after-suggest");
  }

  info("Synthesizing key " + key);
  EventUtils.synthesizeKey(key, {}, view.styleWindow);

  yield onSuggest;
  yield wait(1); // Equivalent of executeSoon

  info("Checking the state");
  if (completion != null) {
    is(editor.input.value, completion, "Correct value is autocompleted");
  }
  if (total == 0) {
    ok(!(editor.popup && editor.popup.isOpen), "Popup is closed");
  } else {
    ok(editor.popup._panel.state == "open" ||
       editor.popup._panel.state == "showing", "Popup is open");
    is(editor.popup.getItems().length, total, "Number of suggestions match");
    is(editor.popup.selectedIndex, index, "Correct item is selected");
  }
}
