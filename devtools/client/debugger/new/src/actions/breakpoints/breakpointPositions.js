/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import {
  getSourceActors,
  getBreakpointPositionsForLine
} from "../../selectors";

import { makeSourceActorLocation } from "../../utils/breakpoint";

import type { SourceLocation } from "../../types";
import type { ThunkArgs } from "../../actions/types";

export function setBreakpointPositions(location: SourceLocation) {
  return async ({ getState, dispatch, client }: ThunkArgs) => {
    if (
      getBreakpointPositionsForLine(
        getState(),
        location.sourceId,
        location.line
      )
    ) {
      return;
    }

    const sourceActors = getSourceActors(getState(), location.sourceId);
    const sourceActor = sourceActors[0];

    const sourceActorLocation = makeSourceActorLocation(sourceActor, location);
    const positions = await client.getBreakpointPositions(sourceActorLocation);

    return dispatch({ type: "ADD_BREAKPOINT_POSITIONS", positions, location });
  };
}
