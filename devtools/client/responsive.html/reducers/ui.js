/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");

const {
  CHANGE_DISPLAY_PIXEL_RATIO,
  TOGGLE_LEFT_ALIGNMENT,
  TOGGLE_TOUCH_SIMULATION,
} = require("../actions/index");

const LEFT_ALIGNMENT_ENABLED = "devtools.responsive.leftAlignViewport.enabled";

const INITIAL_UI = {
  // The pixel ratio of the display.
  displayPixelRatio: 0,
  // Whether or not the viewports are left aligned.
  leftAlignmentEnabled: Services.prefs.getBoolPref(LEFT_ALIGNMENT_ENABLED),
  // Whether or not touch simulation is enabled.
  touchSimulationEnabled: false,
};

const reducers = {

  [CHANGE_DISPLAY_PIXEL_RATIO](ui, { displayPixelRatio }) {
    return Object.assign({}, ui, {
      displayPixelRatio,
    });
  },

  [TOGGLE_LEFT_ALIGNMENT](ui, { enabled }) {
    const leftAlignmentEnabled = enabled !== undefined ?
      enabled : !ui.leftAlignmentEnabled;

    Services.prefs.setBoolPref(LEFT_ALIGNMENT_ENABLED, leftAlignmentEnabled);

    return Object.assign({}, ui, {
      leftAlignmentEnabled,
    });
  },

  [TOGGLE_TOUCH_SIMULATION](ui, { enabled }) {
    return Object.assign({}, ui, {
      touchSimulationEnabled: enabled,
    });
  },

};

module.exports = function(ui = INITIAL_UI, action) {
  const reducer = reducers[action.type];
  if (!reducer) {
    return ui;
  }
  return reducer(ui, action);
};
