/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that adding properties to rules work and reselecting the element still
// show them.

const TEST_URI = URL_ROOT + "doc_content_stylesheet.html";

add_task(async function() {
  await addTab(TEST_URI);
  const {inspector, view} = await openRuleView();
  await selectNode("#target", inspector);

  info("Setting a font-weight property on all rules");
  await setPropertyOnAllRules(view);

  info("Reselecting the element");
  await selectNode("body", inspector);
  await selectNode("#target", inspector);

  checkPropertyOnAllRules(view);
});

async function setPropertyOnAllRules(view) {
  // Wait for the properties to be properly created on the backend and for the
  // view to be updated.
  const onRefreshed = view.once("ruleview-refreshed");
  for (const rule of view._elementStyle.rules) {
    rule.editor.addProperty("font-weight", "bold", "", true);
  }
  await onRefreshed;
}

function checkPropertyOnAllRules(view) {
  for (const rule of view._elementStyle.rules) {
    const lastRule = rule.textProps[rule.textProps.length - 1];

    is(lastRule.name, "font-weight", "Last rule name is font-weight");
    is(lastRule.value, "bold", "Last rule value is bold");
  }
}
