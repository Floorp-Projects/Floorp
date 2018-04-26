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
 * @return {Object|null} Returns, if available, positioning and bounds information for
 *                 the accessible object.
 */
function getBounds(win, { x, y, w, h }) {
  let { mozInnerScreenX, mozInnerScreenY, scrollX, scrollY } = win;
  let zoom = getCurrentZoom(win);
  let left = x, right = x + w, top = y, bottom = y + h;

  left -= mozInnerScreenX - scrollX;
  right -= mozInnerScreenX - scrollX;
  top -= mozInnerScreenY - scrollY;
  bottom -= mozInnerScreenY - scrollY;

  left *= zoom;
  right *= zoom;
  top *= zoom;
  bottom *= zoom;

  let width = right - left;
  let height = bottom - top;

  return { left, right, top, bottom, width, height };
}

exports.getBounds = getBounds;
