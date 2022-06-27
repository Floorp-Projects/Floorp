/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { getThreadPauseState } from "../reducers/pause";
import { getSelectedSourceId, getSelectedLocation } from "./sources";

import { isGeneratedId } from "devtools-source-map";

// eslint-disable-next-line
import { getSelectedLocation as _getSelectedLocation } from "../utils/selected-location";
import { createSelector } from "reselect";

export const getSelectedFrame = createSelector(
  (state, thread) => state.pause.threads[thread],
  threadPauseState => {
    if (!threadPauseState) return null;
    const { selectedFrameId, frames } = threadPauseState;
    if (frames) {
      return frames.find(frame => frame.id == selectedFrameId);
    }
    return null;
  }
);

export const getVisibleSelectedFrame = createSelector(
  getSelectedLocation,
  state => getSelectedFrame(state, getCurrentThread(state)),
  (selectedLocation, selectedFrame) => {
    if (!selectedFrame) {
      return null;
    }

    const { id, displayName } = selectedFrame;

    return {
      id,
      displayName,
      location: _getSelectedLocation(selectedFrame, selectedLocation),
    };
  }
);

export function getContext(state) {
  return state.pause.cx;
}

export function getThreadContext(state) {
  return state.pause.threadcx;
}

export function getPauseReason(state, thread) {
  return getThreadPauseState(state.pause, thread).why;
}

export function getPauseCommand(state, thread) {
  return getThreadPauseState(state.pause, thread).command;
}

export function isStepping(state, thread) {
  return ["stepIn", "stepOver", "stepOut"].includes(
    getPauseCommand(state, thread)
  );
}

export function getCurrentThread(state) {
  return getThreadContext(state).thread;
}

export function getIsPaused(state, thread) {
  return getThreadPauseState(state.pause, thread).isPaused;
}

export function getIsCurrentThreadPaused(state) {
  return getIsPaused(state, getCurrentThread(state));
}

export function isEvaluatingExpression(state, thread) {
  return getThreadPauseState(state.pause, thread).command === "expression";
}

export function getIsWaitingOnBreak(state, thread) {
  return getThreadPauseState(state.pause, thread).isWaitingOnBreak;
}

export function getShouldPauseOnExceptions(state) {
  return state.pause.shouldPauseOnExceptions;
}

export function getShouldPauseOnCaughtExceptions(state) {
  return state.pause.shouldPauseOnCaughtExceptions;
}

export function getFrames(state, thread) {
  const { frames, framesLoading } = getThreadPauseState(state.pause, thread);
  return framesLoading ? null : frames;
}

export function getCurrentThreadFrames(state) {
  const { frames, framesLoading } = getThreadPauseState(
    state.pause,
    getCurrentThread(state)
  );
  return framesLoading ? null : frames;
}

function getGeneratedFrameId(frameId) {
  if (frameId.includes("-originalFrame")) {
    // The mapFrames can add original stack frames -- get generated frameId.
    return frameId.substr(0, frameId.lastIndexOf("-originalFrame"));
  }
  return frameId;
}

export function getGeneratedFrameScope(state, thread, frameId) {
  if (!frameId) {
    return null;
  }

  return getFrameScopes(state, thread).generated[getGeneratedFrameId(frameId)];
}

export function getOriginalFrameScope(state, thread, sourceId, frameId) {
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

// This is only used by tests
export function getFrameScopes(state, thread) {
  return getThreadPauseState(state.pause, thread).frameScopes;
}

export function getSelectedFrameBindings(state, thread) {
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

function getFrameScope(state, thread, sourceId, frameId) {
  return (
    getOriginalFrameScope(state, thread, sourceId, frameId) ||
    getGeneratedFrameScope(state, thread, frameId)
  );
}

// This is only used by tests
export function getSelectedScope(state, thread) {
  const sourceId = getSelectedSourceId(state);
  const frameId = getSelectedFrameId(state, thread);

  const frameScope = getFrameScope(state, thread, sourceId, frameId);
  if (!frameScope) {
    return null;
  }

  return frameScope.scope || null;
}

export function getSelectedOriginalScope(state, thread) {
  const sourceId = getSelectedSourceId(state);
  const frameId = getSelectedFrameId(state, thread);
  return getOriginalFrameScope(state, thread, sourceId, frameId);
}

export function getSelectedGeneratedScope(state, thread) {
  const frameId = getSelectedFrameId(state, thread);
  return getGeneratedFrameScope(state, thread, frameId);
}

export function getSelectedScopeMappings(state, thread) {
  const frameId = getSelectedFrameId(state, thread);
  if (!frameId) {
    return null;
  }

  return getFrameScopes(state, thread).mappings[frameId];
}

export function getSelectedFrameId(state, thread) {
  return getThreadPauseState(state.pause, thread).selectedFrameId;
}

export function isTopFrameSelected(state, thread) {
  const selectedFrameId = getSelectedFrameId(state, thread);
  const topFrame = getTopFrame(state, thread);
  return selectedFrameId == topFrame?.id;
}

export function getTopFrame(state, thread) {
  const frames = getFrames(state, thread);
  return frames?.[0];
}

export function getSkipPausing(state) {
  return state.pause.skipPausing;
}

export function getHighlightedCalls(state, thread) {
  return getThreadPauseState(state.pause, thread).highlightedCalls;
}

export function isMapScopesEnabled(state) {
  return state.pause.mapScopes;
}

export function getInlinePreviews(state, thread, frameId) {
  return getThreadPauseState(state.pause, thread).inlinePreview[
    getGeneratedFrameId(frameId)
  ];
}

// This is only used by tests
export function getSelectedInlinePreviews(state) {
  const thread = getCurrentThread(state);
  const frameId = getSelectedFrameId(state, thread);
  if (!frameId) {
    return null;
  }

  return getInlinePreviews(state, thread, frameId);
}

export function getLastExpandedScopes(state, thread) {
  return getThreadPauseState(state.pause, thread).lastExpandedScopes;
}

export function getPausePreviewLocation(state) {
  return state.pause.previewLocation;
}
