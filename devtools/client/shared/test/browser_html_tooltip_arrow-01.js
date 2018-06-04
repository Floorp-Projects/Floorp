/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_html_tooltip.js */

"use strict";

/**
 * Test the HTMLTooltip "arrow" type on small anchors. The arrow should remain
 * aligned with the anchors as much as possible
 */

const HTML_NS = "http://www.w3.org/1999/xhtml";
const TEST_URI = CHROME_URL_ROOT + "doc_html_tooltip_arrow-01.xul";

const {HTMLTooltip} = require("devtools/client/shared/widgets/tooltip/HTMLTooltip");
loadHelperScript("helper_html_tooltip.js");

let useXulWrapper;

add_task(async function() {
  // Force the toolbox to be 200px high;
  await pushPref("devtools.toolbox.footer.height", 200);

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
  info("Create HTML tooltip");
  const tooltip = new HTMLTooltip(doc, {type: "arrow", useXulWrapper});
  const div = doc.createElementNS(HTML_NS, "div");
  div.style.height = "35px";
  tooltip.setContent(div, {width: 200, height: 35});

  const {right: docRight} = doc.documentElement.getBoundingClientRect();

  const elements = [...doc.querySelectorAll(".anchor")];
  for (const el of elements) {
    info("Display the tooltip on an anchor.");
    await showTooltip(tooltip, el);

    const arrow = tooltip.arrow;
    ok(arrow, "Tooltip has an arrow");

    // Get the geometry of the anchor, the tooltip panel & arrow.
    const arrowBounds = arrow.getBoxQuads({relativeTo: doc})[0].getBounds();
    const panelBounds = tooltip.panel.getBoxQuads({relativeTo: doc})[0].getBounds();
    const anchorBounds = el.getBoxQuads({relativeTo: doc})[0].getBounds();

    const intersects = arrowBounds.left <= anchorBounds.right &&
                     arrowBounds.right >= anchorBounds.left;
    const isBlockedByViewport = arrowBounds.left == 0 ||
                              arrowBounds.right == docRight;
    ok(intersects || isBlockedByViewport,
      "Tooltip arrow is aligned with the anchor, or stuck on viewport's edge.");

    const isInPanel = arrowBounds.left >= panelBounds.left &&
                    arrowBounds.right <= panelBounds.right;
    ok(isInPanel,
      "The tooltip arrow remains inside the tooltip panel horizontally");

    await hideTooltip(tooltip);
  }

  tooltip.destroy();
}
