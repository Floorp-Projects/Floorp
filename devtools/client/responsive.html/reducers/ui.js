/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");

const {
  CHANGE_DISPLAY_PIXEL_RATIO,
  CHANGE_USER_AGENT,
  TOGGLE_LEFT_ALIGNMENT,
  TOGGLE_TOUCH_SIMULATION,
  TOGGLE_USER_AGENT_INPUT,
} = require("../actions/index");

const LEFT_ALIGNMENT_ENABLED = "devtools.responsive.leftAlignViewport.enabled";
const SHOW_USER_AGENT_INPUT = "devtools.responsive.showUserAgentInput";

const INITIAL_UI = {
  // The pixel ratio of the display.
  displayPixelRatio: 0,
  // Whether or not the viewports are left aligned.
  leftAlignmentEnabled: Services.prefs.getBoolPref(LEFT_ALIGNMENT_ENABLED),
  // Whether or not to show the user agent input in the toolbar.
  showUserAgentInput: Services.prefs.getBoolPref(SHOW_USER_AGENT_INPUT),
  // Whether or not touch simulation is enabled.
  touchSimulationEnabled: false,
  // The user agent of the viewport.
  userAgent: "",
};

const reducers = {

  [CHANGE_DISPLAY_PIXEL_RATIO](ui, { displayPixelRatio }) {
    return Object.assign({}, ui, {
      displayPixelRatio,
    });
  },

  [CHANGE_USER_AGENT](ui, { userAgent }) {
    return Object.assign({}, ui, {
      userAgent,
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

  [TOGGLE_USER_AGENT_INPUT](ui, { enabled }) {
    const showUserAgentInput = enabled !== undefined ?
      enabled : !ui.showUserAgentInput;

    Services.prefs.setBoolPref(SHOW_USER_AGENT_INPUT, showUserAgentInput);

    return Object.assign({}, ui, {
      showUserAgentInput,
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
