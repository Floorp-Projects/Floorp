/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that changes in the style inspector are synchronized into the
// style editor.

Services.scriptloader.loadSubScript("chrome://mochitests/content/browser/devtools/client/styleinspector/test/head.js", this);

const TESTCASE_URI = TEST_BASE_HTTP + "sync.html";

const expectedText = `
  body {
    border-width: 15px;
    color: red;
  }

  #testid, span {
    font-size: 4em;
  }
  `;

add_task(function*() {
  yield addTab(TESTCASE_URI);
  let { inspector, view } = yield openRuleView();
  yield selectNode("#testid", inspector);
  let ruleEditor = getRuleViewRuleEditor(view, 1);

  let editor = yield focusEditableField(view, ruleEditor.selectorText);
  editor.input.value = "#testid, span";
  let onRuleViewChanged = once(view, "ruleview-changed");
  EventUtils.synthesizeKey("VK_RETURN", {});
  yield onRuleViewChanged;

  let { ui } = yield openStyleEditor();

  editor = yield ui.editors[0].getSourceEditor();
  let text = editor.sourceEditor.getText();
  is(text, expectedText, "selector edits are synced");
});
