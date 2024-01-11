/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_html_tooltip.js */

"use strict";

/**
 * Test the HTMLTooltip "doorhanger" type's arrow tip is precisely centered on
 * the anchor when the anchor is small.
 */

const HTML_NS = "http://www.w3.org/1999/xhtml";
const TEST_URI = CHROME_URL_ROOT + "doc_html_tooltip_doorhanger-02.xhtml";

const {
  HTMLTooltip,
} = require("resource://devtools/client/shared/widgets/tooltip/HTMLTooltip.js");
loadHelperScript("helper_html_tooltip.js");

let useXulWrapper;

add_task(async function () {
  // Force the toolbox to be 200px high;
  await pushPref("devtools.toolbox.footer.height", 200);

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
  info("Create HTML tooltip");
  const tooltip = new HTMLTooltip(doc, { type: "doorhanger", useXulWrapper });
  const div = doc.createElementNS(HTML_NS, "div");
  div.style.width = "200px";
  div.style.height = "35px";
  tooltip.panel.appendChild(div);

  const elements = [...doc.querySelectorAll(".anchor")];
  for (const el of elements) {
    info("Display the tooltip on an anchor.");
    await showTooltip(tooltip, el);

    const arrow = tooltip.arrow;
    ok(arrow, "Tooltip has an arrow");

    // Get the geometry of the anchor and arrow.
    const anchorBounds = el.getBoxQuads({ relativeTo: doc })[0].getBounds();
    const arrowBounds = arrow.getBoxQuads({ relativeTo: doc })[0].getBounds();

    // Compare the centers
    const center = bounds => bounds.left + bounds.width / 2;
    const delta = Math.abs(center(anchorBounds) - center(arrowBounds));
    const describeBounds = bounds =>
      `${bounds.left}<--[${center(bounds)}]-->${bounds.right}`;
    const params =
      `anchor: ${describeBounds(anchorBounds)}, ` +
      `arrow: ${describeBounds(arrowBounds)}`;
    ok(
      delta <= 1,
      `Arrow center is roughly aligned with anchor center (${params})`
    );

    await hideTooltip(tooltip);
  }

  tooltip.destroy();
}
