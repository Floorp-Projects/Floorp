/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Testing various inplace-editor behaviors in the rule-view

let TEST_URL = 'url("' + TEST_URL_ROOT + 'doc_test_image.png")';
let PAGE_CONTENT = [
  '<style type="text/css">',
  '  #testid {',
  '    background-color: blue;',
  '  }',
  '  .testclass {',
  '    background-color: green;',
  '  }',
  '</style>',
  '<div id="testid" class="testclass">Styled Node</div>'
].join("\n");

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8,test rule view user changes");

  info("Creating the test document");
  content.document.body.innerHTML = PAGE_CONTENT;

  info("Opening the rule-view");
  let {toolbox, inspector, view} = yield openRuleView();

  info("Selecting the test element");
  yield selectNode("#testid", inspector);

  yield testCancelNew(view);
});

function* testCancelNew(view) {
  info("Test adding a new rule to the element's style declaration and leaving it empty.");

  let elementRuleEditor = getRuleViewRuleEditor(view, 0);

  info("Focusing a new property name in the rule-view");
  let editor = yield focusEditableField(view, elementRuleEditor.closeBrace);
  is(inplaceEditor(elementRuleEditor.newPropSpan), editor, "The new property editor got focused");

  info("Bluring the editor input");
  let onBlur = once(editor.input, "blur");
  editor.input.blur();
  yield onBlur;

  info("Checking the state of canceling a new property name editor");
  ok(!elementRuleEditor.rule._applyingModifications, "Shouldn't have an outstanding request after a cancel.");
  is(elementRuleEditor.rule.textProps.length,  0, "Should have canceled creating a new text property.");
  ok(!elementRuleEditor.propertyList.hasChildNodes(), "Should not have any properties.");
}
