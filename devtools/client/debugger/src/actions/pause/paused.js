/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  getHiddenBreakpoint,
  isEvaluatingExpression,
  getSelectedFrame,
  getThreadContext,
} from "../../selectors";

import { mapFrames, fetchFrames } from ".";
import { removeBreakpoint } from "../breakpoints";
import { evaluateExpressions } from "../expressions";
import { selectSpecificLocation } from "../sources";
import assert from "../../utils/assert";

import { fetchScopes } from "./fetchScopes";

/**
 * Debugger has just paused
 *
 * @param {object} pauseInfo
 * @memberof actions/pause
 * @static
 */
export function paused(pauseInfo) {
  return async function({ dispatch, getState, client, sourceMaps }) {
    const { thread, frame, why } = pauseInfo;

    dispatch({ type: "PAUSED", thread, why, frame });

    // Get a context capturing the newly paused and selected thread.
    const cx = getThreadContext(getState());
    assert(cx.thread == thread, "Thread mismatch");

    await dispatch(fetchFrames(cx));

    const hiddenBreakpoint = getHiddenBreakpoint(getState());
    if (hiddenBreakpoint) {
      dispatch(removeBreakpoint(cx, hiddenBreakpoint));
    }

    await dispatch(mapFrames(cx));

    const selectedFrame = getSelectedFrame(getState(), thread);
    if (selectedFrame) {
      await dispatch(selectSpecificLocation(cx, selectedFrame.location));
    }

    await dispatch(fetchScopes(cx));

    // Run after fetching scoping data so that it may make use of the sourcemap
    // expression mappings for local variables.
    const atException = why.type == "exception";
    if (!atException || !isEvaluatingExpression(getState(), thread)) {
      await dispatch(evaluateExpressions(cx));
    }
  };
}
