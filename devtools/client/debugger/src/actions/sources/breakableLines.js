/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  getBreakableLines,
  getSourceActorBreakableLines,
} from "../../selectors";
import { setBreakpointPositions } from "../breakpoints/breakpointPositions";

function calculateBreakableLines(positions) {
  const lines = [];
  for (const line in positions) {
    if (positions[line].length) {
      lines.push(Number(line));
    }
  }

  return lines;
}

/**
 * Ensure that breakable lines for a given source are fetched.
 *
 * @param Object location
 */
export function setBreakableLines(location) {
  return async ({ getState, dispatch, client }) => {
    let breakableLines;
    if (location.source.isOriginal) {
      const positions = await dispatch(setBreakpointPositions(location));
      breakableLines = calculateBreakableLines(positions);

      const existingBreakableLines = getBreakableLines(
        getState(),
        location.source.id
      );
      if (existingBreakableLines) {
        breakableLines = [
          ...new Set([...existingBreakableLines, ...breakableLines]),
        ];
      }

      dispatch({
        type: "SET_ORIGINAL_BREAKABLE_LINES",
        source: location.source,
        breakableLines,
      });
    } else {
      // Ignore re-fetching the breakable lines for source actor we already fetched
      breakableLines = getSourceActorBreakableLines(
        getState(),
        location.sourceActor.id
      );
      if (breakableLines) {
        return;
      }
      breakableLines = await client.getSourceActorBreakableLines(
        location.sourceActor
      );
      dispatch({
        type: "SET_SOURCE_ACTOR_BREAKABLE_LINES",
        sourceActor: location.sourceActor,
        breakableLines,
      });
    }
  };
}
