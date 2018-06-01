/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that we can guess indentation from a style sheet, not just a
// rule.

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

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {toolbox, inspector, view} = await openRuleView();
  await selectNode("#testid", inspector);

  info("Add a new property in the rule-view");
  await addProperty(view, 2, "color", "chartreuse");

  info("Switch to the style-editor");
  const { UI } = await toolbox.selectTool("styleeditor");

  const styleEditor = await UI.editors[0].getSourceEditor();
  const text = styleEditor.sourceEditor.getText();
  is(text, expectedText, "style inspector changes are synced");
});
