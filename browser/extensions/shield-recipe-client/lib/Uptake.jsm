/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {utils: Cu} = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(
  this, "UptakeTelemetry", "resource://services-common/uptake-telemetry.js");

this.EXPORTED_SYMBOLS = ["Uptake"];

const SOURCE_PREFIX = "shield-recipe-client";

this.Uptake = {
  // Action uptake
  ACTION_NETWORK_ERROR: UptakeTelemetry.STATUS.NETWORK_ERROR,
  ACTION_PRE_EXECUTION_ERROR: UptakeTelemetry.STATUS.CUSTOM_1_ERROR,
  ACTION_POST_EXECUTION_ERROR: UptakeTelemetry.STATUS.CUSTOM_2_ERROR,
  ACTION_SERVER_ERROR: UptakeTelemetry.STATUS.SERVER_ERROR,
  ACTION_SUCCESS: UptakeTelemetry.STATUS.SUCCESS,

  // Per-recipe uptake
  RECIPE_ACTION_DISABLED: UptakeTelemetry.STATUS.CUSTOM_1_ERROR,
  RECIPE_EXECUTION_ERROR: UptakeTelemetry.STATUS.APPLY_ERROR,
  RECIPE_INVALID_ACTION: UptakeTelemetry.STATUS.DOWNLOAD_ERROR,
  RECIPE_SUCCESS: UptakeTelemetry.STATUS.SUCCESS,

  // Uptake for the runner as a whole
  RUNNER_INVALID_SIGNATURE: UptakeTelemetry.STATUS.SIGNATURE_ERROR,
  RUNNER_NETWORK_ERROR: UptakeTelemetry.STATUS.NETWORK_ERROR,
  RUNNER_SERVER_ERROR: UptakeTelemetry.STATUS.SERVER_ERROR,
  RUNNER_SUCCESS: UptakeTelemetry.STATUS.SUCCESS,

  reportRunner(status) {
    UptakeTelemetry.report(`${SOURCE_PREFIX}/runner`, status);
  },

  reportRecipe(recipeId, status) {
    UptakeTelemetry.report(`${SOURCE_PREFIX}/recipe/${recipeId}`, status);
  },

  reportAction(actionName, status) {
    UptakeTelemetry.report(`${SOURCE_PREFIX}/action/${actionName}`, status);
  },
};
