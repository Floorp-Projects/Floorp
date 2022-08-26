/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { isOriginalId } from "devtools-source-map";
import { getSourceActorsForSource, getBreakableLines } from "../../selectors";
import { setBreakpointPositions } from "../breakpoints/breakpointPositions";
import { loadSourceActorBreakableLines } from "../source-actors";

function calculateBreakableLines(positions) {
  const lines = [];
  for (const line in positions) {
    if (positions[line].length) {
      lines.push(Number(line));
    }
  }

  return lines;
}

export function setBreakableLines(cx, sourceId) {
  return async ({ getState, dispatch, client }) => {
    let breakableLines;
    if (isOriginalId(sourceId)) {
      const positions = await dispatch(
        setBreakpointPositions({ cx, sourceId })
      );
      breakableLines = calculateBreakableLines(positions);

      const existingBreakableLines = getBreakableLines(getState(), sourceId);
      if (existingBreakableLines) {
        breakableLines = [
          ...new Set([...existingBreakableLines, ...breakableLines]),
        ];
      }

      dispatch({
        type: "SET_ORIGINAL_BREAKABLE_LINES",
        cx,
        sourceId,
        breakableLines,
      });
    } else {
      const actors = getSourceActorsForSource(getState(), sourceId);

      await Promise.all(
        actors.map(({ id }) =>
          dispatch(loadSourceActorBreakableLines({ id, cx }))
        )
      );
    }
  };
}
