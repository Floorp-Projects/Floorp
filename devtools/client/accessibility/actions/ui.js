/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  ENABLE,
  RESET,
  UPDATE_CAN_BE_DISABLED,
  UPDATE_CAN_BE_ENABLED,
  UPDATE_PREF,
  PREF_KEYS,
  UPDATE_DISPLAY_TABBING_ORDER,
} = require("devtools/client/accessibility/constants");

/**
 * Reset accessibility panel UI.
 */
exports.reset = (resetAccessiblity, supports) => async ({ dispatch }) => {
  try {
    const { enabled, canBeDisabled, canBeEnabled } = await resetAccessiblity();
    dispatch({ enabled, canBeDisabled, canBeEnabled, supports, type: RESET });
  } catch (error) {
    dispatch({ type: RESET, error });
  }
};

/**
 * Update a "canBeDisabled" flag for accessibility service.
 */
exports.updateCanBeDisabled = canBeDisabled => ({ dispatch }) =>
  dispatch({ canBeDisabled, type: UPDATE_CAN_BE_DISABLED });

/**
 * Update a "canBeEnabled" flag for accessibility service.
 */
exports.updateCanBeEnabled = canBeEnabled => ({ dispatch }) =>
  dispatch({ canBeEnabled, type: UPDATE_CAN_BE_ENABLED });

exports.updatePref = (name, value) => ({ dispatch }) => {
  dispatch({ type: UPDATE_PREF, name, value });
  Services.prefs.setBoolPref(PREF_KEYS[name], value);
};

/**
 * Enable accessibility services in order to view accessible tree.
 */
exports.enable = enableAccessibility => async ({ dispatch }) => {
  try {
    await enableAccessibility();
    dispatch({ type: ENABLE });
  } catch (error) {
    dispatch({ error, type: ENABLE });
  }
};

exports.updateDisplayTabbingOrder = tabbingOrderDisplayed => async ({
  dispatch,
  options: { toggleDisplayTabbingOrder },
}) => {
  try {
    await toggleDisplayTabbingOrder(tabbingOrderDisplayed);
    dispatch({ tabbingOrderDisplayed, type: UPDATE_DISPLAY_TABBING_ORDER });
  } catch (error) {
    dispatch({ error, type: UPDATE_DISPLAY_TABBING_ORDER });
  }
};
