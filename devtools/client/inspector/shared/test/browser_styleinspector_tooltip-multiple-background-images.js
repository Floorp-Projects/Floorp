/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test for bug 1026921: Ensure the URL of hovered url() node is used instead
// of the first found from the declaration  as there might be multiple urls.

const YELLOW_DOT = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH3gYcDCwCr0o5ngAAABl0RVh0Q29tbWVudABDcmVhdGVkIHdpdGggR0lNUFeBDhcAAAANSURBVAjXY/j/n6EeAAd9An7Z55GEAAAAAElFTkSuQmCC";
const BLUE_DOT = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH3gYcDCwlCkCM9QAAABl0RVh0Q29tbWVudABDcmVhdGVkIHdpdGggR0lNUFeBDhcAAAANSURBVAjXY2Bg+F8PAAKCAX/tPkrkAAAAAElFTkSuQmCC";
const TEST_STYLE = `h1 {background: url(${YELLOW_DOT}), url(${BLUE_DOT});}`;
const TEST_URI = `<style>${TEST_STYLE}</style><h1>test element</h1>`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector} = await openInspector();

  await testRuleViewUrls(inspector);
  await testComputedViewUrls(inspector);
});

async function testRuleViewUrls(inspector) {
  info("Testing tooltips in the rule view");
  const view = selectRuleView(inspector);
  await selectNode("h1", inspector);

  const {valueSpan} = getRuleViewProperty(view, "h1", "background");
  await performChecks(view, valueSpan);
}

async function testComputedViewUrls(inspector) {
  info("Testing tooltips in the computed view");

  const onComputedViewReady = inspector.once("computed-view-refreshed");
  const view = selectComputedView(inspector);
  await onComputedViewReady;

  const {valueSpan} = getComputedViewProperty(view, "background-image");

  await performChecks(view, valueSpan);
}

/**
 * A helper that checks url() tooltips contain correct images
 */
async function performChecks(view, propertyValue) {
  function checkTooltip(panel, imageSrc) {
    const images = panel.getElementsByTagName("img");
    is(images.length, 1, "Tooltip contains an image");
    is(images[0].getAttribute("src"), imageSrc, "The image URL is correct");
  }

  const links = propertyValue.querySelectorAll(".theme-link");

  info("Checking first link tooltip");
  let previewTooltip = await assertShowPreviewTooltip(view, links[0]);
  const panel = view.tooltips.getTooltip("previewTooltip").panel;
  checkTooltip(panel, YELLOW_DOT);

  await assertTooltipHiddenOnMouseOut(previewTooltip, links[0]);

  info("Checking second link tooltip");
  previewTooltip = await assertShowPreviewTooltip(view, links[1]);
  checkTooltip(panel, BLUE_DOT);

  await assertTooltipHiddenOnMouseOut(previewTooltip, links[1]);
}
