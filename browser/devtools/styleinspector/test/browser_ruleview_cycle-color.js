/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test cycling color types in the rule view.

const PAGE_CONTENT = [
  "<style type=\"text/css\">",
  "  body {",
  "    color: #F00;",
  "  }",
  "</style>",
  "Test cycling color types in the rule view!"
].join("\n");

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8,Test cycling color types in the " +
               "rule view.");
  content.document.body.innerHTML = PAGE_CONTENT;
  let {toolbox, inspector, view} = yield openRuleView();

  let container = getRuleViewProperty(view, "body", "color").valueSpan;
  checkColorCycling(container, inspector);
});

function checkColorCycling(container, inspector) {
  let swatch = container.querySelector(".ruleview-colorswatch");
  let valueNode = container.querySelector(".ruleview-color");
  let win = inspector.sidebar.getWindowForTab("ruleview");

  // Hex (default)
  is(valueNode.textContent, "#F00", "Color displayed as a hex value.");

  // HSL
  EventUtils.synthesizeMouseAtCenter(swatch, {type: "mousedown", shiftKey: true}, win);
  is(valueNode.textContent, "hsl(0, 100%, 50%)",
                            "Color displayed as an HSL value.");

  // RGB
  EventUtils.synthesizeMouseAtCenter(swatch, {type: "mousedown", shiftKey: true}, win);
  is(valueNode.textContent, "rgb(255, 0, 0)",
                            "Color displayed as an RGB value.");

  // Color name
  EventUtils.synthesizeMouseAtCenter(swatch, {type: "mousedown", shiftKey: true}, win);
  is(valueNode.textContent, "red",
                            "Color displayed as a color name.");

  // "Authored" (currently the computed value)
  EventUtils.synthesizeMouseAtCenter(swatch, {type: "mousedown", shiftKey: true}, win);
  is(valueNode.textContent, "rgb(255, 0, 0)",
                            "Color displayed as an RGB value.");

  // Back to hex
  EventUtils.synthesizeMouseAtCenter(swatch, {type: "mousedown", shiftKey: true}, win);
  is(valueNode.textContent, "#F00",
                            "Color displayed as hex again.");
}
