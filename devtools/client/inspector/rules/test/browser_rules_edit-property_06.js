/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that editing a property's priority is behaving correctly, and disabling
// and editing the property will re-enable the property.

const TEST_URI = `
  <style type='text/css'>
  body {
    background-color: green !important;
  }
  body {
    background-color: red;
  }
  </style>
`;

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("body", inspector);

  let rule = getRuleViewRuleEditor(view, 1).rule;
  let prop = rule.textProps[0];

  is((yield getComputedStyleProperty("body", null, "background-color")),
    "rgb(0, 128, 0)", "green background color is set.");

  yield setProperty(view, prop, "red !important");

  is(prop.editor.valueSpan.textContent, "red !important",
    "'red !important' property value is correctly set.");
  is((yield getComputedStyleProperty("body", null, "background-color")),
    "rgb(255, 0, 0)", "red background color is set.");

  info("Disabling red background color property");
  yield togglePropStatus(view, prop);

  is((yield getComputedStyleProperty("body", null, "background-color")),
    "rgb(0, 128, 0)", "green background color is set.");

  yield setProperty(view, prop, "red");

  is(prop.editor.valueSpan.textContent, "red",
    "'red' property value is correctly set.");
  ok(prop.enabled, "red background-color property is enabled.");
  is((yield getComputedStyleProperty("body", null, "background-color")),
    "rgb(0, 128, 0)", "green background color is set.");
});
