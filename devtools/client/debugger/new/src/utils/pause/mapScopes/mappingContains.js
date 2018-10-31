/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { Position } from "../../../types";
import { positionCmp } from "./positionCmp";

export function mappingContains(
  mapped: { +start: Position, +end: Position },
  item: { +start: Position, +end: Position }
) {
  return (
    positionCmp(item.start, mapped.start) >= 0 &&
    positionCmp(item.end, mapped.end) <= 0
  );
}
