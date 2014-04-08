/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Checking properties orders and overrides in the rule-view

let test = asyncTest(function*() {
  yield addTab("data:text/html;charset=utf-8,browser_ruleview_manipulation.js");
  let {toolbox, inspector, view} = yield openRuleView();

  info("Creating the test document and getting the test node");
  content.document.body.innerHTML = '<div id="testid">Styled Node</div>';
  let element = getNode("#testid");

  yield selectNode(element, inspector);

  let elementStyle = view._elementStyle;
  let elementRule = elementStyle.rules[0];

  info("Checking rules insertion order and checking the applied style");
  let firstProp = elementRule.createProperty("background-color", "green", "");
  let secondProp = elementRule.createProperty("background-color", "blue", "");
  is(elementRule.textProps[0], firstProp, "Rules should be in addition order.");
  is(elementRule.textProps[1], secondProp, "Rules should be in addition order.");
  yield elementRule._applyingModifications;
  is(element.style.getPropertyValue("background-color"), "blue", "Second property should have been used.");

  info("Removing the second property and checking the applied style again");
  secondProp.remove();
  yield elementRule._applyingModifications;
  is(element.style.getPropertyValue("background-color"), "green", "After deleting second property, first should be used.");

  info("Creating a new second property and checking that the insertion order is still the same");
  secondProp = elementRule.createProperty("background-color", "blue", "");
  yield elementRule._applyingModifications;
  is(element.style.getPropertyValue("background-color"), "blue", "New property should be used.");
  is(elementRule.textProps[0], firstProp, "Rules shouldn't have switched places.");
  is(elementRule.textProps[1], secondProp, "Rules shouldn't have switched places.");

  info("Disabling the second property and checking the applied style");
  secondProp.setEnabled(false);
  yield elementRule._applyingModifications;
  is(element.style.getPropertyValue("background-color"), "green", "After disabling second property, first value should be used");

  info("Disabling the first property too and checking the applied style");
  firstProp.setEnabled(false);
  yield elementRule._applyingModifications;
  is(element.style.getPropertyValue("background-color"), "", "After disabling both properties, value should be empty.");

  info("Re-enabling the second propertyt and checking the applied style");
  secondProp.setEnabled(true);
  yield elementRule._applyingModifications;
  is(element.style.getPropertyValue("background-color"), "blue", "Value should be set correctly after re-enabling");

  info("Re-enabling the first property and checking the insertion order is still respected");
  firstProp.setEnabled(true);
  yield elementRule._applyingModifications;
  is(element.style.getPropertyValue("background-color"), "blue", "Re-enabling an earlier property shouldn't make it override a later property.");
  is(elementRule.textProps[0], firstProp, "Rules shouldn't have switched places.");
  is(elementRule.textProps[1], secondProp, "Rules shouldn't have switched places.");

  info("Modifying the first property and checking the applied style");
  firstProp.setValue("purple", "");
  yield elementRule._applyingModifications;
  is(element.style.getPropertyValue("background-color"), "blue", "Modifying an earlier property shouldn't override a later property.");
});
