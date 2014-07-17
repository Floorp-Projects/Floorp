/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test for bug 1026921: Ensure the URL of hovered url() node is used instead
// of the first found from the declaration  as there might be multiple urls.

let YELLOW_DOT = "data:image/png;base64," +
  "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAABmJLR0QA/wD/AP+gvaeTAAAACX" +
  "BIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH3gYcDCwCr0o5ngAAABl0RVh0Q29tbWVudABDcmVh" +
  "dGVkIHdpdGggR0lNUFeBDhcAAAANSURBVAjXY/j/n6EeAAd9An7Z55GEAAAAAElFTkSuQmCC";

let BLUE_DOT = "data:image/png;base64," +
  "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAABmJLR0QA/wD/AP+gvaeTAAAACX" +
  "BIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH3gYcDCwlCkCM9QAAABl0RVh0Q29tbWVudABDcmVh" +
  "dGVkIHdpdGggR0lNUFeBDhcAAAANSURBVAjXY2Bg+F8PAAKCAX/tPkrkAAAAAElFTkSuQmCC";

let test = asyncTest(function* () {
  let TEST_STYLE = "h1 {background: url(" + YELLOW_DOT + "), url(" + BLUE_DOT + ");}";

  let PAGE_CONTENT = "<style>" + TEST_STYLE + "</style>" +
    "<h1>browser_styleinspector_tooltip-multiple-background-images.js</h1>";

  yield addTab("data:text/html;charset=utf-8,background image tooltip test");
  content.document.body.innerHTML = PAGE_CONTENT;

  yield testRuleViewUrls();
  yield testComputedViewUrls();
});

function* testRuleViewUrls() {
  info("Testing tooltips in the rule view");

  let { view, inspector } = yield openRuleView();
  yield selectNode("h1", inspector);

  let {valueSpan} = getRuleViewProperty(view, "h1", "background");
  yield performChecks(view, valueSpan);
}

function* testComputedViewUrls() {
  info("Testing tooltips in the computed view");

  let {view} = yield openComputedView();
  let {valueSpan} = getComputedViewProperty(view, "background-image");

  yield performChecks(view, valueSpan);
}

/**
 * A helper that checks url() tooltips contain correct images
 */
function* performChecks(view, propertyValue) {
  function checkTooltip(panel, imageSrc) {
    let images = panel.getElementsByTagName("image");
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
