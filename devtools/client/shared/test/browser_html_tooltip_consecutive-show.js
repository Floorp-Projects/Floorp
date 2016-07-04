/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_html_tooltip.js */

"use strict";

/**
 * Test the HTMLTooltip show can be called several times. It should move according to the
 * new anchor/options and should not leak event listeners.
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

function getTooltipContent(doc) {
  let div = doc.createElementNS(HTML_NS, "div");
  div.style.height = "50px";
  div.textContent = "tooltip";
  return div;
}

add_task(function* () {
  let [,, doc] = yield createHost("bottom", TEST_URI);

  let box1 = doc.getElementById("box1");
  let box2 = doc.getElementById("box2");
  let box3 = doc.getElementById("box3");
  let box4 = doc.getElementById("box4");

  let width = 100, height = 50;

  let tooltip = new HTMLTooltip({doc}, {useXulWrapper: false});
  tooltip.setContent(getTooltipContent(doc), {width, height});

  info("Show the tooltip on each of the 4 hbox, without calling hide in between");

  info("Show tooltip on box1");
  tooltip.show(box1);
  checkTooltipGeometry(tooltip, box1, {position: "bottom", width, height});

  info("Show tooltip on box2");
  tooltip.show(box2);
  checkTooltipGeometry(tooltip, box2, {position: "bottom", width, height});

  info("Show tooltip on box3");
  tooltip.show(box3);
  checkTooltipGeometry(tooltip, box3, {position: "top", width, height});

  info("Show tooltip on box4");
  tooltip.show(box4);
  checkTooltipGeometry(tooltip, box4, {position: "top", width, height});

  info("Hide tooltip before leaving test");
  yield hideTooltip(tooltip);

  tooltip.destroy();
});
