/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  getSelectedFrame,
  getCurrentThread,
  getIsCurrentThreadPaused,
  getIsPaused,
} from "../../selectors";
import { PROMISE } from "../utils/middleware/promise";
import { evaluateExpressions } from "../expressions";
import { selectLocation } from "../sources";
import { fetchScopes } from "./fetchScopes";
import { fetchFrames } from "./fetchFrames";
import { recordEvent } from "../../utils/telemetry";
import { validateFrame } from "../../utils/context";

export function selectThread(thread) {
  return async ({ dispatch, getState, client }) => {
    if (getCurrentThread(getState()) === thread) {
      return;
    }
    dispatch({ type: "SELECT_THREAD", thread });

    const selectedFrame = getSelectedFrame(getState(), thread);

    const serverRequests = [];
    // Update the watched expressions as we may never have evaluated them against this thread
    // Note that selectedFrame may be null if the thread isn't paused.
    serverRequests.push(dispatch(evaluateExpressions(selectedFrame)));

    // If we were paused on the newly selected thread, ensure:
    // - select the source where we are paused,
    // - fetching the paused stackframes,
    // - fetching the paused scope, so that variable preview are working on the selected source.
    // (frames and scopes is supposed to be fetched on pause,
    // but if two threads pause concurrently, it might be cancelled)
    if (selectedFrame) {
      serverRequests.push(dispatch(selectLocation(selectedFrame.location)));
      serverRequests.push(dispatch(fetchFrames(thread)));

      serverRequests.push(dispatch(fetchScopes(selectedFrame)));
    }

    await Promise.all(serverRequests);
  };
}

/**
 * Debugger commands like stepOver, stepIn, stepOut, resume
 *
 * @param string type
 */
export function command(type) {
  return async ({ dispatch, getState, client }) => {
    if (!type) {
      return null;
    }
    // For now, all commands are by default against the currently selected thread
    const thread = getCurrentThread(getState());

    const frame = getSelectedFrame(getState(), thread);

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
 *
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
 *
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
 *
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
 *
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
 */
export function restart(frame) {
  return async ({ dispatch, getState, client }) => {
    if (!getIsPaused(getState(), frame.thread)) {
      return null;
    }
    validateFrame(getState(), frame);
    return dispatch({
      type: "COMMAND",
      command: "restart",
      thread: frame.thread,
      [PROMISE]: client.restart(frame.thread, frame.id),
    });
  };
}
