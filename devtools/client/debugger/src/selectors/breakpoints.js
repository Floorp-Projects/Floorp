/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { createSelector } from "reselect";

export function getXHRBreakpoints(state) {
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

export const getBreakpointsList = createSelector(
  state => state.breakpoints.breakpoints,
  breakpoints => Object.values(breakpoints)
);
