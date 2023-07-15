/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  getHiddenBreakpoint,
  isEvaluatingExpression,
  getSelectedFrame,
} from "../../selectors";

import { mapFrames, fetchFrames } from ".";
import { removeBreakpoint } from "../breakpoints";
import { evaluateExpressions } from "../expressions";
import { selectLocation } from "../sources";
import { validateSelectedFrame } from "../../utils/context";

import { fetchScopes } from "./fetchScopes";

/**
 * Debugger has just paused
 *
 * @param {object} pauseInfo
 *        See `createPause` method.
 */
export function paused(pauseInfo) {
  return async function ({ dispatch, getState }) {
    const { thread, frame, why } = pauseInfo;

    dispatch({ type: "PAUSED", thread, why, topFrame: frame });

    // When we use "continue to here" feature we register an "hidden" breakpoint
    // that should be removed on the next paused, even if we didn't hit it and
    // paused for any other reason.
    const hiddenBreakpoint = getHiddenBreakpoint(getState());
    if (hiddenBreakpoint) {
      dispatch(removeBreakpoint(hiddenBreakpoint));
    }

    // The THREAD_STATE's "paused" resource only passes the top level stack frame,
    // we dispatch the PAUSED action with it so that we can right away
    // display it and update the UI to be paused.
    // But we then fetch all the other frames:
    await dispatch(fetchFrames(thread));
    // And map them to original source locations.
    // Note that this will wait for all related original sources to be loaded in the reducers.
    // So this step may pause for a little while.
    await dispatch(mapFrames(thread));

    // If we paused on a particular frame, automatically select the related source
    // and highlight the paused line
    const selectedFrame = getSelectedFrame(getState(), thread);
    if (selectedFrame) {
      await dispatch(selectLocation(selectedFrame.location));
      // We might have resumed while opening the location.
      // Prevent further computation if this happens.
      validateSelectedFrame(getState(), selectedFrame);

      // Fetch the previews for variables visible in the currently selected paused stackframe
      await dispatch(fetchScopes(selectedFrame));

      // Run after fetching scoping data so that it may make use of the sourcemap
      // expression mappings for local variables.
      const atException = why.type == "exception";
      if (!atException || !isEvaluatingExpression(getState(), thread)) {
        await dispatch(evaluateExpressions(selectedFrame));
      }
    }
  };
}
