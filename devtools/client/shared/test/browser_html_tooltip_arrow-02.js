/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_html_tooltip.js */

"use strict";

/**
 * Test the HTMLTooltip "arrow" type on wide anchors. The arrow should remain
 * aligned with the anchors as much as possible
 */

const HTML_NS = "http://www.w3.org/1999/xhtml";
const getAnchor = function (position) {
  return `<html:div class="anchor" style="height: 5px;
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
      ${getAnchor("top:    0; left: 0; width: 50px;")}
      ${getAnchor("top: 10px; left: 0; width: 100px;")}
      ${getAnchor("top: 20px; left: 0; width: 150px;")}
      ${getAnchor("top: 30px; left: 0; width: 200px;")}
      ${getAnchor("top: 40px; left: 0; width: 250px;")}
      ${getAnchor("top: 50px; left: 100px; width: 250px;")}
      ${getAnchor("top: 100px; width:  50px; right: 0;")}
      ${getAnchor("top: 110px; width: 100px; right: 0;")}
      ${getAnchor("top: 120px; width: 150px; right: 0;")}
      ${getAnchor("top: 130px; width: 200px; right: 0;")}
      ${getAnchor("top: 140px; width: 250px; right: 0;")}
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

    // Get the geometry of the anchor, the tooltip frame & arrow.
    let arrowBounds = arrow.getBoxQuads({relativeTo: doc})[0].bounds;
    let frameBounds = tooltip.frame.getBoxQuads({relativeTo: doc})[0].bounds;
    let anchorBounds = el.getBoxQuads({relativeTo: doc})[0].bounds;

    let intersects = arrowBounds.left <= anchorBounds.right &&
                     arrowBounds.right >= anchorBounds.left;
    let isBlockedByViewport = arrowBounds.left == 0 ||
                              arrowBounds.right == docRight;
    ok(intersects || isBlockedByViewport,
      "Tooltip arrow is aligned with the anchor, or stuck on viewport's edge.");

    let isInFrame = arrowBounds.left >= frameBounds.left &&
                    arrowBounds.right <= frameBounds.right;
    ok(isInFrame,
      "The tooltip arrow remains inside the tooltip frame horizontally");
    yield hideTooltip(tooltip);
  }
});
