/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This file lists all of the actions available in responsive design. This
// central list of constants makes it easy to see all possible action names at
// a glance. Please add a comment with each new action type.

const { createEnum } = require("devtools/client/shared/enum");

const {
  CHANGE_NETWORK_THROTTLING,
} = require("devtools/client/shared/components/throttling/actions");

createEnum([

  // Add a new device.
  "ADD_DEVICE",

  // Add a new device type.
  "ADD_DEVICE_TYPE",

  // Add an additional viewport to display the document.
  "ADD_VIEWPORT",

  // Change the device displayed in the viewport.
  "CHANGE_DEVICE",

  // Change the location of the page. This may be triggered by the user
  // directly entering a new URL, navigating with links, etc.
  "CHANGE_LOCATION",

  // The pixel ratio of the display has changed. This may be triggered by the user
  // when changing the monitor resolution, or when the window is dragged to a different
  // display with a different pixel ratio.
  "CHANGE_DISPLAY_PIXEL_RATIO",

  // Change the network throttling profile.
  CHANGE_NETWORK_THROTTLING,

  // Change the user agent of the viewport.
  "CHANGE_USER_AGENT",

  // The pixel ratio of the viewport has changed. This may be triggered by the user
  // when changing the device displayed in the viewport, or when a pixel ratio is
  // selected from the device pixel ratio dropdown.
  "CHANGE_PIXEL_RATIO",

  // Edit a device.
  "EDIT_DEVICE",

  // Indicates that the device list is being loaded.
  "LOAD_DEVICE_LIST_START",

  // Indicates that the device list loading action threw an error.
  "LOAD_DEVICE_LIST_ERROR",

  // Indicates that the device list has been loaded successfully.
  "LOAD_DEVICE_LIST_END",

  // Remove a device.
  "REMOVE_DEVICE",

  // Remove the viewport's device assocation.
  "REMOVE_DEVICE_ASSOCIATION",

  // Resize the viewport.
  "RESIZE_VIEWPORT",

  // Rotate the viewport.
  "ROTATE_VIEWPORT",

  // Take a screenshot of the viewport.
  "TAKE_SCREENSHOT_START",

  // Indicates when the screenshot action ends.
  "TAKE_SCREENSHOT_END",

  // Toggles the left alignment of the viewports.
  "TOGGLE_LEFT_ALIGNMENT",

  // Toggles the reload on touch simulation changes.
  "TOGGLE_RELOAD_ON_TOUCH_SIMULATION",

  // Toggles the reload on user agent changes.
  "TOGGLE_RELOAD_ON_USER_AGENT",

  // Toggles the touch simulation state of the viewports.
  "TOGGLE_TOUCH_SIMULATION",

  // Toggles the user agent input displayed in the toolbar.
  "TOGGLE_USER_AGENT_INPUT",

  // Update the device display state in the device selector.
  "UPDATE_DEVICE_DISPLAYED",

  // Update the device modal state.
  "UPDATE_DEVICE_MODAL",

], module.exports);
