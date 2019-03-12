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
import { getSelectedSourceId } from "./sources";

import type { OriginalScope } from "../utils/pause/mapScopes";
import type { Action } from "../actions/types";
import type { Selector, State } from "./types";
import type {
  Why,
  Scope,
  SourceId,
  ChromeFrame,
  Frame,
  FrameId,
  MappedLocation
} from "../types";

export type Command =
  | null
  | "stepOver"
  | "stepIn"
  | "stepOut"
  | "resume"
  | "rewind"
  | "reverseStepOver"
  | "reverseStepIn"
  | "reverseStepOut";

// Pause state associated with an individual thread.
type ThreadPauseState = {
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
  command: Command,
  lastCommand: Command,
  wasStepping: boolean,
  previousLocation: ?MappedLocation
};

// Pause state describing all threads.
export type PauseState = {
  currentThread: string,
  canRewind: boolean,
  threads: { [string]: ThreadPauseState },
  skipPausing: boolean,
  mapScopes: boolean,
  shouldPauseOnExceptions: boolean,
  shouldPauseOnCaughtExceptions: boolean
};

export const createPauseState = (): PauseState => ({
  currentThread: "UnknownThread",
  threads: {},
  canRewind: false,
  skipPausing: prefs.skipPausing,
  mapScopes: prefs.mapScopes,
  shouldPauseOnExceptions: prefs.pauseOnExceptions,
  shouldPauseOnCaughtExceptions: prefs.pauseOnCaughtExceptions
});

const resumedPauseState = {
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

const createInitialPauseState = () => ({
  ...resumedPauseState,
  isWaitingOnBreak: false,
  canRewind: false,
  command: null,
  lastCommand: null,
  previousLocation: null
});

function getThreadPauseState(state: PauseState, thread: string) {
  // Thread state is lazily initialized so that we don't have to keep track of
  // the current set of worker threads.
  return state.threads[thread] || createInitialPauseState();
}

function update(
  state: PauseState = createPauseState(),
  action: Action
): PauseState {
  // Actions need to specify any thread they are operating on. These helpers
  // manage updating the pause state for that thread.
  const threadState = () => {
    if (!action.thread) {
      throw new Error(`Missing thread in action ${action.type}`);
    }
    return getThreadPauseState(state, action.thread);
  };

  const updateThreadState = newThreadState => {
    if (!action.thread) {
      throw new Error(`Missing thread in action ${action.type}`);
    }
    return {
      ...state,
      threads: {
        ...state.threads,
        [action.thread]: { ...threadState(), ...newThreadState }
      }
    };
  };

  switch (action.type) {
    case "SELECT_THREAD":
      return { ...state, currentThread: action.thread };

    case "PAUSED": {
      const { thread, selectedFrameId, frames, loadedObjects, why } = action;

      // turn this into an object keyed by object id
      const objectMap = {};
      loadedObjects.forEach(obj => {
        objectMap[obj.value.objectId] = obj;
      });

      state = { ...state, currentThread: thread };
      return updateThreadState({
        isWaitingOnBreak: false,
        selectedFrameId,
        frames,
        frameScopes: { ...resumedPauseState.frameScopes },
        loadedObjects: objectMap,
        why
      });
    }

    case "MAP_FRAMES": {
      const { selectedFrameId, frames } = action;
      return updateThreadState({ frames, selectedFrameId });
    }

    case "ADD_SCOPES": {
      const { frame, status, value } = action;
      const selectedFrameId = frame.id;

      const generated = {
        ...threadState().frameScopes.generated,
        [selectedFrameId]: {
          pending: status !== "done",
          scope: value
        }
      };

      return updateThreadState({
        frameScopes: {
          ...threadState().frameScopes,
          generated
        }
      });
    }

    case "TRAVEL_TO":
      return updateThreadState({ ...action.data.paused });

    case "MAP_SCOPES": {
      const { frame, status, value } = action;
      const selectedFrameId = frame.id;

      const original = {
        ...threadState().frameScopes.original,
        [selectedFrameId]: {
          pending: status !== "done",
          scope: value && value.scope
        }
      };

      const mappings = {
        ...threadState().frameScopes.mappings,
        [selectedFrameId]: value && value.mappings
      };

      return updateThreadState({
        frameScopes: {
          ...threadState().frameScopes,
          original,
          mappings
        }
      });
    }

    case "BREAK_ON_NEXT":
      return updateThreadState({ isWaitingOnBreak: true });

    case "SELECT_FRAME":
      return updateThreadState({ selectedFrameId: action.frame.id });

    case "SET_POPUP_OBJECT_PROPERTIES": {
      if (!action.properties) {
        return state;
      }

      return updateThreadState({
        loadedObjects: {
          ...threadState().loadedObjects,
          [action.objectId]: action.properties
        }
      });
    }

    case "CONNECT":
      return {
        ...createPauseState(),
        currentThread: action.mainThread.actor,
        canRewind: action.canRewind
      };

    case "PAUSE_ON_EXCEPTIONS": {
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
    }

    case "COMMAND":
      if (action.status === "start") {
        return updateThreadState({
          ...resumedPauseState,
          command: action.command,
          lastCommand: action.command,
          previousLocation: getPauseLocation(threadState(), action)
        });
      }
      return updateThreadState({ command: null });

    case "RESUME":
      // Workaround for threads resuming before the initial connection.
      if (!action.thread && !state.currentThread) {
        return state;
      }
      return updateThreadState({
        ...resumedPauseState,
        wasStepping: !!action.wasStepping
      });

    case "EVALUATE_EXPRESSION":
      return updateThreadState({
        command: action.status === "start" ? "expression" : null
      });

    case "NAVIGATE":
      return {
        ...state,
        currentThread: action.mainThread.actor,
        threads: {
          [action.mainThread.actor]: {
            ...state.threads[action.mainThread.actor],
            ...resumedPauseState
          }
        }
      };

    case "TOGGLE_SKIP_PAUSING": {
      const { skipPausing } = action;
      prefs.skipPausing = skipPausing;

      return { ...state, skipPausing };
    }

    case "TOGGLE_MAP_SCOPES": {
      const { mapScopes } = action;
      prefs.mapScopes = mapScopes;
      return { ...state, mapScopes };
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

function getCurrentPauseState(state: OuterState): ThreadPauseState {
  return getThreadPauseState(state.pause, state.pause.currentThread);
}

export const getAllPopupObjectProperties: Selector<{}> = createSelector(
  getCurrentPauseState,
  pauseWrapper => pauseWrapper.loadedObjects
);

export function getPauseReason(state: OuterState): ?Why {
  return getCurrentPauseState(state).why;
}

export function getPauseCommand(state: OuterState): Command {
  return getCurrentPauseState(state).command;
}

export function wasStepping(state: OuterState): boolean {
  return getCurrentPauseState(state).wasStepping;
}

export function isStepping(state: OuterState) {
  return ["stepIn", "stepOver", "stepOut"].includes(getPauseCommand(state));
}

export function getCurrentThread(state: OuterState) {
  return state.pause.currentThread;
}

export function getThreadIsPaused(state: OuterState, thread: string) {
  return !!getThreadPauseState(state.pause, thread).frames;
}

export function isPaused(state: OuterState) {
  return !!getFrames(state);
}

export function getIsPaused(state: OuterState) {
  return !!getFrames(state);
}

export function getPreviousPauseFrameLocation(state: OuterState) {
  return getCurrentPauseState(state).previousLocation;
}

export function isEvaluatingExpression(state: OuterState) {
  return getCurrentPauseState(state).command === "expression";
}

export function getPopupObjectProperties(state: OuterState, objectId: string) {
  return getAllPopupObjectProperties(state)[objectId];
}

export function getIsWaitingOnBreak(state: OuterState) {
  return getCurrentPauseState(state).isWaitingOnBreak;
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

export function getFrames(state: OuterState) {
  return getCurrentPauseState(state).frames;
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
  return getCurrentPauseState(state).frameScopes;
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
  const sourceId = getSelectedSourceId(state);
  const frameId = getSelectedFrameId(state);

  const frameScope = getFrameScope(state, sourceId, frameId);
  if (!frameScope) {
    return null;
  }

  return frameScope.scope || null;
}

export function getSelectedOriginalScope(state: OuterState) {
  const sourceId = getSelectedSourceId(state);
  const frameId = getSelectedFrameId(state);
  return getOriginalFrameScope(state, sourceId, frameId);
}

export function getSelectedGeneratedScope(state: OuterState) {
  const frameId = getSelectedFrameId(state);
  return getGeneratedFrameScope(state, frameId);
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
  return getCurrentPauseState(state).selectedFrameId;
}

export function getTopFrame(state: OuterState) {
  const frames = getFrames(state);
  return frames && frames[0];
}

export const getSelectedFrame: Selector<?Frame> = createSelector(
  getSelectedFrameId,
  getFrames,
  (selectedFrameId, frames) => {
    if (!frames) {
      return null;
    }

    return frames.find(frame => frame.id == selectedFrameId);
  }
);

export function getSkipPausing(state: OuterState) {
  return state.pause.skipPausing;
}

export function getMapScopes(state: OuterState) {
  return state.pause.mapScopes;
}

// NOTE: currently only used for chrome
export function getChromeScopes(state: OuterState) {
  const frame: ?ChromeFrame = (getSelectedFrame(state): any);
  return frame ? frame.scopeChain : undefined;
}

export default update;
