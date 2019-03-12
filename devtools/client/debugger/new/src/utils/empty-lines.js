/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { xor, range } from "lodash";
import { getSelectedLocation } from "./source-maps";
import type { BreakpointPositions, Source } from "../types";

export function findEmptyLines(
  source: Source,
  breakpointPositions: BreakpointPositions
): number[] {
  if (!breakpointPositions || source.isWasm) {
    return [];
  }

  const sourceText = source.text || "";
  const lineCount = sourceText.split("\n").length;
  const sourceLines = range(1, lineCount + 1);

  const breakpointLines = breakpointPositions
    .map(point => getSelectedLocation(point, source).line)
    // NOTE: at the moment it is possible the location is an unmapped generated
    // line which could be greater than the line count.
    .filter(line => line <= lineCount);

  return xor(sourceLines, breakpointLines);
}
