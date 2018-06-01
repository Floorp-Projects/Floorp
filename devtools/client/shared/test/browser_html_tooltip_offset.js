/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_html_tooltip.js */
"use strict";

/**
 * Test the HTMLTooltip can be displayed with vertical and horizontal offsets.
 */

const HTML_NS = "http://www.w3.org/1999/xhtml";
const TEST_URI = CHROME_URL_ROOT + "doc_html_tooltip.xul";

const {HTMLTooltip} = require("devtools/client/shared/widgets/tooltip/HTMLTooltip");
loadHelperScript("helper_html_tooltip.js");

add_task(async function() {
  // Force the toolbox to be 200px high;
  await pushPref("devtools.toolbox.footer.height", 200);

  const [,, doc] = await createHost("bottom", TEST_URI);

  info("Test a tooltip is not closed when clicking inside itself");

  const box1 = doc.getElementById("box1");
  const box2 = doc.getElementById("box2");
  const box3 = doc.getElementById("box3");
  const box4 = doc.getElementById("box4");

  const tooltip = new HTMLTooltip(doc, {useXulWrapper: false});

  const div = doc.createElementNS(HTML_NS, "div");
  div.style.height = "100px";
  div.style.boxSizing = "border-box";
  div.textContent = "tooltip";
  tooltip.setContent(div, {width: 50, height: 100});

  info("Display the tooltip on box1.");
  await showTooltip(tooltip, box1, {x: 5, y: 10});

  let panelRect = tooltip.container.getBoundingClientRect();
  let anchorRect = box1.getBoundingClientRect();

  // Tooltip will be displayed below box1
  is(panelRect.top, anchorRect.bottom + 10, "Tooltip top has 10px offset");
  is(panelRect.left, anchorRect.left + 5, "Tooltip left has 5px offset");
  is(panelRect.height, 100, "Tooltip height is at 100px as expected");

  info("Display the tooltip on box2.");
  await showTooltip(tooltip, box2, {x: 5, y: 10});

  panelRect = tooltip.container.getBoundingClientRect();
  anchorRect = box2.getBoundingClientRect();

  // Tooltip will be displayed below box2, but can't be fully displayed because of the
  // offset
  is(panelRect.top, anchorRect.bottom + 10, "Tooltip top has 10px offset");
  is(panelRect.left, anchorRect.left + 5, "Tooltip left has 5px offset");
  is(panelRect.height, 90, "Tooltip height is only 90px");

  info("Display the tooltip on box3.");
  await showTooltip(tooltip, box3, {x: 5, y: 10});

  panelRect = tooltip.container.getBoundingClientRect();
  anchorRect = box3.getBoundingClientRect();

  // Tooltip will be displayed above box3, but can't be fully displayed because of the
  // offset
  is(panelRect.bottom, anchorRect.top - 10, "Tooltip bottom is 10px above anchor");
  is(panelRect.left, anchorRect.left + 5, "Tooltip left has 5px offset");
  is(panelRect.height, 90, "Tooltip height is only 90px");

  info("Display the tooltip on box4.");
  await showTooltip(tooltip, box4, {x: 5, y: 10});

  panelRect = tooltip.container.getBoundingClientRect();
  anchorRect = box4.getBoundingClientRect();

  // Tooltip will be displayed above box4
  is(panelRect.bottom, anchorRect.top - 10, "Tooltip bottom is 10px above anchor");
  is(panelRect.left, anchorRect.left + 5, "Tooltip left has 5px offset");
  is(panelRect.height, 100, "Tooltip height is at 100px as expected");

  await hideTooltip(tooltip);

  tooltip.destroy();
});
