/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
/* eslint complexity: ["error", 30]*/

/**
 * Pause reducer
 * @module reducers/pause
 */

import { isGeneratedId } from "devtools-source-map";
import { prefs } from "../utils/prefs";
import { getSelectedSourceId } from "./sources";
import { getSelectedFrame } from "../selectors/pause";

import type { OriginalScope } from "../utils/pause/mapScopes";
import type { Action } from "../actions/types";
import type { State } from "./types";
import type {
  Why,
  Scope,
  SourceId,
  ChromeFrame,
  FrameId,
  MappedLocation,
  ThreadId
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
  currentThread: ThreadId,
  canRewind: boolean,
  threads: { [ThreadId]: ThreadPauseState },
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

function getThreadPauseState(state: PauseState, thread: ThreadId) {
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

export function getAllPopupObjectProperties(
  state: OuterState,
  thread: ThreadId
) {
  return getThreadPauseState(state.pause, thread).loadedObjects;
}

export function getPauseReason(state: OuterState, thread: ThreadId): ?Why {
  return getThreadPauseState(state.pause, thread).why;
}

export function getPauseCommand(state: OuterState, thread: ThreadId): Command {
  return getThreadPauseState(state.pause, thread).command;
}

export function wasStepping(state: OuterState, thread: ThreadId): boolean {
  return getThreadPauseState(state.pause, thread).wasStepping;
}

export function isStepping(state: OuterState, thread: ThreadId) {
  return ["stepIn", "stepOver", "stepOut"].includes(
    getPauseCommand(state, thread)
  );
}

export function getCurrentThread(state: OuterState) {
  return state.pause.currentThread;
}

export function getIsPaused(state: OuterState, thread: ThreadId) {
  return !!getThreadPauseState(state.pause, thread).frames;
}

export function getPreviousPauseFrameLocation(
  state: OuterState,
  thread: ThreadId
) {
  return getThreadPauseState(state.pause, thread).previousLocation;
}

export function isEvaluatingExpression(state: OuterState, thread: ThreadId) {
  return getThreadPauseState(state.pause, thread).command === "expression";
}

export function getPopupObjectProperties(
  state: OuterState,
  thread: ThreadId,
  objectId: string
) {
  return getAllPopupObjectProperties(state, thread)[objectId];
}

export function getIsWaitingOnBreak(state: OuterState, thread: ThreadId) {
  return getThreadPauseState(state.pause, thread).isWaitingOnBreak;
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

export function getFrames(state: OuterState, thread: ThreadId) {
  return getThreadPauseState(state.pause, thread).frames;
}

export function getCurrentThreadFrames(state: OuterState) {
  return getThreadPauseState(state.pause, getCurrentThread(state)).frames;
}

function getGeneratedFrameId(frameId: string): string {
  if (frameId.includes("-originalFrame")) {
    // The mapFrames can add original stack frames -- get generated frameId.
    return frameId.substr(0, frameId.lastIndexOf("-originalFrame"));
  }
  return frameId;
}

export function getGeneratedFrameScope(
  state: OuterState,
  thread: ThreadId,
  frameId: ?string
) {
  if (!frameId) {
    return null;
  }

  return getFrameScopes(state, thread).generated[getGeneratedFrameId(frameId)];
}

export function getOriginalFrameScope(
  state: OuterState,
  thread: ThreadId,
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
  const original = getFrameScopes(state, thread).original[
    getGeneratedFrameId(frameId)
  ];

  if (!isGenerated && original && (original.pending || original.scope)) {
    return original;
  }

  return null;
}

export function getFrameScopes(state: OuterState, thread: ThreadId) {
  return getThreadPauseState(state.pause, thread).frameScopes;
}

export function getSelectedFrameBindings(state: OuterState, thread: ThreadId) {
  const scopes = getFrameScopes(state, thread);
  const selectedFrameId = getSelectedFrameId(state, thread);
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
  thread: ThreadId,
  sourceId: ?SourceId,
  frameId: ?string
): ?{
  pending: boolean,
  +scope: OriginalScope | Scope
} {
  return (
    getOriginalFrameScope(state, thread, sourceId, frameId) ||
    getGeneratedFrameScope(state, thread, frameId)
  );
}

export function getSelectedScope(state: OuterState, thread: ThreadId) {
  const sourceId = getSelectedSourceId(state);
  const frameId = getSelectedFrameId(state, thread);

  const frameScope = getFrameScope(state, thread, sourceId, frameId);
  if (!frameScope) {
    return null;
  }

  return frameScope.scope || null;
}

export function getSelectedOriginalScope(state: OuterState, thread: ThreadId) {
  const sourceId = getSelectedSourceId(state);
  const frameId = getSelectedFrameId(state, thread);
  return getOriginalFrameScope(state, thread, sourceId, frameId);
}

export function getSelectedGeneratedScope(state: OuterState, thread: ThreadId) {
  const frameId = getSelectedFrameId(state, thread);
  return getGeneratedFrameScope(state, thread, frameId);
}

export function getSelectedScopeMappings(
  state: OuterState,
  thread: ThreadId
): {
  [string]: string | null
} | null {
  const frameId = getSelectedFrameId(state, thread);
  if (!frameId) {
    return null;
  }

  return getFrameScopes(state, thread).mappings[frameId];
}

export function getSelectedFrameId(state: OuterState, thread: ThreadId) {
  return getThreadPauseState(state.pause, thread).selectedFrameId;
}

export function getTopFrame(state: OuterState, thread: ThreadId) {
  const frames = getFrames(state, thread);
  return frames && frames[0];
}

export function getSkipPausing(state: OuterState) {
  return state.pause.skipPausing;
}

export function getMapScopes(state: OuterState) {
  return state.pause.mapScopes;
}

// NOTE: currently only used for chrome
export function getChromeScopes(state: OuterState, thread: ThreadId) {
  const frame: ?ChromeFrame = (getSelectedFrame(state, thread): any);
  return frame ? frame.scopeChain : undefined;
}

export default update;
