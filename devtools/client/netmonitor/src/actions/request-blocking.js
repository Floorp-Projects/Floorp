/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");

const {
  ADD_BLOCKED_URL,
  TOGGLE_BLOCKING_ENABLED,
  TOGGLE_BLOCKED_URL,
  UPDATE_BLOCKED_URL,
  REMOVE_BLOCKED_URL,
  REMOVE_ALL_BLOCKED_URLS,
  ENABLE_ALL_BLOCKED_URLS,
  DISABLE_ALL_BLOCKED_URLS,
  DISABLE_MATCHING_URLS,
  OPEN_ACTION_BAR,
  SELECT_ACTION_BAR_TAB,
  PANELS,
} = require("devtools/client/netmonitor/src/constants");

function toggleRequestBlockingPanel() {
  return (dispatch, getState) => {
    const state = getState();
    state.ui.networkActionOpen &&
    state.ui.selectedActionBarTabId === PANELS.BLOCKING
      ? dispatch({ type: OPEN_ACTION_BAR, open: false })
      : dispatch({ type: OPEN_ACTION_BAR, open: true });
    dispatch({
      type: SELECT_ACTION_BAR_TAB,
      id: PANELS.BLOCKING,
    });
  };
}

function toggleBlockingEnabled(enabled) {
  return {
    type: TOGGLE_BLOCKING_ENABLED,
    enabled,
  };
}

function removeBlockedUrl(url) {
  return {
    type: REMOVE_BLOCKED_URL,
    url,
  };
}

function removeAllBlockedUrls() {
  return { type: REMOVE_ALL_BLOCKED_URLS };
}

function enableAllBlockedUrls() {
  return { type: ENABLE_ALL_BLOCKED_URLS };
}

function disableAllBlockedUrls() {
  return { type: DISABLE_ALL_BLOCKED_URLS };
}

function addBlockedUrl(url) {
  return {
    type: ADD_BLOCKED_URL,
    url,
  };
}

function toggleBlockedUrl(url) {
  return {
    type: TOGGLE_BLOCKED_URL,
    url,
  };
}

function updateBlockedUrl(oldUrl, newUrl) {
  return {
    type: UPDATE_BLOCKED_URL,
    oldUrl,
    newUrl,
  };
}

function openRequestBlockingAndAddUrl(url) {
  return (dispatch, getState) => {
    const showBlockingPanel = Services.prefs.getBoolPref(
      "devtools.netmonitor.features.requestBlocking"
    );

    if (showBlockingPanel) {
      dispatch({ type: OPEN_ACTION_BAR, open: true });
      dispatch({
        type: SELECT_ACTION_BAR_TAB,
        id: PANELS.BLOCKING,
      });
    }
    dispatch({ type: ADD_BLOCKED_URL, url });
  };
}

function openRequestBlockingAndDisableUrls(url) {
  return (dispatch, getState) => {
    const showBlockingPanel = Services.prefs.getBoolPref(
      "devtools.netmonitor.features.requestBlocking"
    );

    if (showBlockingPanel) {
      dispatch({ type: OPEN_ACTION_BAR, open: true });
      dispatch({
        type: SELECT_ACTION_BAR_TAB,
        id: PANELS.BLOCKING,
      });
    }

    dispatch({ type: DISABLE_MATCHING_URLS, url });
  };
}

module.exports = {
  toggleRequestBlockingPanel,
  addBlockedUrl,
  toggleBlockingEnabled,
  toggleBlockedUrl,
  removeBlockedUrl,
  removeAllBlockedUrls,
  enableAllBlockedUrls,
  disableAllBlockedUrls,
  updateBlockedUrl,
  openRequestBlockingAndAddUrl,
  openRequestBlockingAndDisableUrls,
};
