/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import {
  getHiddenBreakpoint,
  isEvaluatingExpression,
  getSelectedFrame,
  wasStepping
} from "../../selectors";

import { mapFrames } from ".";
import { removeBreakpoint } from "../breakpoints";
import { evaluateExpressions } from "../expressions";
import { selectLocation } from "../sources";
import { togglePaneCollapse } from "../ui";

import { fetchScopes } from "./fetchScopes";

import type { Pause } from "../../types";
import type { ThunkArgs } from "../types";

/**
 * Debugger has just paused
 *
 * @param {object} pauseInfo
 * @memberof actions/pause
 * @static
 */
export function paused(pauseInfo: Pause) {
  return async function({ dispatch, getState, client, sourceMaps }: ThunkArgs) {
    const { thread, frames, why, loadedObjects } = pauseInfo;
    const topFrame = frames.length > 0 ? frames[0] : null;

    dispatch({
      type: "PAUSED",
      thread,
      why,
      frames,
      selectedFrameId: topFrame ? topFrame.id : undefined,
      loadedObjects: loadedObjects || []
    });

    const hiddenBreakpoint = getHiddenBreakpoint(getState());
    if (hiddenBreakpoint) {
      dispatch(removeBreakpoint(hiddenBreakpoint));
    }

    await dispatch(mapFrames(thread));

    const selectedFrame = getSelectedFrame(getState(), thread);
    if (selectedFrame) {
      await dispatch(selectLocation(selectedFrame.location));
    }

    if (!wasStepping(getState(), thread)) {
      dispatch(togglePaneCollapse("end", false));
    }

    await dispatch(fetchScopes(thread));

    // Run after fetching scoping data so that it may make use of the sourcemap
    // expression mappings for local variables.
    const atException = why.type == "exception";
    if (!atException || !isEvaluatingExpression(getState(), thread)) {
      await dispatch(evaluateExpressions());
    }
  };
}
