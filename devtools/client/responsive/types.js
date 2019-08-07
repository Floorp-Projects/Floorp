/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { createEnum } = require("devtools/client/shared/enum");

// React PropTypes are used to describe the expected "shape" of various common
// objects that get passed down as props to components.

/* ENUMS */

/**
 * An enum containing the possible states for loadable things.
 */
exports.loadableState = createEnum([
  "INITIALIZED",
  "LOADING",
  "LOADED",
  "ERROR",
]);

/* DEVICE */

/**
 * A single device that can be displayed in the viewport.
 */
const device = {
  // The name of the device
  name: PropTypes.string,

  // The width of the device
  width: PropTypes.number,

  // The height of the device
  height: PropTypes.number,

  // The pixel ratio of the device
  pixelRatio: PropTypes.number,

  // The user agent string of the device
  userAgent: PropTypes.string,

  // Whether or not it is a touch device
  touch: PropTypes.bool,

  // The operating system of the device
  os: PropTypes.String,

  // Whether or not the device is displayed in the device selector
  displayed: PropTypes.bool,
};

/**
 * A list of devices and their types that can be displayed in the viewport.
 */
exports.devices = {
  // An array of device types
  types: PropTypes.arrayOf(PropTypes.string),

  // An array of phone devices
  phones: PropTypes.arrayOf(PropTypes.shape(device)),

  // An array of tablet devices
  tablets: PropTypes.arrayOf(PropTypes.shape(device)),

  // An array of laptop devices
  laptops: PropTypes.arrayOf(PropTypes.shape(device)),

  // An array of television devices
  televisions: PropTypes.arrayOf(PropTypes.shape(device)),

  // An array of console devices
  consoles: PropTypes.arrayOf(PropTypes.shape(device)),

  // An array of watch devices
  watches: PropTypes.arrayOf(PropTypes.shape(device)),

  // Whether or not the device modal is open
  isModalOpen: PropTypes.bool,

  // Viewport id that triggered the modal to open
  modalOpenedFromViewport: PropTypes.number,

  // Device list state, possible values are exported above in an enum
  listState: PropTypes.oneOf(Object.keys(exports.loadableState)),
};

/* VIEWPORT */

/**
 * Network throttling state for a given viewport.
 */
exports.networkThrottling = {
  // Whether or not network throttling is enabled
  enabled: PropTypes.bool,

  // Name of the selected throttling profile
  profile: PropTypes.string,
};

/**
 * A single viewport displaying a document.
 */
exports.viewport = {
  // The id of the viewport
  id: PropTypes.number,

  // The currently selected device applied to the viewport
  device: PropTypes.string,

  // The currently selected device type applied to the viewport
  deviceType: PropTypes.string,

  // The width of the viewport
  width: PropTypes.number,

  // The height of the viewport
  height: PropTypes.number,

  // The device pixel ratio of the viewport
  pixelRatio: PropTypes.number,

  // The user context (container) ID for the viewport
  // Defaults to 0 meaning the default context
  userContextId: PropTypes.number,
};

/* ACTIONS IN PROGRESS */

/**
 * The progression of the screenshot.
 */
exports.screenshot = {
  // Whether screenshot capturing is in progress
  isCapturing: PropTypes.bool,
};
