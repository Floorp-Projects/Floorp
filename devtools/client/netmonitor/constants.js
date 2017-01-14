/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const general = {
  FILTER_SEARCH_DELAY: 200,
  // 100 KB in bytes
  SOURCE_SYNTAX_HIGHLIGHT_MAX_FILE_SIZE: 102400,
};

const actionTypes = {
  ADD_REQUEST: "ADD_REQUEST",
  ADD_TIMING_MARKER: "ADD_TIMING_MARKER",
  BATCH_ACTIONS: "BATCH_ACTIONS",
  BATCH_ENABLE: "BATCH_ENABLE",
  CLEAR_REQUESTS: "CLEAR_REQUESTS",
  CLEAR_TIMING_MARKERS: "CLEAR_TIMING_MARKERS",
  CLONE_SELECTED_REQUEST: "CLONE_SELECTED_REQUEST",
  ENABLE_REQUEST_FILTER_TYPE_ONLY: "ENABLE_REQUEST_FILTER_TYPE_ONLY",
  OPEN_SIDEBAR: "OPEN_SIDEBAR",
  OPEN_STATISTICS: "OPEN_STATISTICS",
  PRESELECT_REQUEST: "PRESELECT_REQUEST",
  REMOVE_SELECTED_CUSTOM_REQUEST: "REMOVE_SELECTED_CUSTOM_REQUEST",
  SELECT_REQUEST: "SELECT_REQUEST",
  SELECT_DETAILS_PANEL_TAB: "SELECT_DETAILS_PANEL_TAB",
  SET_REQUEST_FILTER_TEXT: "SET_REQUEST_FILTER_TEXT",
  SORT_BY: "SORT_BY",
  TOGGLE_REQUEST_FILTER_TYPE: "TOGGLE_REQUEST_FILTER_TYPE",
  UPDATE_REQUEST: "UPDATE_REQUEST",
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
