/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

const asyncStorage = require("devtools/shared/async-storage");

const {
  ADD_VIEWPORT,
  CHANGE_DEVICE,
  CHANGE_PIXEL_RATIO,
  REMOVE_DEVICE_ASSOCIATION,
  RESIZE_VIEWPORT,
  ROTATE_VIEWPORT,
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
    return async function(dispatch) {
      dispatch({
        type: CHANGE_DEVICE,
        id,
        device,
        deviceType,
      });

      try {
        await asyncStorage.setItem("devtools.responsive.deviceState",
          { id, device, deviceType });
      } catch (e) {
        console.error(e);
      }
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
    return async function(dispatch) {
      post(window, "remove-device-association");

      dispatch({
        type: REMOVE_DEVICE_ASSOCIATION,
        id,
      });

      await asyncStorage.removeItem("devtools.responsive.deviceState");
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
    // TODO: Add `orientation` and `angle` properties to message data. See Bug 1357774.

    // There is no window object to post to when ran on XPCShell as part of the unit
    // tests and will immediately throw an error as a result.
    try {
      post(window, {
        type: "viewport-orientation-change",
      });
    } catch (e) {
      console.log("Unable to post message to window");
    }

    return {
      type: ROTATE_VIEWPORT,
      id,
    };
  },

};
