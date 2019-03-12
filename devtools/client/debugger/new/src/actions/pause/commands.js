/* -*- indent-tabs-mode: nil; js-indent-level: 2; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import {
  getIsPaused,
  getCurrentThread,
  getSource,
  getTopFrame,
  getSelectedFrame
} from "../../selectors";
import { PROMISE } from "../utils/middleware/promise";
import { getNextStep } from "../../workers/parser";
import { addHiddenBreakpoint } from "../breakpoints";
import { evaluateExpressions } from "../expressions";
import { selectLocation } from "../sources";
import { features } from "../../utils/prefs";
import { recordEvent } from "../../utils/telemetry";

import type { Source, ThreadId } from "../../types";
import type { ThunkArgs } from "../types";
import type { Command } from "../../reducers/types";

export function selectThread(thread: ThreadId) {
  return async ({ dispatch, getState, client }: ThunkArgs) => {
    await dispatch({ type: "SELECT_THREAD", thread });
    dispatch(evaluateExpressions());

    const frame = getSelectedFrame(getState(), thread);
    if (frame) {
      dispatch(selectLocation(frame.location));
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
export function command(thread: ThreadId, type: Command) {
  return async ({ dispatch, getState, client }: ThunkArgs) => {
    if (type) {
      return dispatch({
        type: "COMMAND",
        command: type,
        thread,
        [PROMISE]: client[type](thread)
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
export function stepIn() {
  return ({ dispatch, getState }: ThunkArgs) => {
    const thread = getCurrentThread(getState());
    if (getIsPaused(getState(), thread)) {
      return dispatch(command(thread, "stepIn"));
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
  return ({ dispatch, getState }: ThunkArgs) => {
    const thread = getCurrentThread(getState());
    if (getIsPaused(getState(), thread)) {
      return dispatch(astCommand(thread, "stepOver"));
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
  return ({ dispatch, getState }: ThunkArgs) => {
    const thread = getCurrentThread(getState());
    if (getIsPaused(getState(), thread)) {
      return dispatch(command(thread, "stepOut"));
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
  return ({ dispatch, getState }: ThunkArgs) => {
    const thread = getCurrentThread(getState());
    if (getIsPaused(getState(), thread)) {
      recordEvent("continue");
      return dispatch(command(thread, "resume"));
    }
  };
}

/**
 * rewind
 * @memberof actions/pause
 * @static
 * @returns {Function} {@link command}
 */
export function rewind() {
  return ({ dispatch, getState }: ThunkArgs) => {
    const thread = getCurrentThread(getState());
    if (getIsPaused(getState(), thread)) {
      return dispatch(command(thread, "rewind"));
    }
  };
}

/**
 * reverseStepIn
 * @memberof actions/pause
 * @static
 * @returns {Function} {@link command}
 */
export function reverseStepIn() {
  return ({ dispatch, getState }: ThunkArgs) => {
    const thread = getCurrentThread(getState());
    if (getIsPaused(getState(), thread)) {
      return dispatch(command(thread, "reverseStepIn"));
    }
  };
}

/**
 * reverseStepOver
 * @memberof actions/pause
 * @static
 * @returns {Function} {@link command}
 */
export function reverseStepOver() {
  return ({ dispatch, getState }: ThunkArgs) => {
    const thread = getCurrentThread(getState());
    if (getIsPaused(getState(), thread)) {
      return dispatch(astCommand(thread, "reverseStepOver"));
    }
  };
}

/**
 * reverseStepOut
 * @memberof actions/pause
 * @static
 * @returns {Function} {@link command}
 */
export function reverseStepOut() {
  return ({ dispatch, getState }: ThunkArgs) => {
    const thread = getCurrentThread(getState());
    if (getIsPaused(getState(), thread)) {
      return dispatch(command(thread, "reverseStepOut"));
    }
  };
}

/*
 * Checks for await or yield calls on the paused line
 * This avoids potentially expensive parser calls when we are likely
 * not at an async expression.
 */
function hasAwait(source: Source, pauseLocation) {
  const { line, column } = pauseLocation;
  if (source.isWasm || !source.text) {
    return false;
  }

  const lineText = source.text.split("\n")[line - 1];

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
export function astCommand(thread: ThreadId, stepType: Command) {
  return async ({ dispatch, getState, sourceMaps }: ThunkArgs) => {
    if (!features.asyncStepping) {
      return dispatch(command(thread, stepType));
    }

    if (stepType == "stepOver") {
      // This type definition is ambiguous:
      const frame: any = getTopFrame(getState(), thread);
      const source = getSource(getState(), frame.location.sourceId);

      if (source && hasAwait(source, frame.location)) {
        const nextLocation = await getNextStep(source.id, frame.location);
        if (nextLocation) {
          await dispatch(addHiddenBreakpoint(nextLocation));
          return dispatch(command(thread, "resume"));
        }
      }
    }

    return dispatch(command(thread, stepType));
  };
}
