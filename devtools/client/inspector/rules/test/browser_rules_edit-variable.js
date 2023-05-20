/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test editing a CSS variable.

const TEST_URI = `
  <style type='text/css'>
    div {
      color: var(--color);
      --color: lime;
    }
  </style>
  <div></div>
`;

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  await selectNode("div", inspector);

  info("Check the initial state of the --color variable");
  checkCSSVariableOutput(
    view,
    "div",
    "color",
    "ruleview-variable",
    "--color = lime"
  );

  info("Edit the CSS variable");
  const prop = getTextProperty(view, 1, { "--color": "lime" });
  const propEditor = prop.editor;
  const editor = await focusEditableField(view, propEditor.valueSpan);
  editor.input.value = "blue";
  const onRuleViewChanged = view.once("ruleview-changed");
  EventUtils.synthesizeKey("VK_RETURN", {}, view.styleWindow);
  await onRuleViewChanged;
  checkCSSVariableOutput(
    view,
    "div",
    "color",
    "ruleview-variable",
    "--color = blue"
  );
});
