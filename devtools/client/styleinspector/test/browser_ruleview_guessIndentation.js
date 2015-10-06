/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that we can guess indentation from a style sheet, not just a
// rule.

// Needed for openStyleEditor.
Services.scriptloader.loadSubScript("chrome://mochitests/content/browser/devtools/client/styleeditor/test/head.js", this);

// Use a weird indentation depth to avoid accidental success.
const TEST_URI = `
  <style type='text/css'>
div {
       background-color: blue;
}

* {
}
</style>
  <div id='testid' class='testclass'>Styled Node</div>
`;

const expectedText = `
div {
       background-color: blue;
}

* {
       color: chartreuse;
}
`;

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("#testid", inspector);
  yield testIndentation(inspector, view);
});

function* testIndentation(inspector, view) {
  let ruleEditor = getRuleViewRuleEditor(view, 2);

  info("Focusing a new property name in the rule-view");
  let editor = yield focusEditableField(view, ruleEditor.closeBrace);

  let input = editor.input;

  info("Entering color in the property name editor");
  input.value = "color";

  info("Pressing return to commit and focus the new value field");
  let onValueFocus = once(ruleEditor.element, "focus", true);
  let onModifications = ruleEditor.rule._applyingModifications;
  EventUtils.synthesizeKey("VK_RETURN", {}, view.styleWindow);
  yield onValueFocus;
  yield onModifications;

  // Getting the new value editor after focus
  editor = inplaceEditor(view.styleDocument.activeElement);
  info("Entering a value and bluring the field to expect a rule change");
  editor.input.value = "chartreuse";
  let onBlur = once(editor.input, "blur");
  onModifications = ruleEditor.rule._applyingModifications;
  editor.input.blur();
  yield onBlur;
  yield onModifications;

  let { ui } = yield openStyleEditor();

  let styleEditor = yield ui.editors[0].getSourceEditor();
  let text = styleEditor.sourceEditor.getText();
  is(text, expectedText, "style inspector changes are synced");
}
