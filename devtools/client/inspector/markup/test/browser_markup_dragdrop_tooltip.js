/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that tooltips don't appear when dragging over tooltip targets.

const TEST_URL = "data:text/html;charset=utf8,<img src=\"about:logo\" /><div>";

add_task(async function() {
  const {inspector} = await openInspectorForURL(TEST_URL);
  const {markup} = inspector;

  info("Get the tooltip target element for the image's src attribute");
  const img = await getContainerForSelector("img", inspector);
  const target = img.editor.getAttributeElement("src").querySelector(".link");

  info("Check that the src attribute of the image is a valid tooltip target");
  await assertTooltipShownOnHover(markup.imagePreviewTooltip, target);
  await assertTooltipHiddenOnMouseOut(markup.imagePreviewTooltip, target);

  info("Start dragging the test div");
  await simulateNodeDrag(inspector, "div");

  info("Now check that the src attribute of the image isn't a valid target");
  const isValid = await markup.imagePreviewTooltip._toggle.isValidHoverTarget(target);
  ok(!isValid, "The element is not a valid tooltip target");

  info("Stop dragging the test div");
  await simulateNodeDrop(inspector, "div");

  info("Check again the src attribute of the image");
  await assertTooltipShownOnHover(markup.imagePreviewTooltip, target);
  await assertTooltipHiddenOnMouseOut(markup.imagePreviewTooltip, target);
});
