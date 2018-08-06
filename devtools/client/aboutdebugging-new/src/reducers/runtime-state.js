/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  CONNECT_RUNTIME_SUCCESS,
  DISCONNECT_RUNTIME_SUCCESS,
  REQUEST_TABS_SUCCESS,
} = require("../constants");

function RuntimeState() {
  return {
    client: null,
    tabs: [],
  };
}

function runtimeReducer(state = RuntimeState(), action) {
  switch (action.type) {
    case CONNECT_RUNTIME_SUCCESS: {
      const { client } = action;
      return Object.assign({}, state, { client });
    }
    case DISCONNECT_RUNTIME_SUCCESS: {
      return RuntimeState();
    }
    case REQUEST_TABS_SUCCESS: {
      const { tabs } = action;
      return Object.assign({}, state, { tabs: toTabComponentData(tabs) });
    }

    default:
      return state;
  }
}

function toTabComponentData(tabs) {
  return tabs.map(tab => {
    const icon = tab.favicon
      ? `data:image/png;base64,${ btoa(String.fromCharCode.apply(String, tab.favicon)) }`
      : "chrome://devtools/skin/images/globe.svg";
    const name = tab.title || tab.url;
    const url = tab.url;
    return { icon, name, url };
  });
}

module.exports = {
  RuntimeState,
  runtimeReducer,
};
