/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint no-unused-vars: [2, {"vars": "local", "args": "none"}] */

"use strict";

/**
 * Helper methods for the HTMLTooltip integration tests.
 */

/**
 * Display an existing HTMLTooltip on an anchor. Returns a promise that will
 * resolve when the tooltip "shown" event has been fired.
 *
 * @param {HTMLTooltip} tooltip
 *        The tooltip instance to display
 * @param {Node} anchor
 *        The anchor that should be used to display the tooltip
 * @param {String} position
 *        The preferred display position ("top", "bottom")
 * @return {Promise} promise that resolves when the "shown" event is fired
 */
function showTooltip(tooltip, anchor, position) {
  let onShown = tooltip.once("shown");
  tooltip.show(anchor, {position});
  return onShown;
}

/**
 * Hide an existing HTMLTooltip. Returns a promise that will resolve when the
 * tooltip "hidden" event has been fired.
 *
 * @param {HTMLTooltip} tooltip
 *        The tooltip instance to hide
 * @return {Promise} promise that resolves when the "hidden" event is fired
 */
function hideTooltip(tooltip) {
  let onPopupHidden = tooltip.once("hidden");
  tooltip.hide();
  return onPopupHidden;
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
function checkTooltipGeometry(tooltip, anchor,
    {position, leftAligned = true, height, width} = {}) {
  info("Check the tooltip geometry matches expected position and dimensions");
  let tooltipRect = tooltip.container.getBoundingClientRect();
  let anchorRect = anchor.getBoundingClientRect();

  if (position === "top") {
    is(tooltipRect.bottom, anchorRect.top, "Tooltip is above the anchor");
  } else if (position === "bottom") {
    is(tooltipRect.top, anchorRect.bottom, "Tooltip is below the anchor");
  } else {
    ok(false, "Invalid position provided to checkTooltipGeometry");
  }

  if (leftAligned) {
    is(tooltipRect.left, anchorRect.left,
      "Tooltip left-aligned with the anchor");
  }

  is(tooltipRect.height, height, "Tooltip has the expected height");
  is(tooltipRect.width, width, "Tooltip has the expected width");
}
