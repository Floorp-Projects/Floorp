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

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode(".testclass", inspector);
  yield testClickOnSelectorEditorInput(view);
});

function* testClickOnSelectorEditorInput(view) {
  info("Test clicking inside the selector editor input");

  let ruleEditor = getRuleViewRuleEditor(view, 1);

  info("Focusing an existing selector name in the rule-view");
  let editor = yield focusEditableField(view, ruleEditor.selectorText);
  let editorInput = editor.input;
  is(inplaceEditor(ruleEditor.selectorText), editor,
    "The selector editor got focused");

  info("Click inside the editor input");
  let onClick = once(editorInput, "click");
  EventUtils.synthesizeMouse(editor.input, 2, 1, {}, view.styleWindow);
  yield onClick;
  is(editor.input, view.styleDocument.activeElement,
    "The editor input should still be focused");
  ok(!ruleEditor.newPropSpan, "No newProperty editor was created");

  info("Doubleclick inside the editor input");
  let onDoubleClick = once(editorInput, "dblclick");
  EventUtils.synthesizeMouse(editor.input, 2, 1, { clickCount: 2 },
    view.styleWindow);
  yield onDoubleClick;
  is(editor.input, view.styleDocument.activeElement,
    "The editor input should still be focused");
  ok(!ruleEditor.newPropSpan, "No newProperty editor was created");

  info("Click outside the editor input");
  let onBlur = once(editorInput, "blur");
  let rect = editorInput.getBoundingClientRect();
  EventUtils.synthesizeMouse(editorInput, rect.width + 5, rect.height / 2, {},
    view.styleWindow);
  yield onBlur;

  isnot(editorInput, view.styleDocument.activeElement,
    "The editor input should no longer be focused");
}
