/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_html_tooltip.js */

"use strict";

/**
 * Test the HTMLTooltip content can have a variable height.
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

const CONTAINER_HEIGHT = 200;
const CONTAINER_WIDTH = 200;
const TOOLTIP_HEIGHT = 50;

const {HTMLTooltip} = require("devtools/client/shared/widgets/HTMLTooltip");
loadHelperScript("helper_html_tooltip.js");

add_task(function* () {
  // Force the toolbox to be 400px tall => 50px for each box.
  yield pushPref("devtools.toolbox.footer.height", 400);

  yield addTab("about:blank");
  let [,, doc] = yield createHost("bottom", TEST_URI);

  let tooltip = new HTMLTooltip({doc}, {});
  info("Set tooltip content 50px tall, but request a container 200px tall");
  let tooltipContent = doc.createElementNS(HTML_NS, "div");
  tooltipContent.style.cssText = "height: " + TOOLTIP_HEIGHT + "px; background: red;";
  tooltip.setContent(tooltipContent, CONTAINER_WIDTH, CONTAINER_HEIGHT);

  info("Show the tooltip and check the container and panel height.");
  yield showTooltip(tooltip, doc.getElementById("box1"));

  let containerRect = tooltip.container.getBoundingClientRect();
  let panelRect = tooltip.panel.getBoundingClientRect();
  is(containerRect.height, CONTAINER_HEIGHT,
    "Tooltip container has the expected height.");
  is(panelRect.height, TOOLTIP_HEIGHT, "Tooltip panel has the expected height.");

  info("Click below the tooltip panel but in the tooltip filler element.");
  let onHidden = once(tooltip, "hidden");
  EventUtils.synthesizeMouse(tooltip.container, 100, 100, {}, doc.defaultView);
  yield onHidden;

  info("Show the tooltip one more time, and increase the content height");
  yield showTooltip(tooltip, doc.getElementById("box1"));
  tooltipContent.style.height = (2 * CONTAINER_HEIGHT) + "px";

  info("Click at the same coordinates as earlier, this time it should hit the tooltip.");
  let onPanelClick = once(tooltip.panel, "click");
  EventUtils.synthesizeMouse(tooltip.container, 100, 100, {}, doc.defaultView);
  yield onPanelClick;
  is(tooltip.isVisible(), true, "Tooltip is still visible");

  info("Click below the tooltip container, the tooltip should be closed.");
  onHidden = once(tooltip, "hidden");
  EventUtils.synthesizeMouse(tooltip.container, 100, CONTAINER_HEIGHT + 10,
    {}, doc.defaultView);
  yield onHidden;
});
