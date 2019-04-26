/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { getSelectedLocation } from "./selected-location";
import type { BreakpointPositions, Source } from "../types";

export function findBreakableLines(
  source: Source,
  breakpointPositions: BreakpointPositions
): number[] {
  if (!breakpointPositions || source.isWasm) {
    return [];
  }

  return Array.from(
    new Set(
      breakpointPositions.map(point => getSelectedLocation(point, source).line)
    )
  );
}
