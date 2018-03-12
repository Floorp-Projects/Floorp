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

add_task(function* () {
  // Force the toolbox to be 200px high;
  yield pushPref("devtools.toolbox.footer.height", 200);

  yield addTab("about:blank");
  let [,, doc] = yield createHost("bottom", TEST_URI);

  info("Run tests for a Tooltip without using a XUL panel");
  useXulWrapper = false;
  yield runTests(doc);

  info("Run tests for a Tooltip with a XUL panel");
  useXulWrapper = true;
  yield runTests(doc);
});

function* runTests(doc) {
  info("Create HTML tooltip");
  let tooltip = new HTMLTooltip(doc, {type: "arrow", useXulWrapper});
  let div = doc.createElementNS(HTML_NS, "div");
  div.style.height = "35px";
  tooltip.setContent(div, {width: 200, height: 35});

  let {right: docRight} = doc.documentElement.getBoundingClientRect();

  let elements = [...doc.querySelectorAll(".anchor")];
  for (let el of elements) {
    info("Display the tooltip on an anchor.");
    yield showTooltip(tooltip, el);

    let arrow = tooltip.arrow;
    ok(arrow, "Tooltip has an arrow");

    // Get the geometry of the anchor, the tooltip panel & arrow.
    let arrowBounds = arrow.getBoxQuads({relativeTo: doc})[0].bounds;
    let panelBounds = tooltip.panel.getBoxQuads({relativeTo: doc})[0].bounds;
    let anchorBounds = el.getBoxQuads({relativeTo: doc})[0].bounds;

    let intersects = arrowBounds.left <= anchorBounds.right &&
                     arrowBounds.right >= anchorBounds.left;
    let isBlockedByViewport = arrowBounds.left == 0 ||
                              arrowBounds.right == docRight;
    ok(intersects || isBlockedByViewport,
      "Tooltip arrow is aligned with the anchor, or stuck on viewport's edge.");

    let isInPanel = arrowBounds.left >= panelBounds.left &&
                    arrowBounds.right <= panelBounds.right;
    ok(isInPanel,
      "The tooltip arrow remains inside the tooltip panel horizontally");

    yield hideTooltip(tooltip);
  }

  tooltip.destroy();
}
