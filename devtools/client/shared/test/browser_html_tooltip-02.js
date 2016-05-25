/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_html_tooltip.js */

"use strict";

/**
 * Test the HTMLTooltip is closed when clicking outside of its container.
 */

const HTML_NS = "http://www.w3.org/1999/xhtml";
const TEST_URI = `data:text/xml;charset=UTF-8,<?xml version="1.0"?>
  <?xml-stylesheet href="chrome://global/skin/global.css"?>
  <?xml-stylesheet href="chrome://devtools/skin/common.css"?>
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

add_task(function* () {
  yield addTab("about:blank");
  let [,, doc] = yield createHost("bottom", TEST_URI);

  yield testTooltipNotClosingOnInsideClick(doc);
  yield testConsumeOutsideClicksFalse(doc);
  yield testConsumeOutsideClicksTrue(doc);
});

function* testTooltipNotClosingOnInsideClick(doc) {
  info("Test a tooltip is not closed when clicking inside itself");

  let tooltip = new HTMLTooltip({doc}, {});
  yield tooltip.setContent(getTooltipContent(doc), 100, 50);
  yield showTooltip(tooltip, doc.getElementById("box1"));

  let onTooltipContainerClick = once(tooltip.container, "click");
  EventUtils.synthesizeMouseAtCenter(tooltip.container, {}, doc.defaultView);
  yield onTooltipContainerClick;
  is(tooltip.isVisible(), true, "Tooltip is still visible");

  tooltip.destroy();
}

function* testConsumeOutsideClicksFalse(doc) {
  info("Test closing a tooltip via click with consumeOutsideClicks: false");
  let box4 = doc.getElementById("box4");

  let tooltip = new HTMLTooltip({doc}, {consumeOutsideClicks: false});
  yield tooltip.setContent(getTooltipContent(doc), 100, 50);
  yield showTooltip(tooltip, doc.getElementById("box1"));

  let onBox4Clicked = once(box4, "click");
  let onHidden = once(tooltip, "hidden");
  EventUtils.synthesizeMouseAtCenter(box4, {}, doc.defaultView);
  yield onHidden;
  yield onBox4Clicked;

  is(tooltip.isVisible(), false, "Tooltip is hidden");

  tooltip.destroy();
}

function* testConsumeOutsideClicksTrue(doc) {
  info("Test closing a tooltip via click with consumeOutsideClicks: true");
  let box4 = doc.getElementById("box4");

  // Count clicks on box4
  let box4clicks = 0;
  box4.addEventListener("click", () => box4clicks++);

  let tooltip = new HTMLTooltip({doc}, {consumeOutsideClicks: true});
  yield tooltip.setContent(getTooltipContent(doc), 100, 50);
  yield showTooltip(tooltip, doc.getElementById("box1"));

  let onHidden = once(tooltip, "hidden");
  EventUtils.synthesizeMouseAtCenter(box4, {}, doc.defaultView);
  yield onHidden;

  is(box4clicks, 0, "box4 catched no click event");
  is(tooltip.isVisible(), false, "Tooltip is hidden");

  tooltip.destroy();
}

function getTooltipContent(doc) {
  let div = doc.createElementNS(HTML_NS, "div");
  div.style.height = "50px";
  div.style.boxSizing = "border-box";
  div.textContent = "tooltip";
  return div;
}
