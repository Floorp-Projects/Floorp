/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_html_tooltip.js */

"use strict";

/**
 * Test the HTMLTooltip "arrow" type on small anchors. The arrow should remain
 * aligned with the anchors as much as possible
 */

const HTML_NS = "http://www.w3.org/1999/xhtml";
const getAnchor = function (position) {
  return `<html:div class="anchor" style="width:10px;
                                          height: 10px;
                                          position: absolute;
                                          background: red;
                                          ${position}"></html:div>`;
};

const TEST_URI = `data:text/xml;charset=UTF-8,<?xml version="1.0"?>
  <?xml-stylesheet href="chrome://global/skin/global.css"?>
  <?xml-stylesheet href="chrome://devtools/skin/light-theme.css"?>

  <window class="theme-light"
          xmlns="http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"
          xmlns:html="http://www.w3.org/1999/xhtml"
          title="Tooltip test">
    <vbox flex="1" style="position: relative">
      ${getAnchor("top: 0; left: 0;")}
      ${getAnchor("top: 0; left: 25px;")}
      ${getAnchor("top: 0; left: 50px;")}
      ${getAnchor("top: 0; left: 75px;")}
      ${getAnchor("bottom: 0; left: 0;")}
      ${getAnchor("bottom: 0; left: 25px;")}
      ${getAnchor("bottom: 0; left: 50px;")}
      ${getAnchor("bottom: 0; left: 75px;")}
      ${getAnchor("bottom: 0; right: 0;")}
      ${getAnchor("bottom: 0; right: 25px;")}
      ${getAnchor("bottom: 0; right: 50px;")}
      ${getAnchor("bottom: 0; right: 75px;")}
      ${getAnchor("top: 0; right: 0;")}
      ${getAnchor("top: 0; right: 25px;")}
      ${getAnchor("top: 0; right: 50px;")}
      ${getAnchor("top: 0; right: 75px;")}
    </vbox>
  </window>`;

const {HTMLTooltip} = require("devtools/client/shared/widgets/HTMLTooltip");
loadHelperScript("helper_html_tooltip.js");

add_task(function* () {
  // Force the toolbox to be 200px high;
  yield pushPref("devtools.toolbox.footer.height", 200);

  yield addTab("about:blank");
  let [,, doc] = yield createHost("bottom", TEST_URI);

  info("Create HTML tooltip");
  let tooltip = new HTMLTooltip({doc}, {type: "arrow"});
  let div = doc.createElementNS(HTML_NS, "div");
  div.style.height = "100%";
  yield tooltip.setContent(div, 200, 35);

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
});
