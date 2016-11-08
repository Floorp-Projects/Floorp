/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  ADD_VIEWPORT,
  CHANGE_DEVICE,
  CHANGE_VIEWPORT_PIXEL_RATIO,
  RESIZE_VIEWPORT,
  ROTATE_VIEWPORT
} = require("./index");

module.exports = {

  /**
   * Add an additional viewport to display the document.
   */
  addViewport() {
    return {
      type: ADD_VIEWPORT,
    };
  },

  /**
   * Change the viewport device.
   */
  changeDevice(id, device) {
    return {
      type: CHANGE_DEVICE,
      id,
      device,
    };
  },

  /**
   * Change the viewport pixel ratio.
   */
  changeViewportPixelRatio(id, pixelRatio = 0) {
    return {
      type: CHANGE_VIEWPORT_PIXEL_RATIO,
      id,
      pixelRatio,
    };
  },

  /**
   * Resize the viewport.
   */
  resizeViewport(id, width, height) {
    return {
      type: RESIZE_VIEWPORT,
      id,
      width,
      height,
    };
  },

  /**
   * Rotate the viewport.
   */
  rotateViewport(id) {
    return {
      type: ROTATE_VIEWPORT,
      id,
    };
  },

};
