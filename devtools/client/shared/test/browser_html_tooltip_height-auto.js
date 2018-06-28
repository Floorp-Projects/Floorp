/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_html_tooltip.js */

"use strict";

/**
 * Test the HTMLTooltip content can automatically calculate its height based on
 * content.
 */

const HTML_NS = "http://www.w3.org/1999/xhtml";
const TEST_URI = CHROME_URL_ROOT + "doc_html_tooltip.xul";

const {HTMLTooltip} =
  require("devtools/client/shared/widgets/tooltip/HTMLTooltip");
loadHelperScript("helper_html_tooltip.js");

let useXulWrapper;

add_task(async function() {
  await addTab("about:blank");
  const [,, doc] = await createHost("bottom", TEST_URI);

  info("Run tests for a Tooltip without using a XUL panel");
  useXulWrapper = false;
  await runTests(doc);

  info("Run tests for a Tooltip with a XUL panel");
  useXulWrapper = true;
  await runTests(doc);
});

async function runTests(doc) {
  const tooltip = new HTMLTooltip(doc, {useXulWrapper});
  info("Create tooltip content height to 150px");
  const tooltipContent = doc.createElementNS(HTML_NS, "div");
  tooltipContent.style.cssText =
    "width: 300px; height: 150px; background: red;";

  info("Set tooltip content using width:auto and height:auto");
  tooltip.setContent(tooltipContent);

  info("Show the tooltip and check the tooltip panel dimensions.");
  await showTooltip(tooltip, doc.getElementById("box1"));

  let panelRect = tooltip.panel.getBoundingClientRect();
  is(panelRect.width, 300, "Tooltip panel has the expected width.");
  is(panelRect.height, 150, "Tooltip panel has the expected width.");

  await hideTooltip(tooltip);

  info("Set tooltip content using fixed width and height:auto");
  tooltipContent.style.cssText =
    "width: auto; height: 200px; background: red;";
  tooltip.setContent(tooltipContent, { width: 400 });

  info("Show the tooltip and check the tooltip panel height.");
  await showTooltip(tooltip, doc.getElementById("box1"));

  panelRect = tooltip.panel.getBoundingClientRect();
  is(panelRect.height, 200, "Tooltip panel has the expected width.");

  await hideTooltip(tooltip);

  tooltip.destroy();
}
