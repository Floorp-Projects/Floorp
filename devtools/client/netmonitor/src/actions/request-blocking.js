/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

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
  SYNCED_BLOCKED_URLS,
  OPEN_ACTION_BAR,
  SELECT_ACTION_BAR_TAB,
  PANELS,
} = require("devtools/client/netmonitor/src/constants");

function toggleRequestBlockingPanel() {
  return async ({ dispatch, getState }) => {
    const state = getState();
    if (
      state.ui.networkActionOpen &&
      state.ui.selectedActionBarTabId === PANELS.BLOCKING
    ) {
      dispatch(closeRequestBlocking());
    } else {
      dispatch(await openRequestBlocking());
    }
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

async function openRequestBlocking() {
  return async ({ dispatch, getState, connector }) => {
    const state = getState();
    if (!state.requestBlocking.blockingSynced) {
      const blockedUrls = state.requestBlocking.blockedUrls;
      const responses = await connector.getBlockedUrls();
      const urls = responses.flat();
      if (urls.length !== blockedUrls.length) {
        urls.forEach(url => dispatch(addBlockedUrl(url)));
      }
      dispatch({ type: SYNCED_BLOCKED_URLS, synced: true });
    }

    dispatch({ type: OPEN_ACTION_BAR, open: true });
    dispatch({
      type: SELECT_ACTION_BAR_TAB,
      id: PANELS.BLOCKING,
    });
  };
}

function closeRequestBlocking() {
  return ({ dispatch }) => {
    dispatch({ type: OPEN_ACTION_BAR, open: false });
    dispatch({
      type: SELECT_ACTION_BAR_TAB,
      id: PANELS.BLOCKING,
    });
  };
}

function openRequestBlockingAndAddUrl(url) {
  return async ({ dispatch, getState }) => {
    const showBlockingPanel = Services.prefs.getBoolPref(
      "devtools.netmonitor.features.requestBlocking"
    );

    if (showBlockingPanel) {
      dispatch(await openRequestBlocking());
    }
    dispatch({ type: ADD_BLOCKED_URL, url });
  };
}

function openRequestBlockingAndDisableUrls(url) {
  return async ({ dispatch, getState }) => {
    const showBlockingPanel = Services.prefs.getBoolPref(
      "devtools.netmonitor.features.requestBlocking"
    );

    if (showBlockingPanel) {
      dispatch(await openRequestBlocking());
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
