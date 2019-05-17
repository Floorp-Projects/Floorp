/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { createSelector } from "reselect";

import type {
  BreakpointsState,
  XHRBreakpointsList,
} from "../reducers/breakpoints";
import type { Selector } from "../reducers/types";
import type { Breakpoint } from "../types";

type OuterState = { breakpoints: BreakpointsState };

export function getXHRBreakpoints(state: OuterState): XHRBreakpointsList {
  return state.breakpoints.xhrBreakpoints;
}

export const shouldPauseOnAnyXHR: Selector<boolean> = createSelector(
  getXHRBreakpoints,
  xhrBreakpoints => {
    const emptyBp = xhrBreakpoints.find(({ path }) => path.length === 0);
    if (!emptyBp) {
      return false;
    }

    return !emptyBp.disabled;
  }
);

export const getBreakpointsList: Selector<Breakpoint[]> = createSelector(
  (state: OuterState) => state.breakpoints.breakpoints,
  breakpoints => (Object.values(breakpoints): any)
);
