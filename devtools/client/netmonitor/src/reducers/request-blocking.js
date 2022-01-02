/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  ADD_BLOCKED_URL,
  DISABLE_MATCHING_URLS,
  TOGGLE_BLOCKED_URL,
  UPDATE_BLOCKED_URL,
  REMOVE_BLOCKED_URL,
  REMOVE_ALL_BLOCKED_URLS,
  ENABLE_ALL_BLOCKED_URLS,
  DISABLE_ALL_BLOCKED_URLS,
  TOGGLE_BLOCKING_ENABLED,
  SYNCED_BLOCKED_URLS,
} = require("devtools/client/netmonitor/src/constants");

function RequestBlocking() {
  return {
    blockedUrls: [],
    blockingSynced: false,
    blockingEnabled: true,
  };
}

function requestBlockingReducer(state = RequestBlocking(), action) {
  switch (action.type) {
    case ADD_BLOCKED_URL:
      return addBlockedUrl(state, action);
    case REMOVE_BLOCKED_URL:
      return removeBlockedUrl(state, action);
    case REMOVE_ALL_BLOCKED_URLS:
      return removeAllBlockedUrls(state, action);
    case UPDATE_BLOCKED_URL:
      return updateBlockedUrl(state, action);
    case TOGGLE_BLOCKED_URL:
      return toggleBlockedUrl(state, action);
    case TOGGLE_BLOCKING_ENABLED:
      return toggleBlockingEnabled(state, action);
    case ENABLE_ALL_BLOCKED_URLS:
      return enableAllBlockedUrls(state, action);
    case DISABLE_ALL_BLOCKED_URLS:
      return disableAllBlockedUrls(state, action);
    case DISABLE_MATCHING_URLS:
      return disableOrRemoveMatchingUrls(state, action);
    case SYNCED_BLOCKED_URLS:
      return syncedBlockedUrls(state, action);
    default:
      return state;
  }
}

function syncedBlockedUrls(state, action) {
  // Indicates whether the blocked url has been synced
  // with the server once. We don't need to do it once netmonitor is open.
  return {
    ...state,
    blockingSynced: action.synced,
  };
}

function toggleBlockingEnabled(state, action) {
  return {
    ...state,
    blockingEnabled: action.enabled,
  };
}

function addBlockedUrl(state, action) {
  // The user can paste in a list of URLS so we need to cleanse the input
  // Pasting a list turns new lines into spaces
  const uniqueUrls = [...new Set(action.url.split(" "))].map(url => url.trim());

  const newUrls = uniqueUrls
    // Ensure the URL isn't already blocked
    .filter(url => url && !state.blockedUrls.some(item => item.url === url))
    // Add new URLs as enabled by default
    .map(url => ({ url, enabled: true }));

  // If the user is trying to block a URL that's currently in the list but disabled,
  // re-enable the old item
  const currentBlockedUrls = state.blockedUrls.map(item =>
    uniqueUrls.includes(item.url) ? { url: item.url, enabled: true } : item
  );

  const blockedUrls = [...currentBlockedUrls, ...newUrls];
  return {
    ...state,
    blockedUrls,
  };
}

function removeBlockedUrl(state, action) {
  return {
    ...state,
    blockedUrls: state.blockedUrls.filter(item => item.url != action.url),
  };
}

function removeAllBlockedUrls(state, action) {
  return {
    ...state,
    blockedUrls: [],
  };
}

function enableAllBlockedUrls(state, action) {
  const blockedUrls = state.blockedUrls.map(item => ({
    ...item,
    enabled: true,
  }));
  return {
    ...state,
    blockedUrls,
  };
}

function disableAllBlockedUrls(state, action) {
  const blockedUrls = state.blockedUrls.map(item => ({
    ...item,
    enabled: false,
  }));
  return {
    ...state,
    blockedUrls,
  };
}

function updateBlockedUrl(state, action) {
  const { oldUrl, newUrl } = action;
  let { blockedUrls } = state;

  if (!blockedUrls.find(item => item.url === newUrl)) {
    blockedUrls = blockedUrls.map(item => {
      if (item.url === oldUrl) {
        return { ...item, url: newUrl };
      }
      return item;
    });
  } else {
    blockedUrls = blockedUrls.filter(item => item.url != oldUrl);
  }

  return {
    ...state,
    blockedUrls,
  };
}

function toggleBlockedUrl(state, action) {
  const blockedUrls = state.blockedUrls.map(item => {
    if (item.url === action.url) {
      return { ...item, enabled: !item.enabled };
    }
    return item;
  });

  return {
    ...state,
    blockedUrls,
  };
}

function disableOrRemoveMatchingUrls(state, action) {
  const blockedUrls = state.blockedUrls
    .map(item => {
      // If the url matches exactly, remove the entry
      if (action.url === item.url) {
        return null;
      }
      // If just a partial match, disable the entry
      if (action.url.includes(item.url)) {
        return { ...item, enabled: false };
      }
      return item;
    })
    .filter(Boolean);

  return {
    ...state,
    blockedUrls,
  };
}

module.exports = requestBlockingReducer;
