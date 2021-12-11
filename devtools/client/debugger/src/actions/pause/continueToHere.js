/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  getSelectedSource,
  getSelectedFrame,
  getClosestBreakpointPosition,
  getBreakpoint,
} from "../../selectors";
import { addHiddenBreakpoint } from "../breakpoints";
import { setBreakpointPositions } from "../breakpoints/breakpointPositions";

import { resume } from "./commands";

export function continueToHere(cx, location) {
  return async function({ dispatch, getState }) {
    const { line, column, sourceId } = location;
    const selectedSource = getSelectedSource(getState());
    const selectedFrame = getSelectedFrame(getState(), cx.thread);

    if (!selectedFrame || !selectedSource) {
      return;
    }

    const debugLine = selectedFrame.location.line;
    // If the user selects a line to continue to,
    // it must be different than the currently paused line.
    if (!column && debugLine == line) {
      return;
    }

    await dispatch(setBreakpointPositions({ cx, sourceId, line }));
    const position = getClosestBreakpointPosition(getState(), location);

    // If the user selects a location in the editor,
    // there must be a place we can pause on that line.
    if (column && !position) {
      return;
    }

    const pauseLocation = column && position ? position.location : location;

    // Set a hidden breakpoint if we do not already have a breakpoint
    // at the closest position
    if (!getBreakpoint(getState(), pauseLocation)) {
      await dispatch(
        addHiddenBreakpoint(cx, {
          sourceId: selectedSource.id,
          line: pauseLocation.line,
          column: pauseLocation.column,
        })
      );
    }

    dispatch(resume(cx));
  };
}
