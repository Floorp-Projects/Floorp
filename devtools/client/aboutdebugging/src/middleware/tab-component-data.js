/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DEBUG_TARGETS, REQUEST_TABS_SUCCESS } = require("../constants");

/**
 * This middleware converts tabs object that get from DebuggerClient.listTabs() to data
 * which is used in DebugTargetItem.
 */
const tabComponentDataMiddleware = store => next => action => {
  switch (action.type) {
    case REQUEST_TABS_SUCCESS: {
      action.tabs = toComponentData(action.tabs);
      break;
    }
  }

  return next(action);
};

function toComponentData(tabs) {
  return tabs.map(tab => {
    const type = DEBUG_TARGETS.TAB;
    const id = tab.outerWindowID;
    const icon = tab.favicon
      ? `data:image/png;base64,${btoa(
          String.fromCharCode.apply(String, tab.favicon)
        )}`
      : "chrome://devtools/skin/images/globe.svg";
    const name = tab.title || tab.url;
    const url = tab.url;
    return {
      name,
      icon,
      id,
      type,
      details: {
        url,
      },
    };
  });
}

module.exports = tabComponentDataMiddleware;
