/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_html_tooltip.js */

"use strict";

/**
 * Test the HTMLTooltip content can automatically calculate its height based on
 * content.
 */

const HTML_NS = "http://www.w3.org/1999/xhtml";
const TEST_URI = CHROME_URL_ROOT + "doc_html_tooltip.xhtml";

const {
  HTMLTooltip,
} = require("resource://devtools/client/shared/widgets/tooltip/HTMLTooltip.js");
loadHelperScript("helper_html_tooltip.js");

let useXulWrapper;

add_task(async function () {
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
  info("Create tooltip content height to 150px");
  const tooltipContent = doc.createElementNS(HTML_NS, "div");
  tooltipContent.style.cssText =
    "width: 300px; height: 150px; background: red;";

  info("Set tooltip content using width:auto and height:auto");
  tooltip.panel.appendChild(tooltipContent);

  info("Show the tooltip and check the tooltip container dimensions.");
  await showTooltip(tooltip, doc.getElementById("box1"));

  let panelRect = tooltip.container.getBoundingClientRect();
  is(panelRect.width, 300, "Tooltip container has the expected width.");
  is(panelRect.height, 150, "Tooltip container has the expected height.");

  await hideTooltip(tooltip);

  info("Set tooltip content using fixed width and height:auto");
  tooltipContent.style.cssText = "width: auto; height: 160px; background: red;";
  tooltip.setContentSize({ width: 400 });

  info("Show the tooltip and check the tooltip container height.");
  await showTooltip(tooltip, doc.getElementById("box1"));

  panelRect = tooltip.container.getBoundingClientRect();
  is(panelRect.height, 160, "Tooltip container has the expected height.");

  await hideTooltip(tooltip);

  info("Update the height and show the tooltip again");
  tooltipContent.style.cssText = "width: auto; height: 165px; background: red;";

  await showTooltip(tooltip, doc.getElementById("box1"));

  panelRect = tooltip.container.getBoundingClientRect();
  is(
    panelRect.height,
    165,
    "Tooltip container has the expected updated height."
  );

  await hideTooltip(tooltip);

  info(
    "Check that refreshing the tooltip when it overflows does keep scroll position"
  );
  // Set the tooltip panel to overflow. Some consumers of the HTMLTooltip are doing that
  // via CSS (e.g. the iframe dropdown, the context selector, …).
  tooltip.panel.style.overflowY = "auto";
  tooltipContent.style.cssText =
    "width: auto; height: 3000px; background: tomato;";
  await showTooltip(tooltip, doc.getElementById("box1"));

  ok(
    tooltip.panel.scrollHeight > tooltip.panel.clientHeight,
    "Tooltip overflows"
  );

  const scrollPosition = 500;
  tooltip.panel.scrollTop = scrollPosition;

  await showTooltip(tooltip, doc.getElementById("box1"));
  is(
    tooltip.panel.scrollTop,
    scrollPosition,
    "scroll position was kept during the update"
  );
  await hideTooltip(tooltip);

  tooltip.destroy();
}
