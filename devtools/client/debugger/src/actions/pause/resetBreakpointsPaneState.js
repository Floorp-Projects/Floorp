/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Action for the breakpoints panel while paused.
 *
 * @memberof actions/pause
 * @static
 */
export function resetBreakpointsPaneState(thread) {
  return async ({ dispatch }) => {
    dispatch({
      type: "RESET_BREAKPOINTS_PANE_STATE",
      thread,
    });
  };
}
