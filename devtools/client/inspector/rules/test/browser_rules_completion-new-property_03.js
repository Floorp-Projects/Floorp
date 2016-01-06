/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Regression test for a case where completing gave the wrong answer.
// See bug 1179318.

const TEST_URI = "<h1 style='color: red'>Header</h1>";

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {toolbox, inspector, view} = yield openRuleView();

  info("Test autocompletion for background-color");
  yield runAutocompletionTest(toolbox, inspector, view);
});

function* runAutocompletionTest(toolbox, inspector, view) {
  info("Selecting the test node");
  yield selectNode("h1", inspector);

  info("Focusing the css property editable field");
  let brace = view.styleDocument.querySelector(".ruleview-ruleclose");
  let editor = yield focusEditableField(view, brace);

  info("Sending \"background\" to the editable field");
  for (let key of "background") {
    let onSuggest = editor.once("after-suggest");
    EventUtils.synthesizeKey(key, {}, view.styleWindow);
    yield onSuggest;
  }

  const itemIndex = 4;

  let bgcItem = editor.popup.getItemAtIndex(itemIndex);
  is(bgcItem.label, "background-color",
     "check the expected completion element");

  editor.popup.selectedIndex = itemIndex;

  let node = editor.popup._list.childNodes[itemIndex];
  EventUtils.synthesizeMouseAtCenter(node, {}, view.styleWindow);

  is(editor.input.value, "background-color", "Correct value is autocompleted");
}
