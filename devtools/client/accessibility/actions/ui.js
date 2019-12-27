/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const Services = require("Services");

const {
  ENABLE,
  DISABLE,
  RESET,
  UPDATE_CAN_BE_DISABLED,
  UPDATE_CAN_BE_ENABLED,
  UPDATE_PREF,
  PREF_KEYS,
} = require("../constants");

/**
 * Reset accessibility panel UI.
 */
exports.reset = (accessibility, supports) => dispatch =>
  dispatch({ accessibility, supports, type: RESET });

/**
 * Update a "canBeDisabled" flag for accessibility service.
 */
exports.updateCanBeDisabled = canBeDisabled => dispatch =>
  dispatch({ canBeDisabled, type: UPDATE_CAN_BE_DISABLED });

/**
 * Update a "canBeEnabled" flag for accessibility service.
 */
exports.updateCanBeEnabled = canBeEnabled => dispatch =>
  dispatch({ canBeEnabled, type: UPDATE_CAN_BE_ENABLED });

exports.updatePref = (name, value) => dispatch => {
  dispatch({ type: UPDATE_PREF, name, value });
  Services.prefs.setBoolPref(PREF_KEYS[name], value);
};

/**
 * Enable accessibility services in order to view accessible tree.
 */
exports.enable = accessibility => dispatch =>
  accessibility
    .enable()
    .then(() => dispatch({ type: ENABLE }))
    .catch(error => dispatch({ error, type: ENABLE }));

/**
 * Enable accessibility services in order to view accessible tree.
 */
exports.disable = accessibility => dispatch =>
  accessibility
    .disable()
    .then(() => dispatch({ type: DISABLE }))
    .catch(error => dispatch({ type: DISABLE, error }));
