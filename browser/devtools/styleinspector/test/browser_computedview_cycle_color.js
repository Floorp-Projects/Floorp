/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Computed view color cycling test.

const PAGE_CONTENT = [
  "<style type=\"text/css\">",
  ".matches {color: #F00;}</style>",
  "<span id=\"matches\" class=\"matches\">Some styled text</span>",
  "</div>"
].join("\n");

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8," +
               "Computed view color cycling test.");
  content.document.body.innerHTML = PAGE_CONTENT;

  info("Opening the computed view");
  let {toolbox, inspector, view} = yield openComputedView();

  info("Selecting the test node");
  yield selectNode("#matches", inspector);

  info("Checking the property itself");
  let container = getComputedViewPropertyView(view, "color").valueNode;
  checkColorCycling(container, inspector);

  info("Checking matched selectors");
  container = yield getComputedViewMatchedRules(view, "color");
  checkColorCycling(container, inspector);
});

function checkColorCycling(container, inspector) {
  let swatch = container.querySelector(".computedview-colorswatch");
  let valueNode = container.querySelector(".computedview-color");
  let win = inspector.sidebar.getWindowForTab("computedview");

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
