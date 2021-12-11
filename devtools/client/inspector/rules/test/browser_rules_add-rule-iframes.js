/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests adding a rule on elements nested in iframes.

const TEST_URI = `<div>outer</div>
  <iframe id="frame1" src="data:text/html;charset=utf-8,<div>inner1</div>">
  </iframe>
  <iframe id="frame2" src="data:text/html;charset=utf-8,<div>inner2</div>">
  </iframe>`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  await selectNode("div", inspector);
  await addNewRuleAndDismissEditor(inspector, view, "div", 1);
  await addNewProperty(view, 1, "color", "red");

  await selectNodeInFrames(["#frame1", "div"], inspector);
  await addNewRuleAndDismissEditor(inspector, view, "div", 1);
  await addNewProperty(view, 1, "color", "blue");

  await selectNodeInFrames(["#frame2", "div"], inspector);
  await addNewRuleAndDismissEditor(inspector, view, "div", 1);
  await addNewProperty(view, 1, "color", "green");
});

/**
 * Add a new property in the rule at the provided index in the rule view.
 *
 * @param {RuleView} view
 * @param {Number} index
 *        The index of the rule in which we should add a new property.
 * @param {String} name
 *        The name of the new property.
 * @param {String} value
 *        The value of the new property.
 */
async function addNewProperty(view, index, name, value) {
  const idRuleEditor = getRuleViewRuleEditor(view, index);
  info(`Adding new property "${name}: ${value};"`);

  const onRuleViewChanged = view.once("ruleview-changed");
  idRuleEditor.addProperty(name, value, "", true);
  await onRuleViewChanged;

  const textProps = idRuleEditor.rule.textProps;
  const lastProperty = textProps[textProps.length - 1];
  is(lastProperty.name, name, "Last property has the expected name");
  is(lastProperty.value, value, "Last property has the expected value");
}
