/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Checking properties orders and overrides in the rule-view.

const TEST_URI = "<style>#testid {}</style><div id='testid'>Styled Node</div>";

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  yield selectNode("#testid", inspector);

  let elementStyle = view._elementStyle;
  let elementRule = elementStyle.rules[1];

  info("Checking rules insertion order and checking the applied style");
  let firstProp = yield addProperty(view, 1, "background-color", "green");
  let secondProp = yield addProperty(view, 1, "background-color", "blue");

  is(elementRule.textProps[0], firstProp,
     "Rules should be in addition order.");
  is(elementRule.textProps[1], secondProp,
     "Rules should be in addition order.");

  // rgb(0, 0, 255) = blue
  is((yield getValue("#testid", "background-color")), "rgb(0, 0, 255)",
     "Second property should have been used.");

  info("Removing the second property and checking the applied style again");
  yield removeProperty(view, secondProp);
  // rgb(0, 128, 0) = green
  is((yield getValue("#testid", "background-color")), "rgb(0, 128, 0)",
     "After deleting second property, first should be used.");

  info("Creating a new second property and checking that the insertion order " +
     "is still the same");

  secondProp = yield addProperty(view, 1, "background-color", "blue");

  is((yield getValue("#testid", "background-color")), "rgb(0, 0, 255)",
     "New property should be used.");
  is(elementRule.textProps[0], firstProp,
     "Rules shouldn't have switched places.");
  is(elementRule.textProps[1], secondProp,
     "Rules shouldn't have switched places.");

  info("Disabling the second property and checking the applied style");
  yield togglePropStatus(view, secondProp);

  is((yield getValue("#testid", "background-color")), "rgb(0, 128, 0)",
     "After disabling second property, first value should be used");

  info("Disabling the first property too and checking the applied style");
  yield togglePropStatus(view, firstProp);

  is((yield getValue("#testid", "background-color")), "transparent",
     "After disabling both properties, value should be empty.");

  info("Re-enabling the second propertyt and checking the applied style");
  yield togglePropStatus(view, secondProp);

  is((yield getValue("#testid", "background-color")), "rgb(0, 0, 255)",
     "Value should be set correctly after re-enabling");

  info("Re-enabling the first property and checking the insertion order " +
       "is still respected");
  yield togglePropStatus(view, firstProp);

  is((yield getValue("#testid", "background-color")), "rgb(0, 0, 255)",
     "Re-enabling an earlier property shouldn't make it override " +
     "a later property.");
  is(elementRule.textProps[0], firstProp,
     "Rules shouldn't have switched places.");
  is(elementRule.textProps[1], secondProp,
     "Rules shouldn't have switched places.");
  info("Modifying the first property and checking the applied style");
  yield setProperty(view, firstProp, "purple");

  is((yield getValue("#testid", "background-color")), "rgb(0, 0, 255)",
     "Modifying an earlier property shouldn't override a later property.");
});

function* getValue(selector, propName) {
  let value = yield getComputedStyleProperty(selector, null, propName);
  return value;
}
