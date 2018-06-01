/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Testing selector inplace-editor remains available and focused after clicking
// in its input.

const TEST_URI = `
  <style type="text/css">
    .testclass {
      text-align: center;
    }
  </style>
  <div class="testclass">Styled Node</div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openRuleView();
  await selectNode(".testclass", inspector);
  await testClickOnSelectorEditorInput(view);
});

async function testClickOnSelectorEditorInput(view) {
  info("Test clicking inside the selector editor input");

  const ruleEditor = getRuleViewRuleEditor(view, 1);

  info("Focusing an existing selector name in the rule-view");
  const editor = await focusEditableField(view, ruleEditor.selectorText);
  const editorInput = editor.input;
  is(inplaceEditor(ruleEditor.selectorText), editor,
    "The selector editor got focused");

  info("Click inside the editor input");
  const onClick = once(editorInput, "click");
  EventUtils.synthesizeMouse(editor.input, 2, 1, {}, view.styleWindow);
  await onClick;
  is(editor.input, view.styleDocument.activeElement,
    "The editor input should still be focused");
  ok(!ruleEditor.newPropSpan, "No newProperty editor was created");

  info("Doubleclick inside the editor input");
  const onDoubleClick = once(editorInput, "dblclick");
  EventUtils.synthesizeMouse(editor.input, 2, 1, { clickCount: 2 },
    view.styleWindow);
  await onDoubleClick;
  is(editor.input, view.styleDocument.activeElement,
    "The editor input should still be focused");
  ok(!ruleEditor.newPropSpan, "No newProperty editor was created");

  info("Click outside the editor input");
  const onBlur = once(editorInput, "blur");
  const rect = editorInput.getBoundingClientRect();
  EventUtils.synthesizeMouse(editorInput, rect.width + 5, rect.height / 2, {},
    view.styleWindow);
  await onBlur;

  isnot(editorInput, view.styleDocument.activeElement,
    "The editor input should no longer be focused");
}
