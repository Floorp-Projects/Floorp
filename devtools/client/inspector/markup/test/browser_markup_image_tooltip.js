/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that image preview tooltips are shown on img and canvas tags in the
// markup-view and that the tooltip actually contains an image and shows the
// right dimension label

const TEST_NODES = [
  {selector: "img.local", size: "192" + " \u00D7 " + "192"},
  {selector: "img.data", size: "64" + " \u00D7 " + "64"},
  {selector: "img.remote", size: "22" + " \u00D7 " + "23"},
  {selector: ".canvas", size: "600" + " \u00D7 " + "600"}
];

add_task(async function() {
  await addTab(URL_ROOT + "doc_markup_image_and_canvas_2.html");
  const {inspector} = await openInspector();

  info("Selecting the first <img> tag");
  await selectNode("img", inspector);

  for (const testNode of TEST_NODES) {
    const target = await getImageTooltipTarget(testNode, inspector);
    await assertTooltipShownOnHover(inspector.markup.imagePreviewTooltip, target);
    checkImageTooltip(testNode, inspector);
    await assertTooltipHiddenOnMouseOut(inspector.markup.imagePreviewTooltip, target);
  }
});

async function getImageTooltipTarget({selector}, inspector) {
  const nodeFront = await getNodeFront(selector, inspector);
  const isImg = nodeFront.tagName.toLowerCase() === "img";

  const container = getContainerForNodeFront(nodeFront, inspector);

  let target = container.editor.tag;
  if (isImg) {
    target = container.editor.getAttributeElement("src").querySelector(".link");
  }
  return target;
}

function checkImageTooltip({selector, size}, {markup}) {
  const panel = markup.imagePreviewTooltip.panel;
  const images = panel.getElementsByTagName("img");
  is(images.length, 1, "Tooltip for [" + selector + "] contains an image");

  const label = panel.querySelector(".devtools-tooltip-caption");
  is(label.textContent, size,
     "Tooltip label for [" + selector + "] displays the right image size");

  markup.imagePreviewTooltip.hide();
}
