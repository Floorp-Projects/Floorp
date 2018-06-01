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

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openComputedView();
  await selectNode("#matches", inspector);

  info("Checking the property itself");
  let container = getComputedViewPropertyView(view, "color").valueNode;
  await checkColorCycling(container, view);

  info("Checking matched selectors");
  container = await getComputedViewMatchedRules(view, "color");
  await checkColorCycling(container, view);
});

async function checkColorCycling(container, view) {
  const valueNode = container.querySelector(".computed-color");
  const win = view.styleWindow;

  // "Authored" (default; currently the computed value)
  is(valueNode.textContent, "rgb(255, 0, 0)",
                            "Color displayed as an RGB value.");

  const tests = [{
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

  for (const test of tests) {
    await checkSwatchShiftClick(container, win, test.value, test.comment);
  }
}

async function checkSwatchShiftClick(container, win, expectedValue, comment) {
  const swatch = container.querySelector(".computed-colorswatch");
  const valueNode = container.querySelector(".computed-color");
  swatch.scrollIntoView();

  const onUnitChange = swatch.once("unit-change");
  EventUtils.synthesizeMouseAtCenter(swatch, {
    type: "mousedown",
    shiftKey: true
  }, win);
  // we need to have the mouse up event in order to make sure that the platform
  // lets go of the last container, and is not waiting for something to happen.
  // Bug: https://bugzilla.mozilla.org/show_bug.cgi?id=1442153
  EventUtils.synthesizeMouseAtCenter(swatch, {type: "mouseup"}, win);
  await onUnitChange;
  is(valueNode.textContent, expectedValue, comment);
}
