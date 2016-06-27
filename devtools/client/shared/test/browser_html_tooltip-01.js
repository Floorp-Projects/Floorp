/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_html_tooltip.js */

"use strict";

/**
 * Test the HTMLTooltip show & hide methods.
 */

const HTML_NS = "http://www.w3.org/1999/xhtml";
const TEST_WINDOW_URI = `data:text/xml;charset=UTF-8,<?xml version="1.0"?>
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

const TEST_PAGE_URI = `data:text/xml;charset=UTF-8,<?xml version="1.0"?>
  <?xml-stylesheet href="chrome://global/skin/global.css"?>
  <?xml-stylesheet href="chrome://devtools/skin/tooltips.css"?>
  <page xmlns="http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"
   title="Tooltip test with document using a Page element">
    <vbox flex="1">
      <hbox id="box1" flex="1">test1</hbox>
    </vbox>
  </page>`;

const {HTMLTooltip} = require("devtools/client/shared/widgets/HTMLTooltip");
loadHelperScript("helper_html_tooltip.js");

function getTooltipContent(doc) {
  let div = doc.createElementNS(HTML_NS, "div");
  div.style.height = "50px";
  div.style.boxSizing = "border-box";
  div.textContent = "tooltip";
  return div;
}

add_task(function* () {
  info("Test showing a basic tooltip in XUL document using <window>");
  yield testTooltipForUri(TEST_WINDOW_URI);

  info("Test showing a basic tooltip in XUL document using <page>");
  yield testTooltipForUri(TEST_PAGE_URI);
});

function* testTooltipForUri(uri) {
  let tab = yield addTab("about:blank");
  let [,, doc] = yield createHost("bottom", uri);

  let tooltip = new HTMLTooltip({doc}, {});

  info("Set tooltip content");
  tooltip.setContent(getTooltipContent(doc), {width: 100, height: 50});

  is(tooltip.isVisible(), false, "Tooltip is not visible");

  info("Show the tooltip and check the expected events are fired.");

  let shown = 0;
  tooltip.on("shown", () => shown++);

  let onShown = tooltip.once("shown");
  tooltip.show(doc.getElementById("box1"));

  yield onShown;
  is(shown, 1, "Event shown was fired once");

  yield waitForReflow(tooltip);
  is(tooltip.isVisible(), true, "Tooltip is visible");

  info("Hide the tooltip and check the expected events are fired.");

  let hidden = 0;
  tooltip.on("hidden", () => hidden++);

  let onPopupHidden = tooltip.once("hidden");
  tooltip.hide();

  yield onPopupHidden;
  is(hidden, 1, "Event hidden was fired once");

  yield waitForReflow(tooltip);
  is(tooltip.isVisible(), false, "Tooltip is not visible");

  yield removeTab(tab);
}
