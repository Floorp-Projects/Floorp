/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the color picker tooltip hides when an image tooltip appears.

const TEST_URI = `
  <style type="text/css">
    body {
      color: red;
      background-color: #ededed;
      background-image: url(chrome://branding/content/icon64.png);
      border: 2em solid rgba(120, 120, 120, .5);
    }
  </style>
  Testing the color picker tooltip!
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { view } = await openRuleView();

  const swatch = getRuleViewProperty(
    view,
    "body",
    "color"
  ).valueSpan.querySelector(".ruleview-colorswatch");

  const bgImageSpan = getRuleViewProperty(view, "body", "background-image")
    .valueSpan;
  const uriSpan = bgImageSpan.querySelector(".theme-link");

  const colorPicker = view.tooltips.getTooltip("colorPicker");
  info("Showing the color picker tooltip by clicking on the color swatch");
  const onColorPickerReady = colorPicker.once("ready");
  swatch.click();
  await onColorPickerReady;

  info("Now showing the image preview tooltip to hide the color picker");
  const onHidden = colorPicker.tooltip.once("hidden");
  // Hiding the color picker refreshes the value.
  const onRuleViewChanged = view.once("ruleview-changed");
  const previewTooltip = await assertShowPreviewTooltip(view, uriSpan);
  await onHidden;
  await onRuleViewChanged;

  await assertTooltipHiddenOnMouseOut(previewTooltip, uriSpan);

  ok(true, "The color picker closed when the image preview tooltip appeared");
});
