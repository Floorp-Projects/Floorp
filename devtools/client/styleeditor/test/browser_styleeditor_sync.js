/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that changes in the style inspector are synchronized into the
// style editor.

const TESTCASE_URI = TEST_BASE_HTTP + "sync.html";

const expectedText = `
  body {
    border-width: 15px;
    /*! color: red; */
  }

  #testid {
    /*! font-size: 4em; */
  }
  `;

async function closeAndReopenToolbox() {
  const target = TargetFactory.forTab(gBrowser.selectedTab);
  await gDevTools.closeToolbox(target);
  const { ui: newui } = await openStyleEditor();
  return newui;
}

add_task(async function() {
  await addTab(TESTCASE_URI);
  const { inspector, view } = await openRuleView();
  await selectNode("#testid", inspector);
  let ruleEditor = getRuleViewRuleEditor(view, 1);

  // Disable the "font-size" property.
  let propEditor = ruleEditor.rule.textProps[0].editor;
  let onModification = view.once("ruleview-changed");
  propEditor.enable.click();
  await onModification;

  // Disable the "color" property.  Note that this property is in a
  // rule that also contains a non-inherited property -- so this test
  // is also testing that property editing works properly in this
  // situation.
  ruleEditor = getRuleViewRuleEditor(view, 3);
  propEditor = ruleEditor.rule.textProps[1].editor;
  onModification = view.once("ruleview-changed");
  propEditor.enable.click();
  await onModification;

  let { ui } = await openStyleEditor();

  let editor = await ui.editors[0].getSourceEditor();
  let text = editor.sourceEditor.getText();
  is(text, expectedText, "style inspector changes are synced");

  // Close and reopen the toolbox, to see that the edited text remains
  // available.
  ui = await closeAndReopenToolbox();
  editor = await ui.editors[0].getSourceEditor();
  text = editor.sourceEditor.getText();
  is(text, expectedText, "changes remain after close and reopen");

  // For the time being, the actor does not update the style's owning
  // node's textContent.  See bug 1205380.
  const textContent = await ContentTask.spawn(gBrowser.selectedBrowser, null,
    async function() {
      return content.document.querySelector("style").textContent;
    });

  isnot(textContent, expectedText, "changes not written back to style node");
});
