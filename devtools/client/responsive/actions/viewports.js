/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

const Services = require("Services");
const asyncStorage = require("devtools/shared/async-storage");

const {
  ADD_VIEWPORT,
  CHANGE_DEVICE,
  CHANGE_PIXEL_RATIO,
  CHANGE_VIEWPORT_ANGLE,
  REMOVE_DEVICE_ASSOCIATION,
  RESIZE_VIEWPORT,
  ROTATE_VIEWPORT,
  ZOOM_VIEWPORT,
} = require("devtools/client/responsive/actions/index");

const { post } = require("devtools/client/responsive/utils/message");

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
        await asyncStorage.setItem("devtools.responsive.deviceState", {
          id,
          device,
          deviceType,
        });
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

  changeViewportAngle(id, angle) {
    return {
      type: CHANGE_VIEWPORT_ANGLE,
      id,
      angle,
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
    return async function(dispatch, getState) {
      if (Services.prefs.getBoolPref("devtools.responsive.browserUI.enabled")) {
        const viewport = getState().viewports[0];

        post(window, {
          type: "viewport-resize",
          height: viewport.width,
          width: viewport.height,
        });
      }

      dispatch({
        type: ROTATE_VIEWPORT,
        id,
      });
    };
  },

  /**
   * Zoom the viewport.
   */
  zoomViewport(id, zoom) {
    return {
      type: ZOOM_VIEWPORT,
      id,
      zoom,
    };
  },
};
