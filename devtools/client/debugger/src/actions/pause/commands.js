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

    await dispatch({ cx, type: "SELECT_THREAD", thread });

    // Get a new context now that the current thread has changed.
    const threadcx = getThreadContext(getState());
    assert(threadcx.thread == thread, "Thread mismatch");

    const serverRequests = [];
    serverRequests.push(dispatch(evaluateExpressions(threadcx)));

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
      return;
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
    if (getIsCurrentThreadPaused(getState())) {
      return dispatch(command("stepIn"));
    }
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
    if (getIsCurrentThreadPaused(getState())) {
      return dispatch(command("stepOver"));
    }
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
    if (getIsCurrentThreadPaused(getState())) {
      return dispatch(command("stepOut"));
    }
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
    if (getIsCurrentThreadPaused(getState())) {
      recordEvent("continue");
      return dispatch(command("resume"));
    }
  };
}

/**
 * restart frame
 * @memberof actions/pause
 * @static
 */
export function restart(cx, frame) {
  return async ({ dispatch, getState, client }) => {
    if (getIsCurrentThreadPaused(getState())) {
      return dispatch({
        type: "COMMAND",
        command: "restart",
        thread: cx.thread,
        [PROMISE]: client.restart(cx.thread, frame.id),
      });
    }
  };
}
