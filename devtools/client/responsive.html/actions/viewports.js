/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

const {
  ADD_VIEWPORT,
  CHANGE_DEVICE,
  CHANGE_PIXEL_RATIO,
  REMOVE_DEVICE_ASSOCIATION,
  RESIZE_VIEWPORT,
  ROTATE_VIEWPORT
} = require("./index");

const { post } = require("../utils/message");

module.exports = {

  /**
   * Add an additional viewport to display the document.
   */
  addViewport(userContextId = 0) {
    return {
      type: ADD_VIEWPORT,
      userContextId,
    };
  },

  /**
   * Change the viewport device.
   */
  changeDevice(id, device, deviceType) {
    return {
      type: CHANGE_DEVICE,
      id,
      device,
      deviceType,
    };
  },

  /**
   * Change the viewport pixel ratio.
   */
  changePixelRatio(id, pixelRatio = 0) {
    return {
      type: CHANGE_PIXEL_RATIO,
      id,
      pixelRatio,
    };
  },

  /**
   * Remove the viewport's device assocation.
   */
  removeDeviceAssociation(id) {
    post(window, "remove-device-association");
    return {
      type: REMOVE_DEVICE_ASSOCIATION,
      id,
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
