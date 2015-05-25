/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Testing selector inplace-editor behaviors in the rule-view with pseudo
// classes.

let PAGE_CONTENT = [
  '<style type="text/css">',
  '  .testclass {',
  '    text-align: center;',
  '  }',
  '  #testid3:first-letter {',
  '    text-decoration: "italic"',
  '  }',
  '</style>',
  '<div id="testid">Styled Node</div>',
  '<span class="testclass">This is a span</span>',
  '<div class="testclass2">A</div>',
  '<div id="testid3">B</div>'
].join("\n");

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8,test rule view selector changes");

  info("Creating the test document");
  content.document.body.innerHTML = PAGE_CONTENT;

  info("Opening the rule-view");
  let {toolbox, inspector, view} = yield openRuleView();

  info("Selecting the test element");
  yield selectNode(".testclass", inspector);
  yield testEditSelector(view, "div:nth-child(2)");

  info("Selecting the modified element");
  yield selectNode("#testid", inspector);
  yield checkModifiedElement(view, "div:nth-child(2)");

  info("Selecting the test element");
  yield selectNode("#testid3", inspector);
  yield testEditSelector(view, ".testclass2::first-letter");

  info("Selecting the modified element");
  yield selectNode(".testclass2", inspector);
  yield checkModifiedElement(view, ".testclass2::first-letter");
});

function* testEditSelector(view, name) {
  info("Test editing existing selector fields");

  let idRuleEditor = getRuleViewRuleEditor(view, 1) ||
    getRuleViewRuleEditor(view, 1, 0);

  info("Focusing an existing selector name in the rule-view");
  let editor = yield focusEditableField(view, idRuleEditor.selectorText);

  is(inplaceEditor(idRuleEditor.selectorText), editor,
    "The selector editor got focused");

  info("Entering a new selector name: " + name);
  editor.input.value = name;

  info("Waiting for rule view to refresh");
  let onRuleViewRefresh = once(view, "ruleview-refreshed");

  info("Entering the commit key");
  EventUtils.synthesizeKey("VK_RETURN", {});
  yield onRuleViewRefresh;

  is(view._elementStyle.rules.length, 1, "Should have 1 rule.");
  is(getRuleViewRule(view, name), undefined,
      name + " selector has been removed.");
}

function* checkModifiedElement(view, name) {
  is(view._elementStyle.rules.length, 2, "Should have 2 rules.");
  ok(getRuleViewRule(view, name), "Rule with " + name + " selector exists.");
}
