// @flow

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { comparePosition } from "../location";
import { getSelectedLocation } from "../selected-location";
import type {
  BreakpointPosition,
  BreakpointPositions,
  SourceLocation,
} from "../../types";

export function findPosition(
  positions: ?BreakpointPositions,
  location: SourceLocation
): ?BreakpointPosition {
  if (!positions) {
    return null;
  }

  const lineBps = positions[location.line];
  if (!lineBps) {
    return null;
  }
  return lineBps.find(pos =>
    comparePosition(getSelectedLocation(pos, location), location)
  );
}
