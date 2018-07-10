"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.log = log;

var _devtoolsEnvironment = require("devtools/client/debugger/new/dist/vendors").vendored["devtools-environment"];

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* global window */
const blacklist = ["SET_POPUP_OBJECT_PROPERTIES", "SET_PAUSE_POINTS", "SET_SYMBOLS", "OUT_OF_SCOPE_LOCATIONS", "MAP_SCOPES", "MAP_FRAMES", "ADD_SCOPES", "IN_SCOPE_LINES", "REMOVE_BREAKPOINT"];

function cloneAction(action) {
  action = action || {};
  action = { ...action
  }; // ADD_TAB, ...

  if (action.source && action.source.text) {
    const source = { ...action.source,
      text: ""
    };
    action.source = source;
  }

  if (action.sources) {
    const sources = action.sources.slice(0, 20).map(source => {
      const url = !source.url || source.url.includes("data:") ? "" : source.url;
      return { ...source,
        url
      };
    });
    action.sources = sources;
  } // LOAD_SOURCE_TEXT


  if (action.text) {
    action.text = "";
  }

  if (action.value && action.value.text) {
    const value = { ...action.value,
      text: ""
    };
    action.value = value;
  }

  return action;
}

function formatFrame(frame) {
  const {
    id,
    location,
    displayName
  } = frame;
  return {
    id,
    location,
    displayName
  };
}

function formatPause(pause) {
  return { ...pause,
    pauseInfo: {
      why: pause.why
    },
    scopes: [],
    frames: pause.frames.map(formatFrame),
    loadedObjects: []
  };
}

function serializeAction(action) {
  try {
    action = cloneAction(action);

    if (blacklist.includes(action.type)) {
      action = {};
    }

    if (action.type === "PAUSED") {
      action = formatPause(action);
    } // dump(`> ${action.type}...\n ${JSON.stringify(action)}\n`);


    return JSON.stringify(action);
  } catch (e) {
    console.error(e);
  }
}
/**
 * A middleware that logs all actions coming through the system
 * to the console.
 */


function log({
  dispatch,
  getState
}) {
  return next => action => {
    const asyncMsg = !action.status ? "" : `[${action.status}]`;

    if ((0, _devtoolsEnvironment.isTesting)()) {
      dump(`[ACTION] ${action.type} ${asyncMsg} - ${serializeAction(action)}\n`);
    } else {
      console.log(action, asyncMsg);
    }

    next(action);
  };
}