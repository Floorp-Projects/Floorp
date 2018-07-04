/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_html_tooltip.js */

"use strict";

/**
 * Test the HTMLTooltip "doorhanger" type's hang direction. It should hang
 * towards the middle of the screen.
 */

const HTML_NS = "http://www.w3.org/1999/xhtml";
const TEST_URI = CHROME_URL_ROOT + "doc_html_tooltip_doorhanger-01.xul";

const {HTMLTooltip} =
  require("devtools/client/shared/widgets/tooltip/HTMLTooltip");
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
  const tooltip = new HTMLTooltip(doc, {type: "doorhanger", useXulWrapper});
  const div = doc.createElementNS(HTML_NS, "div");
  div.style.width = "200px";
  div.style.height = "35px";
  tooltip.setContent(div);

  const docBounds = doc.documentElement.getBoundingClientRect();

  const elements = [...doc.querySelectorAll(".anchor")];
  for (const el of elements) {
    info("Display the tooltip on an anchor.");
    await showTooltip(tooltip, el);

    const arrow = tooltip.arrow;
    ok(arrow, "Tooltip has an arrow");

    // Get the geometry of the anchor, the tooltip panel & arrow.
    const anchorBounds = el.getBoxQuads({ relativeTo: doc })[0].getBounds();
    const panelBounds =
      tooltip.panel.getBoxQuads({ relativeTo: doc })[0].getBounds();
    const arrowBounds = arrow.getBoxQuads({ relativeTo: doc })[0].getBounds();

    // Work out which side of the view the anchor is on.
    const center = bounds => bounds.left + bounds.width / 2;
    const anchorSide =
      center(anchorBounds) < center(docBounds)
      ? "left"
      : "right";

    // Work out which direction the doorhanger hangs.
    //
    // We can do that just by checking which edge of the panel the center of the
    // arrow is closer to.
    const panelDirection =
      center(arrowBounds) - panelBounds.left <
        panelBounds.right - center(arrowBounds)
      ? "right"
      : "left";

    const params =
      `document: ${docBounds.left}<->${docBounds.right}, ` +
      `anchor: ${anchorBounds.left}<->${anchorBounds.right}, ` +
      `panel: ${panelBounds.left}<->${panelBounds.right}, ` +
      `anchor side: ${anchorSide}, ` +
      `panel direction: ${panelDirection}`;
    ok(anchorSide !== panelDirection,
       `Doorhanger hangs towards center (${params})`);

    await hideTooltip(tooltip);
  }

  tooltip.destroy();
}
