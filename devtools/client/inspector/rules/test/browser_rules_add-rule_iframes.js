/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests adding a rule on elements nested in iframes.

const TEST_URI =
 `<div>outer</div>
  <iframe id="frame1" src="data:text/html;charset=utf-8,<div>inner1</div>">
  </iframe>
  <iframe id="frame2" src="data:text/html;charset=utf-8,<div>inner2</div>">
  </iframe>`;

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("div", inspector);
  yield addNewRule(inspector, view);
  yield testNewRule(view, "div", 1);
  yield addNewProperty(view, 1, "color", "red");

  let innerFrameDiv1 = yield getNodeFrontInFrame("div", "#frame1", inspector);
  yield selectNode(innerFrameDiv1, inspector);
  yield addNewRule(inspector, view);
  yield testNewRule(view, "div", 1);
  yield addNewProperty(view, 1, "color", "blue");

  let innerFrameDiv2 = yield getNodeFrontInFrame("div", "#frame2", inspector);
  yield selectNode(innerFrameDiv2, inspector);
  yield addNewRule(inspector, view);
  yield testNewRule(view, "div", 1);
  yield addNewProperty(view, 1, "color", "green");
});

/**
 * Check the newly created rule has the expected selector and submit the
 * selector editor.
 */
function* testNewRule(view, expected, index) {
  let idRuleEditor = getRuleViewRuleEditor(view, index);
  let editor = idRuleEditor.selectorText.ownerDocument.activeElement;
  is(editor.value, expected,
      "Selector editor value is as expected: " + expected);

  info("Entering the escape key");
  EventUtils.synthesizeKey("VK_ESCAPE", {});

  is(idRuleEditor.selectorText.textContent, expected,
      "Selector text value is as expected: " + expected);
}

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
function* addNewProperty(view, index, name, value) {
  let idRuleEditor = getRuleViewRuleEditor(view, index);
  info(`Adding new property "${name}: ${value};"`);

  let onRuleViewChanged = view.once("ruleview-changed");
  idRuleEditor.addProperty(name, value, "");
  yield onRuleViewChanged;

  let textProps = idRuleEditor.rule.textProps;
  let lastProperty = textProps[textProps.length - 1];
  is(lastProperty.name, name, "Last property has the expected name");
  is(lastProperty.value, value, "Last property has the expected value");
}
