/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
/* eslint complexity: ["error", 30]*/

/**
 * Pause reducer
 * @module reducers/pause
 */

import { createSelector } from "reselect";
import { isGeneratedId } from "devtools-source-map";
import { prefs } from "../utils/prefs";
import { getSelectedSource } from "./sources";

import type { OriginalScope } from "../utils/pause/mapScopes";
import type { Action } from "../actions/types";
import type { State } from "./types";
import type { Why, Scope, SourceId, FrameId, MappedLocation } from "../types";

export type Command =
  | null
  | "stepOver"
  | "stepIn"
  | "stepOut"
  | "resume"
  | "rewind"
  | "reverseStepOver"
  | "reverseStepIn"
  | "reverseStepOut"
  | "expression";

export type PauseState = {
  extra: ?Object,
  why: ?Why,
  isWaitingOnBreak: boolean,
  frames: ?(any[]),
  frameScopes: {
    generated: {
      [FrameId]: {
        pending: boolean,
        scope: Scope
      }
    },
    original: {
      [FrameId]: {
        pending: boolean,
        scope: OriginalScope
      }
    },
    mappings: {
      [FrameId]: {
        [string]: string | null
      }
    }
  },
  selectedFrameId: ?string,
  loadedObjects: Object,
  shouldPauseOnExceptions: boolean,
  shouldPauseOnCaughtExceptions: boolean,
  canRewind: boolean,
  debuggeeUrl: string,
  command: Command,
  previousLocation: ?MappedLocation,
  skipPausing: boolean
};

export const createPauseState = (): PauseState => ({
  ...emptyPauseState,
  extra: {},
  isWaitingOnBreak: false,
  shouldPauseOnExceptions: prefs.pauseOnExceptions,
  shouldPauseOnCaughtExceptions: prefs.pauseOnCaughtExceptions,
  canRewind: false,
  debuggeeUrl: "",
  command: null,
  previousLocation: null,
  skipPausing: prefs.skipPausing
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

function update(
  state: PauseState = createPauseState(),
  action: Action
): PauseState {
  switch (action.type) {
    case "PAUSED": {
      const { selectedFrameId, frames, loadedObjects, why } = action;

      // turn this into an object keyed by object id
      const objectMap = {};
      loadedObjects.forEach(obj => {
        objectMap[obj.value.objectId] = obj;
      });

      return {
        ...state,
        isWaitingOnBreak: false,
        selectedFrameId,
        frames,
        frameScopes: { ...emptyPauseState.frameScopes },
        loadedObjects: objectMap,
        why
      };
    }

    case "MAP_FRAMES": {
      const { selectedFrameId, frames } = action;
      return { ...state, frames, selectedFrameId };
    }

    case "ADD_EXTRA": {
      return { ...state, extra: action.extra };
    }

    case "ADD_SCOPES": {
      const { frame, status, value } = action;
      const selectedFrameId = frame.id;

      const generated = {
        ...state.frameScopes.generated,
        [selectedFrameId]: {
          pending: status !== "done",
          scope: value
        }
      };
      return {
        ...state,
        frameScopes: {
          ...state.frameScopes,
          generated
        }
      };
    }

    case "TRAVEL_TO":
      return {
        ...state,
        ...action.data.paused
      };

    case "MAP_SCOPES": {
      const { frame, status, value } = action;
      const selectedFrameId = frame.id;

      const original = {
        ...state.frameScopes.original,
        [selectedFrameId]: {
          pending: status !== "done",
          scope: value && value.scope
        }
      };

      const mappings = {
        ...state.frameScopes.mappings,
        [selectedFrameId]: value && value.mappings
      };
      return {
        ...state,
        frameScopes: {
          ...state.frameScopes,
          original,
          mappings
        }
      };
    }

    case "BREAK_ON_NEXT":
      return { ...state, isWaitingOnBreak: true };

    case "SELECT_FRAME":
      return {
        ...state,
        selectedFrameId: action.frame.id
      };

    case "SET_POPUP_OBJECT_PROPERTIES":
      if (!action.properties) {
        return { ...state };
      }

      return {
        ...state,
        loadedObjects: {
          ...state.loadedObjects,
          [action.objectId]: action.properties
        }
      };

    case "CONNECT":
      return {
        ...createPauseState(),
        debuggeeUrl: action.url,
        canRewind: action.canRewind
      };

    case "PAUSE_ON_EXCEPTIONS":
      const { shouldPauseOnExceptions, shouldPauseOnCaughtExceptions } = action;

      prefs.pauseOnExceptions = shouldPauseOnExceptions;
      prefs.pauseOnCaughtExceptions = shouldPauseOnCaughtExceptions;

      // Preserving for the old debugger
      prefs.ignoreCaughtExceptions = !shouldPauseOnCaughtExceptions;

      return {
        ...state,
        shouldPauseOnExceptions,
        shouldPauseOnCaughtExceptions
      };

    case "COMMAND": {
      return action.status === "start"
        ? {
            ...state,
            ...emptyPauseState,
            command: action.command,
            previousLocation: getPauseLocation(state, action)
          }
        : { ...state, command: null };
    }

    case "RESUME":
      return { ...state, ...emptyPauseState };

    case "EVALUATE_EXPRESSION":
      return {
        ...state,
        command: action.status === "start" ? "expression" : null
      };

    case "NAVIGATE":
      return { ...state, ...emptyPauseState, debuggeeUrl: action.url };

    case "TOGGLE_SKIP_PAUSING": {
      const { skipPausing } = action;
      prefs.skipPausing = skipPausing;

      return { ...state, skipPausing };
    }
  }

  return state;
}

function getPauseLocation(state, action) {
  const { frames, previousLocation } = state;

  // NOTE: We store the previous location so that we ensure that we
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
}

// Selectors

// Unfortunately, it's really hard to make these functions accept just
// the state that we care about and still type it with Flow. The
// problem is that we want to re-export all selectors from a single
// module for the UI, and all of those selectors should take the
// top-level app state, so we'd have to "wrap" them to automatically
// pick off the piece of state we're interested in. It's impossible
// (right now) to type those wrapped functions.
type OuterState = State;

const getPauseState = state => state.pause;

export const getAllPopupObjectProperties = createSelector(
  getPauseState,
  pauseWrapper => pauseWrapper.loadedObjects
);

export function getPauseReason(state: OuterState): ?Why {
  return state.pause.why;
}

export function getPauseCommand(state: OuterState): Command {
  return state.pause && state.pause.command;
}

export function isStepping(state: OuterState) {
  return ["stepIn", "stepOver", "stepOut"].includes(getPauseCommand(state));
}

export function isPaused(state: OuterState) {
  return !!getFrames(state);
}

export function getIsPaused(state: OuterState) {
  return !!getFrames(state);
}

export function getPreviousPauseFrameLocation(state: OuterState) {
  return state.pause.previousLocation;
}

export function isEvaluatingExpression(state: OuterState) {
  return state.pause.command === "expression";
}

export function getPopupObjectProperties(state: OuterState, objectId: string) {
  return getAllPopupObjectProperties(state)[objectId];
}

export function getIsWaitingOnBreak(state: OuterState) {
  return state.pause.isWaitingOnBreak;
}

export function getShouldPauseOnExceptions(state: OuterState) {
  return state.pause.shouldPauseOnExceptions;
}

export function getShouldPauseOnCaughtExceptions(state: OuterState) {
  return state.pause.shouldPauseOnCaughtExceptions;
}

export function getCanRewind(state: OuterState) {
  return state.pause.canRewind;
}

export function getExtra(state: OuterState) {
  return state.pause.extra;
}

export function getFrames(state: OuterState) {
  return state.pause.frames;
}

function getGeneratedFrameId(frameId: string): string {
  if (frameId.includes("-originalFrame")) {
    // The mapFrames can add original stack frames -- get generated frameId.
    return frameId.substr(0, frameId.lastIndexOf("-originalFrame"));
  }
  return frameId;
}

export function getGeneratedFrameScope(state: OuterState, frameId: ?string) {
  if (!frameId) {
    return null;
  }

  return getFrameScopes(state).generated[getGeneratedFrameId(frameId)];
}

export function getOriginalFrameScope(
  state: OuterState,
  sourceId: ?SourceId,
  frameId: ?string
): ?{
  pending: boolean,
  +scope: OriginalScope | Scope
} {
  if (!frameId || !sourceId) {
    return null;
  }

  const isGenerated = isGeneratedId(sourceId);
  const original = getFrameScopes(state).original[getGeneratedFrameId(frameId)];

  if (!isGenerated && original && (original.pending || original.scope)) {
    return original;
  }

  return null;
}

export function getFrameScopes(state: OuterState) {
  return state.pause.frameScopes;
}

export function getSelectedFrameBindings(state: OuterState) {
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
    if (currentScope.bindings) {
      const bindings = Object.keys(currentScope.bindings.variables);
      const args = [].concat(
        ...currentScope.bindings.arguments.map(argument =>
          Object.keys(argument)
        )
      );

      frameBindings = [...frameBindings, ...bindings, ...args];
    }
    currentScope = currentScope.parent;
  }

  return frameBindings;
}

export function getFrameScope(
  state: OuterState,
  sourceId: ?SourceId,
  frameId: ?string
): ?{
  pending: boolean,
  +scope: OriginalScope | Scope
} {
  return (
    getOriginalFrameScope(state, sourceId, frameId) ||
    getGeneratedFrameScope(state, frameId)
  );
}

export function getSelectedScope(state: OuterState) {
  const source = getSelectedSource(state);
  const frameId = getSelectedFrameId(state);

  if (!source) {
    return null;
  }

  const frameScope = getFrameScope(state, source.id, frameId);
  if (!frameScope) {
    return null;
  }

  return frameScope.scope || null;
}

export function getSelectedScopeMappings(
  state: OuterState
): {
  [string]: string | null
} | null {
  const frameId = getSelectedFrameId(state);
  if (!frameId) {
    return null;
  }

  return getFrameScopes(state).mappings[frameId];
}

export function getSelectedFrameId(state: OuterState) {
  return state.pause.selectedFrameId;
}

export function getTopFrame(state: OuterState) {
  const frames = getFrames(state);
  return frames && frames[0];
}

export const getSelectedFrame = createSelector(
  getSelectedFrameId,
  getFrames,
  (selectedFrameId, frames) => {
    if (!frames) {
      return null;
    }

    return frames.find(frame => frame.id == selectedFrameId);
  }
);

export function getDebuggeeUrl(state: OuterState) {
  return state.pause.debuggeeUrl;
}

export function getSkipPausing(state: OuterState) {
  return state.pause.skipPausing;
}

// NOTE: currently only used for chrome
export function getChromeScopes(state: OuterState) {
  const frame = getSelectedFrame(state);
  return frame ? frame.scopeChain : undefined;
}

export default update;
