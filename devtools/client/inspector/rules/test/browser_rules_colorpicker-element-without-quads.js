/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = `
  <style type="text/css">
    button {
      color: tomato;
      background-color: green;
    }
  </style>
  <!-- The space as text context for the button is mandatory here, so that the
       firstChild of the button is an whitespace text node -->
  <button id="button-with-no-quads"> </button>
`;

/**
 * Check that we can still open the color picker on elements for which getQuads
 * returns an empty array, which is used to compute the background-color
 * contrast.
 */
add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));

  const hasEmptyQuads = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    () => {
      const button = content.document.querySelector("button");
      const quads = button.firstChild.getBoxQuads({
        box: "content",
        relativeTo: content.document,
        createFramesForSuppressedWhitespace: false,
      });
      return quads.length === 0;
    }
  );
  ok(hasEmptyQuads, "The test element has empty quads");

  const { inspector, view } = await openRuleView();

  await selectNode("button", inspector);

  const ruleEl = getRuleViewProperty(view, "button", "color");
  ok(ruleEl, "The color declaration was found");

  const swatchEl = ruleEl.valueSpan.querySelector(".ruleview-colorswatch");
  const colorEl = ruleEl.valueSpan.querySelector(".ruleview-color");
  is(colorEl.textContent, "tomato", "The right color value was found");

  info("Open the color picker");
  const cPicker = view.tooltips.getTooltip("colorPicker");
  const onColorPickerReady = cPicker.once("ready");
  swatchEl.click();
  await onColorPickerReady;

  info("Check that the background color of the button was correctly detected");
  const contrastEl = cPicker.tooltip.container.querySelector(
    ".contrast-value-and-swatch.contrast-ratio-single"
  );
  ok(
    contrastEl.style.cssText.includes(
      "--accessibility-contrast-bg: rgba(0,128,0,1)"
    ),
    "The background color contains the expected value"
  );

  info("Check that the color picker can be used");
  await simulateColorPickerChange(view, cPicker, [255, 0, 0, 1], {
    selector: "button",
    name: "color",
    value: "rgb(255, 0, 0)",
  });

  await hideTooltipAndWaitForRuleViewChanged(cPicker, view);
});
