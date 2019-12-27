/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");

const {
  CHANGE_DISPLAY_PIXEL_RATIO,
  CHANGE_USER_AGENT,
  TOGGLE_LEFT_ALIGNMENT,
  TOGGLE_RELOAD_ON_TOUCH_SIMULATION,
  TOGGLE_RELOAD_ON_USER_AGENT,
  TOGGLE_TOUCH_SIMULATION,
  TOGGLE_USER_AGENT_INPUT,
} = require("../actions/index");

const LEFT_ALIGNMENT_ENABLED = "devtools.responsive.leftAlignViewport.enabled";
const RELOAD_ON_TOUCH_SIMULATION =
  "devtools.responsive.reloadConditions.touchSimulation";
const RELOAD_ON_USER_AGENT = "devtools.responsive.reloadConditions.userAgent";
const SHOW_USER_AGENT_INPUT = "devtools.responsive.showUserAgentInput";
const TOUCH_SIMULATION_ENABLED = "devtools.responsive.touchSimulation.enabled";
const USER_AGENT = "devtools.responsive.userAgent";

const INITIAL_UI = {
  // The pixel ratio of the display.
  displayPixelRatio: 0,
  // Whether or not the viewports are left aligned.
  leftAlignmentEnabled: Services.prefs.getBoolPref(
    LEFT_ALIGNMENT_ENABLED,
    false
  ),
  // Whether or not to reload when touch simulation is toggled.
  reloadOnTouchSimulation: Services.prefs.getBoolPref(
    RELOAD_ON_TOUCH_SIMULATION,
    false
  ),
  // Whether or not to reload when user agent is changed.
  reloadOnUserAgent: Services.prefs.getBoolPref(RELOAD_ON_USER_AGENT, false),
  // Whether or not to show the user agent input in the toolbar.
  showUserAgentInput: Services.prefs.getBoolPref(SHOW_USER_AGENT_INPUT, false),
  // Whether or not touch simulation is enabled.
  touchSimulationEnabled: Services.prefs.getBoolPref(
    TOUCH_SIMULATION_ENABLED,
    false
  ),
  // The user agent of the viewport.
  userAgent: Services.prefs.getCharPref(USER_AGENT, ""),
};

const reducers = {
  [CHANGE_DISPLAY_PIXEL_RATIO](ui, { displayPixelRatio }) {
    return {
      ...ui,
      displayPixelRatio,
    };
  },

  [CHANGE_USER_AGENT](ui, { userAgent }) {
    Services.prefs.setCharPref(USER_AGENT, userAgent);

    return {
      ...ui,
      userAgent,
    };
  },

  [TOGGLE_LEFT_ALIGNMENT](ui, { enabled }) {
    const leftAlignmentEnabled =
      enabled !== undefined ? enabled : !ui.leftAlignmentEnabled;

    Services.prefs.setBoolPref(LEFT_ALIGNMENT_ENABLED, leftAlignmentEnabled);

    return {
      ...ui,
      leftAlignmentEnabled,
    };
  },

  [TOGGLE_RELOAD_ON_TOUCH_SIMULATION](ui, { enabled }) {
    const reloadOnTouchSimulation =
      enabled !== undefined ? enabled : !ui.reloadOnTouchSimulation;

    Services.prefs.setBoolPref(
      RELOAD_ON_TOUCH_SIMULATION,
      reloadOnTouchSimulation
    );

    return {
      ...ui,
      reloadOnTouchSimulation,
    };
  },

  [TOGGLE_RELOAD_ON_USER_AGENT](ui, { enabled }) {
    const reloadOnUserAgent =
      enabled !== undefined ? enabled : !ui.reloadOnUserAgent;

    Services.prefs.setBoolPref(RELOAD_ON_USER_AGENT, reloadOnUserAgent);

    return {
      ...ui,
      reloadOnUserAgent,
    };
  },

  [TOGGLE_TOUCH_SIMULATION](ui, { enabled }) {
    Services.prefs.setBoolPref(TOUCH_SIMULATION_ENABLED, enabled);

    return {
      ...ui,
      touchSimulationEnabled: enabled,
    };
  },

  [TOGGLE_USER_AGENT_INPUT](ui, { enabled }) {
    const showUserAgentInput =
      enabled !== undefined ? enabled : !ui.showUserAgentInput;

    Services.prefs.setBoolPref(SHOW_USER_AGENT_INPUT, showUserAgentInput);

    return {
      ...ui,
      showUserAgentInput,
    };
  },
};

module.exports = function(ui = INITIAL_UI, action) {
  const reducer = reducers[action.type];
  if (!reducer) {
    return ui;
  }
  return reducer(ui, action);
};
