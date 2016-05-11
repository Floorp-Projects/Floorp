/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This file lists all of the actions available in responsive design.  This
// central list of constants makes it easy to see all possible action names at
// a glance.  Please add a comment with each new action type.

createEnum([

  // Add a new device.
  "ADD_DEVICE",

  // Add a new device type.
  "ADD_DEVICE_TYPE",

  // Add an additional viewport to display the document.
  "ADD_VIEWPORT",

  // Change the device displayed in the viewport.
  "CHANGE_DEVICE",

  // The location of the page has changed.  This may be triggered by the user
  // directly entering a new URL, navigating with links, etc.
  "CHANGE_LOCATION",

  // Resize the viewport.
  "RESIZE_VIEWPORT",

  // Rotate the viewport.
  "ROTATE_VIEWPORT",

  // Take a screenshot of the viewport.
  "TAKE_SCREENSHOT_START",

  // Indicates when the screenshot action ends.
  "TAKE_SCREENSHOT_END",

  // Update the device display state in the device selector.
  "UPDATE_DEVICE_DISPLAYED",

  // Update the device modal open state.
  "UPDATE_DEVICE_MODAL_OPEN",

  // Update the touch simulation enabled state.
  "UPDATE_TOUCH_SIMULATION_ENABLED",

], module.exports);

/**
 * Create a simple enum-like object with keys mirrored to values from an array.
 * This makes comparison to a specfic value simpler without having to repeat and
 * mis-type the value.
 */
function createEnum(array, target) {
  for (let key of array) {
    target[key] = key;
  }
  return target;
}
