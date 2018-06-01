/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_html_tooltip.js */

"use strict";

/**
 * Test the HTMLTooltip positioning for a huge tooltip element (can not fit in
 * the viewport).
 */

const HTML_NS = "http://www.w3.org/1999/xhtml";
const TEST_URI = CHROME_URL_ROOT + "doc_html_tooltip-05.xul";

const {HTMLTooltip} = require("devtools/client/shared/widgets/tooltip/HTMLTooltip");
loadHelperScript("helper_html_tooltip.js");

const TOOLTIP_HEIGHT = 200;
const TOOLTIP_WIDTH = 200;

add_task(async function() {
  // Force the toolbox to be 200px high;
  await pushPref("devtools.toolbox.footer.height", 200);
  await addTab("about:blank");
  const [,, doc] = await createHost("bottom", TEST_URI);

  info("Create HTML tooltip");
  const tooltip = new HTMLTooltip(doc, {useXulWrapper: false});
  const div = doc.createElementNS(HTML_NS, "div");
  div.style.height = "100%";
  tooltip.setContent(div, {width: TOOLTIP_WIDTH, height: TOOLTIP_HEIGHT});

  const box1 = doc.getElementById("box1");
  const box2 = doc.getElementById("box2");
  const box3 = doc.getElementById("box3");
  const box4 = doc.getElementById("box4");
  const width = TOOLTIP_WIDTH;

  // box1: Can not fit above or below box1, default to bottom with a reduced
  // height of 150px.
  info("Display the tooltip on box1.");
  await showTooltip(tooltip, box1);
  let expectedTooltipGeometry = {position: "bottom", height: 150, width};
  checkTooltipGeometry(tooltip, box1, expectedTooltipGeometry);
  await hideTooltip(tooltip);

  info("Try to display the tooltip on top of box1.");
  await showTooltip(tooltip, box1, {position: "top"});
  expectedTooltipGeometry = {position: "bottom", height: 150, width};
  checkTooltipGeometry(tooltip, box1, expectedTooltipGeometry);
  await hideTooltip(tooltip);

  // box2: Can not fit above or below box2, default to bottom with a reduced
  // height of 100px.
  info("Try to display the tooltip on box2.");
  await showTooltip(tooltip, box2);
  expectedTooltipGeometry = {position: "bottom", height: 100, width};
  checkTooltipGeometry(tooltip, box2, expectedTooltipGeometry);
  await hideTooltip(tooltip);

  info("Try to display the tooltip on top of box2.");
  await showTooltip(tooltip, box2, {position: "top"});
  expectedTooltipGeometry = {position: "bottom", height: 100, width};
  checkTooltipGeometry(tooltip, box2, expectedTooltipGeometry);
  await hideTooltip(tooltip);

  // box3: Can not fit above or below box3, default to top with a reduced height
  // of 100px.
  info("Try to display the tooltip on box3.");
  await showTooltip(tooltip, box3);
  expectedTooltipGeometry = {position: "top", height: 100, width};
  checkTooltipGeometry(tooltip, box3, expectedTooltipGeometry);
  await hideTooltip(tooltip);

  info("Try to display the tooltip on bottom of box3.");
  await showTooltip(tooltip, box3, {position: "bottom"});
  expectedTooltipGeometry = {position: "top", height: 100, width};
  checkTooltipGeometry(tooltip, box3, expectedTooltipGeometry);
  await hideTooltip(tooltip);

  // box4: Can not fit above or below box4, default to top with a reduced height
  // of 150px.
  info("Display the tooltip on box4.");
  await showTooltip(tooltip, box4);
  expectedTooltipGeometry = {position: "top", height: 150, width};
  checkTooltipGeometry(tooltip, box4, expectedTooltipGeometry);
  await hideTooltip(tooltip);

  info("Try to display the tooltip on bottom of box4.");
  await showTooltip(tooltip, box4, {position: "bottom"});
  expectedTooltipGeometry = {position: "top", height: 150, width};
  checkTooltipGeometry(tooltip, box4, expectedTooltipGeometry);
  await hideTooltip(tooltip);

  is(tooltip.isVisible(), false, "Tooltip is not visible");

  tooltip.destroy();
});
