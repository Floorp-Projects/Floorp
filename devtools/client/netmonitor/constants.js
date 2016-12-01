/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const general = {
  FREETEXT_FILTER_SEARCH_DELAY: 200,
  CONTENT_SIZE_DECIMALS: 2,
  REQUEST_TIME_DECIMALS: 2,
};

const actionTypes = {
  BATCH_ACTIONS: "BATCH_ACTIONS",
  BATCH_ENABLE: "BATCH_ENABLE",
  ADD_TIMING_MARKER: "ADD_TIMING_MARKER",
  CLEAR_TIMING_MARKERS: "CLEAR_TIMING_MARKERS",
  ADD_REQUEST: "ADD_REQUEST",
  UPDATE_REQUEST: "UPDATE_REQUEST",
  CLEAR_REQUESTS: "CLEAR_REQUESTS",
  CLONE_SELECTED_REQUEST: "CLONE_SELECTED_REQUEST",
  REMOVE_SELECTED_CUSTOM_REQUEST: "REMOVE_SELECTED_CUSTOM_REQUEST",
  SELECT_REQUEST: "SELECT_REQUEST",
  PRESELECT_REQUEST: "PRESELECT_REQUEST",
  SORT_BY: "SORT_BY",
  TOGGLE_FILTER_TYPE: "TOGGLE_FILTER_TYPE",
  ENABLE_FILTER_TYPE_ONLY: "ENABLE_FILTER_TYPE_ONLY",
  SET_FILTER_TEXT: "SET_FILTER_TEXT",
  OPEN_SIDEBAR: "OPEN_SIDEBAR",
  WATERFALL_RESIZE: "WATERFALL_RESIZE",
};

// Descriptions for what this frontend is currently doing.
const ACTIVITY_TYPE = {
  // Standing by and handling requests normally.
  NONE: 0,

  // Forcing the target to reload with cache enabled or disabled.
  RELOAD: {
    WITH_CACHE_ENABLED: 1,
    WITH_CACHE_DISABLED: 2,
    WITH_CACHE_DEFAULT: 3
  },

  // Enabling or disabling the cache without triggering a reload.
  ENABLE_CACHE: 3,
  DISABLE_CACHE: 4
};

module.exports = Object.assign({ ACTIVITY_TYPE }, general, actionTypes);
