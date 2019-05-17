/* -*- indent-tabs-mode: nil; js-indent-level: 2; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import {
  getSource,
  getSourceContent,
  getTopFrame,
  getSelectedFrame,
  getThreadContext,
} from "../../selectors";
import { PROMISE } from "../utils/middleware/promise";
import { addHiddenBreakpoint } from "../breakpoints";
import { evaluateExpressions } from "../expressions";
import { selectLocation } from "../sources";
import { fetchScopes } from "./fetchScopes";
import { features } from "../../utils/prefs";
import { recordEvent } from "../../utils/telemetry";
import assert from "../../utils/assert";
import { isFulfilled, type AsyncValue } from "../../utils/async-value";

import type {
  SourceContent,
  ThreadId,
  Context,
  ThreadContext,
} from "../../types";
import type { ThunkArgs } from "../types";
import type { Command } from "../../reducers/types";

export function selectThread(cx: Context, thread: ThreadId) {
  return async ({ dispatch, getState, client }: ThunkArgs) => {
    await dispatch({ cx, type: "SELECT_THREAD", thread });

    // Get a new context now that the current thread has changed.
    const threadcx = getThreadContext(getState());
    assert(threadcx.thread == thread, "Thread mismatch");

    dispatch(evaluateExpressions(threadcx));

    const frame = getSelectedFrame(getState(), thread);
    if (frame) {
      dispatch(selectLocation(threadcx, frame.location));
      dispatch(fetchScopes(threadcx));
    }
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
      return dispatch(astCommand(cx, "stepOver"));
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

/**
 * rewind
 * @memberof actions/pause
 * @static
 * @returns {Function} {@link command}
 */
export function rewind(cx: ThreadContext) {
  return ({ dispatch, getState }: ThunkArgs) => {
    if (cx.isPaused) {
      return dispatch(command(cx, "rewind"));
    }
  };
}

/**
 * reverseStepOver
 * @memberof actions/pause
 * @static
 * @returns {Function} {@link command}
 */
export function reverseStepOver(cx: ThreadContext) {
  return ({ dispatch, getState }: ThunkArgs) => {
    if (cx.isPaused) {
      return dispatch(astCommand(cx, "reverseStepOver"));
    }
  };
}

/*
 * Checks for await or yield calls on the paused line
 * This avoids potentially expensive parser calls when we are likely
 * not at an async expression.
 */
function hasAwait(content: AsyncValue<SourceContent> | null, pauseLocation) {
  const { line, column } = pauseLocation;
  if (!content || !isFulfilled(content) || content.value.type !== "text") {
    return false;
  }

  const lineText = content.value.value.split("\n")[line - 1];

  if (!lineText) {
    return false;
  }

  const snippet = lineText.slice(column - 50, column + 50);

  return !!snippet.match(/(yield|await)/);
}

/**
 * @memberOf actions/pause
 * @static
 * @param stepType
 * @returns {function(ThunkArgs)}
 */
export function astCommand(cx: ThreadContext, stepType: Command) {
  return async ({ dispatch, getState, sourceMaps, parser }: ThunkArgs) => {
    if (!features.asyncStepping) {
      return dispatch(command(cx, stepType));
    }

    if (stepType == "stepOver") {
      // This type definition is ambiguous:
      const frame: any = getTopFrame(getState(), cx.thread);
      const source = getSource(getState(), frame.location.sourceId);
      const content = source ? getSourceContent(getState(), source.id) : null;

      if (source && hasAwait(content, frame.location)) {
        const nextLocation = await parser.getNextStep(
          source.id,
          frame.location
        );
        if (nextLocation) {
          await dispatch(addHiddenBreakpoint(cx, nextLocation));
          return dispatch(command(cx, "resume"));
        }
      }
    }

    return dispatch(command(cx, stepType));
  };
}
