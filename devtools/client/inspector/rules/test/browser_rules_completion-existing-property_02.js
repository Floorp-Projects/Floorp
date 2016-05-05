/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
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
//    selectedIndex of the popup,
//    total items in the popup,
//    expect ruleview-changed
//  ]
var testData = [
  ["b", {}, "beige", 0, 8, true],
  ["l", {}, "black", 0, 4, true],
  ["VK_DOWN", {}, "blanchedalmond", 1, 4, true],
  ["VK_DOWN", {}, "blue", 2, 4, true],
  ["VK_RIGHT", {}, "blue", -1, 0, false],
  [" ", {}, "blue aliceblue", 0, 158, true],
  ["!", {}, "blue !important", 0, 0, true],
  ["VK_BACK_SPACE", {}, "blue !", -1, 0, true],
  ["VK_BACK_SPACE", {}, "blue ", -1, 0, true],
  ["VK_BACK_SPACE", {}, "blue", -1, 0, true],
  ["VK_TAB", {shiftKey: true}, "color", -1, 0, true],
  ["VK_BACK_SPACE", {}, "", -1, 0, false],
  ["d", {}, "display", 1, 3, false],
  ["VK_TAB", {}, "blue", -1, 0, true],
  ["n", {}, "none", -1, 0, true],
  ["VK_RETURN", {}, null, -1, 0, true]
];

const TEST_URI = "<h1 style='color: red'>Header</h1>";

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

  let rule = getRuleViewRuleEditor(view, 0).rule;
  let prop = rule.textProps[0];

  info("Focusing the css property editable value");
  let editor = yield focusEditableField(view, prop.editor.valueSpan);

  info("Starting to test for css property completion");
  for (let i = 0; i < testData.length; i++) {
    // Re-define the editor at each iteration, because the focus may have moved
    // from property to value and back
    editor = inplaceEditor(view.styleDocument.activeElement);
    yield testCompletion(testData[i], editor, view);
  }
}

function* testCompletion([key, modifiers, completion, index, total, willChange],
                         editor, view) {
  info("Pressing key " + key);
  info("Expecting " + completion + ", " + index + ", " + total);

  let onDone;
  if (willChange) {
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

  info("Synthesizing key " + key + ", modifiers: " + Object.keys(modifiers));
  EventUtils.synthesizeKey(key, modifiers, view.styleWindow);
  yield onDone;

  // The key might have been a TAB or shift-TAB, in which case the editor will
  // be a new one
  editor = inplaceEditor(view.styleDocument.activeElement);

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
