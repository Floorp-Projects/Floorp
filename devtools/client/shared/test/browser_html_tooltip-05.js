/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_html_tooltip.js */

"use strict";

/**
 * Test the HTMLTooltip positioning for a huge tooltip element (can not fit in
 * the viewport).
 */

const HTML_NS = "http://www.w3.org/1999/xhtml";
const TEST_URI = `data:text/xml;charset=UTF-8,<?xml version="1.0"?>
  <?xml-stylesheet href="chrome://global/skin/global.css"?>
  <?xml-stylesheet href="chrome://devtools/skin/tooltips.css"?>
  <window xmlns="http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"
   title="Tooltip test">
    <vbox flex="1">
      <hbox id="box1" style="height: 50px">test1</hbox>
      <hbox id="box2" style="height: 50px">test2</hbox>
      <hbox id="box3" style="height: 50px">test3</hbox>
      <hbox id="box4" style="height: 50px">test4</hbox>
    </vbox>
  </window>`;

const {HTMLTooltip} = require("devtools/client/shared/widgets/HTMLTooltip");
loadHelperScript("helper_html_tooltip.js");

const TOOLTIP_HEIGHT = 200;
const TOOLTIP_WIDTH = 200;

add_task(function* () {
  // Force the toolbox to be 200px high;
  yield pushPref("devtools.toolbox.footer.height", 200);
  yield addTab("about:blank");
  let [,, doc] = yield createHost("bottom", TEST_URI);

  info("Create HTML tooltip");
  let tooltip = new HTMLTooltip({doc}, {useXulWrapper: false});
  let div = doc.createElementNS(HTML_NS, "div");
  div.style.height = "100%";
  tooltip.setContent(div, {width: TOOLTIP_WIDTH, height: TOOLTIP_HEIGHT});

  let box1 = doc.getElementById("box1");
  let box2 = doc.getElementById("box2");
  let box3 = doc.getElementById("box3");
  let box4 = doc.getElementById("box4");
  let width = TOOLTIP_WIDTH;

  // box1: Can not fit above or below box1, default to bottom with a reduced
  // height of 150px.
  info("Display the tooltip on box1.");
  yield showTooltip(tooltip, box1);
  let expectedTooltipGeometry = {position: "bottom", height: 150, width};
  checkTooltipGeometry(tooltip, box1, expectedTooltipGeometry);
  yield hideTooltip(tooltip);

  info("Try to display the tooltip on top of box1.");
  yield showTooltip(tooltip, box1, {position: "top"});
  expectedTooltipGeometry = {position: "bottom", height: 150, width};
  checkTooltipGeometry(tooltip, box1, expectedTooltipGeometry);
  yield hideTooltip(tooltip);

  // box2: Can not fit above or below box2, default to bottom with a reduced
  // height of 100px.
  info("Try to display the tooltip on box2.");
  yield showTooltip(tooltip, box2);
  expectedTooltipGeometry = {position: "bottom", height: 100, width};
  checkTooltipGeometry(tooltip, box2, expectedTooltipGeometry);
  yield hideTooltip(tooltip);

  info("Try to display the tooltip on top of box2.");
  yield showTooltip(tooltip, box2, {position: "top"});
  expectedTooltipGeometry = {position: "bottom", height: 100, width};
  checkTooltipGeometry(tooltip, box2, expectedTooltipGeometry);
  yield hideTooltip(tooltip);

  // box3: Can not fit above or below box3, default to top with a reduced height
  // of 100px.
  info("Try to display the tooltip on box3.");
  yield showTooltip(tooltip, box3);
  expectedTooltipGeometry = {position: "top", height: 100, width};
  checkTooltipGeometry(tooltip, box3, expectedTooltipGeometry);
  yield hideTooltip(tooltip);

  info("Try to display the tooltip on bottom of box3.");
  yield showTooltip(tooltip, box3, {position: "bottom"});
  expectedTooltipGeometry = {position: "top", height: 100, width};
  checkTooltipGeometry(tooltip, box3, expectedTooltipGeometry);
  yield hideTooltip(tooltip);

  // box4: Can not fit above or below box4, default to top with a reduced height
  // of 150px.
  info("Display the tooltip on box4.");
  yield showTooltip(tooltip, box4);
  expectedTooltipGeometry = {position: "top", height: 150, width};
  checkTooltipGeometry(tooltip, box4, expectedTooltipGeometry);
  yield hideTooltip(tooltip);

  info("Try to display the tooltip on bottom of box4.");
  yield showTooltip(tooltip, box4, {position: "bottom"});
  expectedTooltipGeometry = {position: "top", height: 150, width};
  checkTooltipGeometry(tooltip, box4, expectedTooltipGeometry);
  yield hideTooltip(tooltip);

  is(tooltip.isVisible(), false, "Tooltip is not visible");

  tooltip.destroy();
});
