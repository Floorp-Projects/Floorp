/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_html_tooltip.js */
"use strict";

/**
 * Test the HTMLTooltip can be resized.
 */

const HTML_NS = "http://www.w3.org/1999/xhtml";
const TEST_URI = CHROME_URL_ROOT + "doc_html_tooltip.xul";

const {HTMLTooltip} = require("devtools/client/shared/widgets/tooltip/HTMLTooltip");
loadHelperScript("helper_html_tooltip.js");

const TOOLBOX_WIDTH = 500;

add_task(async function() {
  await pushPref("devtools.toolbox.sidebar.width", TOOLBOX_WIDTH);

  // Open the host on the right so that the doorhangers hang right.
  const [,, doc] = await createHost("right", TEST_URI);

  info("Test resizing of a tooltip");

  const tooltip =
    new HTMLTooltip(doc, { useXulWrapper: true, type: "doorhanger" });
  const div = doc.createElementNS(HTML_NS, "div");
  div.textContent = "tooltip";
  div.style.cssText = "width: 100px; height: 40px";
  tooltip.setContent(div);

  const box1 = doc.getElementById("box1");

  await showTooltip(tooltip, box1, { position: "top" });

  // Get the original position of the panel and arrow.
  const originalPanelBounds =
    tooltip.panel.getBoxQuads({ relativeTo: doc })[0].getBounds();
  const originalArrowBounds =
    tooltip.arrow.getBoxQuads({ relativeTo: doc })[0].getBounds();

  // Resize the content
  div.style.cssText = "width: 200px; height: 30px";
  tooltip.updateContainerBounds(box1, { position: "top" });

  // The panel should have moved 100px to the left and 10px down
  const updatedPanelBounds =
    tooltip.panel.getBoxQuads({ relativeTo: doc })[0].getBounds();

  const panelXMovement = `panel left: ${originalPanelBounds.left}->` +
    updatedPanelBounds.left;
  ok(Math.round(updatedPanelBounds.left - originalPanelBounds.left) === -100,
     `Panel should have moved 100px to the left (actual: ${panelXMovement})`);

  const panelYMovement = `panel top: ${originalPanelBounds.top}->` +
    updatedPanelBounds.top;
  ok(Math.round(updatedPanelBounds.top - originalPanelBounds.top) === 10,
     `Panel should have moved 10px down (actual: ${panelYMovement})`);

  // The arrow should be in the same position
  const updatedArrowBounds =
    tooltip.arrow.getBoxQuads({ relativeTo: doc })[0].getBounds();

  const arrowXMovement = `arrow left: ${originalArrowBounds.left}->` +
    updatedArrowBounds.left;
  ok(Math.round(updatedArrowBounds.left - originalArrowBounds.left) === 0,
     `Arrow should not have moved (actual: ${arrowXMovement})`);

  const arrowYMovement = `arrow top: ${originalArrowBounds.top}->` +
    updatedArrowBounds.top;
  ok(Math.round(updatedArrowBounds.top - originalArrowBounds.top) === 0,
     `Arrow should not have moved (actual: ${arrowYMovement})`);

  await hideTooltip(tooltip);
  tooltip.destroy();
});
