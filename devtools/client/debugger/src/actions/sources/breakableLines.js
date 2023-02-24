/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { isOriginalId } from "devtools/client/shared/source-map-loader/index";
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
 * @param Object cx
 * @param Object source
 * @param Object sourceActor (optional)
 *        If one particular source actor is to be fetched.
 *        Otherwise the first available one will be picked,
 *        or all the source actors in case of HTML sources.
 */
export function setBreakableLines(cx, source, sourceActor) {
  return async ({ getState, dispatch, client }) => {
    let breakableLines;
    if (isOriginalId(source.id)) {
      const positions = await dispatch(
        setBreakpointPositions({ cx, sourceId: source.id })
      );
      breakableLines = calculateBreakableLines(positions);

      const existingBreakableLines = getBreakableLines(getState(), source.id);
      if (existingBreakableLines) {
        breakableLines = [
          ...new Set([...existingBreakableLines, ...breakableLines]),
        ];
      }

      dispatch({
        type: "SET_ORIGINAL_BREAKABLE_LINES",
        cx,
        sourceId: source.id,
        breakableLines,
      });
    } else {
      // Ignore re-fetching the breakable lines for source actor we already fetched
      breakableLines = getSourceActorBreakableLines(getState(), sourceActor.id);
      if (breakableLines) {
        return;
      }
      breakableLines = await client.getSourceActorBreakableLines(sourceActor);
      await dispatch({
        type: "SET_SOURCE_ACTOR_BREAKABLE_LINES",
        sourceActorId: sourceActor.id,
        breakableLines,
      });
    }
  };
}
