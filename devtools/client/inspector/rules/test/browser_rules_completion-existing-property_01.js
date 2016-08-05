/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that CSS property names are autocompleted and cycled correctly when
// editing an existing property in the rule view.

// format :
//  [
//    what key to press,
//    expected input box value after keypress,
//    is the popup open,
//    is a suggestion selected in the popup,
//  ]

const OPEN = true, SELECTED = true;
var testData = [
  ["VK_RIGHT", "font", !OPEN, !SELECTED],
  ["-", "font-size", OPEN, SELECTED],
  ["f", "font-family", OPEN, SELECTED],
  ["VK_BACK_SPACE", "font-f", !OPEN, !SELECTED],
  ["VK_BACK_SPACE", "font-", !OPEN, !SELECTED],
  ["VK_BACK_SPACE", "font", !OPEN, !SELECTED],
  ["VK_BACK_SPACE", "fon", !OPEN, !SELECTED],
  ["VK_BACK_SPACE", "fo", !OPEN, !SELECTED],
  ["VK_BACK_SPACE", "f", !OPEN, !SELECTED],
  ["VK_BACK_SPACE", "", !OPEN, !SELECTED],
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
  ["VK_HOME", "", !OPEN, !SELECTED],
  ["VK_END", "", !OPEN, !SELECTED],
  ["VK_PAGE_UP", "", !OPEN, !SELECTED],
  ["VK_PAGE_DOWN", "", !OPEN, !SELECTED],
  ["d", "display", OPEN, SELECTED],
  ["VK_HOME", "display", !OPEN, !SELECTED],
  ["VK_END", "display", !OPEN, !SELECTED],
  // Press right key to ensure caret move to end of the input on Mac OS since
  // Mac OS doesn't move caret after pressing HOME / END.
  ["VK_RIGHT", "display", !OPEN, !SELECTED],
  ["VK_BACK_SPACE", "displa", !OPEN, !SELECTED],
  ["VK_BACK_SPACE", "displ", !OPEN, !SELECTED],
  ["VK_BACK_SPACE", "disp", !OPEN, !SELECTED],
  ["VK_BACK_SPACE", "dis", !OPEN, !SELECTED],
  ["VK_BACK_SPACE", "di", !OPEN, !SELECTED],
  ["VK_BACK_SPACE", "d", !OPEN, !SELECTED],
  ["VK_BACK_SPACE", "", !OPEN, !SELECTED],
  ["f", "font-size", OPEN, SELECTED],
  ["i", "filter", OPEN, SELECTED],
  ["VK_LEFT", "filter", !OPEN, !SELECTED],
  ["VK_LEFT", "filter", !OPEN, !SELECTED],
  ["i", "fiilter", !OPEN, !SELECTED],
  ["VK_ESCAPE", null, !OPEN, !SELECTED],
];

const TEST_URI = "<h1 style='font: 24px serif'>Header</h1>";

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
  let propertyName = view.styleDocument.querySelectorAll(".ruleview-propertyname")[0];
  let editor = yield focusEditableField(view, propertyName);

  info("Starting to test for css property completion");
  for (let i = 0; i < testData.length; i++) {
    yield testCompletion(testData[i], editor, view);
  }
}

function* testCompletion([key, completion, open, selected],
                         editor, view) {
  info("Pressing key " + key);
  info("Expecting " + completion);
  info("Is popup opened: " + open);
  info("Is item selected: " + selected);

  // Listening for the right event that will tell us when the key has been
  // entered and processed.
  let onSuggest;
  if (/(left|right|back_space|escape|home|end|page_up|page_down)/ig.test(key)) {
    info("Adding event listener for " +
      "left|right|back_space|escape|home|end|page_up|page_down keys");
    onSuggest = once(editor.input, "keypress");
  } else {
    info("Waiting for after-suggest event on the editor");
    onSuggest = editor.once("after-suggest");
  }

  // Also listening for popup opened/closed events if needed.
  let popupEvent = open ? "popup-opened" : "popup-closed";
  let onPopupEvent = editor.popup.isOpen !== open ? once(editor.popup, popupEvent) : null;

  info("Synthesizing key " + key);
  EventUtils.synthesizeKey(key, {}, view.styleWindow);

  // Flush the throttle for the preview text.
  view.throttle.flush();

  yield onSuggest;
  yield onPopupEvent;

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
