/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { PartialPosition } from "../../../types";
import { locColumn } from "./locColumn";

/**
 * * === 0 - Positions are equal.
 * * < 0 - first position before second position
 * * > 0 - first position after second position
 */
export function positionCmp(p1: PartialPosition, p2: PartialPosition): number {
  if (p1.line === p2.line) {
    const l1 = locColumn(p1);
    const l2 = locColumn(p2);

    if (l1 === l2) {
      return 0;
    }
    return l1 < l2 ? -1 : 1;
  }

  return p1.line < p2.line ? -1 : 1;
}
