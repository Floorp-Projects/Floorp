/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { getCurrentZoom } = require("devtools/shared/layout/utils");

/**
 * A helper function that calculate accessible object bounds and positioning to
 * be used for highlighting.
 *
 * @param  {Object} win
 *         window that contains accessible object.
 * @param  {Object} options
 *         Object used for passing options:
 *         - {Number} x
 *           x coordinate of the top left corner of the accessible object
 *         - {Number} y
 *           y coordinate of the top left corner of the accessible object
 *         - {Number} w
 *           width of the the accessible object
 *         - {Number} h
 *           height of the the accessible object
 *         - {Number} zoom
 *           zoom level of the accessible object's parent window
 * @return {Object|null} Returns, if available, positioning and bounds information for
 *                 the accessible object.
 */
function getBounds(win, { x, y, w, h, zoom }) {
  let { mozInnerScreenX, mozInnerScreenY, scrollX, scrollY } = win;
  let zoomFactor = getCurrentZoom(win);
  let left = x, right = x + w, top = y, bottom = y + h;

  // For a XUL accessible, normalize the top-level window with its current zoom level.
  // We need to do this because top-level browser content does not allow zooming.
  if (zoom) {
    zoomFactor = zoom;
    mozInnerScreenX /= zoomFactor;
    mozInnerScreenY /= zoomFactor;
    scrollX /= zoomFactor;
    scrollY /= zoomFactor;
  }

  left -= mozInnerScreenX - scrollX;
  right -= mozInnerScreenX - scrollX;
  top -= mozInnerScreenY - scrollY;
  bottom -= mozInnerScreenY - scrollY;

  left *= zoomFactor;
  right *= zoomFactor;
  top *= zoomFactor;
  bottom *= zoomFactor;

  const width = right - left;
  const height = bottom - top;

  return { left, right, top, bottom, width, height };
}

exports.getBounds = getBounds;
