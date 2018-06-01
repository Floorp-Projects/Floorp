/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Regression test for a case where completing gave the wrong answer.
// See bug 1179318.

const TEST_URI = "<h1 style='color: red'>Header</h1>";

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {toolbox, inspector, view} = await openRuleView();

  info("Test autocompletion for background-color");
  await runAutocompletionTest(toolbox, inspector, view);
});

async function runAutocompletionTest(toolbox, inspector, view) {
  info("Selecting the test node");
  await selectNode("h1", inspector);

  info("Focusing the new property editable field");
  const ruleEditor = getRuleViewRuleEditor(view, 0);
  const editor = await focusNewRuleViewProperty(ruleEditor);

  info("Sending \"background\" to the editable field");
  for (const key of "background") {
    const onSuggest = editor.once("after-suggest");
    EventUtils.synthesizeKey(key, {}, view.styleWindow);
    await onSuggest;
  }

  const itemIndex = 4;

  const bgcItem = editor.popup.getItemAtIndex(itemIndex);
  is(bgcItem.label, "background-color",
     "check the expected completion element");

  editor.popup.selectedIndex = itemIndex;

  const node = editor.popup._list.childNodes[itemIndex];
  EventUtils.synthesizeMouseAtCenter(node, {}, editor.popup._window);

  is(editor.input.value, "background-color", "Correct value is autocompleted");
}
