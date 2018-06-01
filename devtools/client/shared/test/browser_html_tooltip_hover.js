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

add_task(async function() {
  const [,, doc] = await createHost("bottom", TEST_URI);
  // Wait for full page load before synthesizing events on the page.
  await waitUntil(() => doc.readyState === "complete");

  const width = 100, height = 50;
  const tooltipContent = doc.createElementNS(HTML_NS, "div");
  tooltipContent.textContent = "tooltip";
  const tooltip = new HTMLTooltip(doc, {useXulWrapper: false});
  tooltip.setContent(tooltipContent, {width, height});

  const container = doc.getElementById("container");
  tooltip.startTogglingOnHover(container, () => true);

  info("Hover on each of the 4 boxes, expect the tooltip to appear");
  async function showAndCheck(boxId, position) {
    info(`Show tooltip on ${boxId}`);
    const box = doc.getElementById(boxId);
    const shown = tooltip.once("shown");
    EventUtils.synthesizeMouseAtCenter(box, { type: "mousemove" }, doc.defaultView);
    await shown;
    checkTooltipGeometry(tooltip, box, {position, width, height});
  }

  await showAndCheck("box1", "bottom");
  await showAndCheck("box2", "bottom");
  await showAndCheck("box3", "top");
  await showAndCheck("box4", "top");

  info("Move out of the container");
  const hidden = tooltip.once("hidden");
  EventUtils.synthesizeMouseAtCenter(container, { type: "mousemove" }, doc.defaultView);
  await hidden;

  info("Destroy the tooltip and finish");
  tooltip.destroy();
});
