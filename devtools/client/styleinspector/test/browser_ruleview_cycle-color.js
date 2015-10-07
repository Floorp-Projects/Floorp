/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test cycling color types in the rule view.

const TEST_URI = `
  <style type="text/css">
    body {
      color: #F00;
    }
  </style>
  Test cycling color types in the rule view!
`;

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openRuleView();
  let container = getRuleViewProperty(view, "body", "color").valueSpan;
  checkColorCycling(container, inspector);
});

function checkColorCycling(container, inspector) {
  let swatch = container.querySelector(".ruleview-colorswatch");
  let valueNode = container.querySelector(".ruleview-color");
  let win = inspector.sidebar.getWindowForTab("ruleview");

  // Hex
  is(valueNode.textContent, "#F00", "Color displayed as a hex value.");

  // HSL
  EventUtils.synthesizeMouseAtCenter(swatch,
                                     {type: "mousedown", shiftKey: true}, win);
  is(valueNode.textContent, "hsl(0, 100%, 50%)",
                            "Color displayed as an HSL value.");

  // RGB
  EventUtils.synthesizeMouseAtCenter(swatch,
                                     {type: "mousedown", shiftKey: true}, win);
  is(valueNode.textContent, "rgb(255, 0, 0)",
                            "Color displayed as an RGB value.");

  // Color name
  EventUtils.synthesizeMouseAtCenter(swatch,
                                     {type: "mousedown", shiftKey: true}, win);
  is(valueNode.textContent, "red",
                            "Color displayed as a color name.");

  // "Authored"
  EventUtils.synthesizeMouseAtCenter(swatch,
                                     {type: "mousedown", shiftKey: true}, win);
  is(valueNode.textContent, "#F00",
                            "Color displayed as an authored value.");

  // One more click skips hex, because it is the same as authored, and
  // instead goes back to HSL.
  EventUtils.synthesizeMouseAtCenter(swatch,
                                     {type: "mousedown", shiftKey: true}, win);
  is(valueNode.textContent, "hsl(0, 100%, 50%)",
                            "Color displayed as an HSL value again.");
}
