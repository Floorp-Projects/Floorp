/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  getSelectedFrame,
  getThreadContext,
  getCurrentThread,
  getIsCurrentThreadPaused,
} from "../../selectors";
import { PROMISE } from "../utils/middleware/promise";
import { evaluateExpressions } from "../expressions";
import { selectLocation } from "../sources";
import { fetchScopes } from "./fetchScopes";
import { fetchFrames } from "./fetchFrames";
import { recordEvent } from "../../utils/telemetry";
import { features } from "../../utils/prefs";
import assert from "../../utils/assert";

export function selectThread(cx, thread) {
  return async ({ dispatch, getState, client }) => {
    if (getCurrentThread(getState()) === thread) {
      return;
    }

    dispatch({ cx, type: "SELECT_THREAD", thread });

    // Get a new context now that the current thread has changed.
    const threadcx = getThreadContext(getState());
    // Note that this is a rethorical assertion as threadcx.thread is updated by SELECT_THREAD action
    assert(threadcx.thread == thread, "Thread mismatch");

    const serverRequests = [];
    // Update the watched expressions as we may never have evaluated them against this thread
    serverRequests.push(dispatch(evaluateExpressions(threadcx)));

    // If we were paused on the newly selected thread, ensure:
    // - select the source where we are paused,
    // - fetching the paused stackframes,
    // - fetching the paused scope, so that variable preview are working on the selected source.
    // (frames and scopes is supposed to be fetched on pause,
    // but if two threads pause concurrently, it might be cancelled)
    const frame = getSelectedFrame(getState(), thread);
    if (frame) {
      serverRequests.push(dispatch(selectLocation(threadcx, frame.location)));
      serverRequests.push(dispatch(fetchFrames(threadcx)));
      serverRequests.push(dispatch(fetchScopes(threadcx)));
    }

    await Promise.all(serverRequests);
  };
}

/**
 * Debugger commands like stepOver, stepIn, stepUp
 *
 * @param string $0.type
 * @memberof actions/pause
 * @static
 */
export function command(type) {
  return async ({ dispatch, getState, client }) => {
    if (!type) {
      return null;
    }
    // For now, all commands are by default against the currently selected thread
    const thread = getCurrentThread(getState());

    const frame = features.frameStep && getSelectedFrame(getState(), thread);

    return dispatch({
      type: "COMMAND",
      command: type,
      thread,
      [PROMISE]: client[type](thread, frame?.id),
    });
  };
}

/**
 * StepIn
 * @memberof actions/pause
 * @static
 * @returns {Function} {@link command}
 */
export function stepIn() {
  return ({ dispatch, getState }) => {
    if (!getIsCurrentThreadPaused(getState())) {
      return null;
    }
    return dispatch(command("stepIn"));
  };
}

/**
 * stepOver
 * @memberof actions/pause
 * @static
 * @returns {Function} {@link command}
 */
export function stepOver() {
  return ({ dispatch, getState }) => {
    if (!getIsCurrentThreadPaused(getState())) {
      return null;
    }
    return dispatch(command("stepOver"));
  };
}

/**
 * stepOut
 * @memberof actions/pause
 * @static
 * @returns {Function} {@link command}
 */
export function stepOut() {
  return ({ dispatch, getState }) => {
    if (!getIsCurrentThreadPaused(getState())) {
      return null;
    }
    return dispatch(command("stepOut"));
  };
}

/**
 * resume
 * @memberof actions/pause
 * @static
 * @returns {Function} {@link command}
 */
export function resume() {
  return ({ dispatch, getState }) => {
    if (!getIsCurrentThreadPaused(getState())) {
      return null;
    }
    recordEvent("continue");
    return dispatch(command("resume"));
  };
}

/**
 * restart frame
 * @memberof actions/pause
 * @static
 */
export function restart(cx, frame) {
  return async ({ dispatch, getState, client }) => {
    if (!getIsCurrentThreadPaused(getState())) {
      return null;
    }
    return dispatch({
      type: "COMMAND",
      command: "restart",
      thread: cx.thread,
      [PROMISE]: client.restart(cx.thread, frame.id),
    });
  };
}
