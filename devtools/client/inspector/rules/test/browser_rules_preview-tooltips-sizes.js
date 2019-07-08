/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the dimensions of the preview tooltips are correctly updated to fit their
// content.

// Small 32x32 image.
const BASE_64_URL =
  "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAYAAABzenr" +
  "0AAAAUElEQVRYR+3UsQkAQAhD0TjJ7T+Wk3gbxMIizbcVITwwJWlkZtptpXp+v94TAAEE4gLTvgfOf770RB" +
  "EAAQTiAvEiIgACCMQF4kVEAAQQSAt8xsyeAW6R8eIAAAAASUVORK5CYII=";

add_task(async function() {
  await addTab(
    "data:text/html;charset=utf-8," +
      encodeURIComponent(`
      <style>
        html {
          /* Using a long variable name to ensure preview tooltip for variable will be */
          /* wider than the preview tooltip for the test 32x32 image. */
          --test-var-wider-than-image: red;
        }

        div {
          color: var(--test-var-wider-than-image);
          background: url(${BASE_64_URL});
        }
      </style>
      <div id="target">inspect me</div>
    `)
  );
  const { inspector, view } = await openRuleView();
  await selectNode("#target", inspector);

  // Retrieve the element for `--test-var` on which the CSS variable tooltip will appear.
  const colorPropertySpan = getRuleViewProperty(view, "div", "color").valueSpan;
  const colorVariableElement = colorPropertySpan.querySelector(
    ".ruleview-variable"
  );

  // Retrieve the element for the background url on which the image preview will appear.
  const backgroundPropertySpan = getRuleViewProperty(view, "div", "background")
    .valueSpan;
  const backgroundUrlElement = backgroundPropertySpan.querySelector(
    ".theme-link"
  );

  info("Show preview tooltip for CSS variable");
  let previewTooltip = await assertShowPreviewTooltip(
    view,
    colorVariableElement
  );
  // Measure tooltip dimensions.
  let tooltipRect = previewTooltip.panel.getBoundingClientRect();
  const originalHeight = tooltipRect.height;
  const originalWidth = tooltipRect.width;
  info(`Original dimensions: ${originalWidth} x ${originalHeight}`);
  await assertTooltipHiddenOnMouseOut(previewTooltip, colorVariableElement);

  info("Show preview tooltip for background url");
  previewTooltip = await assertShowPreviewTooltip(view, backgroundUrlElement);
  // Compare new tooltip dimensions to previous measures.
  tooltipRect = previewTooltip.panel.getBoundingClientRect();
  info(
    `Image preview dimensions: ${tooltipRect.width} x ${tooltipRect.height}`
  );
  ok(
    tooltipRect.height > originalHeight,
    "Tooltip is taller for image preview"
  );
  ok(
    tooltipRect.width < originalWidth,
    "Tooltip is narrower for image preview"
  );
  await assertTooltipHiddenOnMouseOut(previewTooltip, colorVariableElement);

  info("Show preview tooltip for CSS variable again");
  previewTooltip = await assertShowPreviewTooltip(view, colorVariableElement);
  // Check measures are identical to initial ones.
  tooltipRect = previewTooltip.panel.getBoundingClientRect();
  info(
    `CSS variable tooltip dimensions: ${tooltipRect.width} x ${
      tooltipRect.height
    }`
  );
  is(
    tooltipRect.height,
    originalHeight,
    "Tooltip has the same height as the original"
  );
  is(
    tooltipRect.width,
    originalWidth,
    "Tooltip has the same width as the original"
  );
  await assertTooltipHiddenOnMouseOut(previewTooltip, colorVariableElement);
});
