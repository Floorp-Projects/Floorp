/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that user set style properties can be changed from the markup-view and
// don't survive page reload

const TEST_URI = `
  <p id='id1' style='width:200px;'>element 1</p>
  <p id='id2' style='width:100px;'>element 2</p>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view, testActor} = await openRuleView();

  await selectNode("#id1", inspector);
  await modifyRuleViewWidth("300px", view, inspector);
  await assertRuleAndMarkupViewWidth("id1", "300px", view, inspector);

  await selectNode("#id2", inspector);
  await assertRuleAndMarkupViewWidth("id2", "100px", view, inspector);
  await modifyRuleViewWidth("50px", view, inspector);
  await assertRuleAndMarkupViewWidth("id2", "50px", view, inspector);

  await reloadPage(inspector, testActor);

  await selectNode("#id1", inspector);
  await assertRuleAndMarkupViewWidth("id1", "200px", view, inspector);
  await selectNode("#id2", inspector);
  await assertRuleAndMarkupViewWidth("id2", "100px", view, inspector);
});

function getStyleRule(ruleView) {
  return ruleView.styleDocument.querySelector(".ruleview-rule");
}

async function modifyRuleViewWidth(value, ruleView, inspector) {
  info("Getting the property value element");
  const valueSpan = getStyleRule(ruleView)
    .querySelector(".ruleview-propertyvalue");

  info("Focusing the property value to set it to edit mode");
  const editor = await focusEditableField(ruleView, valueSpan.parentNode);

  ok(editor.input, "The inplace-editor field is ready");
  info("Setting the new value");
  editor.input.value = value;

  info("Pressing return and waiting for the field to blur and for the " +
    "markup-view to show the mutation");
  const onBlur = once(editor.input, "blur", true);
  const onStyleChanged = waitForStyleModification(inspector);
  EventUtils.sendKey("return");
  await onBlur;
  await onStyleChanged;

  info("Escaping out of the new property field that has been created after " +
    "the value was edited");
  const onNewFieldBlur = once(ruleView.styleDocument.activeElement, "blur", true);
  EventUtils.sendKey("escape");
  await onNewFieldBlur;
}

async function getContainerStyleAttrValue(id, {walker, markup}) {
  const front = await walker.querySelector(walker.rootNode, "#" + id);
  const container = markup.getContainer(front);

  let attrIndex = 0;
  for (const attrName of container.elt.querySelectorAll(".attr-name")) {
    if (attrName.textContent === "style") {
      return container.elt.querySelectorAll(".attr-value")[attrIndex];
    }
    attrIndex++;
  }
  return undefined;
}

async function assertRuleAndMarkupViewWidth(id, value, ruleView, inspector) {
  const valueSpan = getStyleRule(ruleView)
    .querySelector(".ruleview-propertyvalue");
  is(valueSpan.textContent, value,
    "Rule-view style width is " + value + " as expected");

  const attr = await getContainerStyleAttrValue(id, inspector);
  is(attr.textContent.replace(/\s/g, ""),
    "width:" + value + ";", "Markup-view style attribute width is " + value);
}
