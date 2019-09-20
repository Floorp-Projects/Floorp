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
} = require("../constants");

/**
 * Toggle blocking enabled
 */
function toggleBlockingEnabled(isChecked) {
  /*
    TODO:  Send a signal to server to toggle blocking
  */
  return {
    type: TOGGLE_BLOCKING_ENABLED,
    enabled: isChecked,
  };
}

function removeBlockedUrl(url) {
  /*
    TODO:  Sync blocked URL list
  */
  return {
    type: REMOVE_BLOCKED_URL,
    url,
  };
}

function addBlockedUrl(url) {
  /*
    TODO:  Sync blocked URL list
  */
  return {
    type: ADD_BLOCKED_URL,
    url,
  };
}

function toggleBlockedUrl(url) {
  /*
    TODO:  Sync blocked URL list
  */
  return {
    type: TOGGLE_BLOCKED_URL,
    url,
  };
}

function updateBlockedUrl(oldUrl, newUrl) {
  /*
    TODO:  Sync blocked URL list
  */
  return {
    type: UPDATE_BLOCKED_URL,
    oldUrl,
    newUrl,
  };
}

module.exports = {
  addBlockedUrl,
  toggleBlockingEnabled,
  toggleBlockedUrl,
  removeBlockedUrl,
  updateBlockedUrl,
};
