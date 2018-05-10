"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.log = log;

var _devtoolsEnvironment = require("devtools/client/debugger/new/dist/vendors").vendored["devtools-environment"];

function _objectSpread(target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i] != null ? arguments[i] : {}; var ownKeys = Object.keys(source); if (typeof Object.getOwnPropertySymbols === 'function') { ownKeys = ownKeys.concat(Object.getOwnPropertySymbols(source).filter(function (sym) { return Object.getOwnPropertyDescriptor(source, sym).enumerable; })); } ownKeys.forEach(function (key) { _defineProperty(target, key, source[key]); }); } return target; }

function _defineProperty(obj, key, value) { if (key in obj) { Object.defineProperty(obj, key, { value: value, enumerable: true, configurable: true, writable: true }); } else { obj[key] = value; } return obj; }

const blacklist = ["SET_POPUP_OBJECT_PROPERTIES", "SET_PAUSE_POINTS", "SET_SYMBOLS", "OUT_OF_SCOPE_LOCATIONS", "MAP_SCOPES", "MAP_FRAMES", "ADD_SCOPES", "IN_SCOPE_LINES", "REMOVE_BREAKPOINT", "ADD_BREAKPOINT"];

function cloneAction(action) {
  action = action || {};
  action = _objectSpread({}, action); // ADD_TAB, ...

  if (action.source && action.source.text) {
    const source = _objectSpread({}, action.source, {
      text: ""
    });

    action.source = source;
  }

  if (action.sources) {
    const sources = action.sources.slice(0, 20).map(source => {
      const url = !source.url || source.url.includes("data:") ? "" : source.url;
      return _objectSpread({}, source, {
        url
      });
    });
    action.sources = sources;
  } // LOAD_SOURCE_TEXT


  if (action.text) {
    action.text = "";
  }

  if (action.value && action.value.text) {
    const value = _objectSpread({}, action.value, {
      text: ""
    });

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
  return _objectSpread({}, pause, {
    pauseInfo: {
      why: pause.why
    },
    scopes: [],
    frames: pause.frames.map(formatFrame),
    loadedObjects: []
  });
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