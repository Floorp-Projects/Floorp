/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { PartialPosition } from "../../../types";

export function locColumn(loc: PartialPosition): number {
  if (typeof loc.column !== "number") {
    // This shouldn't really happen with locations from the AST, but
    // the datatype we are using allows null/undefined column.
    return 0;
  }

  return loc.column;
}
