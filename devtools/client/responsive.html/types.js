/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PropTypes } = require("devtools/client/shared/vendor/react");
const { createEnum } = require("./utils/enum");

// React PropTypes are used to describe the expected "shape" of various common
// objects that get passed down as props to components.

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
 * An enum containing the possible values for the device list state
 */
exports.deviceListState = createEnum([
  "INITIALIZED",
  "LOADING",
  "LOADED",
  "ERROR",
]);

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

  // Device list state, possible values are exported above in an enum
  listState: PropTypes.oneOf(Object.keys(exports.deviceListState)),

};

/**
 * The location of the document displayed in the viewport(s).
 */
exports.location = PropTypes.string;

/**
 * The progression of the screenshot
 */
exports.screenshot = {

  isCapturing: PropTypes.bool.isRequired,

};

/**
 * Touch simulation.
 */
exports.touchSimulation = {

  // Whether or not the touch simulation is enabled
  enabled: PropTypes.bool.isRequired,

};

/**
 * A single viewport displaying a document.
 */
exports.viewport = {

  // The id of the viewport
  id: PropTypes.number.isRequired,

  // The currently selected device applied to the viewport.
  device: PropTypes.string,

  // The width of the viewport
  width: PropTypes.number,

  // The height of the viewport
  height: PropTypes.number,

};
