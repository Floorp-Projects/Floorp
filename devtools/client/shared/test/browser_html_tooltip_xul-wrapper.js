/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_html_tooltip.js */

"use strict";

/**
 * Test the HTMLTooltip can overflow out of the toolbox when using a XUL panel wrapper.
 */

const HTML_NS = "http://www.w3.org/1999/xhtml";
const TEST_URI = CHROME_URL_ROOT + "doc_html_tooltip-05.xul";

const {HTMLTooltip} = require("devtools/client/shared/widgets/tooltip/HTMLTooltip");
loadHelperScript("helper_html_tooltip.js");

// The test toolbox will be 200px tall, the anchors are 50px tall, therefore, the maximum
// tooltip height that could fit in the toolbox is 150px. Setting 160px, the tooltip will
// either have to overflow or to be resized.
const TOOLTIP_HEIGHT = 160;
const TOOLTIP_WIDTH = 200;

add_task(async function() {
  // Force the toolbox to be 200px high;
  await pushPref("devtools.toolbox.footer.height", 200);

  const [, win, doc] = await createHost("bottom", TEST_URI);

  info("Resize and move the window to have space below.");
  const originalWidth = win.top.outerWidth;
  const originalHeight = win.top.outerHeight;
  win.top.resizeBy(-100, -200);
  const originalTop = win.top.screenTop;
  const originalLeft = win.top.screenLeft;
  win.top.moveTo(100, 100);

  registerCleanupFunction(() => {
    info("Restore original window dimensions and position.");
    win.top.resizeTo(originalWidth, originalHeight);
    win.top.moveTo(originalTop, originalLeft);
  });

  info("Create HTML tooltip");
  const tooltip = new HTMLTooltip(doc, {useXulWrapper: true});
  const div = doc.createElementNS(HTML_NS, "div");
  div.style.height = "200px";
  div.style.background = "red";
  tooltip.setContent(div, {width: TOOLTIP_WIDTH, height: TOOLTIP_HEIGHT});

  const box1 = doc.getElementById("box1");

  // Above box1: check that the tooltip can overflow onto the content page.
  info("Display the tooltip above box1.");
  await showTooltip(tooltip, box1, {position: "top"});
  checkTooltip(tooltip, "top", TOOLTIP_HEIGHT);
  await hideTooltip(tooltip);

  // Below box1: check that the tooltip can overflow out of the browser window.
  info("Display the tooltip below box1.");
  await showTooltip(tooltip, box1, {position: "bottom"});
  checkTooltip(tooltip, "bottom", TOOLTIP_HEIGHT);
  await hideTooltip(tooltip);

  is(tooltip.isVisible(), false, "Tooltip is not visible");

  tooltip.destroy();
});

function checkTooltip(tooltip, position, height) {
  is(tooltip.position, position, "Actual tooltip position is " + position);
  const rect = tooltip.container.getBoundingClientRect();
  is(rect.height, height, "Actual tooltip height is " + height);
  // Testing the actual left/top offsets is not relevant here as it is handled by the XUL
  // panel.
}
