/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that user set style properties can be changed from the markup-view and
// don't survive page reload

let TEST_PAGE = [
  "data:text/html;charset=utf-8,",
  "<p id='id1' style='width:200px;'>element 1</p>",
  "<p id='id2' style='width:100px;'>element 2</p>"
].join("");

let test = asyncTest(function*() {
  yield addTab(TEST_PAGE);
  let {toolbox, inspector, view} = yield openRuleView();

  yield selectNode("#id1", inspector);
  yield modifyRuleViewWidth("300px", view, inspector);
  assertRuleAndMarkupViewWidth("id1", "300px", view, inspector);

  yield selectNode("#id2", inspector);
  assertRuleAndMarkupViewWidth("id2", "100px", view, inspector);
  yield modifyRuleViewWidth("50px", view, inspector);
  assertRuleAndMarkupViewWidth("id2", "50px", view, inspector);

  yield reloadPage(inspector);

  yield selectNode("#id1", inspector);
  assertRuleAndMarkupViewWidth("id1", "200px", view, inspector);
  yield selectNode("#id2", inspector);
  assertRuleAndMarkupViewWidth("id2", "100px", view, inspector);
});

function getStyleRule(ruleView) {
  return ruleView.doc.querySelector(".ruleview-rule");
}

function* modifyRuleViewWidth(value, ruleView, inspector) {
  info("Getting the property value element");
  let valueSpan = getStyleRule(ruleView).querySelector(".ruleview-propertyvalue");

  info("Focusing the property value to set it to edit mode");
  let editor = yield focusEditableField(valueSpan.parentNode);

  ok(editor.input, "The inplace-editor field is ready");
  info("Setting the new value");
  editor.input.value = value;

  info("Pressing return and waiting for the field to blur and for the markup-view to show the mutation");
  let onBlur = once(editor.input, "blur", true);
  let onMutation = inspector.once("markupmutation");
  EventUtils.sendKey("return");
  yield onBlur;
  yield onMutation;

  info("Escaping out of the new property field that has been created after the value was edited");
  let onNewFieldBlur = once(ruleView.doc.activeElement, "blur", true);
  EventUtils.sendKey("escape");
  yield onNewFieldBlur;
}

function getContainerStyleAttrValue(id, {markup}) {
  let front = markup.walker.frontForRawNode(content.document.getElementById(id));
  let container = markup.getContainer(front);

  let attrIndex = 0;
  for (let attrName of container.elt.querySelectorAll(".attr-name")) {
    if (attrName.textContent === "style") {
      return container.elt.querySelectorAll(".attr-value")[attrIndex];
    }
    attrIndex ++;
  }
}

function assertRuleAndMarkupViewWidth(id, value, ruleView, inspector) {
  let valueSpan = getStyleRule(ruleView).querySelector(".ruleview-propertyvalue");
  is(valueSpan.textContent, value, "Rule-view style width is " + value + " as expected");

  let attr = getContainerStyleAttrValue(id, inspector);
  is(attr.textContent.replace(/\s/g, ""), "width:" + value + ";", "Markup-view style attribute width is " + value);
}

function reloadPage(inspector) {
  let onNewRoot = inspector.once("new-root");
  content.location.reload();
  return onNewRoot.then(inspector.markup._waitForChildren);
}
