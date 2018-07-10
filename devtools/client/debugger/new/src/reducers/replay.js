"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.initialState = initialState;
exports.getHistory = getHistory;
exports.getHistoryFrame = getHistoryFrame;
exports.getHistoryPosition = getHistoryPosition;

var _prefs = require("../utils/prefs");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function initialState() {
  return {
    history: [],
    position: -1
  };
}

const defaultFrameScopes = {
  original: {},
  generated: {}
};

function update(state = initialState(), action) {
  if (!_prefs.features.replay) {
    return state;
  }

  switch (action.type) {
    case "TRAVEL_TO":
      {
        return { ...state,
          position: action.position
        };
      }

    case "ADD_SCOPES":
      {
        return addScopes(state, action);
      }

    case "MAP_SCOPES":
      {
        return mapScopes(state, action);
      }

    case "CLEAR_HISTORY":
      {
        return {
          history: [],
          position: -1
        };
      }

    case "PAUSED":
      {
        return paused(state, action);
      }

    case "EVALUATE_EXPRESSION":
      {
        return evaluateExpression(state, action);
      }
  }

  return state;
}

function addScopes(state, action) {
  const {
    frame,
    status
  } = action;
  const selectedFrameId = frame.id;
  const instance = state.history[state.position];

  if (!instance) {
    return state;
  }

  const pausedInst = instance.paused;
  const scopeValue = status === "done" ? {
    pending: false,
    scope: action.value
  } : {
    pending: true,
    scope: null
  };
  const generated = { ...pausedInst.frameScopes.generated,
    [selectedFrameId]: scopeValue
  };
  const newPaused = { ...pausedInst,
    frameScopes: { ...pausedInst.frameScopes,
      generated
    }
  };
  const history = [...state.history];
  history[state.position] = { ...instance,
    paused: newPaused
  };
  return { ...state,
    history
  };
}

function mapScopes(state, action) {
  const {
    frame,
    status,
    value
  } = action;
  const selectedFrameId = frame.id;
  const instance = state.history[state.position];

  if (!instance) {
    return state;
  }

  const pausedInst = instance.paused;
  const original = { ...pausedInst.frameScopes.original,
    [selectedFrameId]: {
      pending: status !== "done",
      scope: value
    }
  };
  const newPaused = { ...pausedInst,
    frameScopes: { ...pausedInst.frameScopes,
      original
    }
  };
  const history = [...state.history];
  history[state.position] = { ...instance,
    paused: newPaused
  };
  return { ...state,
    history
  };
}

function evaluateExpression(state, action) {
  const {
    input,
    value
  } = action;
  const instance = state.history[state.position];

  if (!instance) {
    return state;
  }

  const prevExpressions = instance.expressions || [];
  const expression = {
    input,
    value
  };
  const expressions = [...prevExpressions, expression];
  const history = [...state.history];
  history[state.position] = { ...instance,
    expressions
  };
  return { ...state,
    history
  };
}

function paused(state, action) {
  const {
    selectedFrameId,
    frames,
    loadedObjects,
    why
  } = action; // turn this into an object keyed by object id

  const objectMap = {};
  loadedObjects.forEach(obj => {
    objectMap[obj.value.objectId] = obj;
  });
  const pausedInfo = {
    isWaitingOnBreak: false,
    selectedFrameId,
    frames,
    frameScopes: defaultFrameScopes,
    loadedObjects: objectMap,
    why
  };
  const history = [...state.history, {
    paused: pausedInfo
  }];
  const position = state.position + 1;
  return { ...state,
    history,
    position
  };
}

function getHistory(state) {
  return state.replay.history;
}

function getHistoryFrame(state, position) {
  return state.replay.history[position];
}

function getHistoryPosition(state) {
  return state.replay.position;
}

exports.default = update;