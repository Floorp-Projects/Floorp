/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_html_tooltip.js */

"use strict";

/**
 * Test the HTMLTooltip content can automatically calculate its width based on content.
 */

const HTML_NS = "http://www.w3.org/1999/xhtml";
const TEST_URI = `data:text/xml;charset=UTF-8,<?xml version="1.0"?>
  <?xml-stylesheet href="chrome://global/skin/global.css"?>
  <?xml-stylesheet href="chrome://devtools/skin/tooltips.css"?>
  <window xmlns="http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"
   title="Tooltip test">
    <vbox flex="1">
      <hbox id="box1" flex="1">test1</hbox>
      <hbox id="box2" flex="1">test2</hbox>
      <hbox id="box3" flex="1">test3</hbox>
      <hbox id="box4" flex="1">test4</hbox>
    </vbox>
  </window>`;

const {HTMLTooltip} = require("devtools/client/shared/widgets/HTMLTooltip");
loadHelperScript("helper_html_tooltip.js");

let useXulWrapper;

add_task(function* () {
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
  let tooltip = new HTMLTooltip({doc}, {useXulWrapper});
  info("Create tooltip content width to 150px");
  let tooltipContent = doc.createElementNS(HTML_NS, "div");
  tooltipContent.style.cssText = "height: 100%; width: 150px; background: red;";

  info("Set tooltip content using width:auto");
  tooltip.setContent(tooltipContent, {width: "auto", height: 50});

  info("Show the tooltip and check the tooltip panel width.");
  yield showTooltip(tooltip, doc.getElementById("box1"));

  let panelRect = tooltip.panel.getBoundingClientRect();
  is(panelRect.width, 150, "Tooltip panel has the expected width.");

  yield hideTooltip(tooltip);

  tooltip.destroy();
}
