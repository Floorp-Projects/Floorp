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

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector} = yield openInspector();

  yield testRuleViewUrls(inspector);
  yield testComputedViewUrls(inspector);
});

function* testRuleViewUrls(inspector) {
  info("Testing tooltips in the rule view");
  let view = selectRuleView(inspector);
  yield selectNode("h1", inspector);

  let {valueSpan} = getRuleViewProperty(view, "h1", "background");
  yield performChecks(view, valueSpan);
}

function* testComputedViewUrls(inspector) {
  info("Testing tooltips in the computed view");

  let onComputedViewReady = inspector.once("computed-view-refreshed");
  let view = selectComputedView(inspector);
  yield onComputedViewReady;

  let {valueSpan} = getComputedViewProperty(view, "background-image");

  yield performChecks(view, valueSpan);
}

/**
 * A helper that checks url() tooltips contain correct images
 */
function* performChecks(view, propertyValue) {
  function checkTooltip(panel, imageSrc) {
    let images = panel.getElementsByTagName("img");
    is(images.length, 1, "Tooltip contains an image");
    is(images[0].getAttribute("src"), imageSrc, "The image URL is correct");
  }

  let links = propertyValue.querySelectorAll(".theme-link");
  let panel = view.tooltips.previewTooltip.panel;

  info("Checking first link tooltip");
  yield assertHoverTooltipOn(view.tooltips.previewTooltip, links[0]);
  checkTooltip(panel, YELLOW_DOT);

  info("Checking second link tooltip");
  yield assertHoverTooltipOn(view.tooltips.previewTooltip, links[1]);
  checkTooltip(panel, BLUE_DOT);
}
