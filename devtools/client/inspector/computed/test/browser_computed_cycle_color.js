/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Computed view color cycling test.

const TEST_URI = `
  <style type="text/css">
    .matches {
      color: #f00;
    }
  </style>
  <span id="matches" class="matches">Some styled text</span>
`;

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openComputedView();
  yield selectNode("#matches", inspector);

  info("Checking the property itself");
  let container = getComputedViewPropertyView(view, "color").valueNode;
  checkColorCycling(container, view);

  info("Checking matched selectors");
  container = yield getComputedViewMatchedRules(view, "color");
  yield checkColorCycling(container, view);
});

function* checkColorCycling(container, view) {
  let valueNode = container.querySelector(".computed-color");
  let win = view.styleWindow;

  // "Authored" (default; currently the computed value)
  is(valueNode.textContent, "rgb(255, 0, 0)",
                            "Color displayed as an RGB value.");

  let tests = [{
    value: "red",
    comment: "Color displayed as a color name."
  }, {
    value: "#f00",
    comment: "Color displayed as an authored value."
  }, {
    value: "hsl(0, 100%, 50%)",
    comment: "Color displayed as an HSL value again."
  }, {
    value: "rgb(255, 0, 0)",
    comment: "Color displayed as an RGB value again."
  }];

  for (let test of tests) {
    yield checkSwatchShiftClick(container, win, test.value, test.comment);
  }
}

function* checkSwatchShiftClick(container, win, expectedValue, comment) {
  let swatch = container.querySelector(".computed-colorswatch");
  let valueNode = container.querySelector(".computed-color");
  swatch.scrollIntoView();

  let onUnitChange = swatch.once("unit-change");
  EventUtils.synthesizeMouseAtCenter(swatch, {
    type: "mousedown",
    shiftKey: true
  }, win);
  yield onUnitChange;
  is(valueNode.textContent, expectedValue, comment);
}
