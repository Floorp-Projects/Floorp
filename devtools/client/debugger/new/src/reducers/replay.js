"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.initialState = initialState;
exports.getHistory = getHistory;
exports.getHistoryFrame = getHistoryFrame;
exports.getHistoryPosition = getHistoryPosition;

var _prefs = require("../utils/prefs");

function _objectSpread(target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i] != null ? arguments[i] : {}; var ownKeys = Object.keys(source); if (typeof Object.getOwnPropertySymbols === 'function') { ownKeys = ownKeys.concat(Object.getOwnPropertySymbols(source).filter(function (sym) { return Object.getOwnPropertyDescriptor(source, sym).enumerable; })); } ownKeys.forEach(function (key) { _defineProperty(target, key, source[key]); }); } return target; }

function _defineProperty(obj, key, value) { if (key in obj) { Object.defineProperty(obj, key, { value: value, enumerable: true, configurable: true, writable: true }); } else { obj[key] = value; } return obj; }

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
        return _objectSpread({}, state, {
          position: action.position
        });
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
    status,
    value
  } = action;
  const selectedFrameId = frame.id;
  const instance = state.history[state.position];

  if (!instance) {
    return state;
  }

  const pausedInst = instance.paused;

  const generated = _objectSpread({}, pausedInst.frameScopes.generated, {
    [selectedFrameId]: {
      pending: status !== "done",
      scope: value
    }
  });

  const newPaused = _objectSpread({}, pausedInst, {
    frameScopes: _objectSpread({}, pausedInst.frameScopes, {
      generated
    })
  });

  const history = [...state.history];
  history[state.position] = _objectSpread({}, instance, {
    paused: newPaused
  });
  return _objectSpread({}, state, {
    history
  });
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

  const original = _objectSpread({}, pausedInst.frameScopes.original, {
    [selectedFrameId]: {
      pending: status !== "done",
      scope: value
    }
  });

  const newPaused = _objectSpread({}, pausedInst, {
    frameScopes: _objectSpread({}, pausedInst.frameScopes, {
      original
    })
  });

  const history = [...state.history];
  history[state.position] = _objectSpread({}, instance, {
    paused: newPaused
  });
  return _objectSpread({}, state, {
    history
  });
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
  history[state.position] = _objectSpread({}, instance, {
    expressions
  });
  return _objectSpread({}, state, {
    history
  });
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
  return _objectSpread({}, state, {
    history,
    position
  });
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