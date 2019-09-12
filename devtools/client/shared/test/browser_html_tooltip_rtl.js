/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_html_tooltip.js */
"use strict";

/**
 * Test the HTMLTooltip anchor alignment changes with the anchor direction.
 * - should be aligned to the right of RTL anchors
 * - should be aligned to the left of LTR anchors
 */

const HTML_NS = "http://www.w3.org/1999/xhtml";
const TEST_URI = CHROME_URL_ROOT + "doc_html_tooltip_rtl.xul";

const {
  HTMLTooltip,
} = require("devtools/client/shared/widgets/tooltip/HTMLTooltip");
loadHelperScript("helper_html_tooltip.js");

const TOOLBOX_WIDTH = 500;
const TOOLTIP_WIDTH = 150;
const TOOLTIP_HEIGHT = 30;

add_task(async function() {
  await pushPref("devtools.toolbox.sidebar.width", TOOLBOX_WIDTH);

  const [, , doc] = await createHost("right", TEST_URI);

  info("Test the positioning of tooltips in RTL and LTR directions");

  const tooltip = new HTMLTooltip(doc, { useXulWrapper: false });
  const div = doc.createElementNS(HTML_NS, "div");
  div.textContent = "tooltip";
  div.style.cssText = "box-sizing: border-box; border: 1px solid black";
  tooltip.panel.appendChild(div);
  tooltip.setContentSize({ width: TOOLTIP_WIDTH, height: TOOLTIP_HEIGHT });

  await testRtlAnchors(doc, tooltip);
  await testLtrAnchors(doc, tooltip);
  await hideTooltip(tooltip);

  tooltip.destroy();

  await testRtlArrow(doc);
});

async function testRtlAnchors(doc, tooltip) {
  /*
   * The layout of the test page is as follows:
   *   _______________________________
   *  | toolbox                       |
   *  | _____   _____   _____   _____ |
   *  ||     | |     | |     | |     ||
   *  || box1| | box2| | box3| | box4||
   *  ||_____| |_____| |_____| |_____||
   *  |_______________________________|
   *
   * - box1 is aligned with the left edge of the toolbox
   * - box2 is displayed right after box1
   * - total toolbox width is 500px so each box is 125px wide
   */

  const box1 = doc.getElementById("box1");
  const box2 = doc.getElementById("box2");

  const { offsetTop, offsetLeft } = getOffsets(tooltip.doc);

  info("Display the tooltip on box1.");
  await showTooltip(tooltip, box1, { position: "bottom" });

  let panelRect = tooltip.container.getBoundingClientRect();
  let anchorRect = box1.getBoundingClientRect();

  // box1 uses RTL direction, so the tooltip should be aligned with the right edge of the
  // anchor, but it is shifted to the right to fit in the toolbox.
  is(
    panelRect.left,
    0 + offsetLeft,
    "Tooltip is aligned with left edge of the toolbox"
  );
  is(
    panelRect.top,
    anchorRect.bottom + offsetTop,
    "Tooltip aligned with the anchor bottom edge"
  );
  is(
    panelRect.height,
    TOOLTIP_HEIGHT,
    "Tooltip height is at 100px as expected"
  );

  info("Display the tooltip on box2.");
  await showTooltip(tooltip, box2, { position: "bottom" });

  panelRect = tooltip.container.getBoundingClientRect();
  anchorRect = box2.getBoundingClientRect();

  // box2 uses RTL direction, so the tooltip is aligned with the right edge of the anchor
  is(
    panelRect.right,
    anchorRect.right + offsetLeft,
    "Tooltip is aligned with right edge of anchor"
  );
  is(
    panelRect.top,
    anchorRect.bottom + offsetTop,
    "Tooltip aligned with the anchor bottom edge"
  );
  is(
    panelRect.height,
    TOOLTIP_HEIGHT,
    "Tooltip height is at 100px as expected"
  );
}

async function testLtrAnchors(doc, tooltip) {
  /*
   * The layout of the test page is as follows:
   *   _______________________________
   *  | toolbox                       |
   *  | _____   _____   _____   _____ |
   *  ||     | |     | |     | |     ||
   *  || box1| | box2| | box3| | box4||
   *  ||_____| |_____| |_____| |_____||
   *  |_______________________________|
   *
   * - box3 is is displayed right after box2
   * - box4 is aligned with the right edge of the toolbox
   * - total toolbox width is 500px so each box is 125px wide
   */

  const box3 = doc.getElementById("box3");
  const box4 = doc.getElementById("box4");

  const { offsetTop, offsetLeft } = getOffsets(tooltip.doc);

  info("Display the tooltip on box3.");
  await showTooltip(tooltip, box3, { position: "bottom" });

  let panelRect = tooltip.container.getBoundingClientRect();
  let anchorRect = box3.getBoundingClientRect();

  // box3 uses LTR direction, so the tooltip is aligned with the left edge of the anchor.
  is(
    panelRect.left,
    anchorRect.left + offsetLeft,
    "Tooltip is aligned with left edge of anchor"
  );
  is(
    panelRect.top,
    anchorRect.bottom + offsetTop,
    "Tooltip aligned with the anchor bottom edge"
  );
  is(
    panelRect.height,
    TOOLTIP_HEIGHT,
    "Tooltip height is at 100px as expected"
  );

  info("Display the tooltip on box4.");
  await showTooltip(tooltip, box4, { position: "bottom" });

  panelRect = tooltip.container.getBoundingClientRect();
  anchorRect = box4.getBoundingClientRect();

  // box4 uses LTR direction, so the tooltip should be aligned with the left edge of the
  // anchor, but it is shifted to the left to fit in the toolbox.
  is(
    panelRect.right,
    TOOLBOX_WIDTH + offsetLeft,
    "Tooltip is aligned with right edge of toolbox"
  );
  is(
    panelRect.top,
    anchorRect.bottom + offsetTop,
    "Tooltip aligned with the anchor bottom edge"
  );
  is(
    panelRect.height,
    TOOLTIP_HEIGHT,
    "Tooltip height is at 100px as expected"
  );
}

async function testRtlArrow(doc) {
  // Set up the arrow-style tooltip
  const arrowTooltip = new HTMLTooltip(doc, {
    type: "arrow",
    useXulWrapper: false,
  });
  const div = doc.createElementNS(HTML_NS, "div");
  div.textContent = "tooltip";
  div.style.cssText = "box-sizing: border-box; border: 1px solid black";
  arrowTooltip.panel.appendChild(div);
  arrowTooltip.setContentSize({
    width: TOOLTIP_WIDTH,
    height: TOOLTIP_HEIGHT,
  });

  // box2 uses RTL direction and is far enough from the edge that the arrow
  // should not be squashed in the wrong direction.
  const box2 = doc.getElementById("box2");

  info("Display the arrow tooltip on box2.");
  await showTooltip(arrowTooltip, box2, { position: "top" });

  const arrow = arrowTooltip.arrow;
  ok(arrow, "Tooltip has an arrow");

  const panelRect = arrowTooltip.container.getBoundingClientRect();
  const arrowRect = arrow.getBoundingClientRect();

  // The arrow should be offset from the right edge, but still closer to the
  // right edge than the left edge.
  ok(
    arrowRect.right < panelRect.right,
    "Right edge of the arrow " +
      `(${arrowRect.right}) is less than the right edge of the panel ` +
      `(${panelRect.right})`
  );
  const rightMargin = panelRect.right - arrowRect.right;
  const leftMargin = arrowRect.left - panelRect.right;
  ok(
    rightMargin > leftMargin,
    "Arrow should be closer to the right side of " +
      ` the panel (margin: ${rightMargin}) than the left side ` +
      ` (margin: ${leftMargin})`
  );

  await hideTooltip(arrowTooltip);
  arrowTooltip.destroy();
}
