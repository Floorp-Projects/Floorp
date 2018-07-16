"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.getSelectedFrame = exports.getAllPopupObjectProperties = exports.createPauseState = undefined;
exports.getPauseReason = getPauseReason;
exports.getPauseCommand = getPauseCommand;
exports.isStepping = isStepping;
exports.isPaused = isPaused;
exports.getIsPaused = getIsPaused;
exports.getPreviousPauseFrameLocation = getPreviousPauseFrameLocation;
exports.isEvaluatingExpression = isEvaluatingExpression;
exports.getPopupObjectProperties = getPopupObjectProperties;
exports.getIsWaitingOnBreak = getIsWaitingOnBreak;
exports.getShouldPauseOnExceptions = getShouldPauseOnExceptions;
exports.getShouldPauseOnCaughtExceptions = getShouldPauseOnCaughtExceptions;
exports.getCanRewind = getCanRewind;
exports.getExtra = getExtra;
exports.getFrames = getFrames;
exports.getGeneratedFrameScope = getGeneratedFrameScope;
exports.getOriginalFrameScope = getOriginalFrameScope;
exports.getFrameScopes = getFrameScopes;
exports.getSelectedFrameBindings = getSelectedFrameBindings;
exports.getFrameScope = getFrameScope;
exports.getSelectedScope = getSelectedScope;
exports.getSelectedScopeMappings = getSelectedScopeMappings;
exports.getSelectedFrameId = getSelectedFrameId;
exports.getSelectedComponentIndex = getSelectedComponentIndex;
exports.getTopFrame = getTopFrame;
exports.getDebuggeeUrl = getDebuggeeUrl;
exports.getSkipPausing = getSkipPausing;
exports.getChromeScopes = getChromeScopes;

var _reselect = require("devtools/client/debugger/new/dist/vendors").vendored["reselect"];

var _devtoolsSourceMap = require("devtools/client/shared/source-map/index.js");

var _prefs = require("../utils/prefs");

var _sources = require("./sources");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* eslint complexity: ["error", 30]*/

/**
 * Pause reducer
 * @module reducers/pause
 */
const createPauseState = exports.createPauseState = () => ({
  extra: {},
  why: null,
  isWaitingOnBreak: false,
  frames: undefined,
  selectedFrameId: undefined,
  selectedComponentIndex: undefined,
  frameScopes: {
    generated: {},
    original: {},
    mappings: {}
  },
  loadedObjects: {},
  shouldPauseOnExceptions: _prefs.prefs.pauseOnExceptions,
  shouldPauseOnCaughtExceptions: _prefs.prefs.pauseOnCaughtExceptions,
  canRewind: false,
  debuggeeUrl: "",
  command: null,
  previousLocation: null,
  skipPausing: _prefs.prefs.skipPausing
});

const emptyPauseState = {
  frames: null,
  frameScopes: {
    generated: {},
    original: {},
    mappings: {}
  },
  selectedFrameId: null,
  loadedObjects: {},
  why: null
};

function update(state = createPauseState(), action) {
  switch (action.type) {
    case "PAUSED":
      {
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
        return { ...state,
          isWaitingOnBreak: false,
          selectedFrameId,
          frames,
          frameScopes: { ...emptyPauseState.frameScopes
          },
          loadedObjects: objectMap,
          why
        };
      }

    case "MAP_FRAMES":
      {
        return { ...state,
          frames: action.frames
        };
      }

    case "ADD_EXTRA":
      {
        return { ...state,
          extra: action.extra
        };
      }

    case "ADD_SCOPES":
      {
        const {
          frame,
          status,
          value
        } = action;
        const selectedFrameId = frame.id;
        const generated = { ...state.frameScopes.generated,
          [selectedFrameId]: {
            pending: status !== "done",
            scope: value
          }
        };
        return { ...state,
          frameScopes: { ...state.frameScopes,
            generated
          }
        };
      }

    case "TRAVEL_TO":
      return { ...state,
        ...action.data.paused
      };

    case "MAP_SCOPES":
      {
        const {
          frame,
          status,
          value
        } = action;
        const selectedFrameId = frame.id;
        const original = { ...state.frameScopes.original,
          [selectedFrameId]: {
            pending: status !== "done",
            scope: value && value.scope
          }
        };
        const mappings = { ...state.frameScopes.mappings,
          [selectedFrameId]: value && value.mappings
        };
        return { ...state,
          frameScopes: { ...state.frameScopes,
            original,
            mappings
          }
        };
      }

    case "BREAK_ON_NEXT":
      return { ...state,
        isWaitingOnBreak: true
      };

    case "SELECT_FRAME":
      return { ...state,
        selectedFrameId: action.frame.id
      };

    case "SELECT_COMPONENT":
      return { ...state,
        selectedComponentIndex: action.componentIndex
      };

    case "SET_POPUP_OBJECT_PROPERTIES":
      if (!action.properties) {
        return { ...state
        };
      }

      return { ...state,
        loadedObjects: { ...state.loadedObjects,
          [action.objectId]: action.properties
        }
      };

    case "CONNECT":
      return { ...createPauseState(),
        debuggeeUrl: action.url,
        canRewind: action.canRewind
      };

    case "PAUSE_ON_EXCEPTIONS":
      const {
        shouldPauseOnExceptions,
        shouldPauseOnCaughtExceptions
      } = action;
      _prefs.prefs.pauseOnExceptions = shouldPauseOnExceptions;
      _prefs.prefs.pauseOnCaughtExceptions = shouldPauseOnCaughtExceptions; // Preserving for the old debugger

      _prefs.prefs.ignoreCaughtExceptions = !shouldPauseOnCaughtExceptions;
      return { ...state,
        shouldPauseOnExceptions,
        shouldPauseOnCaughtExceptions
      };

    case "COMMAND":
      {
        return action.status === "start" ? { ...state,
          ...emptyPauseState,
          command: action.command,
          previousLocation: getPauseLocation(state, action)
        } : { ...state,
          command: null
        };
      }

    case "RESUME":
      return { ...state,
        ...emptyPauseState
      };

    case "EVALUATE_EXPRESSION":
      return { ...state,
        command: action.status === "start" ? "expression" : null
      };

    case "NAVIGATE":
      return { ...state,
        ...emptyPauseState,
        debuggeeUrl: action.url
      };

    case "TOGGLE_SKIP_PAUSING":
      {
        const {
          skipPausing
        } = action;
        _prefs.prefs.skipPausing = skipPausing;
        return { ...state,
          skipPausing
        };
      }
  }

  return state;
}

function getPauseLocation(state, action) {
  const {
    frames,
    previousLocation
  } = state; // NOTE: We store the previous location so that we ensure that we
  // do not stop at the same location twice when we step over.

  if (action.command !== "stepOver") {
    return null;
  }

  const frame = frames && frames[0];

  if (!frame) {
    return previousLocation;
  }

  return {
    location: frame.location,
    generatedLocation: frame.generatedLocation
  };
} // Selectors
// Unfortunately, it's really hard to make these functions accept just
// the state that we care about and still type it with Flow. The
// problem is that we want to re-export all selectors from a single
// module for the UI, and all of those selectors should take the
// top-level app state, so we'd have to "wrap" them to automatically
// pick off the piece of state we're interested in. It's impossible
// (right now) to type those wrapped functions.


const getPauseState = state => state.pause;

const getAllPopupObjectProperties = exports.getAllPopupObjectProperties = (0, _reselect.createSelector)(getPauseState, pauseWrapper => pauseWrapper.loadedObjects);

function getPauseReason(state) {
  return state.pause.why;
}

function getPauseCommand(state) {
  return state.pause && state.pause.command;
}

function isStepping(state) {
  return ["stepIn", "stepOver", "stepOut"].includes(getPauseCommand(state));
}

function isPaused(state) {
  return !!getFrames(state);
}

function getIsPaused(state) {
  return !!getFrames(state);
}

function getPreviousPauseFrameLocation(state) {
  return state.pause.previousLocation;
}

function isEvaluatingExpression(state) {
  return state.pause.command === "expression";
}

function getPopupObjectProperties(state, objectId) {
  return getAllPopupObjectProperties(state)[objectId];
}

function getIsWaitingOnBreak(state) {
  return state.pause.isWaitingOnBreak;
}

function getShouldPauseOnExceptions(state) {
  return state.pause.shouldPauseOnExceptions;
}

function getShouldPauseOnCaughtExceptions(state) {
  return state.pause.shouldPauseOnCaughtExceptions;
}

function getCanRewind(state) {
  return state.pause.canRewind;
}

function getExtra(state) {
  return state.pause.extra;
}

function getFrames(state) {
  return state.pause.frames;
}

function getGeneratedFrameScope(state, frameId) {
  if (!frameId) {
    return null;
  }

  return getFrameScopes(state).generated[frameId];
}

function getOriginalFrameScope(state, sourceId, frameId) {
  if (!frameId || !sourceId) {
    return null;
  }

  const isGenerated = (0, _devtoolsSourceMap.isGeneratedId)(sourceId);
  const original = getFrameScopes(state).original[frameId];

  if (!isGenerated && original && (original.pending || original.scope)) {
    return original;
  }

  return null;
}

function getFrameScopes(state) {
  return state.pause.frameScopes;
}

function getSelectedFrameBindings(state) {
  const scopes = getFrameScopes(state);
  const selectedFrameId = getSelectedFrameId(state);

  if (!scopes || !selectedFrameId) {
    return null;
  }

  const frameScope = scopes.generated[selectedFrameId];

  if (!frameScope || frameScope.pending) {
    return;
  }

  let currentScope = frameScope.scope;
  let frameBindings = [];

  while (currentScope && currentScope.type != "object") {
    const bindings = Object.keys(currentScope.bindings.variables);
    const args = [].concat(...currentScope.bindings.arguments.map(argument => Object.keys(argument)));
    frameBindings = [...frameBindings, ...bindings, ...args];
    currentScope = currentScope.parent;
  }

  return frameBindings;
}

function getFrameScope(state, sourceId, frameId) {
  return getOriginalFrameScope(state, sourceId, frameId) || getGeneratedFrameScope(state, frameId);
}

function getSelectedScope(state) {
  const sourceRecord = (0, _sources.getSelectedSource)(state);
  const frameId = getSelectedFrameId(state);
  const {
    scope
  } = getFrameScope(state, sourceRecord && sourceRecord.id, frameId) || {};
  return scope || null;
}

function getSelectedScopeMappings(state) {
  const frameId = getSelectedFrameId(state);

  if (!frameId) {
    return null;
  }

  return getFrameScopes(state).mappings[frameId];
}

function getSelectedFrameId(state) {
  return state.pause.selectedFrameId;
}

function getSelectedComponentIndex(state) {
  return state.pause.selectedComponentIndex;
}

function getTopFrame(state) {
  const frames = getFrames(state);
  return frames && frames[0];
}

const getSelectedFrame = exports.getSelectedFrame = (0, _reselect.createSelector)(getSelectedFrameId, getFrames, (selectedFrameId, frames) => {
  if (!frames) {
    return null;
  }

  return frames.find(frame => frame.id == selectedFrameId);
});

function getDebuggeeUrl(state) {
  return state.pause.debuggeeUrl;
}

function getSkipPausing(state) {
  return state.pause.skipPausing;
} // NOTE: currently only used for chrome


function getChromeScopes(state) {
  const frame = getSelectedFrame(state);
  return frame ? frame.scopeChain : undefined;
}

exports.default = update;