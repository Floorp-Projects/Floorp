/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { createSelector } from "../utils/createSelector";

import type {
  BreakpointsState,
  XHRBreakpointsList
} from "../reducers/breakpoints";

type OuterState = { breakpoints: BreakpointsState };

export function getXHRBreakpoints(state: OuterState): XHRBreakpointsList {
  return state.breakpoints.xhrBreakpoints;
}

export const shouldPauseOnAnyXHR = createSelector(
  getXHRBreakpoints,
  xhrBreakpoints => {
    const emptyBp = xhrBreakpoints.find(({ path }) => path.length === 0);
    if (!emptyBp) {
      return false;
    }

    return !emptyBp.disabled;
  }
);
