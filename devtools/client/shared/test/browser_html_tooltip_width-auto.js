/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_html_tooltip.js */

"use strict";

/**
 * Test the HTMLTooltip content can automatically calculate its width based on content.
 */

const HTML_NS = "http://www.w3.org/1999/xhtml";
const TEST_URI = CHROME_URL_ROOT + "doc_html_tooltip.xhtml";

const {
  HTMLTooltip,
} = require("devtools/client/shared/widgets/tooltip/HTMLTooltip");
loadHelperScript("helper_html_tooltip.js");

let useXulWrapper;

add_task(async function() {
  await addTab("about:blank");
  const { doc } = await createHost("bottom", TEST_URI);

  info("Run tests for a Tooltip without using a XUL panel");
  useXulWrapper = false;
  await runTests(doc);

  info("Run tests for a Tooltip with a XUL panel");
  useXulWrapper = true;
  await runTests(doc);
});

async function runTests(doc) {
  const tooltip = new HTMLTooltip(doc, { useXulWrapper });
  info("Create tooltip content width to 150px");
  const tooltipContent = doc.createElementNS(HTML_NS, "div");
  tooltipContent.style.cssText = "height: 100%; width: 150px; background: red;";

  info("Set tooltip content using width:auto");
  tooltip.panel.appendChild(tooltipContent);
  tooltip.setContentSize({ width: "auto", height: 50 });

  info("Show the tooltip and check the tooltip panel width.");
  await showTooltip(tooltip, doc.getElementById("box1"));

  const containerRect = tooltip.container.getBoundingClientRect();
  is(containerRect.width, 150, "Tooltip container has the expected width.");

  await hideTooltip(tooltip);

  tooltip.destroy();
}
