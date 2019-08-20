/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_html_tooltip.js */

"use strict";

/**
 * Test the HTMLTooltip show can be called several times. It should move according to the
 * new anchor/options and should not leak event listeners.
 */

const HTML_NS = "http://www.w3.org/1999/xhtml";
const TEST_URI = CHROME_URL_ROOT + "doc_html_tooltip.xul";

const {
  HTMLTooltip,
} = require("devtools/client/shared/widgets/tooltip/HTMLTooltip");
loadHelperScript("helper_html_tooltip.js");

function getTooltipContent(doc) {
  const div = doc.createElementNS(HTML_NS, "div");
  div.style.height = "50px";
  div.textContent = "tooltip";
  return div;
}

add_task(async function() {
  // Run DevTools in a chrome frame temporarily, otherwise this test is intermittent.
  // See Bug 1571421.
  await pushPref("devtools.toolbox.content-frame", false);

  const [, , doc] = await createHost("bottom", TEST_URI);

  const box1 = doc.getElementById("box1");
  const box2 = doc.getElementById("box2");
  const box3 = doc.getElementById("box3");
  const box4 = doc.getElementById("box4");

  const width = 100,
    height = 50;

  const tooltip = new HTMLTooltip(doc, { useXulWrapper: false });
  tooltip.panel.appendChild(getTooltipContent(doc));
  tooltip.setContentSize({ width, height });

  info(
    "Show the tooltip on each of the 4 hbox, without calling hide in between"
  );

  info("Show tooltip on box1");
  tooltip.show(box1);
  checkTooltipGeometry(tooltip, box1, { position: "bottom", width, height });

  info("Show tooltip on box2");
  tooltip.show(box2);
  checkTooltipGeometry(tooltip, box2, { position: "bottom", width, height });

  info("Show tooltip on box3");
  tooltip.show(box3);
  checkTooltipGeometry(tooltip, box3, { position: "top", width, height });

  info("Show tooltip on box4");
  tooltip.show(box4);
  checkTooltipGeometry(tooltip, box4, { position: "top", width, height });

  info("Hide tooltip before leaving test");
  await hideTooltip(tooltip);

  tooltip.destroy();
});
