/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_html_tooltip.js */

"use strict";

/**
 * Test the TooltipToggle helper class for HTMLTooltip
 */

const HTML_NS = "http://www.w3.org/1999/xhtml";
const TEST_URI = CHROME_URL_ROOT + "doc_html_tooltip_hover.xul";

const {HTMLTooltip} = require("devtools/client/shared/widgets/tooltip/HTMLTooltip");
loadHelperScript("helper_html_tooltip.js");

add_task(function* () {
  let [,, doc] = yield createHost("bottom", TEST_URI);
  // Wait for full page load before synthesizing events on the page.
  yield waitUntil(() => doc.readyState === "complete");

  let width = 100, height = 50;
  let tooltipContent = doc.createElementNS(HTML_NS, "div");
  tooltipContent.textContent = "tooltip";
  let tooltip = new HTMLTooltip(doc, {useXulWrapper: false});
  tooltip.setContent(tooltipContent, {width, height});

  let container = doc.getElementById("container");
  tooltip.startTogglingOnHover(container, () => true);

  info("Hover on each of the 4 boxes, expect the tooltip to appear");
  function* showAndCheck(boxId, position) {
    info(`Show tooltip on ${boxId}`);
    let box = doc.getElementById(boxId);
    let shown = tooltip.once("shown");
    EventUtils.synthesizeMouseAtCenter(box, { type: "mousemove" }, doc.defaultView);
    yield shown;
    checkTooltipGeometry(tooltip, box, {position, width, height});
  }

  yield showAndCheck("box1", "bottom");
  yield showAndCheck("box2", "bottom");
  yield showAndCheck("box3", "top");
  yield showAndCheck("box4", "top");

  info("Move out of the container");
  let hidden = tooltip.once("hidden");
  EventUtils.synthesizeMouseAtCenter(container, { type: "mouseout" }, doc.defaultView);
  yield hidden;

  info("Destroy the tooltip and finish");
  tooltip.destroy();
});
