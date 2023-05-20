/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_html_tooltip.js */

"use strict";

/**
 * Test the HTMLTooltip "doorhanger" when the anchor is near the edge of the
 * screen and uses a XUL wrapper. The XUL panel cannot be displayed off screen
 * at all so this verifies that the calculated position of the tooltip always
 * ensure that the whole tooltip is rendered on the screen
 *
 * See Bug 1590408
 */

const HTML_NS = "http://www.w3.org/1999/xhtml";
const TEST_URI = CHROME_URL_ROOT + "doc_html_tooltip_doorhanger-01.xhtml";

const {
  HTMLTooltip,
} = require("resource://devtools/client/shared/widgets/tooltip/HTMLTooltip.js");
loadHelperScript("helper_html_tooltip.js");

add_task(async function () {
  // Force the toolbox to be 200px high;
  await pushPref("devtools.toolbox.footer.height", 200);

  const { win, doc } = await createHost("bottom", TEST_URI);

  const originalTop = win.screenTop;
  const originalLeft = win.screenLeft;
  const screenWidth = win.screen.width;
  await moveWindowTo(win, screenWidth - win.outerWidth, originalTop);

  registerCleanupFunction(async () => {
    info(`Restore original window position. ${originalLeft}, ${originalTop}`);
    await moveWindowTo(win, originalLeft, originalTop);
  });

  info("Create a doorhanger HTML tooltip with XULPanel");
  const tooltip = new HTMLTooltip(doc, {
    type: "doorhanger",
    useXulWrapper: true,
  });
  const div = doc.createElementNS(HTML_NS, "div");
  div.style.width = "200px";
  div.style.height = "35px";
  tooltip.panel.appendChild(div);

  const anchor = doc.querySelector("#anchor5");

  info("Display the tooltip on an anchor.");
  await showTooltip(tooltip, anchor);

  const arrow = tooltip.arrow;
  ok(arrow, "Tooltip has an arrow");

  const panelBounds = tooltip.panel
    .getBoxQuads({ relativeTo: doc })[0]
    .getBounds();

  const anchorBounds = anchor.getBoxQuads({ relativeTo: doc })[0].getBounds();
  ok(
    anchorBounds.left < panelBounds.right &&
      panelBounds.left < anchorBounds.right,
    `The tooltip panel is over (ie intersects) the anchor horizontally: ` +
      `${anchorBounds.left} < ${panelBounds.right} and ` +
      `${panelBounds.left} < ${anchorBounds.right}`
  );

  await hideTooltip(tooltip);

  tooltip.destroy();
});
