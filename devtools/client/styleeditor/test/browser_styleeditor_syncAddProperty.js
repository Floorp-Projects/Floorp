/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that adding a new rule is synced to the style editor.

const TESTCASE_URI = TEST_BASE_HTTP + "sync.html";
const TESTCASE_URI_WITH_CSP = TEST_BASE_HTTP + "sync_with_csp.html";

const expectedText = `
  body {
    border-width: 15px;
    color: red;
  }

  #testid {
    font-size: 4em;
    /*! background-color: yellow; */
  }
  `;

add_task(async function() {
  const URIs = [TESTCASE_URI, TESTCASE_URI_WITH_CSP];

  for (const URI of URIs) {
    await addTab(URI);
    const { inspector, view } = await openRuleView();
    await selectNode("#testid", inspector);

    info("Focusing a new property name in the rule-view on " + URI);
    const ruleEditor = getRuleViewRuleEditor(view, 1);
    const editor = await focusEditableField(view, ruleEditor.closeBrace);
    is(
      inplaceEditor(ruleEditor.newPropSpan),
      editor,
      "The new property editor has focus"
    );

    const input = editor.input;
    input.value = "/* background-color: yellow; */";

    info("Pressing return to commit and focus the new value field");
    const onModifications = view.once("ruleview-changed");
    EventUtils.synthesizeKey("VK_RETURN", {}, view.styleWindow);
    await onModifications;

    const { ui } = await openStyleEditor();
    const sourceEditor = await ui.editors[0].getSourceEditor();
    const text = sourceEditor.sourceEditor.getText();
    is(text, expectedText, "selector edits are synced");
  }
});
