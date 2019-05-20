/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { isOriginalId } from "devtools-source-map";
import { getSourceActorsForSource, getBreakableLines } from "../../selectors";
import { setBreakpointPositions } from "../breakpoints/breakpointPositions";
import { union } from "lodash";
import type { Context } from "../../types";
import type { ThunkArgs } from "../../actions/types";

function calculateBreakableLines(positions) {
  const lines = [];
  for (const line in positions) {
    if (positions[line].length > 0) {
      lines.push(Number(line));
    }
  }

  return lines;
}

export function setBreakableLines(cx: Context, sourceId: string) {
  return async ({ getState, dispatch, client }: ThunkArgs) => {
    let breakableLines;
    if (isOriginalId(sourceId)) {
      const positions = await dispatch(
        setBreakpointPositions({ cx, sourceId })
      );
      breakableLines = calculateBreakableLines(positions);
    } else {
      breakableLines = await client.getBreakableLines(
        getSourceActorsForSource(getState(), sourceId)
      );
    }

    const existingBreakableLines = getBreakableLines(getState(), sourceId);
    if (existingBreakableLines) {
      breakableLines = union(existingBreakableLines, breakableLines);
    }

    dispatch({
      type: "SET_BREAKABLE_LINES",
      cx,
      sourceId,
      breakableLines,
    });
  };
}
