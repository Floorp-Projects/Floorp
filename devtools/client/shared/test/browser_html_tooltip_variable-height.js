/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_html_tooltip.js */

"use strict";

/**
 * Test the HTMLTooltip content can have a variable height.
 */

const HTML_NS = "http://www.w3.org/1999/xhtml";
const TEST_URI = CHROME_URL_ROOT + "doc_html_tooltip.xul";

const CONTAINER_HEIGHT = 300;
const CONTAINER_WIDTH = 200;
const TOOLTIP_HEIGHT = 50;

const {HTMLTooltip} = require("devtools/client/shared/widgets/tooltip/HTMLTooltip");
loadHelperScript("helper_html_tooltip.js");

add_task(async function() {
  // Force the toolbox to be 400px tall => 50px for each box.
  await pushPref("devtools.toolbox.footer.height", 400);

  await addTab("about:blank");
  const [,, doc] = await createHost("bottom", TEST_URI);

  const tooltip = new HTMLTooltip(doc, {useXulWrapper: false});
  info("Set tooltip content 50px tall, but request a container 200px tall");
  const tooltipContent = doc.createElementNS(HTML_NS, "div");
  tooltipContent.style.cssText = "height: " + TOOLTIP_HEIGHT + "px; background: red;";
  tooltip.setContent(tooltipContent, {width: CONTAINER_WIDTH, height: Infinity});

  info("Show the tooltip and check the container and panel height.");
  await showTooltip(tooltip, doc.getElementById("box1"));

  const containerRect = tooltip.container.getBoundingClientRect();
  const panelRect = tooltip.panel.getBoundingClientRect();
  is(containerRect.height, CONTAINER_HEIGHT,
    "Tooltip container has the expected height.");
  is(panelRect.height, TOOLTIP_HEIGHT, "Tooltip panel has the expected height.");

  info("Click below the tooltip panel but in the tooltip filler element.");
  let onHidden = once(tooltip, "hidden");
  EventUtils.synthesizeMouse(tooltip.container, 100, 100, {}, doc.defaultView);
  await onHidden;

  info("Show the tooltip one more time, and increase the content height");
  await showTooltip(tooltip, doc.getElementById("box1"));
  tooltipContent.style.height = (2 * CONTAINER_HEIGHT) + "px";

  info("Click at the same coordinates as earlier, this time it should hit the tooltip.");
  const onPanelClick = once(tooltip.panel, "click");
  EventUtils.synthesizeMouse(tooltip.container, 100, 100, {}, doc.defaultView);
  await onPanelClick;
  is(tooltip.isVisible(), true, "Tooltip is still visible");

  info("Click above the tooltip container, the tooltip should be closed.");
  onHidden = once(tooltip, "hidden");
  EventUtils.synthesizeMouse(tooltip.container, 100, -10, {}, doc.defaultView);
  await onHidden;

  tooltip.destroy();
});
