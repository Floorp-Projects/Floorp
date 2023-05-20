/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = `
  <style>
    div {
      color: red;
      width: 10; /* This document is in quirks mode so this value should be valid */
    }
  </style>
  <div></div>
`;

// Test that CSS property names are case insensitive when validating, and that
// quirks mode is accounted for when validating.
add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view: ruleView } = await openRuleView();

  await selectNode("div", inspector);
  let prop = getTextProperty(ruleView, 1, { color: "red" });

  let onRuleViewChanged;

  info(`Rename the CSS property name to "Color"`);
  onRuleViewChanged = ruleView.once("ruleview-changed");
  await renameProperty(ruleView, prop, "Color");
  info("Wait for Rule view to update");
  await onRuleViewChanged;

  is(prop.overridden, false, "Titlecase property is not overriden");
  is(prop.enabled, true, "Titlecase property is enabled");
  is(prop.isNameValid(), true, "Titlecase property is valid");

  info(`Rename the CSS property name to "COLOR"`);
  onRuleViewChanged = ruleView.once("ruleview-changed");
  await renameProperty(ruleView, prop, "COLOR");
  info("Wait for Rule view to update");
  await onRuleViewChanged;

  is(prop.overridden, false, "Uppercase property is not overriden");
  is(prop.enabled, true, "Uppercase property is enabled");
  is(prop.isNameValid(), true, "Uppercase property is valid");

  info(`Checking width validity`);
  prop = getTextProperty(ruleView, 1, { width: "10" });
  is(prop.isNameValid(), true, "width is a valid property");
  is(prop.isValid(), true, "10 is a valid property value in quirks mode");
});
