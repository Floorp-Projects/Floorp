/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that adding properties to rules work and reselecting the element still
// show them.

const TEST_URI = URL_ROOT + "doc_content_stylesheet.html";

add_task(async function() {
  await addTab(TEST_URI);
  const { inspector, view } = await openRuleView();
  await selectNode("#target", inspector);

  info("Setting a font-weight property on all rules");
  await setPropertyOnAllRules(view, inspector);

  info("Reselecting the element");
  await selectNode("body", inspector);
  await selectNode("#target", inspector);

  checkPropertyOnAllRules(view);
});

async function setPropertyOnAllRules(view, inspector) {
  // Set the inline style rule first independently because it needs to wait for specific
  // events and the DOM mutation that it causes refreshes the rules view, so we need to
  // get the list of rules again later.
  info("Adding font-weight:bold in the inline style rule");
  const inlineStyleRuleEditor = view._elementStyle.rules[0].editor;

  const onMutation = inspector.once("markupmutation");
  const onRuleViewRefreshed = view.once("ruleview-refreshed");

  inlineStyleRuleEditor.addProperty("font-weight", "bold", "", true);

  await Promise.all([onMutation, onRuleViewRefreshed]);

  // Now set the other rules after having retrieved the list.
  const allRules = view._elementStyle.rules;

  for (let i = 1; i < allRules.length; i++) {
    info(`Adding font-weight:bold in rule ${i}`);
    const rule = allRules[i];
    const ruleEditor = rule.editor;

    const onRuleViewChanged = view.once("ruleview-changed");

    ruleEditor.addProperty("font-weight", "bold", "", true);

    await onRuleViewChanged;
  }
}

function checkPropertyOnAllRules(view) {
  for (const rule of view._elementStyle.rules) {
    const lastProperty = rule.textProps[rule.textProps.length - 1];

    is(lastProperty.name, "font-weight", "Last property name is font-weight");
    is(lastProperty.value, "bold", "Last property value is bold");
  }
}
