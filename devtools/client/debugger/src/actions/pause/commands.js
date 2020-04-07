/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import {
  getSelectedFrame,
  getThreadContext,
  getCurrentThread,
} from "../../selectors";
import { PROMISE } from "../utils/middleware/promise";
import { evaluateExpressions } from "../expressions";
import { selectLocation } from "../sources";
import { fetchScopes } from "./fetchScopes";
import { fetchFrames } from "./fetchFrames";
import { recordEvent } from "../../utils/telemetry";
import assert from "../../utils/assert";

import type { ThreadId, Context, ThreadContext } from "../../types";

import type { ThunkArgs } from "../types";
import type { Command } from "../../reducers/types";

export function selectThread(cx: Context, thread: ThreadId) {
  return async ({ dispatch, getState, client }: ThunkArgs) => {
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
export function command(cx: ThreadContext, type: Command) {
  return async ({ dispatch, getState, client }: ThunkArgs) => {
    if (type) {
      return dispatch({
        type: "COMMAND",
        command: type,
        cx,
        thread: cx.thread,
        [PROMISE]: client[type](cx.thread),
      });
    }
  };
}

/**
 * StepIn
 * @memberof actions/pause
 * @static
 * @returns {Function} {@link command}
 */
export function stepIn(cx: ThreadContext) {
  return ({ dispatch, getState }: ThunkArgs) => {
    if (cx.isPaused) {
      return dispatch(command(cx, "stepIn"));
    }
  };
}

/**
 * stepOver
 * @memberof actions/pause
 * @static
 * @returns {Function} {@link command}
 */
export function stepOver(cx: ThreadContext) {
  return ({ dispatch, getState }: ThunkArgs) => {
    if (cx.isPaused) {
      return dispatch(command(cx, "stepOver"));
    }
  };
}

/**
 * stepOut
 * @memberof actions/pause
 * @static
 * @returns {Function} {@link command}
 */
export function stepOut(cx: ThreadContext) {
  return ({ dispatch, getState }: ThunkArgs) => {
    if (cx.isPaused) {
      return dispatch(command(cx, "stepOut"));
    }
  };
}

/**
 * resume
 * @memberof actions/pause
 * @static
 * @returns {Function} {@link command}
 */
export function resume(cx: ThreadContext) {
  return ({ dispatch, getState }: ThunkArgs) => {
    if (cx.isPaused) {
      recordEvent("continue");
      return dispatch(command(cx, "resume"));
    }
  };
}
