/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  OPEN_ACTION_BAR,
  SELECT_ACTION_BAR_TAB,
  PANELS,
  RIGHT_CLICK_REQUEST,
  PRESELECT_REQUEST,
} = require("devtools/client/netmonitor/src/constants");

const {
  selectRequest,
} = require("devtools/client/netmonitor/src/actions/selection");

const {
  openNetworkDetails,
} = require("devtools/client/netmonitor/src/actions/ui");

const {
  getRequestByChannelId,
} = require("devtools/client/netmonitor/src/selectors/index");
/**
 * Open the entire HTTP Custom Request panel
 * @returns {Function}
 */
function openHTTPCustomRequest(isOpen) {
  return ({ dispatch, getState }) => {
    dispatch({ type: OPEN_ACTION_BAR, open: isOpen });

    dispatch({
      type: SELECT_ACTION_BAR_TAB,
      id: PANELS.HTTP_CUSTOM_REQUEST,
    });
  };
}

/**
 * Toggle visibility of New Custom Request panel in network panel
 */
function toggleHTTPCustomRequestPanel() {
  return ({ dispatch, getState }) => {
    const state = getState();

    const shouldClose =
      state.ui.networkActionOpen &&
      state.ui.selectedActionBarTabId === PANELS.HTTP_CUSTOM_REQUEST;
    dispatch({ type: OPEN_ACTION_BAR, open: !shouldClose });

    // reset the right clicked request
    dispatch({ type: RIGHT_CLICK_REQUEST, id: null });

    dispatch({
      type: SELECT_ACTION_BAR_TAB,
      id: PANELS.HTTP_CUSTOM_REQUEST,
    });
  };
}

/**
 * Send a new HTTP request using the data in the custom request form.
 */
function sendHTTPCustomRequest(connector, request) {
  return async ({ dispatch, getState }) => {
    if (!request) {
      return;
    }

    // Send a new HTTP request using the data in the custom request form
    const data = {
      cause: request.cause || {},
      url: request.url,
      method: request.method,
      httpVersion: request.httpVersion,
    };

    if (request.headers) {
      data.headers = request.headers;
    }

    if (request.requestPostData) {
      data.body = request.requestPostData.postData?.text;
    }

    const { channelId } = await connector.sendHTTPRequest(data);

    const newRequest = getRequestByChannelId(getState(), channelId);
    // If the new custom request is available already select the request, else
    // preselect the request.
    if (newRequest) {
      await dispatch(selectRequest(newRequest.id));
    } else {
      await dispatch({
        type: PRESELECT_REQUEST,
        id: channelId,
      });
    }
    dispatch(openNetworkDetails(true));
  };
}

module.exports = {
  openHTTPCustomRequest,
  toggleHTTPCustomRequestPanel,
  sendHTTPCustomRequest,
};
