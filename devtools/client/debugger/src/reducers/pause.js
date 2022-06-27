/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* eslint complexity: ["error", 35]*/

/**
 * Pause reducer
 * @module reducers/pause
 */

import { prefs } from "../utils/prefs";

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
    highlightedCalls: null,
    threads: {},
    skipPausing: prefs.skipPausing,
    mapScopes: prefs.mapScopes,
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
  highlightedCalls: null,
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
        [action.thread]: { ...threadState(), ...newThreadState },
      },
    };
  };

  switch (action.type) {
    case "SELECT_THREAD": {
      return {
        ...state,
        threadcx: {
          ...state.threadcx,
          thread: action.thread,
          pauseCounter: state.threadcx.pauseCounter + 1,
        },
      };
    }

    case "PAUSED": {
      const { thread, frame, why } = action;
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
        selectedFrameId: frame ? frame.id : undefined,
        isPaused: true,
        frames: frame ? [frame] : undefined,
        framesLoading: true,
        frameScopes: { ...resumedPauseState.frameScopes },
        why,
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

    case "ADD_SCOPES": {
      const { frame, status, value } = action;
      const selectedFrameId = frame.id;

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
      const { frame, status, value } = action;
      const selectedFrameId = frame.id;

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

    case "CONNECT":
      return {
        ...initialPauseState(action.mainThreadActorID),
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
      const { frame, previews } = action;
      const selectedFrameId = frame.id;

      return updateThreadState({
        inlinePreview: {
          ...threadState().inlinePreview,
          [selectedFrameId]: previews,
        },
      });
    }

    case "HIGHLIGHT_CALLS": {
      const { highlightedCalls } = action;
      return updateThreadState({ ...threadState(), highlightedCalls });
    }

    case "UNHIGHLIGHT_CALLS": {
      return updateThreadState({
        ...threadState(),
        highlightedCalls: null,
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

export default update;
