/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint no-unused-vars: [2, {"vars": "local", "args": "none"}] */

"use strict";

/**
 * Helper methods for the HTMLTooltip integration tests.
 */

/**
 * Display an existing HTMLTooltip on an anchor and properly wait for the popup to be
 * repainted.
 *
 * @param {HTMLTooltip} tooltip
 *        The tooltip instance to display
 * @param {Node} anchor
 *        The anchor that should be used to display the tooltip
 * @param {Object} see HTMLTooltip:show documentation
 * @return {Promise} promise that resolves when reflow and repaint are done.
 */
async function showTooltip(tooltip, anchor, { position, x, y } = {}) {
  await tooltip.show(anchor, { position, x, y });
  await waitForReflow(tooltip);

  // Wait for next tick. Tooltip tests sometimes fail to successively hide and
  // show tooltips on Win32 debug.
  await waitForTick();
}

/**
 * Hide an existing HTMLTooltip. After the tooltip "hidden" event has been fired
 * a reflow will be triggered.
 *
 * @param {HTMLTooltip} tooltip
 *        The tooltip instance to hide
 * @return {Promise} promise that resolves when "hidden" has been fired, reflow
 *         and repaint done.
 */
async function hideTooltip(tooltip) {
  const onPopupHidden = tooltip.once("hidden");
  tooltip.hide();
  await onPopupHidden;
  await waitForReflow(tooltip);

  // Wait for next tick. Tooltip tests sometimes fail to successively hide and
  // show tooltips on Win32 debug.
  await waitForTick();
}

/**
 * Forces the reflow of an HTMLTooltip document and waits for the next repaint.
 *
 * @param {HTMLTooltip} the tooltip to reflow
 * @return {Promise} a promise that will resolve after the reflow and repaint
 *         have been executed.
 */
function waitForReflow(tooltip) {
  const { doc } = tooltip;
  return new Promise(resolve => {
    doc.documentElement.offsetWidth;
    doc.defaultView.requestAnimationFrame(resolve);
  });
}

/**
 * Test helper designed to check that a tooltip is displayed at the expected
 * position relative to an anchor, given a set of expectations.
 *
 * @param {HTMLTooltip} tooltip
 *        The HTMLTooltip instance to check
 * @param {Node} anchor
 *        The tooltip's anchor
 * @param {Object} expected
 *        - {String} position : "top" or "bottom"
 *        - {Boolean} leftAligned
 *        - {Number} width: expected tooltip width
 *        - {Number} height: expected tooltip height
 */
function checkTooltipGeometry(
  tooltip,
  anchor,
  { position, leftAligned = true, height, width } = {}
) {
  info("Check the tooltip geometry matches expected position and dimensions");
  const tooltipRect = tooltip.container.getBoundingClientRect();
  const anchorRect = anchor.getBoundingClientRect();

  if (position === "top") {
    is(
      tooltipRect.bottom,
      Math.round(anchorRect.top),
      "Tooltip is above the anchor"
    );
  } else if (position === "bottom") {
    is(
      tooltipRect.top,
      Math.round(anchorRect.bottom),
      "Tooltip is below the anchor"
    );
  } else {
    ok(false, "Invalid position provided to checkTooltipGeometry");
  }

  if (leftAligned) {
    is(
      tooltipRect.left,
      Math.round(anchorRect.left),
      "Tooltip left-aligned with the anchor"
    );
  }

  is(tooltipRect.height, height, "Tooltip has the expected height");
  is(tooltipRect.width, width, "Tooltip has the expected width");
}
