/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* eslint complexity: ["error", 36]*/

/**
 * Pause reducer
 * @module reducers/pause
 */

import { prefs } from "../utils/prefs";
import { findClosestFunction } from "../utils/ast";

// Pause state associated with an individual thread.

// Pause state describing all threads.

export function initialPauseState(thread = "UnknownThread") {
  return {
    cx: {
      navigateCounter: 0,
    },
    // This `threadcx` is the `cx` variable we pass around in components and actions.
    // This is pulled via getThreadContext().
    // This stores information about the currently selected thread and its paused state.
    threadcx: {
      navigateCounter: 0,
      thread,
      pauseCounter: 0,
    },
    threads: {},
    skipPausing: prefs.skipPausing,
    mapScopes: prefs.mapScopes,
    shouldPauseOnDebuggerStatement: prefs.pauseOnDebuggerStatement,
    shouldPauseOnExceptions: prefs.pauseOnExceptions,
    shouldPauseOnCaughtExceptions: prefs.pauseOnCaughtExceptions,
  };
}

const resumedPauseState = {
  isPaused: false,
  frames: null,
  framesLoading: false,
  frameScopes: {
    generated: {},
    original: {},
    mappings: {},
  },
  selectedFrameId: null,
  why: null,
  inlinePreview: {},
};

const createInitialPauseState = () => ({
  ...resumedPauseState,
  isWaitingOnBreak: false,
  command: null,
  previousLocation: null,
  expandedScopes: new Set(),
  lastExpandedScopes: [],
});

export function getThreadPauseState(state, thread) {
  // Thread state is lazily initialized so that we don't have to keep track of
  // the current set of worker threads.
  return state.threads[thread] || createInitialPauseState();
}

function update(state = initialPauseState(), action) {
  // All the actions updating pause state must pass an object which designate
  // the related thread.
  const getActionThread = () => {
    const thread =
      action.thread || action.selectedFrame?.thread || action.frame?.thread;
    if (!thread) {
      throw new Error(`Missing thread in action ${action.type}`);
    }
    return thread;
  };

  // `threadState` and `updateThreadState` help easily get and update
  // the pause state for a given thread.
  const threadState = () => {
    return getThreadPauseState(state, getActionThread());
  };
  const updateThreadState = newThreadState => {
    return {
      ...state,
      threads: {
        ...state.threads,
        [getActionThread()]: { ...threadState(), ...newThreadState },
      },
    };
  };

  switch (action.type) {
    case "SELECT_THREAD": {
      // Ignore the action if the related thread doesn't exist.
      if (!state.threads[action.thread]) {
        console.warn(
          `Trying to select a destroyed or non-existent thread '${action.thread}'`
        );
        return state;
      }

      return {
        ...state,
        threadcx: {
          ...state.threadcx,
          thread: action.thread,
          pauseCounter: state.threadcx.pauseCounter + 1,
        },
      };
    }

    case "INSERT_THREAD": {
      // When navigating to a new location,
      // we receive NAVIGATE early, which clear things
      // then we have REMOVE_THREAD of the previous thread.
      // INSERT_THREAD will be the very first event with the new thread actor ID.
      // Automatically select the new top level thread.
      if (action.newThread.isTopLevel) {
        return {
          ...state,
          threadcx: {
            ...state.threadcx,
            thread: action.newThread.actor,
            pauseCounter: state.threadcx.pauseCounter + 1,
          },
          threads: {
            ...state.threads,
            [action.newThread.actor]: createInitialPauseState(),
          },
        };
      }

      return {
        ...state,
        threads: {
          ...state.threads,
          [action.newThread.actor]: createInitialPauseState(),
        },
      };
    }

    case "REMOVE_THREAD": {
      if (
        action.threadActorID in state.threads ||
        action.threadActorID == state.threadcx.thread
      ) {
        // Remove the thread from the cached list
        const threads = { ...state.threads };
        delete threads[action.threadActorID];
        let threadcx = state.threadcx;

        // And also switch to another thread if this was the currently selected one.
        // As we don't store thread objects in this reducer, and only store thread actor IDs,
        // we can't try to find the top level thread. So we pick the first available thread,
        // and hope that's the top level one.
        if (state.threadcx.thread == action.threadActorID) {
          threadcx = {
            ...threadcx,
            thread: Object.keys(threads)[0],
            pauseCounter: threadcx.pauseCounter + 1,
          };
        }
        return {
          ...state,
          threadcx,
          threads,
        };
      }
      break;
    }

    case "PAUSED": {
      const { thread, topFrame, why } = action;
      state = {
        ...state,
        threadcx: {
          ...state.threadcx,
          pauseCounter: state.threadcx.pauseCounter + 1,
          thread,
        },
      };

      return updateThreadState({
        isWaitingOnBreak: false,
        selectedFrameId: topFrame.id,
        isPaused: true,
        // On pause, we only receive the top frame, all subsequent ones
        // will be asynchronously populated via `fetchFrames` action
        frames: [topFrame],
        framesLoading: true,
        frameScopes: { ...resumedPauseState.frameScopes },
        why,
        shouldBreakpointsPaneOpenOnPause: why.type === "breakpoint",
      });
    }

    case "FETCHED_FRAMES": {
      const { frames } = action;

      // We typically receive a PAUSED action before this one,
      // with only the first frame. Here, we avoid replacing it
      // with a copy of it in order to avoid triggerring selectors
      // uncessarily
      // (note that in jest, action's frames might be empty)
      // (and if we resume in between PAUSED and FETCHED_FRAMES
      //  threadState().frames might be null)
      if (threadState().frames) {
        const previousFirstFrame = threadState().frames[0];
        if (previousFirstFrame.id == frames[0]?.id) {
          frames.splice(0, 1, previousFirstFrame);
        }
      }
      return updateThreadState({ frames, framesLoading: false });
    }

    case "MAP_FRAMES": {
      const { selectedFrameId, frames } = action;
      return updateThreadState({ frames, selectedFrameId });
    }

    case "MAP_FRAME_DISPLAY_NAMES": {
      const { frames } = action;
      return updateThreadState({ frames });
    }

    case "SET_SYMBOLS": {
      // Ignore the beginning of this async action
      if (action.status === "start") {
        return state;
      }
      return updateFrameOriginalDisplayName(
        state,
        action.location.source,
        action.value
      );
    }

    case "ADD_SCOPES": {
      const { status, value } = action;
      const selectedFrameId = action.selectedFrame.id;

      const generated = {
        ...threadState().frameScopes.generated,
        [selectedFrameId]: {
          pending: status !== "done",
          scope: value,
        },
      };

      return updateThreadState({
        frameScopes: {
          ...threadState().frameScopes,
          generated,
        },
      });
    }

    case "MAP_SCOPES": {
      const { status, value } = action;
      const selectedFrameId = action.selectedFrame.id;

      const original = {
        ...threadState().frameScopes.original,
        [selectedFrameId]: {
          pending: status !== "done",
          scope: value?.scope,
        },
      };

      const mappings = {
        ...threadState().frameScopes.mappings,
        [selectedFrameId]: value?.mappings,
      };

      return updateThreadState({
        frameScopes: {
          ...threadState().frameScopes,
          original,
          mappings,
        },
      });
    }

    case "BREAK_ON_NEXT":
      return updateThreadState({ isWaitingOnBreak: true });

    case "SELECT_FRAME":
      return updateThreadState({ selectedFrameId: action.frame.id });

    case "PAUSE_ON_DEBUGGER_STATEMENT": {
      const { shouldPauseOnDebuggerStatement } = action;

      prefs.pauseOnDebuggerStatement = shouldPauseOnDebuggerStatement;

      return {
        ...state,
        shouldPauseOnDebuggerStatement,
      };
    }

    case "PAUSE_ON_EXCEPTIONS": {
      const { shouldPauseOnExceptions, shouldPauseOnCaughtExceptions } = action;

      prefs.pauseOnExceptions = shouldPauseOnExceptions;
      prefs.pauseOnCaughtExceptions = shouldPauseOnCaughtExceptions;

      // Preserving for the old debugger
      prefs.ignoreCaughtExceptions = !shouldPauseOnCaughtExceptions;

      return {
        ...state,
        shouldPauseOnExceptions,
        shouldPauseOnCaughtExceptions,
      };
    }

    case "COMMAND":
      if (action.status === "start") {
        return updateThreadState({
          ...resumedPauseState,
          command: action.command,
          previousLocation: getPauseLocation(threadState(), action),
        });
      }
      return updateThreadState({ command: null });

    case "RESUME": {
      if (action.thread == state.threadcx.thread) {
        state = {
          ...state,
          threadcx: {
            ...state.threadcx,
            pauseCounter: state.threadcx.pauseCounter + 1,
          },
        };
      }

      return updateThreadState({
        ...resumedPauseState,
        expandedScopes: new Set(),
        lastExpandedScopes: [...threadState().expandedScopes],
        shouldBreakpointsPaneOpenOnPause: false,
      });
    }

    case "EVALUATE_EXPRESSION":
      return updateThreadState({
        command: action.status === "start" ? "expression" : null,
      });

    case "NAVIGATE": {
      const navigateCounter = state.cx.navigateCounter + 1;
      return {
        ...state,
        cx: {
          navigateCounter,
        },
        threadcx: {
          navigateCounter,
          thread: action.mainThread.actor,
          pauseCounter: 0,
        },
        threads: {
          ...state.threads,
          [action.mainThread.actor]: {
            ...getThreadPauseState(state, action.mainThread.actor),
            ...resumedPauseState,
          },
        },
      };
    }

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

    case "SET_EXPANDED_SCOPE": {
      const { path, expanded } = action;
      const expandedScopes = new Set(threadState().expandedScopes);
      if (expanded) {
        expandedScopes.add(path);
      } else {
        expandedScopes.delete(path);
      }
      return updateThreadState({ expandedScopes });
    }

    case "ADD_INLINE_PREVIEW": {
      const { selectedFrame, previews } = action;
      const selectedFrameId = selectedFrame.id;

      return updateThreadState({
        inlinePreview: {
          ...threadState().inlinePreview,
          [selectedFrameId]: previews,
        },
      });
    }

    case "RESET_BREAKPOINTS_PANE_STATE": {
      return updateThreadState({
        ...threadState(),
        shouldBreakpointsPaneOpenOnPause: false,
      });
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

  const frame = frames?.[0];
  if (!frame) {
    return previousLocation;
  }

  return {
    location: frame.location,
    generatedLocation: frame.generatedLocation,
  };
}

/**
 * For frames related to non-WASM original sources, try to lookup for the function
 * name in the original source. The server will provide the function name in `displayName`
 * in the generated source, but not in the original source.
 * This information will be stored in frame's `originalDisplayName` attribute
 */
function mapDisplayName(frame, symbols) {
  // Ignore WASM original frames
  if (frame.isOriginal) {
    return frame;
  }
  // When it is a regular (non original) JS source, the server already returns a valid displayName attribute.
  if (!frame.location.source.isOriginal) {
    return frame;
  }
  // Ignore the frame if we already computed this attribute from `mapFrames` action
  if (frame.originalDisplayName) {
    return frame;
  }

  if (!symbols.functions) {
    return frame;
  }

  const originalDisplayName = getOriginalDisplayNameForOriginalLocation(
    symbols,
    frame.location
  );
  if (!originalDisplayName) {
    return frame;
  }

  // Fork the object to force a re-render
  return { ...frame, originalDisplayName };
}

function updateFrameOriginalDisplayName(state, source, symbols) {
  // Only map display names for original sources
  if (!source.isOriginal) {
    return state;
  }

  // Update all thread's frame's `originalDisplayName` attribute
  // if some frames relate to the symbols source.
  let newState = null;
  for (const threadActorId in state.threads) {
    const thread = state.threads[threadActorId];
    if (!thread.frames) {
      continue;
    }
    // Only try updating frames if at least one frame object relates to the source
    // for which we just fetched the symbols
    const shouldUpdateThreadFrames = thread.frames.some(
      frame => frame.location.source == source
    );
    if (!shouldUpdateThreadFrames) {
      continue;
    }
    // Now only process frames matching the symbols source
    const frames = thread.frames.map(frame =>
      frame.location.source == source ? mapDisplayName(frame, symbols) : frame
    );
    if (!newState) {
      newState = { ...state };
    }
    newState.threads[threadActorId] = {
      ...thread,
      frames,
    };
  }
  return newState ? newState : state;
}

/**
 * For a given location, return the closest function name.
 *
 * @param {Object} symbols
 *        Symbols object for the passed location.
 * @param {Object} location
 *        A location
 */
export function getOriginalDisplayNameForOriginalLocation(symbols, location) {
  if (!symbols.functions) {
    return null;
  }

  const originalFunction = findClosestFunction(symbols, location);

  return originalFunction ? originalFunction.name : null;
}

export default update;
