/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that depending where the user last clicked in the inspector, the right search
// field is focused when ctrl+F is pressed.

add_task(async function() {
  const {inspector} = await openInspectorForURL("data:text/html;charset=utf-8,Search!");

  info("Check that by default, the inspector search field gets focused");
  pressCtrlF();
  isInInspectorSearchBox(inspector);

  info("Click somewhere in the rule-view");
  clickInRuleView(inspector);

  info("Check that the rule-view search field gets focused");
  pressCtrlF();
  isInRuleViewSearchBox(inspector);

  info("Click in the inspector again");
  await clickContainer("head", inspector);

  info("Check that now we're back in the inspector, its search field gets focused");
  pressCtrlF();
  isInInspectorSearchBox(inspector);

  info("Switch to the computed view, and click somewhere inside it");
  selectComputedView(inspector);
  clickInComputedView(inspector);

  info("Check that the computed-view search field gets focused");
  pressCtrlF();
  isInComputedViewSearchBox(inspector);

  info("Click in the inspector yet again");
  await clickContainer("body", inspector);

  info("We're back in the inspector again, check the inspector search field focuses");
  pressCtrlF();
  isInInspectorSearchBox(inspector);
});

function pressCtrlF() {
  EventUtils.synthesizeKey("f", {accelKey: true});
}

function clickInRuleView(inspector) {
  const el = inspector.panelDoc.querySelector("#sidebar-panel-ruleview");
  EventUtils.synthesizeMouseAtCenter(el, {}, inspector.panelDoc.defaultView);
}

function clickInComputedView(inspector) {
  const el = inspector.panelDoc.querySelector("#sidebar-panel-computedview");
  EventUtils.synthesizeMouseAtCenter(el, {}, inspector.panelDoc.defaultView);
}

function isInInspectorSearchBox(inspector) {
  // Focus ends up in an anonymous child of the XUL textbox.
  ok(inspector.panelDoc.activeElement.closest("#inspector-searchbox"),
     "The inspector search field is focused when ctrl+F is pressed");
}

function isInRuleViewSearchBox(inspector) {
  is(inspector.panelDoc.activeElement, inspector.getPanel("ruleview").view.searchField,
     "The rule-view search field is focused when ctrl+F is pressed");
}

function isInComputedViewSearchBox(inspector) {
  is(inspector.panelDoc.activeElement,
     inspector.getPanel("computedview").computedView.searchField,
     "The computed-view search field is focused when ctrl+F is pressed");
}
