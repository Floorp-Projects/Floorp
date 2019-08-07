/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PORTRAIT_PRIMARY, LANDSCAPE_PRIMARY } = require("../constants");

/**
 * Helper that gets the screen orientation of the device displayed in the RDM viewport.
 * This function take in both a device and viewport object and an optional rotated angle.
 * If a rotated angle is passed, then we calculate what the orientation type of the device
 * would be in relation to its current orientation. Otherwise, return the current
 * orientation and angle.
 *
 * @param {Object} device
 *        The device whose content is displayed in the viewport. Used to determine the
 *        primary orientation.
 * @param {Object} viewport
 *        The viewport displaying device content. Used to determine the current
 *        orientation type of the device while in RDM.
 * @param {Number|null} angleToRotateTo
 *        Optional. The rotated angle specifies the degree to which the device WILL be
 *        turned to. If undefined, then only return the current orientation and angle
 *        of the device.
 * @return {Object} the orientation of the device.
 */
function getOrientation(device, viewport, angleToRotateTo = null) {
  const { width: deviceWidth, height: deviceHeight } = device;
  const { width: viewportWidth, height: viewportHeight } = viewport;

  // Determine the primary orientation of the device screen.
  const primaryOrientation =
    deviceHeight >= deviceWidth ? PORTRAIT_PRIMARY : LANDSCAPE_PRIMARY;

  // Determine the current orientation of the device screen.
  const currentOrientation =
    viewportHeight >= viewportWidth ? PORTRAIT_PRIMARY : LANDSCAPE_PRIMARY;

  // Calculate the orientation angle of the device.
  let angle;

  if (typeof angleToRotateTo === "number") {
    angle = angleToRotateTo;
  } else if (currentOrientation !== primaryOrientation) {
    angle = 90;
  } else {
    angle = 0;
  }

  // Calculate the orientation type of the device.
  let orientationType = currentOrientation;

  // If the viewport orientation is different from the primary orientation and the angle
  // to rotate to is 0, then we are moving the device orientation back to its primary
  // orientation.
  if (currentOrientation !== primaryOrientation && angleToRotateTo === 0) {
    orientationType = primaryOrientation;
  } else if (angleToRotateTo === 90 || angleToRotateTo === 270) {
    if (currentOrientation.includes("portrait")) {
      orientationType = LANDSCAPE_PRIMARY;
    } else if (currentOrientation.includes("landscape")) {
      orientationType = PORTRAIT_PRIMARY;
    }
  }

  return {
    type: orientationType,
    angle,
  };
}

exports.getOrientation = getOrientation;
